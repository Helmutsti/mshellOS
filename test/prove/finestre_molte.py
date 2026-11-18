"""finestre_molte: con tante finestre aperte il compositore resta in piedi: non c'e' un tetto.

Il compositore non ha piu' un numero massimo di finestre: i posti stanno in tabelle che crescono
e la memoria di ogni finestra (ombra, buchi, vetro) si prende della sua misura quando si apre e
si restituisce quando si chiude. Il limite e' la memoria: se finisce, il collegamento nuovo si
rifiuta con una riga nel registro e il compositore resta (prima il tetto era 8).
  1. Il disco ha otto client Wayland di prova (wayland_1 ... wayland_8, "Prova Wayland"): init li
     fa partire, e ognuno apre la sua finestra. Con la scrivania (lo sfondo e la barra) sono 10
     finestre, gia' oltre il vecchio tetto.
  2. Si aprono dalla userbar tutte le 7 app, Doom compreso: 17 finestre in tutto, e 7 porte.
     Ogni app e' segnata "aperta" nella userbar, e la userbar si vede ancora.
  3. Nel registro non c'e' "troppe finestre" ne' "memoria finita", nessun client Wayland ha
     perso la finestra ("wayland: chiusa") e init non ha fatto ripartire nessun servizio.
"""

import time

import scrivania as s
from qemu import PUNTO_VUOTO

NOME = "finestre_molte"
ORDINE = 21
DISCO = "disco_finestre_molte.img"   # otto client Wayland di prova + la userbar con Esempio (scrivania.py)
DESCRIZIONE = "8 client Wayland + 7 app + scrivania = 17 finestre: il compositore resta, nessun tetto"

APPS = ["Attivita", "BloccoNote", "Esempio", "Files", "Impostazioni", "Store"]   # e Doom, per ultimo
CLIENT_WAYLAND = 8


def _mostrate(m):
    return m.testo_seriale().count("wayland: finestra mostrata")


def esegui(p):
    m = p.m
    # 1. le finestre Wayland, tutte
    fine = time.time() + 60.0
    while _mostrate(m) < CLIENT_WAYLAND and time.time() < fine:
        time.sleep(0.5)
    p.controlla(_mostrate(m) == CLIENT_WAYLAND,
                f"i client Wayland con la finestra sono {_mostrate(m)}, dovevano essere {CLIENT_WAYLAND}")
    f = m.aspetta_schermo_fermo()
    m.foto("wayland")
    p.controlla(s.userbar_visibile(f), "con otto finestre Wayland la userbar non si vede piu'")

    # 2. le app della userbar
    for app in APPS:
        s.apri(m, app, attesa=60.0)
    # Doom per ultimo e senza aspettare lo schermo fermo: in QEMU anima il suo schermo senza smettere
    s.clic_app(m, "Doom")
    m.muovi(*PUNTO_VUOTO)
    f = m.aspetta_che(lambda f: s.segno_sotto(f, "Doom") > 0, 60.0, cosa="Doom aperta nella userbar")
    time.sleep(1.0)
    m.foto("diciassette")
    for app in APPS + ["Doom"]:
        p.controlla(s.segno_sotto(f, app) > 0, f"{app} non risulta aperta nella userbar")
    p.controlla(s.userbar_visibile(f), "con 17 finestre la userbar non si vede piu'")

    # 3. il registro
    seriale = m.testo_seriale()
    p.controlla("troppe finestre" not in seriale, "il compositore ha rifiutato una finestra")
    p.controlla("memoria finita" not in seriale, "il compositore dice di avere finito la memoria")
    p.controlla("wayland: chiusa" not in seriale, "un client Wayland ha perso la sua finestra")
    p.controlla("lo faccio ripartire" not in seriale, "init ha fatto ripartire un servizio")
    p.controlla(_mostrate(m) == CLIENT_WAYLAND, "le finestre Wayland sono state rifatte (il compositore e' ripartito?)")
