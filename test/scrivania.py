"""scrivania.py: quello che le prove sanno della scrivania di mshell (dove sono le cose).

Sono numeri presi dallo schermo vero (1280x800). Se la scrivania o il compositore
cambiano disegno (l'isola, dove si aprono le finestre), si cambiano QUI e basta.

    L'ISOLA, in alto al centro (y = 38 il centro dei tondi):
        menu (griglia)  x = 464
        Attivita        x = 510
        Contatore       x = 548
        Esempio         x = 586
        Files           x = 624
        Impostazioni    x = 662
        l'ora           x = 699..839 (cambia: nei confronti fra foto si copre)
    Sotto un'applicazione aperta c'e' un puntino (y = 57), sotto quella che ha il fuoco
    un trattino.

    LE FINESTRE si aprono "a scala": la prima con l'angolo in (140, 130), ognuna delle
    successive 40 pixel piu' in basso e a destra.

    LA CARTELLA HOME, sullo sfondo in alto a sinistra: l'elemento (84 x 93) con l'angolo in
    (16, 16), l'icona al centro in (58, 52).
"""

import time

from qemu import ErroreProva, pixel_diversi, PUNTO_VUOTO

ISOLA_Y = 38
MENU_X = 464
APP_X = {"Attivita": 510, "Contatore": 548, "Esempio": 586, "Files": 624, "Impostazioni": 662}
HOME = (58, 52)       # il centro dell'icona della cartella Home sulla scrivania
SEGNO_Y = 57          # il puntino / trattino sotto le applicazioni aperte

# Le misure delle finestre (larghezza, altezza), senza l'ombra.
MISURE = {"Attivita": (640, 460), "Contatore": (200, 146), "Esempio": (320, 220),
          "Impostazioni": (460, 420), "Files": (800, 480)}

BARRA = 36            # l'altezza della barra del titolo delle finestre del toolkit

# I colori di Esempio (esempio.c): disegna tutto da sola, a colori pieni, quindi i suoi
# pixel si possono cercare esattamente.
ESEMPIO_SFONDI = [0xF4E8C8, 0xC8E8F4, 0xD8F4C8, 0xF4C8D8]
ESEMPIO_BARRA_FUOCO = 0x3A6EA5
ESEMPIO_BARRA_SENZA = 0x707880


def angolo(posto):
    """L'angolo in alto a sinistra della finestra aperta per 'posto'-esima (da 0)."""
    return 140 + 40 * posto, 130 + 40 * posto


def comandi(x, y, larghezza):
    """Dove sono "riduci" e "chiudi" nella barra di una finestra del toolkit."""
    return (x + larghezza - 49, y + 18), (x + larghezza - 21, y + 18)


def segno_sotto(foto, app):
    """Quanti pixel chiari ci sono sotto l'icona dell'applicazione nell'isola: 0 se e'
    chiusa, qualcuno (il puntino o il trattino) se e' aperta."""
    x = APP_X[app]
    zona = foto.crop((x - 10, SEGNO_Y - 2, x + 11, SEGNO_Y + 3)).convert("L")
    return sum(1 for v in zona.getdata() if v > 170)


def apri(m, app, attesa=15.0):
    """Clicca l'applicazione nell'isola, rimette il puntatore in un angolo e aspetta
    che sotto l'icona compaia il segno 'aperta' e che lo schermo si fermi."""
    m.clic(APP_X[app], ISOLA_Y)
    m.muovi(*PUNTO_VUOTO)
    m.aspetta_che(lambda f: segno_sotto(f, app) > 0, attesa, cosa=f"{app} aperta nell'isola")
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

