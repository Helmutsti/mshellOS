"""picker: il servizio che consegna all'app il file scelto dalla persona (corekit/picker.h).

Si accende disco_picker.img (build.sh): come disco_app.img, ma il picker del sistema e' legato
con la finestra FINTA, senza grafica (utente/prove/picker_finestra_finta.c), che "sceglie" da
sola leggendo /data/tests/window/choice: la cartella Documenti, e in Apri il file prova.txt.
In /data/tests c'e' anche l'osservatore (utente/prove/picker.c), che guarda che fuori.txt non
compaia mai ne' nella Home ne' fuori. La finestra vera (UIKit) qui non c'e': questa prova
controlla il servizio (porta, sicurezza, file consegnato), non l'aspetto.

Si apre Esempio (che non ha nessun permesso sui file) e si premono i suoi tasti (esempio.c):
  1. "s": Salva con il nome proposto prova.txt. Esempio riceve il file aperto, ci scrive "ciao"
     e dice "salvato prova.txt"; l'osservatore vede Documenti/prova.txt;
  2. "o": Apri. La finestra finta elenca Documenti (con il servizio: openat2 sotto la Home),
     trova prova.txt e lo sceglie; Esempio legge 4 byte;
  3. "m": Salva con il nome proposto ../fuori.txt: il servizio lo rifiuta (invalid) senza
     mostrare la finestra, e nessun fuori.txt compare, ne' nella Home ne' fuori.
Il collegamento arriva dalla porta di Esempio (la porta 7, data dal launcher a tutte le app).
"""

import time

import scrivania as s
from qemu import ErroreProva

NOME = "picker"
ORDINE = 52
DESCRIZIONE = "servizio picker: Salva e Apri da Esempio con la finestra finta, ../fuori.txt rifiutato"
DISCO = "disco_picker.img"
SOLO_QEMU = "il picker con la finestra finta sta solo in disco_picker.img (build.sh)"


def _aspetta(m, testo, attesa=30.0):
    fine = time.time() + attesa
    while testo not in m.testo_seriale():
        if time.time() > fine:
            righe = [r for r in m.testo_seriale().splitlines() if "picker" in r or "esempio" in r]
            raise ErroreProva(f"dopo {attesa:.0f} s nella seriale manca '{testo}':\n" + "\n".join(righe))
        time.sleep(0.25)


def esegui(p):
    m = p.m
    _aspetta(m, "prova picker: osservo la Home")
    s.apri(m, "Esempio")

    m.premi("s")
    _aspetta(m, "esempio: salvato prova.txt\n")
    _aspetta(m, "prova picker: prova.txt c'e'")

    m.premi("o")
    _aspetta(m, "esempio: aperto prova.txt, 4 byte\n")

    m.premi("m")
    _aspetta(m, "esempio: salva ../fuori.txt: ")
    time.sleep(2.0)                      # l'osservatore guarda ogni 200 ms
    seriale = m.testo_seriale()
    p.controlla("esempio: salva ../fuori.txt: non valido\n" in seriale, "../fuori.txt non e' stato rifiutato")
    p.controlla("picker: nome proposto non valido da /system/apps/esempio/code" in seriale,
                "il picker non ha scritto il rifiuto di ../fuori.txt")
    sbagliati = [r for r in seriale.splitlines() if r.startswith("prova picker:") and "FALLITA" in r]
    p.controlla(not sbagliati, "fuori dalla Home: " + "; ".join(sbagliati))

    # dalla porta di Esempio, e la finestra (finta) sopra Esempio
    p.controlla("picker: collegamento dalla porta di /system/apps/esempio/code" in seriale,
                "il picker non ha ricevuto il collegamento di Esempio dalla sua porta")
    p.controlla("picker finto: Salva sopra /system/apps/esempio/code" in seriale,
                "la finestra di Salva non era sopra Esempio")
    p.controlla("picker finto: Apri sopra /system/apps/esempio/code" in seriale,
                "la finestra di Apri non era sopra Esempio")
    # il rifiuto arriva prima della finestra: una Salva sola
    p.controlla(seriale.count("picker finto: Salva sopra") == 1,
                "per ../fuori.txt la finestra e' stata mostrata lo stesso")
