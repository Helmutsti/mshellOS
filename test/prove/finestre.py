"""finestre: portare davanti, trascinare, ridurre e riaprire dall'isola, chiudere.

Si aprono Contatore (primo posto, angolo in 140,130) ed Esempio (secondo posto, 180,170):
si sovrappongono nel rettangolo 180..340 x 170..276. Il punto SOPRA (320, 250) sta in
tutte e due: se si vede il colore pieno di Esempio, davanti c'e' Esempio; se no Contatore.
  1. Esempio, aperta per ultima, e' davanti (e la sua barra e' blu: ha il fuoco).
  2. Un clic sulla barra di Contatore la porta davanti (e la barra di Esempio diventa
     grigia); un clic sulla barra di Esempio la riporta davanti.
  3. Esempio si trascina per la barra di (400, 300): il suo sfondo si deve spostare
     esattamente di tanto, e dove era prima deve tornare la scrivania.
  4. Contatore si riduce ("-" nella barra): sparisce, ma nell'isola resta aperta. Un
     clic sulla sua icona nell'isola la fa ricomparire.
  5. Contatore si chiude ("x" nella barra): sparisce, e nell'isola non e' piu' aperta.
"""

import scrivania as s
from qemu import colore_in, riquadro_colore, PUNTO_VUOTO

NOME = "finestre"
ORDINE = 50
DESCRIZIONE = "porta davanti, trascina, riduci e riapri dall'isola, chiudi"


def esegui(p):
    m = p.m
    s.apri(m, "Contatore")
    s.apri(m, "Esempio")
    cx, cy = s.angolo(0)                     # Contatore
    ex, ey = s.angolo(1)                     # Esempio
    cl, ca = s.MISURE["Contatore"]
    contatore = (cx, cy, cx + cl, cy + ca)
    sopra = (320, 250)
    barra_esempio = (ex + 270, ey + 15)      # un punto della barra di Esempio a destra
    sfondo = s.ESEMPIO_SFONDI[0]

    # 1. davanti c'e' Esempio
    f = m.foto("due_finestre")
    p.controlla(colore_in(f, *sopra) == sfondo, "Esempio, aperta per ultima, non e' davanti")
    p.controlla(colore_in(f, *barra_esempio) == s.ESEMPIO_BARRA_FUOCO,
                "la barra di Esempio non e' blu (non ha il fuoco)")

    # 2. davanti Contatore, poi di nuovo Esempio
    m.clic(cx + 20, cy + 18)
    m.muovi(*PUNTO_VUOTO)
    m.aspetta_che(lambda f: colore_in(f, *sopra) != sfondo, 5.0, cosa="Contatore davanti")
    f = m.aspetta_che(lambda f: colore_in(f, *barra_esempio) == s.ESEMPIO_BARRA_SENZA, 3.0,
                      cosa="la barra di Esempio grigia (senza fuoco)")
    m.foto("contatore_davanti")
    m.clic(*barra_esempio)
    m.muovi(*PUNTO_VUOTO)
    m.aspetta_che(lambda f: colore_in(f, *sopra) == sfondo, 5.0, cosa="Esempio di nuovo davanti")

    # 3. trascinare Esempio di (400, 300)
    prima = riquadro_colore(m.aspetta_schermo_fermo(), sfondo)
    m.trascina(*barra_esempio, barra_esempio[0] + 400, barra_esempio[1] + 300)
    m.muovi(*PUNTO_VUOTO)
    atteso = (prima[0] + 400, prima[1] + 300, prima[2] + 400, prima[3] + 300)
    f = m.aspetta_che(lambda f: riquadro_colore(f, sfondo) == atteso, 5.0,
                      cosa=f"lo sfondo di Esempio spostato in {atteso}")
    f = m.aspetta_schermo_fermo()
    m.foto("trascinata")
    vecchio = (contatore[2] + 40, ey + 60, ex + 320, ey + 220)   # dove era Esempio, fuori da Contatore
    p.controlla(s.zona_uguale(f, p.scrivania, vecchio),
                "dove era Esempio non e' tornata la scrivania")

    # 4. ridurre Contatore e riaprirla dall'isola
    riduci, chiudi = s.comandi(cx, cy, cl)
    m.clic(*riduci)
    m.muovi(*PUNTO_VUOTO)
    f = s.aspetta_zona(m, p.scrivania, contatore, cosa="Contatore sparire (ridotta)")
    m.foto("ridotta")
    p.controlla(s.segno_sotto(f, "Contatore") > 0, "ridotta, Contatore non e' piu' aperta nell'isola")
    m.clic(s.APP_X["Contatore"], s.ISOLA_Y)
    m.muovi(*PUNTO_VUOTO)
    s.aspetta_zona(m, p.scrivania, contatore, uguale=False, cosa="Contatore ricomparire dall'isola")
    m.aspetta_schermo_fermo()
    m.foto("riaperta")

    # 5. chiudere Contatore
    m.clic(*chiudi)
    m.muovi(*PUNTO_VUOTO)
    s.aspetta_zona(m, p.scrivania, contatore, cosa="Contatore sparire (chiusa)")
    m.aspetta_che(lambda f: s.segno_sotto(f, "Contatore") == 0, 5.0,
                  cosa="Contatore chiusa nell'isola (senza puntino)")
