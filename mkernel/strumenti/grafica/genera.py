"""Prepara la grafica dei programmi: font, icone, immagini e sfondo, come tabelle C.

Gira sul computer che compila, dentro il container alpine (vedi genera.sh), e scrive
utente/libreria/grafica_dati.h. I programmi di mkernel non sanno leggere un file TrueType
o un SVG: qui li si trasforma una volta per tutte in pixel.

  - I FONT: Inter (licenza SIL OFL) in ogni misura e peso che usa l'interfaccia. Ogni
    lettera diventa una piccola tabella di "alfa": quanto e' coperto ogni pixel (0-255).
    I bordi sono gia' lisci (antialiasing): li calcola FreeType, qui.
  - LE ICONE: Lucide (licenza ISC), dagli SVG del file Figma "mshell - interfaccia". Anche
    loro solo alfa: il colore lo sceglie chi disegna.
  - LE IMMAGINI A COLORI (la scelta singola accesa): pixel 0xAARRGGBB premoltiplicati.
  - LO SFONDO: e' una sfumatura molto morbida, quindi lo si tiene piccolo (320x200) e il
    compositore lo ingrandisce all'avvio.

I caratteri sono quelli della code page 437 (come i tasti e la console): l'ASCII piu'
le lettere accentate italiane."""
import io, os, subprocess, sys
from PIL import Image, ImageDraw, ImageFont

QUI = os.path.dirname(os.path.abspath(__file__))
FIGMA = os.path.join(QUI, 'figma')
FONT = sys.argv[1]                  # la cartella con Inter-Regular.ttf, -Medium, -SemiBold
USCITA = sys.argv[2]

# I font dell'interfaccia (dal file Figma): nome C, file, misura in pixel
FONT_USATI = [
    ('inter_14',            'Inter-Regular.ttf',  14),
    ('inter_medio_13',      'Inter-Medium.ttf',   13),
    ('inter_medio_14',      'Inter-Medium.ttf',   14),
    ('inter_medio_16',      'Inter-Medium.ttf',   16),
    ('inter_semigrassetto_13', 'Inter-SemiBold.ttf', 13),
    ('inter_semigrassetto_15', 'Inter-SemiBold.ttf', 15),
]

# code page 437 -> carattere vero, per le lettere accentate che servono all'italiano
CP437 = {0x85: 'à', 0x8A: 'è', 0x82: 'é', 0x8D: 'ì', 0x95: 'ò', 0x97: 'ù', 0xA2: 'ó', 0xA0: 'á'}
CARATTERI = [(c, chr(c)) for c in range(32, 127)] + sorted(CP437.items())

ICONE = ['save', 'check', 'minus', 'plus', 'x', 'user', 'triangle-alert', 'layout-grid', 'settings', 'square']
LATI = [12, 14, 16, 20, 24]

def svg_in_png(file, l, a):
    return Image.open(io.BytesIO(subprocess.check_output(
        ['rsvg-convert', '-w', str(l), '-h', str(a), file]))).convert('RGBA')

def icona_in_png(file, lato):
    """Come le disegna Figma: il tratto resta di 2 pixel a qualunque misura (non si
    rimpicciolisce con l'icona). In una griglia da G diventa quindi 2 * G / lato."""
    svg = open(file, encoding='utf-8').read()
    griglia = float(svg.split('viewBox="')[1].split()[2])   # di solito 24, a volte 16
    svg = svg.replace('stroke-width="2"', 'stroke-width="%.4f"' % (2 * griglia / lato))
    return Image.open(io.BytesIO(subprocess.check_output(
        ['rsvg-convert', '-w', str(lato), '-h', str(lato)], input=svg.encode()))).convert('RGBA')

def tabella_byte(valori, per_riga=24):
    righe = []
    for i in range(0, len(valori), per_riga):
        righe.append('    ' + ', '.join(str(v) for v in valori[i:i + per_riga]) + ',')
    return '\n'.join(righe)

def tabella_parole(valori, per_riga=8):
    righe = []
    for i in range(0, len(valori), per_riga):
        righe.append('    ' + ', '.join('0x%08X' % v for v in valori[i:i + per_riga]) + ',')
    return '\n'.join(righe)

def premoltiplicato(img):
    out = []
    for r, g, b, a in img.get_flattened_data():
        out.append(a << 24 | (r * a // 255) << 16 | (g * a // 255) << 8 | (b * a // 255))
    return out

c = []
c.append('''/* GENERATO da strumenti/grafica/genera.py: non modificare a mano.
 *
 * Font Inter, di Rasmus Andersson (https://rsms.me/inter), licenza SIL Open Font
 * License 1.1. Icone Lucide (https://lucide.dev), licenza ISC: Copyright (c) for portions
 * of Lucide are held by Cole Bemis 2013-2022 as part of Feather (MIT); all other
 * copyright (c) for Lucide are held by Lucide Contributors 2022.
 * Icone, immagini e sfondo vengono dal file Figma "mshell - interfaccia". */
#ifndef GRAFICA_DATI_H
#define GRAFICA_DATI_H

/* Una lettera: dove sta la sua tabella di alfa, dove va rispetto al punto di scrittura
 * (x a destra, y in su dalla linea di base) e di quanto si avanza dopo. */
struct glifo {
    unsigned int inizio;
    signed char x, y;
    unsigned char l, a, avanzo;
};

struct font {
    const struct glifo *glifi;
    const unsigned char *alfa;
    unsigned char indice[256];    /* carattere (code page 437) -> glifo; 0 = non c'e' */
    unsigned char altezza;        /* l'altezza di una riga */
    unsigned char ascesa;         /* dalla cima della riga alla linea di base */
};
''')

for nome, file, misura in FONT_USATI:
    f = ImageFont.truetype(os.path.join(FONT, file), misura)
    ascesa, discesa = f.getmetrics()
    glifi, alfa, indice = [], [], [0] * 256
    for codice, ch in CARATTERI:
        tela = Image.new('L', (misura * 4, misura * 4), 0)
        ImageDraw.Draw(tela).text((misura, misura * 3), ch, font=f, fill=255, anchor='ls')
        riquadro = tela.getbbox()
        avanzo = int(round(f.getlength(ch)))
        if riquadro:
            pezzo = tela.crop(riquadro)
            gx, gy = riquadro[0] - misura, misura * 3 - riquadro[1]
            gl, ga = pezzo.size
            dati = list(pezzo.get_flattened_data())
        else:
            gx = gy = gl = ga = 0
            dati = []
        indice[codice] = len(glifi)
        glifi.append((len(alfa), gx, gy, gl, ga, avanzo))
        alfa += dati
    c.append('static const unsigned char %s_alfa[] = {\n%s\n};' % (nome, tabella_byte(alfa)))
    c.append('static const struct glifo %s_glifi[] = {' % nome)
    for g in glifi:
        c.append('    { %d, %d, %d, %d, %d, %d },' % g)
    c.append('};')
    c.append('static const struct font %s = { %s_glifi, %s_alfa, {%s}, %d, %d };\n' % (
        nome, nome, nome, ', '.join(str(i) for i in indice), ascesa + discesa, ascesa))

c.append('''/* Un'icona a una misura: lato x lato pixel di alfa. */
struct icona {
    unsigned char lato;
    const unsigned char *alfa;
};
''')
nomi_icone = []
for icona in ICONE:
    file = os.path.join(FIGMA, 'icona-%s.svg' % icona)
    nome = icona.replace('-', '_')
    nomi_icone.append(nome)
    for lato in LATI:
        img = icona_in_png(file, lato)
        c.append('static const unsigned char icona_%s_%d[] = {\n%s\n};' % (
            nome, lato, tabella_byte(list(img.getchannel("A").get_flattened_data()))))
c.append('#define ICONA_MISURE %d' % len(LATI))
c.append('static const struct icona icone[][ICONA_MISURE] = {')
for nome in nomi_icone:
    c.append('    { ' + ', '.join('{ %d, icona_%s_%d }' % (l, nome, l) for l in LATI) + ' },')
c.append('};')
for i, nome in enumerate(nomi_icone):
    c.append('#define ICONA_%s %d' % (nome.upper(), i))
c.append('')

c.append('''/* Un'immagine a colori: l x a pixel 0xAARRGGBB premoltiplicati. */
struct immagine {
    unsigned short l, a;
    const unsigned int *pixel;
};
''')
for nome, file in [('scelta_accesa', 'scelta-accesa.svg'), ('scelta_accesa_sopra', 'scelta-accesa-sopra.svg')]:
    img = Image.open(io.BytesIO(subprocess.check_output(
        ['rsvg-convert', os.path.join(FIGMA, file)]))).convert('RGBA')
    c.append('static const unsigned int %s_pixel[] = {\n%s\n};' % (nome, tabella_parole(premoltiplicato(img))))
    c.append('static const struct immagine %s = { %d, %d, %s_pixel };\n' % (nome, img.size[0], img.size[1], nome))

sfondo = svg_in_png(os.path.join(FIGMA, 'sfondo.svg'), 320, 200).convert('RGB')
c.append('/* Lo sfondo, rimpicciolito: 0xRRGGBB. Il compositore lo ingrandisce a 1280x800. */')
c.append('#define SFONDO_L 320\n#define SFONDO_A 200')
c.append('static const unsigned int sfondo_piccolo[] = {\n%s\n};' % tabella_parole(
    [r << 16 | g << 8 | b for r, g, b in sfondo.get_flattened_data()]))
c.append('\n#endif')

open(USCITA, 'w', encoding='utf-8', newline='\n').write('\n'.join(c) + '\n')
print('scritto', USCITA)
