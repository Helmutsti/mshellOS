"""wayland_appunti: copia e incolla tra programmi Wayland (wl_data_device_manager).

Il disco ha due copie del client di prova (utente/prove/wayland.c): wayland_copia, che appena ha la
finestra mette come selezione il testo "ciao dagli appunti" (tipo text/plain;charset=utf-8), e
wayland_incolla, che si apre 3 secondi dopo (la sua finestra e' l'ultima, quindi ha il fuoco della
tastiera) e scrive nel registro quello che riceve.
  1. Chi copia dice "copiato" e l'altro, prendendo il fuoco, riceve la selezione e legge dal pipe
     il testo giusto: "appunti ricevuto: ciao dagli appunti"; chi copia ha dato il dato.
  2. Il tasto 1 (all'incolla, che ha il fuoco) mette una selezione sua: chi copia riceve "cancelled" ed esce.
  3. Il tasto 2 distrugge la sorgente: l'incolla riceve la selezione vuota ("appunti selezione vuota").
  4. Niente "FALLITA" nel registro.
"""

import time

from qemu import PUNTO_VUOTO, ErroreProva

NOME = "wayland_appunti"
ORDINE = 89
DESCRIZIONE = "copia e incolla tra due client Wayland: ricevi il testo, selezione sostituita, selezione vuota"
DISCO = "disco_wayland_appunti.img"


def _righe(m, da):
    testo = m.testo_seriale()[da:]
    return [x.split("wayland: ", 1)[1].strip() for x in testo.splitlines() if "wayland: " in x]


def _aspetta(m, da, condizione, cosa, attesa=30.0):
    fine = time.time() + attesa
    while time.time() < fine:
        if condizione(_righe(m, da)):
            return
        time.sleep(0.2)
    raise ErroreProva(f"dopo {attesa:.0f} s {cosa}: {_righe(m, da)}")


def esegui(p):
    m = p.m
    m.muovi(*PUNTO_VUOTO)
    # 1. copia e incolla
    _aspetta(m, 0, lambda l: "appunti copiato: ciao dagli appunti" in l, "chi copia non ha messo la selezione")
    _aspetta(m, 0, lambda l: "appunti ricevuto: ciao dagli appunti" in l, "l'altro non ha ricevuto il testo giusto")
    p.controlla("appunti dato a chi lo chiede: text/plain;charset=utf-8" in _righe(m, 0),
                "chi copia non ha dato il dato (send)")
    m.aspetta_schermo_fermo()
    m.foto("due_finestre")
    # 2. un'altra selezione sostituisce la prima
    da = len(m.testo_seriale())
    m.premi("1")
    _aspetta(m, da, lambda l: "appunti copiato: secondo testo" in l, "il tasto 1 non ha messo la selezione")
    _aspetta(m, da, lambda l: "appunti sorgente cancellata" in l, "chi copiava prima non e' stato avvisato (cancelled)")
    _aspetta(m, da, lambda l: "appunti selezione propria" in l, "l'altro non ha ricevuto la sua selezione")
    # 3. la sorgente distrutta: selezione vuota
    da = len(m.testo_seriale())
    m.premi("2")
    _aspetta(m, da, lambda l: "appunti selezione vuota" in l, "la selezione non e' diventata vuota")
    p.controlla("FALLITA" not in m.testo_seriale(),
                "un client ha avuto un errore: " + "; ".join(v for v in m.testo_seriale().splitlines() if "FALLITA" in v))
