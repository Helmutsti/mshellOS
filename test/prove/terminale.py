"""terminale: il Terminale (libvterm dentro) mostra la shell e prende i tasti.

Si apre il Terminale dal vassoio delle app (non e' nella userbar) e si guarda la finestra:
  1. la shell dice il suo avviso e il prompt: nella finestra c'e' del testo e il cursore;
  2. "help" e Invio: sotto il prompt compare l'elenco dei comandi (la griglia di libvterm
     si e' aggiornata e le righe cambiate si sono ridisegnate);
  3. un tasto fisico (freccia su) e Ctrl+C: la finestra resta viva (il Ctrl+C da' una riga
     nuova con il prompt).
Le foto della finestra restano nella cartella della prova (con --tieni).
"""

import time

import scrivania as s
from qemu import PUNTO_VUOTO, pixel_diversi

NOME = "terminale"
ORDINE = 45
DESCRIZIONE = "il Terminale mostra la shell, l'elenco di help e prende i tasti"

FINESTRA = (140, 130, 140 + 760, 130 + 588)


def _dentro(foto):
    """La parte del pannello dove sta il testo (senza il vetro della finestra)."""
    return foto.crop((148, 180, 892, 710))


def esegui(p):
    m = p.m
    m.clic(*s.APP_PULSANTE)                    # il vassoio
    time.sleep(0.4)
    m.clic(*s.VASSOIO["Terminale"])
    m.muovi(*PUNTO_VUOTO)
    # la finestra si apre con la shell: aspetta che nel pannello ci sia del testo
    prima = m.aspetta_che(lambda f: pixel_diversi(_dentro(f), _dentro(p.scrivania), None, 40) > 300,
                          20.0, cosa="il testo della shell nel Terminale")
    avvio = m.aspetta_schermo_fermo()
    avvio.crop(FINESTRA).save(p.cartella / "terminale_avvio.png")

    # help + Invio
    for t in "help":
        m.premi(t)
    m.premi("ret")
    dopo = m.aspetta_che(lambda f: pixel_diversi(_dentro(f), _dentro(avvio), None, 40) > 500,
                         10.0, cosa="l'elenco di help")
    dopo = m.aspetta_schermo_fermo()
    dopo.crop(FINESTRA).save(p.cartella / "terminale_help.png")

    # freccia su e Ctrl+C: la finestra deve restare viva e rispondere
    m.premi("up")
    m.tasto_giu("ctrl")
    m.premi("c")
    m.tasto_su("ctrl")
    time.sleep(0.5)
    fine = m.aspetta_schermo_fermo()
    fine.crop(FINESTRA).save(p.cartella / "terminale_ctrl_c.png")
    p.controlla(pixel_diversi(_dentro(fine), _dentro(dopo), None, 40) > 20,
                "dopo Ctrl+C la finestra non e' cambiata (manca la riga nuova)")
    p.nota("foto in " + str(p.cartella))
