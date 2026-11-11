"""Disegna la freccia del puntatore e la scrive come tabella C (da incollare in
utente/programmi/compositore.c, al posto di quella che c'e').

    uv run --with pillow strumenti/grafica/puntatore.py

La freccia e' nera col bordo bianco e un'ombra morbida sotto, come quella del Mac, alta
19 pixel (dalla punta alla fine della coda, bordo compreso) e larga 12, come la vecchia
freccia a quadretti.

  - IL CORPO (nero) e' un poligono: la punta, il lato sinistro, la tacca, la coda, l'ala.
  - IL BORDO (bianco) e' tutto quello che sta a meno di BORDO pixel dal corpo: il poligono
    piu' i suoi lati ripassati con un tratto largo 2*BORDO e un cerchio su ogni vertice.
  - Tutto si disegna SCALA volte piu' grande e poi si rimpicciolisce facendo la media: gli
    orli vengono sfumati (antialiasing).
  - L'OMBRA e' la sagoma intera spostata in basso a destra, sfocata e nera al 30%; la
    freccia si mette sopra.

Ogni pixel della tabella e' 0xAAGG: AA quanto copre (alfa), GG quanto bianco c'e' (gia'
moltiplicato per alfa). Si stampano anche FRECCIA_L, FRECCIA_A e il punto caldo
(FRECCIA_PX, FRECCIA_PY): il pixel della tabella dove sta la punta.
"""
from PIL import Image, ImageDraw, ImageFilter

SCALA = 8                    # quante volte piu' grande si disegna
BORDO = 1.0                  # lo spessore del bordo bianco, in pixel
OMBRA_DX, OMBRA_DY = 0.5, 1.0
OMBRA_SFOCATURA = 1.0        # raggio della sfocatura, in pixel
OMBRA_NERO = 0.30
SOGLIA = 3                   # l'ombra con alfa fino a 3 (su 255) non si tiene
MARGINE = 4                  # spazio attorno alla freccia per l'ombra (poi si taglia)

# Il corpo nero, in pixel, con la punta della sagoma col bordo in (0, 0): la punta del
# corpo sta quindi in (BORDO, BORDO).
CORPO = [
    (1.0, 1.0),     # la punta
    (11.0, 11.0),   # la punta dell'ala (il lato destro va giu' a 45 gradi)
    (7.2, 11.2),    # dove l'ala incontra la coda
    (9.4, 16.2),    # la coda, in fondo a destra
    (7.2, 17.3),    # la coda, in fondo a sinistra
    (4.9, 12.6),    # la tacca tra la coda e il lato sinistro
    (1.0, 16.0),    # il lato sinistro, in fondo
]


def grande(p):
    return ((p[0] + MARGINE) * SCALA, (p[1] + MARGINE) * SCALA)


def maschere():
    lato_l = (14 + 2 * MARGINE) * SCALA
    lato_a = (20 + 2 * MARGINE) * SCALA
    corpo = Image.new("L", (lato_l, lato_a), 0)
    ImageDraw.Draw(corpo).polygon([grande(p) for p in CORPO], fill=255)
    sagoma = corpo.copy()
    d = ImageDraw.Draw(sagoma)
    r = BORDO * SCALA
    punti = [grande(p) for p in CORPO]
    for k in range(len(punti)):
        a, b = punti[k], punti[(k + 1) % len(punti)]
        d.line([a, b], fill=255, width=round(2 * r))
        d.ellipse([a[0] - r, a[1] - r, a[0] + r, a[1] + r], fill=255)
    ombra = Image.new("L", sagoma.size, 0)
    ombra.paste(sagoma, (round(OMBRA_DX * SCALA), round(OMBRA_DY * SCALA)))
    ombra = ombra.filter(ImageFilter.GaussianBlur(OMBRA_SFOCATURA * SCALA))
    piccola = lambda m: m.reduce(SCALA)
    return piccola(corpo), piccola(sagoma), piccola(ombra)


def tabella():
    corpo, sagoma, ombra = maschere()
    l, a = corpo.size
    pixel = {}
    for y in range(a):
        for x in range(l):
            cb = corpo.getpixel((x, y)) / 255
            co = sagoma.getpixel((x, y)) / 255
            s = ombra.getpixel((x, y)) / 255 * OMBRA_NERO
            alfa = co + s * (1 - co)
            grigio = co - cb                      # il bianco del bordo, gia' per alfa
            aa, gg = round(alfa * 255), round(grigio * 255)
            if aa > SOGLIA or gg:                 # l'ombra quasi invisibile si toglie
                pixel[x, y] = (aa, min(gg, aa))
    x0 = min(x for x, _ in pixel); x1 = max(x for x, _ in pixel) + 1
    y0 = min(y for _, y in pixel); y1 = max(y for _, y in pixel) + 1
    righe = [[pixel.get((x, y), (0, 0)) for x in range(x0, x1)] for y in range(y0, y1)]
    return righe, MARGINE - x0, MARGINE - y0


def main():
    righe, px, py = tabella()
    print(f"#define FRECCIA_L  {len(righe[0])}")
    print(f"#define FRECCIA_A  {len(righe)}")
    print(f"#define FRECCIA_PX {px}")
    print(f"#define FRECCIA_PY {py}")
    print("static const unsigned short freccia[FRECCIA_A][FRECCIA_L] = {")
    for riga in righe:
        print("    {" + ",".join(f"0x{aa:02X}{gg:02X}" for aa, gg in riga) + "},")
    print("};")


if __name__ == "__main__":
    main()
