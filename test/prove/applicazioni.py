"""applicazioni: ogni applicazione si apre dall'isola.

Si cliccano una dopo l'altra le applicazioni dell'isola, tranne Doom (pesante, e la sua
finestra non e' del toolkit), con Files per ultima (la sua finestra, 800 x 480, al quarto
posto sta ancora tutta nello schermo). Per ognuna:
  - sotto la sua icona deve comparire il segno "aperta" (il puntino o il trattino);
  - la sua finestra deve comparire dove la mette il compositore (a scala: la prima in
    (140, 130), le altre 40 pixel piu' in basso e a destra), cioe' quella zona dello
    schermo deve cambiare rispetto a prima.
Alla fine si preme il pulsante del menu (la griglia a sinistra nell'isola): per ora non
apre niente, ma non deve rompere niente (lo schermo resta, il sistema risponde).
"""

import time

import scrivania as s
from qemu import isola_visibile, PUNTO_VUOTO

NOME = "applicazioni"
ORDINE = 20
DESCRIZIONE = "ogni applicazione si apre dall'isola; il pulsante del menu non rompe niente"


def esegui(p):
    m = p.m
    prima = p.scrivania
    for posto, app in enumerate(["Attivita", "Esempio", "Impostazioni", "Files"]):
        p.controlla(s.segno_sotto(prima, app) == 0, f"{app} risulta gia' aperta nell'isola")
        dopo = s.apri(m, app)
        x, y = s.angolo(posto)
        l, a = s.MISURE[app]
        # la barra del titolo della nuova finestra: deve essere cambiata
        barra = (x, y, x + l, y + s.BARRA)
        p.controlla(not s.zona_uguale(prima, dopo, barra),
                    f"{app}: la finestra non compare in ({x}, {y})")
        m.foto(f"aperta_{app}")
        prima = dopo

    # il menu: un clic, poi lo schermo deve restare quello (l'isola, le finestre)
    m.clic(s.MENU_X, s.ISOLA_Y)
    m.muovi(*PUNTO_VUOTO)
    time.sleep(0.5)
    dopo = m.aspetta_schermo_fermo()
    p.controlla(isola_visibile(dopo), "dopo il menu l'isola non si vede piu'")
    # e il sistema risponde ancora: Esempio (gia' aperta) torna davanti
    m.clic(s.APP_X["Esempio"], s.ISOLA_Y)
    m.muovi(*PUNTO_VUOTO)
    x, y = s.angolo(1)
    s.aspetta_zona(m, dopo, (x, y, x + s.MISURE["Esempio"][0], y + s.BARRA), uguale=False,
                   cosa="Esempio tornare davanti dopo il menu")
