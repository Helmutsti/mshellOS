"""scrivania.py: quello che le prove sanno della scrivania di mshell (dove sono le cose).

Sono numeri presi dallo schermo vero (1280x800). Se la scrivania o il compositore
cambiano disegno (la userbar, il vassoio, dove si aprono le finestre), si cambiano QUI e
basta.

    LA USERBAR, sul bordo sinistro (x 8..48), centrata in altezza (y 260..540): in colonna
    il pulsante App e le app del sistema, tondi di 32 pixel a 40 l'uno dall'altro, con il
    centro in x = 28:
        App             y = 280
        Attivita        y = 320
        Doom            y = 360
        Esempio         y = 400
        Files           y = 440
        Impostazioni    y = 480
        Store           y = 520
    A sinistra di un'app aperta, nel margine della barra (x 9..11), c'e' un segno scuro
    (nel tema chiaro): un puntino alto 4, una lineetta alta 14 per quella in primo piano.

    IL VASSOIO DELLE APP si apre con il pulsante App, a destra della barra (x = 52), centrato
    sul pulsante App: tutte le app installate, 3 per riga, voci di 80 x 68 a 88 l'una
    dall'altra. Con le 6 app del sistema: 288 x 176, angolo in (52, 192); il centro del
    tondo della prima voce in (108, 232).

    LE FINESTRE si aprono "a scala": la prima con l'angolo in (140, 130), ognuna delle
    successive 40 pixel piu' in basso e a destra (a destra della userbar).
"""

import time

from qemu import pixel_diversi

FISSATE = ["Attivita", "Doom", "Esempio", "Files", "Impostazioni", "Store"]
BARRA_X = 8                       # la userbar: bordo sinistro, larga 40
BARRA_Y = (800 - (8 + 7 * 32 + 6 * 8)) // 2   # 260: centrata, con App e 6 app
VOCE_X = BARRA_X + 4 + 16         # il centro dei tondi
APP_PULSANTE = (VOCE_X, BARRA_Y + 4 + 16)     # il pulsante App: (28, 280)
# Contatore non e' nella userbar: non e' un'app del sistema, il suo pacchetto sta sulla
# Scrivania della persona (build.sh). Le misure della sua finestra restano in MISURE, per
# la prova pacchetti, che la installa e la apre.
APP = {nome: (VOCE_X, APP_PULSANTE[1] + 40 * (i + 1)) for i, nome in enumerate(FISSATE)}
SEGNO_X = (BARRA_X + 1, BARRA_X + 4)   # dove sta il segno "aperta", nel margine a sinistra

# Il vassoio con le 6 app del sistema (con Contatore installata ha una riga in piu', e
# si apre piu' in alto: VASSOIO vale solo per le 6).
VASSOIO_ANGOLO = (52, 192)
VASSOIO = {nome: (52 + 16 + 40 + 88 * (i % 3), 192 + 16 + 24 + 76 * (i // 3))
           for i, nome in enumerate(FISSATE)}
VASSOIO_RETTANGOLO = (52, 192, 52 + 288, 192 + 176)

# Le misure delle finestre (larghezza, altezza), senza l'ombra.
MISURE = {"Attivita": (640, 460), "Contatore": (200, 146), "Esempio": (320, 220),
          "Impostazioni": (460, 420), "Files": (800, 480)}

BARRA = 36            # l'altezza della barra del titolo delle finestre di UIKit

# I colori di Esempio (esempio.c): disegna tutto da sola, a colori pieni, quindi i suoi
# pixel si possono cercare esattamente.
ESEMPIO_SFONDI = [0xF4E8C8, 0xC8E8F4, 0xD8F4C8, 0xF4C8D8]
ESEMPIO_BARRA_FUOCO = 0x3A6EA5
ESEMPIO_BARRA_SENZA = 0x707880


def angolo(posto):
    """L'angolo in alto a sinistra della finestra aperta per 'posto'-esima (da 0)."""
    return 140 + 40 * posto, 130 + 40 * posto


def comandi(x, y, larghezza):
    """Dove sono "riduci" e "chiudi" nella barra di una finestra di UIKit."""
    return (x + larghezza - 49, y + 18), (x + larghezza - 21, y + 18)


def _luce(foto, rettangolo):
    zona = foto.crop(rettangolo).convert("L")
    valori = list(zona.getdata())
    return sum(valori) / len(valori)


def userbar_visibile(foto):
    """La userbar c'e': il pulsante App (Primario, pieno) si stacca dal vetro della barra
    intorno, in tutti e due i temi. Senza userbar li' c'e' solo lo sfondo, uniforme."""
    x, y = APP_PULSANTE
    pulsante = _luce(foto, (x - 14, y - 2, x - 11, y + 2))      # dentro il tondo, a sinistra dell'icona
    vetro = _luce(foto, (x - 4, y + 18, x + 4, y + 22))          # tra App e la prima app
    return abs(pulsante - vetro) > 60


def segno_sotto(foto, app):
    """Quanti pixel scuri (il segno nel tema chiaro) ci sono a sinistra dell'icona
    dell'applicazione nella userbar: 0 se e' chiusa, qualcuno (il puntino o la lineetta)
    se e' aperta."""
    _x, y = APP[app]
    zona = foto.crop((SEGNO_X[0], y - 8, SEGNO_X[1], y + 8)).convert("L")
    return sum(1 for v in zona.getdata() if v < 110)


def clic_app(m, app):
    """Un clic sull'applicazione nella userbar."""
    m.clic(*APP[app])


def apri(m, app, attesa=15.0):
    """Clicca l'applicazione nella userbar, rimette il puntatore in un angolo e aspetta
    che accanto all'icona compaia il segno 'aperta' e che lo schermo si fermi."""
    from qemu import PUNTO_VUOTO
    clic_app(m, app)
    m.muovi(*PUNTO_VUOTO)
    m.aspetta_che(lambda f: segno_sotto(f, app) > 0, attesa, cosa=f"{app} aperta nella userbar")
    return m.aspetta_schermo_fermo()


def zona_uguale(a, b, rettangolo, soglia=6, tollerati=0):
    """Vero se le due foto, dentro il rettangolo, sono uguali (a meno di sfumature)."""
    return pixel_diversi(a.crop(rettangolo), b.crop(rettangolo), None, soglia) <= tollerati


def aspetta_zona(m, riferimento, rettangolo, uguale=True, attesa=10.0, cosa="la zona"):
    """Aspetta che il rettangolo dello schermo diventi uguale (o diverso) a quello di
    'riferimento'. Torna la foto."""
    return m.aspetta_che(lambda f: zona_uguale(f, riferimento, rettangolo) == uguale,
                         attesa, cosa=cosa)


def pausa_breve():
    """Dopo un clic che non cambia niente di visibile (per esempio "porta davanti" una
    finestra che lo e' gia'), un attimo per lasciarlo arrivare."""
    time.sleep(0.3)
