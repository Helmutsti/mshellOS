"""wayland_ingresso: mouse e tastiera nelle finestre Wayland (wl_seat, passo 2 della migrazione).

Il disco e' quello di wayland.py: un client Wayland di prova (utente/prove/wayland.c) apre la sua
finestra "Prova Wayland" (contenuto 320 x 200 sotto la testata di 40) e scrive nel registro tutto
quello che gli arriva da wl_pointer e wl_keyboard. La prova guida mouse e tastiera dall'esterno e
legge il registro (le coordinate sono quelle del CONTENUTO, sotto la testata).
  1. Il seat: il programma lo vede con le capacita' puntatore e tastiera, riceve la keymap XKB_V1
     (valida: finisce con lo zero) e il ritmo di ripetizione (33 al secondo dopo 500 ms), e ha il
     fuoco della tastiera.
  2. Il puntatore: il mouse entra nel contenuto (enter con le coordinate relative al contenuto, a
     meno di 1 pixel: la tavoletta ha una scala sua), si muove, passa sulla testata (leave: la
     cornice e' del compositore), torna dentro. Un clic sinistro e uno destro (BTN_LEFT, BTN_RIGHT:
     premuto e rilasciato), la rotella (uno scatto verso il basso = 10 e scatto discreto 1; due scatti
     in su = -10 e -1, due volte). Un clic sulla testata NON arriva al programma.
  3. La tastiera, con il fuoco sulla finestra: "a" (codice 30), Maiusc+a (Maiusc 42, modificatori
     0x1), AltGr+e (100 e 18, modificatori 0x80), la freccia su (103), Ctrl e Alt, Bloc Maiusc (si
     accende e si spegne: bloccati 0x2) e Bloc Num (spento: il 7 del tastierino e' il tasto 71 grezzo,
     non Home). Un tasto tenuto giu' (la ripetizione la fa il kernel, il compositore la scarta)
     arriva UNA volta premuto e UNA volta rilasciato.
  4. Il fuoco: il Blocco note si apre e prende il fuoco: il programma Wayland riceve leave
     ("tastiera fuoco no") e non vede piu' i tasti, che invece scrivono nel Blocco note. Un clic sul
     contenuto del programma Wayland gli ridanno il fuoco ("tastiera fuoco si"): ora i tasti sono
     suoi e il Blocco note non cambia. E ancora il contrario con un clic sul Blocco note.
  5. Niente "FALLITA" nel registro (i numeri di serie non tornano mai indietro).
"""

import re
import time

import scrivania as s
from qemu import PUNTO_VUOTO, ErroreProva, pixel_diversi, riquadro_colore

NOME = "wayland_ingresso"
ORDINE = 86
DESCRIZIONE = "un client Wayland riceve mouse (wl_pointer) e tastiera (wl_keyboard) con il fuoco giusto"
DISCO = "disco_wayland.img"

LARGHEZZA, ALTEZZA, TESTATA = 320, 200, 40
QUADRATO, MARGINE = 48, 16
ROSSO = 0xE61E1E

# i bit dei modificatori come li vuole XKB (compositore_wayland.c, maschere_xkb)
MAIUSC, BLOC_MAIUSC, CTRL, ALT, BLOC_NUM, ALTGR = 0x1, 0x2, 0x4, 0x8, 0x10, 0x80


def _trova_finestra(foto):
    r = riquadro_colore(foto, ROSSO)
    if not r or (r[2] - r[0], r[3] - r[1]) != (QUADRATO, QUADRATO):
        return None
    return r[0] - MARGINE, r[1] - MARGINE - TESTATA


class Registro:
    """Le righe "wayland: ..." della seriale che arrivano DOPO un certo punto."""

    def __init__(self, m):
        self.m = m
        self.da = 0
        self.segna()

    def segna(self):
        self.da = len(self.m.testo_seriale())

    def righe(self, filtro=None):
        testo = self.m.testo_seriale()[self.da:]
        r = [x.split("wayland: ", 1)[1].strip() for x in testo.splitlines() if "wayland: " in x]
        return [x for x in r if re.match(filtro, x)] if filtro else r

    def aspetta(self, condizione, attesa=8.0, cosa="il registro"):
        fine = time.time() + attesa
        while time.time() < fine:
            if condizione(self.righe()):
                return self.righe()
            time.sleep(0.1)
        raise ErroreProva(f"dopo {attesa:.0f} s {cosa} non e' come atteso: {self.righe()}")


def _vicini(riga, prefisso, x, y, tolleranza=1):
    """La riga "<prefisso> X,Y" con X,Y a meno di 'tolleranza' da (x, y)."""
    mt = re.match(re.escape(prefisso) + r" (-?\d+),(-?\d+)$", riga)
    return bool(mt) and abs(int(mt[1]) - x) <= tolleranza and abs(int(mt[2]) - y) <= tolleranza


def _tasti(righe):
    """Solo tasti e modificatori, nella forma breve: "30+", "30-", "mod 0x1/0x10" (premuti/bloccati)."""
    fuori = []
    for r in righe:
        mt = re.match(r"tasto (\d+) (premuto|rilasciato)$", r)
        if mt:
            fuori.append(mt[1] + ("+" if mt[2] == "premuto" else "-"))
            continue
        mt = re.match(r"modificatori premuti 0x([0-9a-f]+) agganciati 0x0 bloccati 0x([0-9a-f]+)$", r)
        if mt:
            fuori.append(f"mod 0x{int(mt[1], 16):x}/0x{int(mt[2], 16):x}")
    return fuori


def esegui(p):
    m = p.m
    r = Registro(m)
    r.da = 0
    m.aspetta_seriale("wayland: finestra mostrata", 30.0)
    m.muovi(*PUNTO_VUOTO)
    f = m.aspetta_che(lambda f: _trova_finestra(f) is not None, 10.0, cosa="la finestra Wayland sulla scrivania")
    f = m.aspetta_schermo_fermo()
    x, y = _trova_finestra(f)

    # 1. il seat
    r.aspetta(lambda l: "tastiera fuoco si (tasti gia' giu': nessuno)" in l, 10.0, "l'enter della tastiera")
    righe = r.righe()
    p.controlla("seat capacita' puntatore tastiera" in righe, f"le capacita' del seat non sono puntatore e tastiera: {righe}")
    p.controlla(any(re.match(r"keymap XKB_V1 \d+ byte$", x) for x in righe), f"la keymap non e' arrivata valida: {righe}")
    p.controlla("ripetizione 33 al secondo dopo 500 ms" in righe, f"la ripetizione non e' 33 al secondo dopo 500 ms: {righe}")
    p.controlla("seat nome seat0" in righe, "il seat non si chiama seat0")
    p.controlla(f"modificatori premuti 0x0 agganciati 0x0 bloccati 0x{BLOC_NUM:x}" in righe,
                "all'enter i modificatori non sono 'solo Bloc Num acceso'")

    # 2. il puntatore. Il contenuto comincia a (x, y + 40).
    cx, cy = x, y + TESTATA
    r.segna()
    m.muovi(cx + 100, cy + 60)
    r.aspetta(lambda l: any(_vicini(v, "puntatore entra", 100, 60) for v in l), cosa="l'enter del puntatore a 100,60")
    m.muovi(cx + 150, cy + 80)
    r.aspetta(lambda l: any(_vicini(v, "puntatore muove", 150, 80) for v in l), cosa="il movimento a 150,80")
    # la testata e' della cornice: il programma non la sente (leave), e un clic li' non gli arriva
    r.segna()
    m.muovi(cx + 100, y + 20)
    r.aspetta(lambda l: "puntatore esce" in l, cosa="il leave sulla testata")
    m.pulsante(True)
    time.sleep(0.2)
    m.pulsante(False)
    time.sleep(0.4)
    p.controlla(not any(v.startswith("clic ") for v in r.righe()), f"un clic sulla testata e' arrivato al programma: {r.righe()}")
    # clic sinistro e destro nel contenuto
    r.segna()
    m.muovi(cx + 200, cy + 120)
    r.aspetta(lambda l: any(_vicini(v, "puntatore entra", 200, 120) for v in l), cosa="il rientro a 200,120")
    for quale, nome in (("left", "BTN_LEFT"), ("right", "BTN_RIGHT")):
        r.segna()
        m.pulsante(True, quale)
        time.sleep(0.15)
        m.pulsante(False, quale)
        r.aspetta(lambda l: any(v.startswith(f"clic {nome} rilasciato") for v in l), cosa=f"il clic {nome}")
        l = r.righe("clic ")
        p.controlla(len(l) == 2 and _vicini(l[0], f"clic {nome} premuto", 200, 120) and _vicini(l[1], f"clic {nome} rilasciato", 200, 120),
                    f"il clic {nome} non e' premuto e rilasciato a 200,120: {l}")
    # la rotella
    r.segna()
    m.rotella(1)
    r.aspetta(lambda l: "rotella verticale 10" in l, cosa="la rotella in basso")
    p.controlla("rotella scatti verticale 1" in r.righe(), f"manca lo scatto discreto verticale 1: {r.righe()}")
    r.segna()
    m.rotella(-2)
    r.aspetta(lambda l: l.count("rotella verticale -10") == 2, cosa="due scatti in su")
    p.controlla(r.righe().count("rotella scatti verticale -1") == 2, f"mancano i due scatti discreti -1: {r.righe()}")
    # fuori dal contenuto ma a mouse giu' (la "presa"): il clic cominciato dentro continua fuori
    r.segna()
    m.muovi(cx + 100, cy + 100)
    time.sleep(0.2)
    m.pulsante(True)
    time.sleep(0.15)
    m.muovi(cx + 100, y + 10)            # sulla testata, con il pulsante giu'
    time.sleep(0.2)
    m.pulsante(False)
    r.aspetta(lambda l: any(v.startswith("clic BTN_LEFT rilasciato") for v in l) and "puntatore esce" in l,
              cosa="il rilascio e poi il leave dopo una presa uscita dal contenuto")
    righe = r.righe()
    p.controlla(righe.index("puntatore esce") > max(i for i, v in enumerate(righe) if v.startswith("clic BTN_LEFT rilasciato")),
                f"il leave arriva prima del rilascio: {righe}")
    m.muovi(*PUNTO_VUOTO)

    # 3. la tastiera (il fuoco e' ancora sulla finestra Wayland)
    def tasti(cosa, azione, atteso, attesa=8.0):
        r.segna()
        azione()
        try:
            r.aspetta(lambda l: len(_tasti(l)) >= len(atteso), attesa, cosa)
        except ErroreProva:
            pass                       # il confronto qui sotto dice cosa e' arrivato
        time.sleep(0.3)
        visto = _tasti(r.righe())
        p.controlla(visto == atteso, f"{cosa}: arrivato {visto}, atteso {atteso}")

    nb = f"0x{BLOC_NUM:x}"
    tasti("la lettera a", lambda: m.premi("a"), ["30+", "30-"])
    tasti("Maiusc+a", lambda: (m.tasto_giu("shift"), time.sleep(0.1), m.premi("a"), time.sleep(0.1), m.tasto_su("shift")),
          ["42+", f"mod 0x{MAIUSC:x}/{nb}", "30+", "30-", "42-", f"mod 0x0/{nb}"])
    tasti("AltGr+e", lambda: (m.tasto_giu("alt_r"), time.sleep(0.1), m.premi("e"), time.sleep(0.1), m.tasto_su("alt_r")),
          ["100+", f"mod 0x{ALTGR:x}/{nb}", "18+", "18-", "100-", f"mod 0x0/{nb}"])
    tasti("la freccia su", lambda: m.premi("up"), ["103+", "103-"])
    tasti("Ctrl e Alt", lambda: (m.tasto_giu("ctrl"), m.tasto_giu("alt"), time.sleep(0.1), m.tasto_su("alt"), m.tasto_su("ctrl")),
          ["29+", f"mod 0x{CTRL:x}/{nb}", "56+", f"mod 0x{CTRL | ALT:x}/{nb}", "56-", f"mod 0x{CTRL:x}/{nb}", "29-", f"mod 0x0/{nb}"])
    tasti("Bloc Maiusc acceso", lambda: m.premi("caps_lock"), ["58+", f"mod 0x0/0x{BLOC_MAIUSC | BLOC_NUM:x}", "58-"])
    tasti("Bloc Maiusc spento", lambda: m.premi("caps_lock"), ["58+", f"mod 0x0/{nb}", "58-"])
    # Bloc Num spento: il 7 del tastierino e' il tasto 71 com'e', non Home (102): al programma serve il tasto
    # fisico, e la keymap dice che con Bloc Num spento e' Home
    tasti("Bloc Num spento e il 7 del tastierino",
          lambda: (m.premi("num_lock"), time.sleep(0.2), m.premi("kp_7"), time.sleep(0.2), m.premi("num_lock")),
          ["69+", "mod 0x0/0x0", "69-", "71+", "71-", "69+", f"mod 0x0/{nb}", "69-"])
    # un tasto tenuto giu': il kernel lo ripete, il compositore scarta le ripetizioni
    tasti("la lettera a tenuta giu' piu' di un secondo",
          lambda: (m.tasto_giu("a"), time.sleep(1.4), m.tasto_su("a")), ["30+", "30-"], attesa=12.0)

    # 4. il fuoco. Si sposta la finestra Wayland a destra, fuori da dove si apre il Blocco note.
    m.trascina(x + 40, y + 20, x + 40 + 900 - x, y + 20 + 480 - y)
    m.muovi(*PUNTO_VUOTO)
    nuova = (900, 480)
    m.aspetta_che(lambda f: _trova_finestra(f) == nuova, 5.0, cosa=f"la finestra spostata in {nuova}")
    x, y = nuova
    cx, cy = x, y + TESTATA
    r.segna()
    s.apri(m, "BloccoNote")
    m.muovi(*PUNTO_VUOTO)
    r.aspetta(lambda l: "tastiera fuoco no" in l, 10.0, "il leave della tastiera all'apertura del Blocco note")
    # senza il fuoco il programma non sente il puntatore che passa (come le nostre finestre), ma la
    # rotella si': per quello scatto il puntatore "entra", gira, "esce"
    r.segna()
    m.muovi(cx + 100, cy + 100)
    time.sleep(0.5)
    p.controlla(not r.righe("puntatore"), f"senza il fuoco il programma sente il puntatore che passa: {r.righe()}")
    m.rotella(1)
    r.aspetta(lambda l: "puntatore esce" in l, cosa="enter, rotella e leave senza fuoco")
    righe = [v for v in r.righe() if v.startswith("puntatore") or v.startswith("rotella verticale")]
    p.controlla(len(righe) == 3 and _vicini(righe[0], "puntatore entra", 100, 100) and righe[1] == "rotella verticale 10" and righe[2] == "puntatore esce",
                f"la rotella senza fuoco non e' enter, rotella, leave: {righe}")
    m.muovi(*PUNTO_VUOTO)
    # le zone da confrontare: tutto lo schermo senza la finestra Wayland, la userbar (l'ora cambia) e il puntatore
    esclusa = [(x - 12, y - 12, x + LARGHEZZA + 12, y + TESTATA + ALTEZZA + 12), (0, 0, 60, 800), (1230, 750, 1280, 800)]
    prima = m.aspetta_schermo_fermo()
    r.segna()
    for c in "ciao":
        m.premi(c)
        time.sleep(0.05)
    time.sleep(0.5)
    dopo = m.aspetta_schermo_fermo()
    p.controlla(not any(v.startswith("tasto ") for v in r.righe()), f"il programma Wayland riceve i tasti senza il fuoco: {r.righe()}")
    p.controlla(pixel_diversi(prima, dopo, esclusa) > 20, "il Blocco note con il fuoco non ha ricevuto i tasti")
    m.foto("blocconote_scritto")

    # un clic sul contenuto del programma Wayland gli da' il fuoco: ora i tasti sono suoi
    r.segna()
    m.clic(cx + 100, cy + 100)
    r.aspetta(lambda l: "tastiera fuoco si (tasti gia' giu': nessuno)" in l, 10.0, "l'enter della tastiera dopo il clic")
    righe = r.righe()
    p.controlla(any(_vicini(v, "puntatore entra", 100, 100) for v in righe) and any(_vicini(v, "clic BTN_LEFT premuto", 100, 100) for v in righe),
                f"il clic che da' il fuoco non e' arrivato con enter e coordinate giuste: {righe}")
    m.muovi(*PUNTO_VUOTO)
    time.sleep(0.4)
    prima = m.aspetta_schermo_fermo()
    tasti("le lettere con il fuoco sul programma Wayland", lambda: [(m.premi(c), time.sleep(0.05)) for c in "xyz"],
          ["45+", "45-", "21+", "21-", "44+", "44-"])
    dopo = m.aspetta_schermo_fermo()
    p.controlla(pixel_diversi(prima, dopo, esclusa) <= 20, "il Blocco note ha ricevuto i tasti mentre il fuoco era sul programma Wayland")

    # e il contrario: un clic sul Blocco note (lontano da dove stava la finestra Wayland)
    r.segna()
    m.clic(500, 400)
    m.muovi(*PUNTO_VUOTO)
    r.aspetta(lambda l: "tastiera fuoco no" in l, 10.0, "il leave della tastiera dopo il clic sul Blocco note")
    time.sleep(0.5)
    prima = m.aspetta_schermo_fermo()
    r.segna()
    for c in "abc":
        m.premi(c)
        time.sleep(0.05)
    time.sleep(0.5)
    dopo = m.aspetta_schermo_fermo()
    p.controlla(not any(v.startswith("tasto ") for v in r.righe()), f"il programma Wayland riceve i tasti senza il fuoco: {r.righe()}")
    p.controlla(pixel_diversi(prima, dopo, esclusa) > 20, "il Blocco note non ha ricevuto i tasti con il fuoco")
    m.foto("blocconote_di_nuovo")

    # 5. nessun errore
    seriale = m.testo_seriale()
    p.controlla("FALLITA" not in seriale, "il client ha avuto un errore: " + "; ".join(v for v in seriale.splitlines() if "FALLITA" in v))
    p.controlla("NON VALIDA" not in seriale, "la keymap non e' valida")
