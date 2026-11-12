"""finestre: portare davanti, trascinare, ridurre e riaprire dalla userbar, chiudere.

Si aprono Esempio (primo posto, angolo in 140,130, 320 x 220) e Impostazioni (secondo
posto, 180,170, 460 x 420): Impostazioni copre quasi tutta Esempio, tranne una striscia a
sinistra (x 140..180) e la barra del titolo (y 130..170). Il punto SOPRA (300, 250) sta in
tutte e due: se si vede il colore pieno di Esempio, davanti c'e' Esempio; se no
Impostazioni. La barra di Esempio a destra (410, 145) si vede sempre: e' blu se Esempio ha
il fuoco, grigia se no.
(Prima la prova usava Contatore, che ora non e' piu' un'app del sistema: va installata.)
  1. Impostazioni, aperta per ultima, e' davanti (e la barra di Esempio e' grigia).
  2. Un clic sulla barra di Esempio la porta davanti (barra blu); un clic sulla barra di
     Impostazioni, fuori da Esempio (530, 188), la riporta davanti.
  3. Esempio (portata davanti con un clic) si trascina per la barra di (400, 300): il suo
     sfondo si deve spostare esattamente di tanto, e nella striscia a sinistra dove era
     prima deve tornare la scrivania.
  4. Impostazioni si riduce ("-" nella barra): sparisce, ma nella userbar resta aperta. Un
     clic sulla sua icona nella userbar la fa ricomparire.
  5. Impostazioni si chiude ("x" nella barra): sparisce, e nella userbar non e' piu' aperta.
"""

import scrivania as s
from qemu import colore_in, riquadro_colore, PUNTO_VUOTO

NOME = "finestre"
ORDINE = 50
DESCRIZIONE = "porta davanti, trascina, riduci e riapri dalla userbar, chiudi"


def esegui(p):
    m = p.m
    s.apri(m, "Esempio")
    s.apri(m, "Impostazioni")
    ex, ey = s.angolo(0)                     # Esempio
    ix, iy = s.angolo(1)                     # Impostazioni
    il, ia = s.MISURE["Impostazioni"]
    # la parte di Impostazioni lontana da dove finira' Esempio trascinata (e dalla sua ombra)
    impostazioni = (ix, iy, ix + 320, iy + 220)
    sopra = (300, 250)
    barra_esempio = (ex + 270, ey + 15)      # un punto della barra di Esempio a destra
    barra_impostazioni = (ix + 350, iy + 18) # fuori da Esempio, lontano dai pulsanti
    sfondo = s.ESEMPIO_SFONDI[0]

    # 1. davanti c'e' Impostazioni
    f = m.foto("due_finestre")
    p.controlla(colore_in(f, *sopra) != sfondo, "Impostazioni, aperta per ultima, non e' davanti")
    p.controlla(colore_in(f, *barra_esempio) == s.ESEMPIO_BARRA_SENZA,
                "la barra di Esempio non e' grigia (ha ancora il fuoco)")

    # 2. davanti Esempio, poi di nuovo Impostazioni
    m.clic(*barra_esempio)
    m.muovi(*PUNTO_VUOTO)
    m.aspetta_che(lambda f: colore_in(f, *sopra) == sfondo, 5.0, cosa="Esempio davanti")
    m.aspetta_che(lambda f: colore_in(f, *barra_esempio) == s.ESEMPIO_BARRA_FUOCO, 3.0,
                  cosa="la barra di Esempio blu (con il fuoco)")
    m.foto("esempio_davanti")
    m.clic(*barra_impostazioni)
    m.muovi(*PUNTO_VUOTO)
    m.aspetta_che(lambda f: colore_in(f, *sopra) != sfondo, 5.0, cosa="Impostazioni di nuovo davanti")

    # 3. Esempio davanti e trascinata di (400, 300)
    m.clic(*barra_esempio)
    m.muovi(*PUNTO_VUOTO)
    m.aspetta_che(lambda f: colore_in(f, *sopra) == sfondo, 5.0, cosa="Esempio davanti per trascinarla")
    prima = riquadro_colore(m.aspetta_schermo_fermo(), sfondo)
    m.trascina(*barra_esempio, barra_esempio[0] + 400, barra_esempio[1] + 300)
    m.muovi(*PUNTO_VUOTO)
    atteso = (prima[0] + 400, prima[1] + 300, prima[2] + 400, prima[3] + 300)
    f = m.aspetta_che(lambda f: riquadro_colore(f, sfondo) == atteso, 5.0,
                      cosa=f"lo sfondo di Esempio spostato in {atteso}")
    f = m.aspetta_schermo_fermo()
    m.foto("trascinata")
    vecchio = (ex + 2, ey + 50, ix - 22, ey + 200)   # la striscia di Esempio fuori da Impostazioni
    p.controlla(s.zona_uguale(f, p.scrivania, vecchio),
                "dove era Esempio non e' tornata la scrivania")

    # 4. ridurre Impostazioni e riaprirla dalla userbar
    riduci, chiudi = s.comandi(ix, iy, il)
    m.clic(*riduci)
    m.muovi(*PUNTO_VUOTO)
    f = s.aspetta_zona(m, p.scrivania, impostazioni, cosa="Impostazioni sparire (ridotta)")
    m.foto("ridotta")
    p.controlla(s.segno_sotto(f, "Impostazioni") > 0, "ridotta, Impostazioni non e' piu' aperta nella userbar")
    s.clic_app(m, "Impostazioni")
    m.muovi(*PUNTO_VUOTO)
    s.aspetta_zona(m, p.scrivania, impostazioni, uguale=False, cosa="Impostazioni ricomparire dalla userbar")
    m.aspetta_schermo_fermo()
    m.foto("riaperta")

    # 5. chiudere Impostazioni
    m.clic(*chiudi)
    m.muovi(*PUNTO_VUOTO)
    s.aspetta_zona(m, p.scrivania, impostazioni, cosa="Impostazioni sparire (chiusa)")
    m.aspetta_che(lambda f: s.segno_sotto(f, "Impostazioni") == 0, 5.0,
                  cosa="Impostazioni chiusa nella userbar (senza segno)")


DISCO = "disco_app.img"   # con Esempio e Contatore, le app di prova (utente/prove/applicazioni): non sono nel disco normale
