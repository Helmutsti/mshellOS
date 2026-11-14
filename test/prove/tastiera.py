"""tastiera: i tasti arrivano all'applicazione col fuoco, e un tasto tenuto si ripete.

Si usa Esempio, che a ogni tasto cambia il colore di sfondo, a giro fra quattro colori
(esempio.c). Il colore si legge direttamente dai pixel: Esempio si disegna a colori
pieni, senza vetro.
  1. Quattro tasti, uno alla volta: lo sfondo deve passare per i quattro colori, in
     ordine, e tornare al primo.
  2. Un tasto TENUTO GIU' per 1,5 secondi: il kernel lo ripete (dopo mezzo secondo, uno
     ogni 30 ms: hid.c), quindi mentre e' giu' lo sfondo deve continuare a cambiare.
  3. Lasciato il tasto, lo sfondo si deve fermare.
"""

import time

import scrivania as s
from qemu import colore_in

NOME = "tastiera"
ORDINE = 40
DESCRIZIONE = "Esempio cambia colore a ogni tasto; un tasto tenuto si ripete"


def esegui(p):
    m = p.m
    s.apri(m, "Esempio")
    x, y = s.angolo(0)
    punto = (x + 300, y + 200)       # in basso a destra nel contenuto: niente quadretti

    def sfondo(f=None):
        return colore_in(f or m.foto(), *punto)

    p.controlla(sfondo() == s.ESEMPIO_SFONDI[0],
                f"Esempio non ha lo sfondo iniziale (c'e' {sfondo():06X})")

    # 1. un tasto alla volta
    for i in range(1, 5):
        atteso = s.ESEMPIO_SFONDI[i % 4]
        m.premi("a")
        m.aspetta_che(lambda f: sfondo(f) == atteso, 3.0,
                      cosa=f"dopo il tasto {i} lo sfondo {atteso:06X}")

    # 2. tenuto giu': mentre e' giu' il colore deve cambiare ancora
    m.tasto_giu("a")
    try:
        time.sleep(0.8)              # oltre il mezzo secondo prima della ripetizione
        visti = []
        # Le pause sono IRREGOLARI di proposito: i colori sono 4 e il kernel ripete ogni 30 ms,
        # quindi i colori tornano uguali ogni 120 ms. Con foto a intervalli fissi (pausa + tempo
        # della foto) che sommano un multiplo di 120 ms, si vedrebbe sempre lo stesso colore
        # anche con la ripetizione in corso (succede su arm64, dove la foto dura ~50 ms).
        for pausa in (0.07, 0.013, 0.041, 0.023, 0.057, 0.031, 0.019, 0.05):
            visti.append(sfondo())
            time.sleep(pausa)
    finally:
        m.tasto_su("a")
    p.controlla(all(c in s.ESEMPIO_SFONDI for c in visti),
                f"colori strani mentre il tasto e' giu': {[f'{c:06X}' for c in visti]}")
    cambi = sum(1 for a, b in zip(visti, visti[1:]) if a != b)
    p.controlla(cambi >= 2, f"il tasto tenuto giu' non si ripete (colori visti: "
                            f"{[f'{c:06X}' for c in visti]})")

    # 3. lasciato: il colore si ferma
    time.sleep(0.3)
    fermo = sfondo()
    time.sleep(0.6)
    p.controlla(sfondo() == fermo, "lasciato il tasto, lo sfondo continua a cambiare")
    p.nota(f"{cambi} cambi di colore in 8 foto col tasto giu'")


DISCO = "disco_app.img"   # con Esempio e Contatore, le app di prova (utente/prove/applicazioni): non sono nel disco normale
