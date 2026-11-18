"""wayland: un programma Wayland apre la sua finestra sulla scrivania, con la cornice nostra.

Passo 1 della decisione "il compositore passa a Wayland" (SISTEMA.md): il compositore parla
Wayland accanto al suo protocollo. Si accende il disco delle prove di Wayland (disco_wayland.img):
in /data/tests c'e' il programma wayland (utente/prove/wayland.c), un piccolo client che si
collega al socket di prova del compositore (/run/mshell/wayland-prova), apre una finestra
(xdg_toplevel) "Prova Wayland" di 320 x 200 e ci disegna una sfumatura con quattro quadrati
colorati (rosso in alto a sinistra, verde a destra, blu e giallo sotto: i colori sono nel .c).
  1. Il programma si collega e dice "finestra mostrata". La finestra compare: i quattro quadrati
     sono dove devono stare dentro il contenuto (16 dai bordi, sotto la testata di 40), la
     sfumatura al centro e' quella attesa, e sopra c'e' la testata nostra (vetro chiaro, con il
     titolo e i comandi), senza il programma che la disegni. Ha il fuoco (il programma lo sa:
     "configure attiva").
  2. Si trascina per la testata: i quadrati si spostano esattamente di quanto si e' trascinato.
  3. Un clic sulla X: il compositore manda "chiuditi", il programma dice "chiusa" ed esce
     ("uscita"), e la finestra sparisce dalla scrivania.
Il sistema resta in piedi (il compositore non e' caduto: la scrivania si vede ancora).
"""

import scrivania as s
from qemu import colore_in, riquadro_colore, PUNTO_VUOTO

NOME = "wayland"
ORDINE = 85
DESCRIZIONE = "un client Wayland apre una finestra con la cornice nostra: si sposta e si chiude con la X"
DISCO = "disco_wayland.img"

# Il disegno del client (utente/prove/wayland.c)
LARGHEZZA, ALTEZZA, TESTATA = 320, 200, 40
QUADRATO, MARGINE = 48, 16
ROSSO, VERDE, BLU, GIALLO = 0xE61E1E, 0x1EC83C, 0x2850E6, 0xF0D228


def _trova_finestra(foto):
    """L'angolo in alto a sinistra della finestra, dal quadrato rosso (il suo angolo e'
    a MARGINE dal contenuto, che comincia sotto la testata). None se non c'e'."""
    r = riquadro_colore(foto, ROSSO)
    if not r or (r[2] - r[0], r[3] - r[1]) != (QUADRATO, QUADRATO):
        return None
    return r[0] - MARGINE, r[1] - MARGINE - TESTATA


def esegui(p):
    m = p.m
    m.aspetta_seriale("wayland: finestra mostrata", 30.0)
    m.muovi(*PUNTO_VUOTO)
    f = m.aspetta_che(lambda f: _trova_finestra(f) is not None, 10.0, cosa="la finestra Wayland sulla scrivania")
    f = m.aspetta_schermo_fermo()
    x, y = _trova_finestra(f)
    m.foto("finestra")
    seriale = m.testo_seriale()
    p.controlla("wayland: collegata: wl_compositor, wl_shm, xdg_wm_base" in seriale, "il client non dice di essersi collegato")
    p.controlla("wayland: configure attiva" in seriale, "il programma non sa di avere il fuoco (configure attiva)")

    # 1. il contenuto: i quattro quadrati e la sfumatura
    giusti = {
        "rosso": (ROSSO, x + MARGINE, y + TESTATA + MARGINE),
        "verde": (VERDE, x + LARGHEZZA - MARGINE - QUADRATO, y + TESTATA + MARGINE),
        "blu": (BLU, x + MARGINE, y + TESTATA + ALTEZZA - MARGINE - QUADRATO),
        "giallo": (GIALLO, x + LARGHEZZA - MARGINE - QUADRATO, y + TESTATA + ALTEZZA - MARGINE - QUADRATO),
    }
    for nome, (colore, qx, qy) in giusti.items():
        r = riquadro_colore(f, colore)
        p.controlla(r == (qx, qy, qx + QUADRATO, qy + QUADRATO), f"il quadrato {nome} e' in {r}, doveva stare in {(qx, qy, qx + QUADRATO, qy + QUADRATO)}")
    centro = colore_in(f, x + LARGHEZZA // 2, y + TESTATA + ALTEZZA // 2)
    rosso, verde = (centro >> 16) & 255, (centro >> 8) & 255
    p.controlla(abs(rosso - 127) <= 3 and abs(verde - 128) <= 3 and (centro & 255) == 96,
                f"la sfumatura al centro e' {centro:06X}, doveva essere circa 7F8060")

    # la testata c'e', ed e' nostra: il vetro (chiaro o scuro, secondo il tema) con il titolo
    # leggibile al centro e i due comandi a destra, cioe' pixel molto piu' chiari o piu' scuri del vetro
    def contrasto(zona):
        v = list(f.crop(zona).convert("L").getdata())
        return max(v) - min(v)
    p.controlla(contrasto((x + 90, y + 4, x + 230, y + 36)) > 60, "nella testata non si legge il titolo")
    p.controlla(contrasto((x + 252, y + 4, x + 316, y + 36)) > 60, "nella testata mancano i comandi riduci e chiudi")
    p.controlla(contrasto((x + 4, y + 4, x + 80, y + 36)) < 30, "a sinistra del titolo la testata non e' vetro liscio")
    f1 = f
    x1, y1 = x, y

    # 2. si trascina per la testata (a sinistra del titolo: non sui comandi)
    m.trascina(x + 40, y + 20, x + 40 + 180, y + 20 + 90)
    m.muovi(*PUNTO_VUOTO)
    atteso = (x + 180, y + 90)
    m.aspetta_che(lambda f: _trova_finestra(f) == atteso, 5.0, cosa=f"la finestra spostata in {atteso}")
    f = m.aspetta_schermo_fermo()
    m.foto("trascinata")
    x, y = atteso
    f2 = f
    x2, y2 = x, y

    # 3. la X: "chiuditi", il programma esce e la finestra sparisce
    m.clic(x + LARGHEZZA - 4 - 16, y + 20)
    m.muovi(*PUNTO_VUOTO)
    m.aspetta_seriale("wayland: uscita", 10.0)
    m.aspetta_che(lambda f: riquadro_colore(f, ROSSO) is None, 5.0, cosa="la finestra sparire")
    f = m.aspetta_schermo_fermo()
    m.foto("chiusa")
    seriale = m.testo_seriale()
    p.controlla("wayland: chiusa" in seriale, "il programma non dice di aver ricevuto 'chiuditi'")
    p.controlla("FALLITA" not in seriale, "il client ha avuto un errore: " + "; ".join(r for r in seriale.splitlines() if "FALLITA" in r))
    p.controlla(s.userbar_visibile(f), "dopo la chiusura la scrivania non c'e' piu' (il compositore e' caduto?)")
    # la scrivania vuota e' quella di adesso (all'accensione il programma puo' avere gia' aperto la
    # finestra): la finestra la copriva, e dove era prima di spostarla ora c'e' la scrivania
    vuota = f
    zona = (x1, y1, x1 + LARGHEZZA, y1 + TESTATA + ALTEZZA)
    p.controlla(not s.zona_uguale(f1, vuota, zona), "la zona della finestra e' uguale alla scrivania vuota")
    p.controlla(s.zona_uguale(f2, vuota, (x1 + 4, y1 + 4, x1 + 60, y1 + 60)),
                "dove era la finestra non e' tornata la scrivania")
