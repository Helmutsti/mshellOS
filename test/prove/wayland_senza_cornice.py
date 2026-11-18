"""wayland_senza_cornice: un programma Wayland che si disegna da solo la barra del titolo (xdg-decoration).

Il disco e' quello di wayland.py, ma il client di prova (utente/prove/wayland.c) si chiama
wayland_csd: chiede al compositore xdg-decoration "client_side" e si disegna la barra del titolo
(40 pixel, viola, con un quadratino chiaro a destra: "riduci"), sopra lo stesso disegno di sempre.
  1. Niente cornice nostra: il compositore risponde "client_side", la finestra e' il buffer del
     programma cosi' com'e' (la barra viola parte dal bordo, il disegno e' 40 sotto, e la finestra
     e' nata a cascata nel suo posto senza i 40 pixel di testata nostra).
  2. La forma del puntatore (cursor-shape): sulla barra a sinistra la barra a I, al centro la mano,
     a destra la freccia: le tre immagini sono diverse tra loro.
  3. Trascinare per la barra: il programma chiama xdg_toplevel.move e la finestra segue il mouse.
  4. I ping: il programma risponde (almeno due ping nel registro) e la finestra non e' "bloccata".
  5. Il programma smette di rispondere (tasto X): dopo 3 secondi la barra di emergenza copre la
     barra; riprende (tasto Z) e la barra torna quella del programma.
  6. Un clic sul quadratino: set_minimized, la finestra sparisce.
"""

import time

from qemu import colore_in, riquadro_colore, PUNTO_VUOTO

NOME = "wayland_senza_cornice"
ORDINE = 87
DESCRIZIONE = "un client Wayland si disegna la barra (client_side): niente cornice nostra, move, riduci, forma, ping"
DISCO = "disco_wayland_csd.img"

LARGHEZZA, ALTEZZA, BARRA, QUADRATO, MARGINE = 320, 200, 40, 48, 16
ROSSO, BARRA_COLORE = 0xE61E1E, 0x6A2FA8


def _finestra(foto):
    """L'angolo in alto a sinistra della finestra (la barra), dal quadrato rosso. None se non c'e'."""
    r = riquadro_colore(foto, ROSSO)
    if not r or (r[2] - r[0], r[3] - r[1]) != (QUADRATO, QUADRATO):
        return None
    return r[0] - MARGINE, r[1] - MARGINE - BARRA


def _barra_viola(foto, x, y):
    """Quanti pixel della barra (a sinistra del quadratino) sono del viola del programma, su 100 campionati."""
    giusti = tot = 0
    for dx in range(4, 250, 25):
        for dy in range(4, BARRA - 3, 8):
            tot += 1
            giusti += colore_in(foto, x + dx, y + dy) == BARRA_COLORE
    return giusti * 100 // tot


def _differenze(a, b):
    """Quanti pixel di due ritagli uguali sono diversi."""
    da, db = a.getdata(), b.getdata()
    return sum(1 for p, q in zip(da, db) if max(abs(p[i] - q[i]) for i in range(3)) > 24)


def _non_barra(ritaglio):
    return sum(1 for p in ritaglio.getdata() if (p[0] << 16 | p[1] << 8 | p[2]) != BARRA_COLORE)


def _aspetta_riga(m, testo, attesa=10.0, quante=1):
    fine = time.time() + attesa
    while time.time() < fine:
        if m.testo_seriale().count(testo) >= quante:
            return True
        time.sleep(0.2)
    return False


def esegui(p):
    m = p.m
    m.aspetta_seriale("wayland: finestra mostrata", 30.0)
    m.muovi(*PUNTO_VUOTO)
    m.aspetta_che(lambda f: _finestra(f) is not None, 10.0, cosa="la finestra Wayland sulla scrivania")
    f = m.aspetta_schermo_fermo()
    x, y = _finestra(f)
    m.foto("finestra")
    seriale = m.testo_seriale()

    # 1. niente cornice nostra
    p.controlla("wayland: decorazione client_side" in seriale, "il compositore non ha risposto client_side")
    p.controlla(_barra_viola(f, x, y) >= 95, f"la barra del programma non e' sola e piena: {_barra_viola(f, x, y)}% di viola")
    p.controlla(colore_in(f, x + 160, y + BARRA + ALTEZZA - 1) & 255 == 96,
                "l'ultima riga del programma non e' il bordo della finestra")
    # a cascata la finestra nasce a (60 + 40 i, 50 + 40 i): con la testata nostra la y sarebbe di 40 piu' in basso
    p.controlla((y - 50) - (x - 60) == 0, f"la finestra e' in ({x}, {y}): con la cornice nostra sarebbe 40 piu' in basso")
    # sopra la barra non c'e' una testata di vetro con il titolo e i comandi
    p.controlla("FALLITA" not in seriale, "il client ha avuto un errore: " + "; ".join(r for r in seriale.splitlines() if "FALLITA" in r))

    # 2. la forma del puntatore, sulla barra viola (liscia: si vede solo la forma)
    sagome = {}
    for nome, dx, riga in (("testo", 40, "wayland: forma testo"), ("collegamento", 140, "wayland: forma collegamento"),
                           ("normale", 240, "wayland: forma normale")):
        m.muovi(x + dx, y + 20)
        p.controlla(_aspetta_riga(m, riga), f"il programma non dice '{riga}'")
        foto = m.aspetta_schermo_fermo()
        m.foto("forma_" + nome)
        sagome[nome] = foto.crop((x + dx - 12, y + 2, x + dx + 24, y + BARRA - 2))
        p.controlla(_non_barra(sagome[nome]) > 20, f"la forma '{nome}' non si vede sulla barra")
    nomi = list(sagome)
    for i in range(3):
        for j in range(i + 1, 3):
            d = _differenze(sagome[nomi[i]], sagome[nomi[j]])
            p.controlla(d > 30, f"le forme '{nomi[i]}' e '{nomi[j]}' sono quasi uguali ({d} pixel diversi)")

    # 3. si trascina per la barra: xdg_toplevel.move
    m.trascina(x + 40, y + 20, x + 40 + 180, y + 20 + 90)
    m.muovi(*PUNTO_VUOTO)
    atteso = (x + 180, y + 90)
    m.aspetta_che(lambda f: _finestra(f) == atteso, 5.0, cosa=f"la finestra spostata in {atteso}")
    f = m.aspetta_schermo_fermo()
    m.foto("trascinata")
    x, y = atteso
    p.controlla("wayland: move chiesto" in m.testo_seriale(), "il programma non dice di aver chiesto move")

    # 4. i ping: il programma risponde, la finestra non e' bloccata
    p.controlla(_aspetta_riga(m, "wayland: ping ", 10.0, 2), "il compositore non ha mandato almeno due ping")
    time.sleep(3.5)
    f = m.foto("dopo_i_ping")
    p.controlla(_barra_viola(f, x, y) >= 95, "dopo i ping la barra non e' piu' quella del programma (finestra giudicata bloccata?)")
    p.controlla("(non rispondo)" not in m.testo_seriale(), "il programma ha mancato dei ping prima del tempo")

    # 5. il programma non risponde piu': la barra di emergenza; poi riprende
    m.premi("x")
    p.controlla(_aspetta_riga(m, "wayland: non rispondo piu' ai ping"), "il programma non ha ricevuto il tasto X")
    m.aspetta_che(lambda f: _barra_viola(f, x, y) < 50, 10.0, cosa="la barra di emergenza sulla finestra che non risponde")
    m.foto("non_risponde")
    m.premi("z")
    p.controlla(_aspetta_riga(m, "wayland: rispondo di nuovo ai ping"), "il programma non ha ricevuto il tasto Z")
    m.aspetta_che(lambda f: _barra_viola(f, x, y) >= 95, 10.0, cosa="la barra del programma tornare")
    m.foto("risponde_ancora")

    # 6. il quadratino: set_minimized, la finestra sparisce
    m.clic(x + LARGHEZZA - 8 - 12, y + 8 + 12)
    m.muovi(*PUNTO_VUOTO)
    m.aspetta_che(lambda f: riquadro_colore(f, ROSSO) is None, 5.0, cosa="la finestra ridursi")
    m.foto("ridotta")
    seriale = m.testo_seriale()
    p.controlla("wayland: riduci chiesto" in seriale, "il programma non dice di aver chiesto set_minimized")
    p.controlla("FALLITA" not in seriale, "il client ha avuto un errore: " + "; ".join(r for r in seriale.splitlines() if "FALLITA" in r))
