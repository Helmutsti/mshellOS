"""Disegna le forme del puntatore e le scrive come tabelle C in utente/programmi/puntatori.h.

    uv run --with pillow strumenti/grafica/puntatore.py            riscrive puntatori.h
    uv run --with pillow strumenti/grafica/puntatore.py --anteprima chiaro out.png
    uv run --with pillow strumenti/grafica/puntatore.py --anteprima scuro out.png

Le forme sono quelle del disegno ("Componenti base", Puntatore, frame 95:788, scuro
160:3786): Normale (Idle), Collegamento (Link, la mano), Attesa (Caricamento, l'anello che
gira), Testo (Paragrafo, la barra a I), Non disponibile (Disabilitato, la freccia col
divieto). Ognuna sta su un canvas 32 x 32 con il suo punto caldo; i tracciati qui sotto
sono copiati a mano dai file SVG del disegno (stesse coordinate, il canvas ha l'origine in
(0, 0): 'sposta' dice dove cade l'origine di ogni SVG sul canvas).

Ogni forma ha un contorno (bianco) e una forma (scura), piu' l'ombra "ombra/puntatore": due
ombre nere, la prima spostata di 1 e sfocata 0,5 (opacita' 0,20), la seconda spostata di 3,
sfocata 3, ristretta di 1 (0,26). IL PUNTATORE NON CAMBIA COL TEMA (deciso dalla persona):
stessi colori e stessa ombra anche nello scuro. La tabella porta ancora l'ombra di un tempo
dello scuro (DD), che il compositore non usa piu'. Ogni pixel porta QUATTRO numeri, uno per byte,
0xCCFFLLDD:

    CC  quanto copre il contorno          FF  quanto copre la forma
    LL  quanto e' scura l'ombra (chiaro)  DD  quanto e' scura l'ombra (scuro)

Il compositore li mescola con i colori del tema (cursore_contorno, cursore_riempimento) e con
la scena: prima l'ombra, poi il contorno, poi la forma, come nell'SVG.

Tutto si disegna SCALA volte piu' grande e poi si rimpicciolisce facendo la media (orli
sfumati). L'attesa ha FOTOGRAMMI immagini, l'anello ruotato di 360/FOTOGRAMMI gradi ogni volta.
"""
import math
import re
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

SCALA = 8
MARGINE = 16            # attorno al canvas 32 x 32, per le ombre
CANVAS = 32
FOTOGRAMMI = 12         # l'attesa gira di 360 gradi in 1 secondo: un fotogramma ogni 1/12 s
SOGLIA = 3              # un'ombra con opacita' fino a 3 (su 255) non si tiene

# colori del disegno, per l'anteprima (il compositore li legge dal tema)
TEMI = {
    "chiaro": dict(contorno=(255, 255, 255), forma=(0x37, 0x37, 0x3B), fondo=(223, 223, 223), ombra=(0.20, 0.26)),
    # il puntatore non cambia col tema (deciso dalla persona): cambia solo il fondo
    "scuro": dict(contorno=(255, 255, 255), forma=(0x37, 0x37, 0x3B), fondo=(0x1C, 0x1C, 0x1E), ombra=(0.20, 0.26)),
}

# --- i tracciati (dai file SVG del disegno) ---
FRECCIA = "M6.5 4V24L11.5 19.4L14.7 26.6L18.1 25.1L14.9 18H21.7L6.5 4Z"
DIVIETO = "M26 31.5C29.3137 31.5 32 28.8137 32 25.5C32 22.1863 29.3137 19.5 26 19.5C22.6863 19.5 20 22.1863 20 25.5C20 28.8137 22.6863 31.5 26 31.5Z"
DIVIETO_BARRA = "M22 29.5L30 21.5"

# la mano: cinque tracciati, gli stessi nel contorno (tratto 5) e nella forma (tratto 2). I due
# SVG del disegno hanno le coordinate sfalsate di 1,5 (il contorno e' piu' largo) e stanno in
# posti diversi del canvas: sommando le due cose il tracciato e' lo stesso, e qui ce n'e' uno
MANO = [
    "M21.1056 13C21.1056 15.1217 20.2627 17.1566 18.7624 18.6569C17.2621 20.1571 15.2273 21 13.1056 21",
    "M17.1056 10V9C17.1056 8.46957 16.8949 7.96086 16.5198 7.58579C16.1447 7.21071 15.636 7 15.1056 7C14.5751 7 14.0664 7.21071 13.6914 7.58579C13.3163 7.96086 13.1056 8.46957 13.1056 9",
    "M13.1056 9V8C13.1056 7.46957 12.8949 6.96086 12.5198 6.58579C12.1447 6.21071 11.636 6 11.1056 6C10.5751 6 10.0664 6.21071 9.69136 6.58579C9.31629 6.96086 9.10557 7.46957 9.10557 8V9",
    "M9.10557 8.5V3C9.10557 2.46957 8.89486 1.96086 8.51979 1.58579C8.14471 1.21071 7.63601 1 7.10557 1C6.57514 1 6.06643 1.21071 5.69136 1.58579C5.31629 1.96086 5.10557 2.46957 5.10557 3V13",
    "M17.1056 10C17.1056 9.46957 17.3163 8.96086 17.6914 8.58579C18.0664 8.21071 18.5751 8 19.1056 8C19.636 8 20.1447 8.21071 20.5198 8.58579C20.8949 8.96086 21.1056 9.46957 21.1056 10V13C21.1056 15.1217 20.2627 17.1566 18.7624 18.6569C17.2621 20.1571 15.2273 21 13.1056 21H11.1056C8.30557 21 6.60557 20.14 5.11557 18.66L1.51557 15.06C1.17151 14.6789 0.987159 14.1802 1.0007 13.6669C1.01423 13.1537 1.22462 12.6653 1.58829 12.303C1.95197 11.9406 2.44107 11.7319 2.95434 11.7202C3.46761 11.7085 3.96574 11.8946 4.34557 12.24L6.10557 14",
]
# dove cade l'origine dell'SVG della forma sul canvas (dalla posizione dei riquadri nel disegno)
MANO_A = (4.8944, 5.0)

ANELLO = "M26.25 16C26.25 20.9706 22.2206 25 17.25 25C12.2794 25 8.25 20.9706 8.25 16C8.25 11.0294 12.2794 7 17.25 7C22.2206 7 26.25 11.0294 26.25 16Z"
ARCO = "M17.25 7C19.03 7 20.7701 7.52784 22.2501 8.51677C23.7302 9.50571 24.8837 10.9113 25.5649 12.5558C26.2461 14.2004 26.4243 16.01 26.0771 17.7558C25.7298 19.5016 24.8726 21.1053 23.614 22.364C22.3553 23.6226 20.7516 24.4798 19.0058 24.8271C17.26 25.1743 15.4504 24.9961 13.8058 24.3149C12.1613 23.6337 10.7557 22.4802 9.76677 21.0001C8.77784 19.5201 8.25 17.78 8.25 16"

BARRA_I = "M12 6H16M16 6H20M16 6V26M16 26H12M16 26H20"


def percorso(d, a=(0.0, 0.0)):
    """Un tracciato SVG (solo M L H V C Z assoluti) come lista di sotto-tracciati: punti sul
    canvas, con le curve spezzate in tanti segmentini. Per ognuno, se e' chiuso."""
    pezzi, x, y, cur, chiuso = [], 0.0, 0.0, None, False
    for cmd, args in re.findall(r"([MLHVCZ])([^MLHVCZ]*)", d):
        n = [float(v) for v in re.findall(r"-?\d+\.?\d*", args)]
        if cmd == "M":
            if cur: pezzi.append((cur, chiuso))
            x, y = n[0], n[1]; cur = [(x, y)]; chiuso = False
        elif cmd == "L":
            x, y = n[0], n[1]; cur.append((x, y))
        elif cmd == "H":
            x = n[0]; cur.append((x, y))
        elif cmd == "V":
            y = n[0]; cur.append((x, y))
        elif cmd == "C":
            for k in range(0, len(n), 6):
                x1, y1, x2, y2, x3, y3 = n[k:k + 6]
                for t in range(1, 25):
                    u = t / 24
                    b = ((1 - u) ** 3, 3 * u * (1 - u) ** 2, 3 * u * u * (1 - u), u ** 3)
                    cur.append((b[0] * x + b[1] * x1 + b[2] * x2 + b[3] * x3,
                                b[0] * y + b[1] * y1 + b[2] * y2 + b[3] * y3))
                x, y = x3, y3
        elif cmd == "Z":
            chiuso = True
    if cur: pezzi.append((cur, chiuso))
    return [([(px + a[0], py + a[1]) for px, py in p], c) for p, c in pezzi]


def ruota(pezzi, gradi, cx=16.0, cy=16.0):
    r = math.radians(gradi)
    c, s = math.cos(r), math.sin(r)
    return [([(cx + (x - cx) * c - (y - cy) * s, cy + (x - cx) * s + (y - cy) * c) for x, y in p], ch) for p, ch in pezzi]


def grande(p):
    return ((p[0] + MARGINE) * SCALA, (p[1] + MARGINE) * SCALA)


def maschera(pezzi, riempi, tratto):
    """Una maschera (L) grande SCALA volte: i sotto-tracciati riempiti e/o ripassati con un
    tratto largo 'tratto' pixel, con estremi e giunzioni tondi."""
    lato = (CANVAS + 2 * MARGINE) * SCALA
    m = Image.new("L", (lato, lato), 0)
    d = ImageDraw.Draw(m)
    for punti, chiuso in pezzi:
        g = [grande(p) for p in punti]
        if riempi and len(g) > 2: d.polygon(g, fill=255)
        if tratto:
            r = tratto * SCALA / 2
            seq = g + [g[0]] if chiuso else g
            for a, b in zip(seq, seq[1:]):
                d.line([a, b], fill=255, width=round(tratto * SCALA))
            for a in seq:
                d.ellipse([a[0] - r, a[1] - r, a[0] + r, a[1] + r], fill=255)
    return m


def unione(*ms):
    r = ms[0]
    for m in ms[1:]: r = ImageChops.lighter(r, m)
    return r


def forma_normale():
    p = percorso(FRECCIA, (-0.5, 0))
    return unione(maschera(p, True, 3)), unione(maschera(p, True, 0)), (6, 4)


def forma_collegamento():
    c = unione(*[maschera(percorso(d, MANO_A), False, 5) for d in MANO])
    f = unione(*[maschera(percorso(d, MANO_A), False, 2) for d in MANO])
    return c, f, (12, 6)


def forma_testo():
    p = percorso(BARRA_I)
    return maschera(p, False, 5.2), maschera(p, False, 2.4), (16, 16)


def forma_non_disponibile():
    fr, di, ba = percorso(FRECCIA, (-0.5, 0)), percorso(DIVIETO, (-0.5, 0)), percorso(DIVIETO_BARRA, (-0.5, 0))
    c = unione(maschera(fr, True, 3), maschera(di, True, 3))
    f = unione(maschera(fr, True, 0), maschera(di, False, 2), maschera(ba, False, 2))
    return c, f, (6, 4)


PISTA = 0.22   # quanto si vede l'anello intero sotto l'arco (nel disegno chiaro; vedi forma_attesa)


def forma_attesa(gradi):
    """L'anello: il contorno e' tutto l'anello (tratto 6,5); la forma e' l'arco di 270 gradi
    (tratto 3), che gira, sopra l'anello intero (la "pista") poco visibile. Nel disegno chiaro
    la pista e' al 22% (si misura sulla foto); in quello scuro e' quasi piena: il disegno non
    e' coerente, qui si usa il 22% in tutti e due (vedi il resoconto)."""
    a = (-1.25, 0)
    c = maschera(ruota(percorso(ANELLO, a), gradi), False, 6.5)
    pista = maschera(percorso(ANELLO, a), False, 3).point(lambda v: round(v * PISTA))
    f = unione(maschera(ruota(percorso(ARCO, a), gradi), False, 3), pista)
    return c, f, (16, 16)


def piccola(m):
    return m.reduce(SCALA)


def ombra(unita, dy, sfuma, stringi, opacita_chiaro, opacita_scuro):
    """L'ombra di un'unione c|f: spostata, ristretta, sfocata. Restituisce la maschera L
    (grande) a opacita' 1; l'opacita' si applica dopo."""
    m = unita
    if stringi:
        m = m.filter(ImageFilter.MinFilter(2 * stringi * SCALA + 1))
    s = Image.new("L", m.size, 0)
    s.paste(m, (0, round(dy * SCALA)))
    return s.filter(ImageFilter.GaussianBlur(sfuma * SCALA))


def forma_completa(c, f, caldo):
    """(c, f, ombra chiara, ombra scura) piccole, tutte della stessa misura (canvas con
    margine), e il punto caldo sul canvas con margine."""
    u = unione(c, f)
    s1 = ombra(u, 1, 0.5, 0, 0, 0)
    s2 = ombra(u, 3, 3, 1, 0, 0)
    out = []
    for o1, o2 in ((0.20, 0.26), (0.55, 0.55)):
        # due ombre una sull'altra: 1 - (1 - o1*s1) * (1 - o2*s2)
        a = s1.point(lambda v: round(v * o1))
        b = s2.point(lambda v: round(v * o2))
        out.append(ImageChops.invert(ImageChops.multiply(ImageChops.invert(a), ImageChops.invert(b))))
    return piccola(c), piccola(f), piccola(out[0]), piccola(out[1]), (caldo[0] + MARGINE, caldo[1] + MARGINE)


def tutte_le_forme():
    """[(nome, [forma_completa...])]: i fotogrammi (uno, tranne l'attesa)."""
    out = [("normale", [forma_completa(*forma_normale())]),
           ("collegamento", [forma_completa(*forma_collegamento())]),
           ("testo", [forma_completa(*forma_testo())]),
           ("non_disponibile", [forma_completa(*forma_non_disponibile())]),
           ("attesa", [forma_completa(*forma_attesa(360 * k / FOTOGRAMMI)) for k in range(FOTOGRAMMI)])]
    return out


def riquadro(fotogrammi):
    """Il rettangolo che contiene tutti i pixel utili di tutti i fotogrammi: (x0, y0, x1, y1)."""
    x0 = y0 = 10 ** 9
    x1 = y1 = -1
    for c, f, sl, sd, _ in fotogrammi:
        u = unione(c, f, sl.point(lambda v: 255 if v > SOGLIA else 0), sd.point(lambda v: 255 if v > SOGLIA else 0))
        b = u.getbbox()
        if b:
            x0, y0, x1, y1 = min(x0, b[0]), min(y0, b[1]), max(x1, b[2]), max(y1, b[3])
    return x0, y0, x1, y1


def pixel_forma(fotogramma, r):
    c, f, sl, sd, _ = fotogramma
    x0, y0, x1, y1 = r
    return [[(c.getpixel((x, y)) << 24) | (f.getpixel((x, y)) << 16) | (sl.getpixel((x, y)) << 8) | sd.getpixel((x, y))
             for x in range(x0, x1)] for y in range(y0, y1)]


def scrivi_c():
    righe = ["/* Le forme del puntatore (Normale, Collegamento, Testo, Non disponibile, Attesa).",
             " * FILE GENERATO da strumenti/grafica/puntatore.py: non si modifica a mano (si cambia lo",
             " * script e si rigenera; il disegno e' in Figma, \"Componenti base\", Puntatore).",
             " * Ogni pixel e' 0xCCFFLLDD: quanto copre il contorno, quanto la forma, quanto e' scura",
             " * l'ombra nel tema chiaro e nello scuro (vedi il compositore: disegna_puntatore). */",
             "#ifndef PUNTATORI_H", "#define PUNTATORI_H", "",
             f"#define FOTOGRAMMI_ATTESA {FOTOGRAMMI}", ""]
    elenco = []
    for nome, foto in tutte_le_forme():
        r = riquadro(foto)
        l, a = r[2] - r[0], r[3] - r[1]
        px, py = foto[0][4][0] - r[0], foto[0][4][1] - r[1]
        for k, fg in enumerate(foto):
            righe.append(f"static const unsigned int forma_{nome}_{k}[{a}][{l}] = {{")
            for riga in pixel_forma(fg, r):
                righe.append("    {" + ",".join(f"0x{v:08X}" for v in riga) + "},")
            righe.append("};")
        elenco.append((nome, l, a, px, py, len(foto)))
        righe.append("")
    righe.append("struct forma_puntatore_immagine { int l, a, px, py; const unsigned int *pixel; };")
    righe.append("")
    righe.append("/* una per forma, nell'ordine di FORMA_... (gkit.h); i fotogrammi dell'attesa sono a parte */")
    righe.append("static const struct forma_puntatore_immagine forme_puntatore[] = {")
    for nome, l, a, px, py, n in elenco:
        righe.append(f"    {{ {l}, {a}, {px}, {py}, &forma_{nome}_0[0][0] }},   /* {nome} */")
    righe.append("};")
    righe.append("")
    righe.append("static const unsigned int *const fotogrammi_attesa[FOTOGRAMMI_ATTESA] = {")
    righe.append("    " + ", ".join(f"&forma_attesa_{k}[0][0]" for k in range(FOTOGRAMMI)))
    righe.append("};")
    righe.append("")
    righe.append("#endif")
    return "\n".join(righe) + "\n"


def anteprima(tema, fuori, zoom):
    """Tutte le forme sul fondo del tema, ciascuna in un riquadro 32 x 32 del disegno (il
    punto caldo al suo posto), come i riquadri del disegno."""
    t = TEMI[tema]
    forme = tutte_le_forme()
    img = Image.new("RGB", (len(forme) * CANVAS, CANVAS), t["fondo"])
    for i, (nome, foto) in enumerate(forme):
        c, f, sl, sd, caldo = foto[0]
        s = sl                    # l'ombra del chiaro anche nello scuro (il compositore fa lo stesso)
        dest = Image.new("RGB", c.size, t["fondo"])
        dest = Image.composite(Image.new("RGB", c.size, (0, 0, 0)), dest, s)
        dest = Image.composite(Image.new("RGB", c.size, t["contorno"]), dest, c)
        dest = Image.composite(Image.new("RGB", c.size, t["forma"]), dest, f)
        img.paste(dest.crop((MARGINE, MARGINE, MARGINE + CANVAS, MARGINE + CANVAS)), (i * CANVAS, 0))
    img.resize((img.width * zoom, img.height * zoom), Image.NEAREST).save(fuori)


if __name__ == "__main__":
    if len(sys.argv) >= 4 and sys.argv[1] == "--anteprima":
        anteprima(sys.argv[2], sys.argv[3], 8)
    else:
        dest = Path(__file__).resolve().parent.parent.parent / "utente" / "programmi" / "puntatori.h"
        dest.write_text(scrivi_c())
        print("scritto", dest)
