"""terminale: il Terminale (libvterm dentro) mostra la shell e prende i tasti.

Si apre il Terminale dal vassoio delle app (non e' nella userbar) e si guarda la finestra:
  1. la shell dice il suo avviso e il prompt: nella finestra c'e' del testo e il cursore;
  2. "help" e Invio: sotto il prompt compare l'elenco dei comandi (la griglia di libvterm
     si e' aggiornata e le righe cambiate si sono ridisegnate);
  3. un tasto fisico (freccia su) e Ctrl+C: la finestra resta viva (il Ctrl+C da' una riga
     nuova con il prompt);
  4. le lettere accentate (tastiera italiana, UTF-8): "a' e' E' i' o' u'" e l'euro (Alt Gr+e)
     si scrivono e si vedono, e il tasto di cancellazione toglie un carattere intero (7
     volte: la riga torna com'era);
  5. un tasto fisico tenuto giu' (la freccia su) si ripete come un carattere: la shell
     riceve molte volte "ESC [ A" e il kernel le ripete a video ("^[[A"), tante di piu'
     che con una pressione sola.
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

    # 4. lettere accentate e cancellazione di un carattere intero
    m.premi("ret")                             # una riga pulita
    base = m.aspetta_schermo_fermo()
    m.premi("apostrophe")                      # a'
    m.premi("bracket_left")                    # e'
    m.tasto_giu("shift"); m.premi("bracket_left"); m.tasto_su("shift")   # E' (e acuta)
    m.premi("equal")                           # i'
    m.premi("semicolon")                       # o'
    m.premi("backslash")                       # u'
    m.tasto_giu("alt_r"); m.premi("e"); m.tasto_su("alt_r")              # l'euro
    scritto = m.aspetta_che(lambda f: pixel_diversi(_dentro(f), _dentro(base), None, 40) > 100,
                            5.0, cosa="le lettere accentate scritte")
    scritto = m.aspetta_schermo_fermo()
    scritto.crop(FINESTRA).save(p.cartella / "terminale_accenti.png")
    for _ in range(7):
        m.premi("backspace")
    time.sleep(0.3)
    pulito = m.aspetta_schermo_fermo()
    p.controlla(pixel_diversi(_dentro(pulito), _dentro(base), None, 40) < 10,
                "dopo 7 cancellazioni la riga non e' tornata com'era (il tasto cancella un byte, non un carattere?)")

    # 5. un tasto fisico tenuto giu' si ripete
    m.premi("up")
    una = m.aspetta_schermo_fermo()
    n_una = pixel_diversi(_dentro(una), _dentro(pulito), None, 40)
    m.tasto_giu("ctrl"); m.premi("c"); m.tasto_su("ctrl")                 # riga nuova
    time.sleep(0.4)
    prima = m.aspetta_schermo_fermo()
    m.tasto_giu("up")
    try:
        time.sleep(1.6)                        # mezzo secondo, poi circa 30 al secondo
    finally:
        m.tasto_su("up")
    tenuta = m.aspetta_schermo_fermo()
    tenuta.crop(FINESTRA).save(p.cartella / "terminale_freccia_tenuta.png")
    n_tenuta = pixel_diversi(_dentro(tenuta), _dentro(prima), None, 40)
    p.controlla(n_una > 20, "la freccia premuta una volta non ha scritto niente")
    p.controlla(n_tenuta > 3 * n_una,
                f"la freccia tenuta giu' non si ripete (una pressione: {n_una} pixel, tenuta: {n_tenuta})")
    p.nota(f"freccia: {n_una} pixel con una pressione, {n_tenuta} tenuta giu'; foto in " + str(p.cartella))


DISCO = "disco_app.img"   # la userbar con Esempio: il vassoio ha le 8 app che scrivania.VASSOIO si aspetta (nel disco normale Esempio non c'e')
