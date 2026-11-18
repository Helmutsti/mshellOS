"""wayland_sottosuperfici: wl_subcompositor e wl_subsurface (le sottosuperfici, come le usano GTK, Firefox, foot).

Il disco e' quello di wayland.py, ma il client di prova (utente/prove/wayland.c) si chiama wayland_sotto:
sopra il disegno di sempre (contenuto 320 x 200 sotto la testata nostra di 40) crea quattro sottosuperfici
(vedi la testa di wayland.c): A blu al 50% in (40,30) 96x64, B arancione in (120,60) 64x64 sopra A, C viola
in (290,150) 64x64 in parte fuori dal bordo, D verde acqua in (200,20) 64x48 che prende il puntatore solo
a sinistra (regione di input). I tasti 1-8 fanno i passi (posizione, ordine, sincrono/asincrono, distruzioni).
  1. La composizione: A lascia vedere il disegno (alfa), B copre A dove si sovrappongono, C e' tagliata al
     bordo della finestra (30 x 50 pixel, non di piu'), la finestra non e' diventata piu' grande.
  2. Il puntatore va alla superficie sotto: su A, su B, su D (meta' sinistra) con le coordinate DI QUELLA
     superficie; sulla meta' destra di D (fuori dalla regione) e sul resto va alla principale. Un clic su A
     arriva ad A con le coordinate di A.
  3. Posizione e ordine valgono al commit del genitore: il tasto 1 non cambia la foto, il 2 si'
     (A si sposta, B finisce sotto A).
  4. Lo stato di una sottosuperficie sincrona vale col genitore: il 3 (A verde, commit suo) non cambia la foto,
     il 4 si'; poi A asincrona (5) cambia subito.
  5. Niente crash: D distrutta col puntatore sopra (6), un genitore distrutto con la figlia che resta (7), e un
     errore di protocollo voluto (8: genitore uguale alla superficie) che chiude solo quel programma e scrive
     "errore di protocollo bad_parent"; il compositore resta in piedi (la scrivania risponde ancora).
"""

import re
import time

from qemu import PUNTO_VUOTO, ErroreProva, colore_in, riquadro_colore

NOME = "wayland_sottosuperfici"
ORDINE = 88
DESCRIZIONE = "un client Wayland con sottosuperfici: composizione, alfa, ordine, puntatore, sync/desync, distruzioni"
DISCO = "disco_wayland_sotto.img"

LARGHEZZA, ALTEZZA, TESTATA, QUADRATO, MARGINE = 320, 200, 40, 48, 16
BLU, VIOLA = 0x2850E6, 0x8C28D2
ARANCIONE = 0xF08C14


def _finestra(foto):
    """L'angolo in alto a sinistra della finestra (la testata), dal quadrato blu in basso a sinistra del disegno
    di base: nessuna sottosuperficie lo copre all'inizio. None se non c'e'."""
    r = riquadro_colore(foto, BLU)
    if not r or (r[2] - r[0], r[3] - r[1]) != (QUADRATO, QUADRATO):
        return None
    return r[0] - MARGINE, r[1] - (ALTEZZA - MARGINE - QUADRATO) - TESTATA


def _sfondo(cx, cy):
    """Il colore del disegno di base (sfumatura, sul contenuto) al punto (cx, cy)."""
    return (cx * 255 // (LARGHEZZA - 1), cy * 255 // (ALTEZZA - 1), 96)


def _sopra(src, a, dst):
    """src premoltiplicato con alfa a (0-255) messo sopra dst."""
    return tuple(round(s + d * (255 - a) / 255) for s, d in zip(src, dst))


def _colore(foto, x, y):
    c = colore_in(foto, x, y)
    return (c >> 16) & 255, (c >> 8) & 255, c & 255


def _vicino(p, atteso, tol=10):
    return all(abs(a - b) <= tol for a, b in zip(p, atteso))


def _rgb(c):
    return (c >> 16) & 255, (c >> 8) & 255, c & 255


class Registro:
    """Le righe "wayland: ..." della seriale che arrivano DOPO un certo punto."""

    def __init__(self, m):
        self.m = m
        self.da = 0

    def segna(self):
        self.da = len(self.m.testo_seriale())

    def righe(self):
        testo = self.m.testo_seriale()[self.da:]
        return [x.split("wayland: ", 1)[1].strip() for x in testo.splitlines() if "wayland: " in x]

    def aspetta(self, condizione, attesa=8.0, cosa="il registro"):
        fine = time.time() + attesa
        while time.time() < fine:
            if condizione(self.righe()):
                return self.righe()
            time.sleep(0.1)
        raise ErroreProva(f"dopo {attesa:.0f} s {cosa} non e' come atteso: {self.righe()}")


def _vicini(righe, prefisso, x, y, nome=""):
    """C'e' una riga "<prefisso> X,Y<nome>" con X,Y a meno di 1 pixel da (x, y)?"""
    for r in righe:
        mt = re.match(re.escape(prefisso) + r" (-?\d+),(-?\d+)(.*)$", r)
        if mt and abs(int(mt[1]) - x) <= 1 and abs(int(mt[2]) - y) <= 1 and mt[3] == nome:
            return True
    return False


def esegui(p):
    m = p.m
    r = Registro(m)
    m.aspetta_seriale("wayland: sottosuperfici create", 30.0)
    m.aspetta_seriale("wayland: finestra mostrata", 30.0)
    m.muovi(*PUNTO_VUOTO)
    m.aspetta_che(lambda f: _finestra(f) is not None, 10.0, cosa="la finestra Wayland sulla scrivania")
    f = m.aspetta_schermo_fermo()
    x, y = _finestra(f)
    cx, cy = x, y + TESTATA                     # il contenuto
    m.foto("composizione")
    p.controlla(riquadro_colore(f, BLU) is not None, "il quadrato blu del disegno non si vede")

    def punto(px, py):
        return cx + px, cy + py

    # 1. la composizione
    # A (blu al 50%) sopra il disegno: fuori dal quadrato rosso e da B, e poi sopra il quadrato rosso
    atteso = _sopra((0, 0, 0x7F), 0x80, _sfondo(100, 40))
    letto = _colore(f, *punto(100, 40))
    p.controlla(_vicino(letto, atteso), f"A non e' blu al 50% sul disegno: letto {letto}, atteso {atteso}")
    atteso = _sopra((0, 0, 0x7F), 0x80, _rgb(0xE61E1E))
    letto = _colore(f, *punto(50, 40))
    p.controlla(_vicino(letto, atteso), f"A non e' blu al 50% sul quadrato rosso: letto {letto}, atteso {atteso}")
    # B copre A dove si sovrappongono, e sta da sola altrove
    for pt in ((128, 75), (150, 100)):
        letto = _colore(f, *punto(*pt))
        p.controlla(_vicino(letto, _rgb(ARANCIONE), 4), f"B non copre il punto {pt}: letto {letto}")
    # C e' tagliata al bordo della finestra
    c = riquadro_colore(f, VIOLA)
    p.controlla(c is not None and (c[2] - c[0], c[3] - c[1]) == (30, 50),
                f"C non e' tagliata a 30 x 50 dal bordo della finestra: {c and (c[2] - c[0], c[3] - c[1])}")
    p.controlla(c is not None and c[0] == cx + 290, f"C non parte da 290: {c}")
    # la finestra non e' diventata piu' grande: l'ultima colonna del disegno e' il bordo
    p.controlla(colore_in(f, cx + LARGHEZZA + 6, cy + 170) != VIOLA, "C esce dal bordo della finestra")

    # 2. il puntatore
    r.segna()
    m.muovi(*punto(60, 40))
    r.aspetta(lambda l: _vicini(l, "puntatore entra", 20, 10, " (sotto A)"), cosa="l'enter su A a 20,10")
    m.foto("puntatore_su_A")
    r.segna()
    m.muovi(*punto(150, 100))                   # solo B (A arriva a x 136)
    r.aspetta(lambda l: _vicini(l, "puntatore entra", 30, 40, " (sotto B)"), cosa="l'enter su B a 30,40")
    p.controlla("puntatore esce (sotto A)" in r.righe(), f"l'uscita da A non e' arrivata prima: {r.righe()}")
    r.segna()
    m.muovi(*punto(210, 40))                    # D, meta' sinistra (regione di input): D parte da 200,20
    r.aspetta(lambda l: _vicini(l, "puntatore entra", 10, 20, " (sotto D)"), cosa="l'enter su D a 10,20")
    r.segna()
    m.muovi(*punto(250, 40))                    # D, meta' destra: fuori dalla regione, va alla principale
    r.aspetta(lambda l: _vicini(l, "puntatore entra", 250, 40), cosa="l'enter sulla principale a 250,40 (fuori dalla regione di D)")
    p.controlla("puntatore esce (sotto D)" in r.righe(), f"l'uscita da D non e' arrivata: {r.righe()}")
    # un clic su A, con le coordinate di A
    m.muovi(*punto(60, 40))
    time.sleep(0.4)
    r.segna()
    m.pulsante(True)
    time.sleep(0.15)
    m.pulsante(False)
    r.aspetta(lambda l: any(v.startswith("clic BTN_LEFT rilasciato") for v in l), cosa="il clic su A")
    l = [v for v in r.righe() if v.startswith("clic ")]
    p.controlla(len(l) == 2 and _vicini([l[0].replace("clic BTN_LEFT premuto", "x")], "x", 20, 10, " (sotto A)"),
                f"il clic su A non ha le coordinate di A: {l}")
    # la principale, sotto B: sul quadrato blu
    r.segna()
    m.muovi(*punto(30, 150))
    r.aspetta(lambda l: _vicini(l, "puntatore entra", 30, 150), cosa="l'enter sulla principale a 30,150")

    # 3. posizione e ordine valgono al commit del genitore
    m.muovi(*PUNTO_VUOTO)
    prima = m.aspetta_schermo_fermo()
    r.segna()
    m.premi("1")
    r.aspetta(lambda l: any(v.startswith("passo 1") for v in l), cosa="il passo 1")
    time.sleep(0.6)
    dopo = m.aspetta_schermo_fermo()
    letto = _colore(dopo, *punto(100, 40))
    p.controlla(_vicino(letto, _sopra((0, 0, 0x7F), 0x80, _sfondo(100, 40))),
                f"senza il commit del genitore A si e' gia' spostata: {letto}")
    m.premi("2")
    r.aspetta(lambda l: any(v.startswith("passo 2") for v in l), cosa="il passo 2")
    time.sleep(0.6)
    f = m.aspetta_schermo_fermo()
    m.foto("dopo_il_commit")
    letto = _colore(f, *punto(100, 40))
    p.controlla(_vicino(letto, _sfondo(100, 40)), f"A non ha lasciato il vecchio posto: {letto}")
    atteso = _sopra((0, 0, 0x7F), 0x80, _sfondo(60, 120))
    letto = _colore(f, *punto(60, 120))
    p.controlla(_vicino(letto, atteso), f"A non e' nel nuovo posto (40,100): letto {letto}, atteso {atteso}")
    # B e' sotto A: A (50%) sopra B arancione
    atteso = _sopra((0, 0, 0x7F), 0x80, _rgb(ARANCIONE))
    letto = _colore(f, *punto(128, 110))
    p.controlla(_vicino(letto, atteso), f"B non e' sotto A: letto {letto}, atteso {atteso}")

    # 4. sincrona e asincrona
    r.segna()
    m.premi("3")
    r.aspetta(lambda l: any(v.startswith("passo 3") for v in l), cosa="il passo 3")
    time.sleep(0.6)
    letto = _colore(m.aspetta_schermo_fermo(), *punto(60, 120))
    p.controlla(_vicino(letto, _sopra((0, 0, 0x7F), 0x80, _sfondo(60, 120))),
                f"A sincrona e' cambiata senza il commit del genitore: {letto}")
    m.premi("4")
    r.aspetta(lambda l: any(v.startswith("passo 4") for v in l), cosa="il passo 4")
    time.sleep(0.6)
    letto = _colore(m.aspetta_schermo_fermo(), *punto(60, 120))
    atteso = _sopra((0, 0x7F, 0), 0x80, _sfondo(60, 120))
    p.controlla(_vicino(letto, atteso), f"A non e' diventata verde al commit del genitore: letto {letto}, atteso {atteso}")
    m.premi("5")
    r.aspetta(lambda l: any(v.startswith("passo 5") for v in l), cosa="il passo 5")
    time.sleep(0.6)
    f = m.aspetta_schermo_fermo()
    m.foto("asincrona")
    letto = _colore(f, *punto(60, 120))
    p.controlla(_vicino(letto, _rgb(0xDCDC00), 4), f"A asincrona non e' gialla subito: {letto}")

    # 5. niente crash
    r.segna()
    m.muovi(*punto(210, 40))
    r.aspetta(lambda l: _vicini(l, "puntatore entra", 10, 20, " (sotto D)"), cosa="l'enter su D per la distruzione")
    m.premi("6")
    r.aspetta(lambda l: any(v.startswith("passo 6") for v in l), cosa="il passo 6")
    time.sleep(0.6)
    f = m.aspetta_schermo_fermo()
    m.foto("D_distrutta")
    # D non si vede piu' (verde acqua) e il puntatore, mosso, entra nella principale
    p.controlla(riquadro_colore(f, 0x14B4A0) is None, "D e' distrutta ma si vede ancora")
    r.segna()
    m.muovi(*punto(212, 42))
    r.aspetta(lambda l: _vicini(l, "puntatore entra", 212, 42), cosa="l'enter sulla principale dopo la distruzione di D")
    m.premi("7")
    r.aspetta(lambda l: any(v.startswith("passo 7") for v in l), cosa="il passo 7")
    time.sleep(0.6)
    m.aspetta_schermo_fermo()
    p.controlla("FALLITA" not in m.testo_seriale(), "il client ha avuto un errore: " + "; ".join(v for v in m.testo_seriale().splitlines() if "FALLITA" in v))
    m.premi("8")
    r.aspetta(lambda l: "errore di protocollo bad_parent su wl_subcompositor" in l, cosa="l'errore di protocollo bad_parent")
    time.sleep(1.0)
    # il compositore e' in piedi: la scrivania risponde ancora (il puntatore si muove, lo schermo si aggiorna)
    m.muovi(*PUNTO_VUOTO)
    f = m.aspetta_schermo_fermo()
    m.foto("dopo_l_errore")
    p.controlla(riquadro_colore(f, BLU) is None, "dopo l'errore la finestra del programma c'e' ancora (il programma doveva essere chiuso)")
