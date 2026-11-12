"""tasti_speciali: i modificatori (Ctrl, Maiusc, Alt, Alt Gr) e i tasti senza carattere
(frecce, Invio, Esc, F2, Canc) arrivano alle applicazioni, da soli e insieme.

  1. DOOM (solo tasti fisici, niente UIKit). Si apre dalla userbar; Invio quattro volte
     (Nuova partita, episodio, difficolta') fa cominciare la partita. Poi:
       - Ctrl tenuto giu' spara, e continua a sparare: il numero delle munizioni (in basso
         a sinistra) cambia, e poi cambia ancora;
       - Freccia su tenuta giu' fa camminare: la scena cambia mentre e' giu';
       - Esc apre il menu, un altro Esc lo chiude.
  2. FILES (le scorciatoie di UIKit): Ctrl+Maiusc+N fa una cartella nuova, Esc lascia il
     nome, Canc la toglie; Ctrl+D duplica leggimi.txt, Canc toglie la copia; Ctrl+I apre le
     informazioni, Esc le chiude.
  3. L'AVVISO di UIKit: Canc su una cartella piena apre l'avviso "Eliminare ...?"; Esc fa
     come Annulla (l'avviso si chiude e la cartella resta).
  4. UN CAMPO DI TESTO (la ricerca di Files): Maiusc fa le maiuscole, Alt Gr i caratteri
     della tastiera italiana (Alt Gr+o' = @); con Ctrl giu' non si scrive niente.
     Si confrontano le foto del campo: "l" e "L" devono essere diverse, "o'" e "@" pure.
"""

import time

import scrivania as s
from qemu import PUNTO_VUOTO, pixel_diversi

NOME = "tasti_speciali"
ORDINE = 41
DESCRIZIONE = "Ctrl, Maiusc, Alt, Alt Gr, frecce, Esc: in Doom e nelle scorciatoie di UIKit"

# Doom: la finestra (640 x 480 sotto una barra da 28) al primo posto
DOOM_MUNIZIONI = (6, 28 + 405, 96, 28 + 470)     # il numero AMMO della barra di stato
DOOM_SCENA = (0, 28, 640, 28 + 400)               # la vista del gioco, sopra la barra di stato

# Files (vedi files.py)
RICERCA = (700, 65)
CAMPO = (600, 52, 790, 78)                        # il campo della ricerca, dentro la finestra
CORPO_Y = 89


def elemento(k):
    return 264 + (k % 5) * 104, 133 + (k // 5) * 124


def insieme(m, *tasti):
    """Una scorciatoia: i modificatori giu', il tasto, poi su."""
    for t in tasti[:-1]:
        m.tasto_giu(t)
        time.sleep(0.05)
    m.premi(tasti[-1])
    time.sleep(0.05)
    for t in reversed(tasti[:-1]):
        m.tasto_su(t)
        time.sleep(0.05)


def dentro(angolo, r):
    return angolo[0] + r[0], angolo[1] + r[1], angolo[0] + r[2], angolo[1] + r[3]


def doom(p):
    m = p.m
    s.clic_app(m, "Doom")
    m.muovi(*PUNTO_VUOTO)
    m.aspetta_che(lambda f: s.segno_sotto(f, "Doom") > 0, 15.0, cosa="Doom aperto nella userbar")
    time.sleep(2.0)
    w = s.angolo(0)
    munizioni, scena = dentro(w, DOOM_MUNIZIONI), dentro(w, DOOM_SCENA)
    for _ in range(4):               # menu, Nuova partita, episodio, difficolta'
        m.premi("ret")
        time.sleep(0.6)
    time.sleep(1.5)                  # la tendina che scende all'inizio della partita
    m.foto("doom_partita")

    # Ctrl tenuto giu': spara, e continua a sparare finche' e' giu'
    prima = m.foto()
    m.tasto_giu("ctrl")
    try:
        sparato = m.aspetta_che(lambda f: not s.zona_uguale(f, prima, munizioni), 5.0,
                                cosa="le munizioni che calano tenendo giu' Ctrl")
        m.aspetta_che(lambda f: not s.zona_uguale(f, sparato, munizioni), 5.0,
                      cosa="altri colpi con Ctrl ancora giu'")
    finally:
        m.tasto_su("ctrl")
    m.foto("doom_sparato")

    # Freccia su tenuta giu': la scena continua a cambiare finche' e' giu'
    m.tasto_giu("up")
    try:
        time.sleep(0.5)
        visti = []
        for _ in range(4):
            visti.append(m.foto())
            time.sleep(0.25)
    finally:
        m.tasto_su("up")
    cambi = sum(1 for a, b in zip(visti, visti[1:]) if not s.zona_uguale(a, b, scena))
    p.controlla(cambi >= 2, f"con la freccia su tenuta giu' la scena cambia solo {cambi} volte su 3")

    # Esc: il menu; un altro Esc lo chiude
    time.sleep(0.5)
    prima = m.foto()
    m.premi("esc")
    menu = m.aspetta_che(lambda f: not s.zona_uguale(f, prima, scena), 5.0, cosa="il menu di Doom (Esc)")
    m.foto("doom_menu")
    m.premi("esc")
    time.sleep(0.5)
    m.foto("doom_menu_chiuso")
    p.controlla(not s.zona_uguale(m.foto(), menu, scena), "Esc non chiude il menu di Doom")

    # chiusa con la X della sua barra
    m.clic(w[0] + 640 - 14, w[1] + 14)
    m.muovi(*PUNTO_VUOTO)
    m.aspetta_che(lambda f: s.segno_sotto(f, "Doom") == 0, 10.0, cosa="Doom chiuso")


def files(p):
    m = p.m
    s.apri(m, "Files")
    wx, wy = s.angolo(0)
    l, a = s.MISURE["Files"]
    corpo = (wx + 1, wy + CORPO_Y, wx + l - 1, wy + a - 1)
    schermo = (0, 0, 1280, 800)
    campo = dentro((wx, wy), CAMPO)

    def clic(punto):
        m.clic(wx + punto[0], wy + punto[1])
        m.muovi(*PUNTO_VUOTO)

    def cambia(prima, cosa, zona=corpo):
        m.muovi(*PUNTO_VUOTO)
        s.aspetta_zona(m, prima, zona, uguale=False, attesa=10.0, cosa=cosa)
        return m.aspetta_schermo_fermo()

    def torna(foto, cosa, zona=corpo):
        return s.aspetta_zona(m, foto, zona, attesa=10.0, cosa=cosa)

    casa = m.aspetta_schermo_fermo()

    # --- le scorciatoie ---
    insieme(m, "ctrl", "shift", "n")                                    # una cartella nuova
    nuova = cambia(casa, "la cartella nuova con Ctrl+Maiusc+N")
    m.foto("ctrl_maiusc_n")
    m.premi("esc")                                                      # il nome resta
    time.sleep(0.3)
    clic(elemento(2))                                                   # Documenti, Immagini, Nuova cartella
    m.premi("delete")                                                   # vuota: via subito
    torna(casa, "la cartella nuova tolta con Canc")
    clic(elemento(4))                                                   # leggimi.txt
    scelto = m.aspetta_schermo_fermo()
    insieme(m, "ctrl", "d")
    cambia(scelto, "la copia con Ctrl+D")
    m.foto("ctrl_d")
    m.premi("delete")                                                   # la copia e' scelta
    torna(casa, "la copia tolta con Canc")
    clic(elemento(4))
    insieme(m, "ctrl", "i")
    informazioni = cambia(scelto, "le informazioni con Ctrl+I", schermo)
    m.foto("ctrl_i")
    m.premi("esc")
    s.aspetta_zona(m, informazioni, schermo, uguale=False, attesa=10.0, cosa="le informazioni chiuse (Esc)")
    clic(elemento(4))
    m.premi("esc")                                                      # niente scelto
    torna(casa, "la scelta tolta con Esc")
    p.controlla(not s.zona_uguale(nuova, casa, corpo), "Ctrl+Maiusc+N non ha fatto niente")

    # --- l'avviso: Esc fa come Annulla ---
    clic(elemento(0))                                                   # Documenti (piena)
    documenti = m.aspetta_schermo_fermo()
    m.premi("delete")
    avviso = cambia(documenti, "l'avviso per eliminare Documenti", schermo)
    m.foto("avviso")
    m.premi("esc")
    torna(documenti, "l'avviso chiuso con Esc, Documenti ancora li'", schermo)
    m.foto("avviso_esc")
    p.controlla(not s.zona_uguale(avviso, documenti, schermo), "l'avviso non si era aperto")
    clic(elemento(4))                                                   # via la scelta: leggimi.txt
    m.premi("esc")
    torna(casa, "la casa senza scelta")

    # --- il campo della ricerca: Maiusc, Alt Gr, Ctrl ---
    clic(RICERCA)
    vuoto = m.aspetta_schermo_fermo()

    def scritto(*tasti, nome):
        """Scrive (un tasto, o una combinazione), fotografa il campo, poi lo svuota."""
        if len(tasti) == 1:
            m.premi(tasti[0])
        else:
            insieme(m, *tasti)
        foto = cambia(vuoto, f"il campo con {nome}", campo)
        m.foto(f"campo_{nome}")
        m.premi("backspace")
        torna(vuoto, f"il campo di nuovo vuoto (dopo {nome})", campo)
        return foto.crop(campo)

    minuscola = scritto("l", nome="l")
    maiuscola = scritto("shift", "l", nome="L")
    o_grave = scritto("semicolon", nome="o_grave")
    chiocciola = scritto("alt_r", "semicolon", nome="chiocciola")
    quadra = scritto("alt_r", "bracket_left", nome="quadra")
    p.controlla(pixel_diversi(minuscola, maiuscola) > 0, "con Maiusc la lettera non e' maiuscola")
    p.controlla(pixel_diversi(o_grave, chiocciola) > 0, "con Alt Gr il tasto o' non da' la @")
    p.controlla(pixel_diversi(chiocciola, quadra) > 0, "Alt Gr+o' e Alt Gr+e' danno lo stesso carattere")

    # con Ctrl giu' (e con Alt) non si scrive: il campo resta vuoto
    insieme(m, "ctrl", "l")
    insieme(m, "alt", "l")
    time.sleep(0.5)
    p.controlla(s.zona_uguale(m.foto(), vuoto, campo), "Ctrl+L o Alt+L hanno scritto nel campo")
    m.premi("esc")


def esegui(p):
    doom(p)
    files(p)


DISCO = "disco_app.img"   # la userbar con Esempio, come nelle altre prove (scrivania.APP)
