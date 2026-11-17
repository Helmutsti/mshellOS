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
  6. il tastierino numerico: con Bloc Num acceso (com'e' all'avvio) il tasto 8 scrive la
     cifra, con Bloc Num spento fa la freccia su ("^[[A"), il 5 spento non fa niente, e
     premendo Bloc Num due volte si torna alle cifre;
  7. i tasti multimediali (volume, muto, riproduci, brano, stop) sono del sistema: nel
     Terminale non cambia niente e il compositore scrive nel registro "tasto di sistema: ...".
     (La prova funziona solo nella sandbox: m.aspetta_seriale; in QEMU c'e' un altro modo.)
Le foto della finestra restano nella cartella della prova (con --tieni).
"""

import time

from PIL import ImageChops

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

    # 6. il tastierino numerico: con Bloc Num acceso (all'avvio) da' cifre, spento da' le frecce.
    # Ogni tasto si prova su una riga nuova (Ctrl+C) e si guarda SOLO quello che ha scritto
    # (il rettangolo dei pixel cambiati dal tasto): la riga si sposta in basso a ogni prova,
    # quindi le foto intere non si possono confrontare, quei ritagli si'.
    def scrivi(tasto):
        """Il ritaglio di quello che il tasto ha scritto (None: niente)."""
        m.tasto_giu("ctrl"); m.premi("c"); m.tasto_su("ctrl")
        time.sleep(0.4)
        base = m.aspetta_schermo_fermo()
        m.premi(tasto)
        time.sleep(0.4)
        dopo = m.aspetta_schermo_fermo()
        dopo.crop(FINESTRA).save(p.cartella / f"terminale_tastierino_{tasto}.png")
        cambiato = ImageChops.difference(_dentro(dopo), _dentro(base)).convert("L").point(lambda v: 255 if v > 40 else 0)
        riquadro = cambiato.getbbox()
        return _dentro(dopo).crop(riquadro) if riquadro else None

    def uguali(a, b):
        return a is not None and b is not None and a.size == b.size and pixel_diversi(a, b, None, 40) < 20

    cifra = scrivi("8")                        # la cifra di riferimento
    freccia = scrivi("up")                     # la freccia di riferimento: "^[[A"
    p.controlla(cifra is not None and freccia is not None and not uguali(cifra, freccia),
                "la cifra e la freccia di riferimento non si distinguono")
    p.controlla(uguali(scrivi("kp_8"), cifra), "con Bloc Num acceso il tasto 8 del tastierino non scrive la cifra")
    m.premi("num_lock")                        # spento
    p.controlla(uguali(scrivi("kp_8"), freccia), "con Bloc Num spento il tasto 8 non fa la freccia su")
    p.controlla(scrivi("kp_5") is None, "con Bloc Num spento il 5 del tastierino ha scritto qualcosa")
    m.premi("num_lock")                        # di nuovo acceso (premuto due volte)
    p.controlla(uguali(scrivi("kp_8"), cifra), "dopo due pressioni di Bloc Num il tasto 8 non scrive piu' la cifra")

    # 7. i tasti multimediali sono del sistema: il compositore li ferma e lo scrive nel registro
    # (qui si vede solo che l'app non scrive niente e che la riga c'e'; che l'app non li riceva
    # e' garantito dal ritorno anticipato del compositore, non da quello che si vede)
    for tasto, nome in (("volumeup", "volume su"), ("volumedown", "volume giu'"), ("audiomute", "muto"),
                        ("audioplay", "riproduci/pausa"), ("audionext", "brano successivo"),
                        ("audioprev", "brano precedente"), ("audiostop", "stop")):
        p.controlla(scrivi(tasto) is None, f"il tasto {tasto} ha scritto qualcosa nel Terminale")
        m.aspetta_seriale(f"compositore: tasto di sistema: {nome}", 5.0)


DISCO = "disco_app.img"   # la userbar con Esempio: il vassoio ha le 8 app che scrivania.VASSOIO si aspetta (nel disco normale Esempio non c'e')
