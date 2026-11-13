"""Prepara i materiali del sistema: font, icone, immagini e sfondo, come file su disco.

Gira sul computer che compila, dentro il container alpine (vedi genera.sh), e scrive:
  - risorse/ (nel repository): i file, che build.sh mette in /system/resources --
    fonts/<famiglia>-<peso>-<misura>.font, icons/lucide/<nome>.icon, icons/objects/,
    icons/files/ e icons/controls/ (.image), wallpapers/default.image. Com'e' fatto ogni
    tipo di file: formato.py;
  - utente/libreria/uikit/materiali_elenco.h: l'elenco per UIKit (nomi e file, niente dati).
Quali materiali ci sono e come si chiamano i file: materiali.py. I materiali sono di UIKit
(vedi LIBRERIE.md), che li carica la prima volta che servono; i tipi (struct font, struct
icona, struct immagine) e il codice che li legge e li disegna sono di GKit
(gkit/gkit_disegno.h, gkit/interno/materiali_file.c). I programmi di mshell non sanno leggere un
file TrueType o un SVG: qui li si trasforma una volta per tutte in pixel.

  - I FONT: Inter e IBM Plex Sans (licenza SIL OFL) in ogni misura e peso che usa
    l'interfaccia (Inter e' in via di sostituzione con Plex), e IBM Plex Mono (stessa
    licenza, a larghezza fissa) per il terminale. Ogni
    lettera diventa una piccola tabella di "alfa": quanto e' coperto ogni pixel (0-255).
    I bordi sono gia' lisci (antialiasing): li calcola FreeType, qui.
  - LE ICONE: Lucide (licenza ISC), dagli SVG del file Figma "mshell - interfaccia". Anche
    loro solo alfa: il colore lo sceglie chi disegna.
  - LE IMMAGINI A COLORI (la scelta singola accesa): pixel 0xAARRGGBB premoltiplicati.
  - LO SFONDO: e' una sfumatura molto morbida, quindi lo si tiene piccolo (320x200) e la
    scrivania lo ingrandisce all'avvio.

I caratteri sono quelli della code page 437 (come i tasti e la console): l'ASCII piu'
le lettere accentate italiane.

Licenze: font Inter, di Rasmus Andersson (https://rsms.me/inter), e IBM Plex Sans e IBM Plex Mono,
di IBM (https://github.com/IBM/plex), licenza SIL Open Font License 1.1. Icone Lucide
(https://lucide.dev), licenza ISC: Copyright (c) for portions of Lucide are held by Cole
Bemis 2013-2022 as part of Feather (MIT); all other copyright (c) for Lucide are held by
Lucide Contributors 2022. Icone, immagini e sfondo vengono dal file Figma "mshell -
interfaccia"."""
import io, os, subprocess, sys
from PIL import Image, ImageDraw, ImageFont

import formato
import materiali
from materiali import FONT_USATI, ICONE, LATI, SCELTE, TIPI_FILE, OGGETTI_GRANDI, OGGETTI_PICCOLI

QUI = os.path.dirname(os.path.abspath(__file__))
FIGMA = os.path.join(QUI, 'figma')
FONT = sys.argv[1]                  # la cartella con Inter-Regular.ttf, -Medium, -SemiBold
REPO = sys.argv[2]                  # il repository: si scrivono risorse/ e l'elenco di UIKit
RISORSE = os.path.join(REPO, 'risorse')
FONT_PLEX = os.path.join(QUI, 'font', 'IBMPlexSans')   # IBM Plex Sans: TTF e OFL.txt nel repo
FONT_PLEX_MONO = os.path.join(QUI, 'font', 'IBMPlexMono')   # IBM Plex Mono (il terminale): TTF e OFL.txt

# code page 437 -> carattere vero, per le lettere accentate che servono all'italiano
CP437 = {0x85: 'à', 0x8A: 'è', 0x82: 'é', 0x8D: 'ì', 0x95: 'ò', 0x97: 'ù', 0xA2: 'ó', 0xA0: 'á',
         0xFA: '·', 0x96: '–'}   # il punto a mezza altezza, per separare ("8 processi · 34,6 MB")
CARATTERI = [(c, chr(c)) for c in range(32, 127)] + sorted(CP437.items())
FILE_IMMAGINI = dict(materiali.immagini())


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


def premoltiplicato(img):
    out = []
    for r, g, b, a in img.get_flattened_data():
        out.append(a << 24 | (r * a // 255) << 16 | (g * a // 255) << 8 | (b * a // 255))
    return out


def scrivi_immagine(nome, img):
    formato.immagine(os.path.join(RISORSE, FILE_IMMAGINI[nome]), img.size[0], img.size[1],
                     premoltiplicato(img))


for nome, file, misura in FONT_USATI:
    cartella = FONT_PLEX_MONO if file.startswith('IBMPlexMono') else FONT_PLEX if file.startswith('IBMPlex') else FONT
    f = ImageFont.truetype(os.path.join(cartella, file), misura)
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
    formato.font(os.path.join(RISORSE, materiali.file_font(nome)), ascesa + discesa, ascesa,
                 indice, glifi, alfa)

for icona in ICONE:
    svg = os.path.join(FIGMA, 'icona-%s.svg' % icona)
    formato.icona(os.path.join(RISORSE, materiali.file_icona(icona)),
                  [(lato, list(icona_in_png(svg, lato).getchannel('A').get_flattened_data()))
                   for lato in LATI])

for nome, file in SCELTE:
    scrivi_immagine(nome, Image.open(io.BytesIO(subprocess.check_output(
        ['rsvg-convert', os.path.join(FIGMA, file)]))).convert('RGBA'))

# LE ICONE DEI FILE (il set "Icona file" del file Figma): non sono icone dell'interfaccia ma
# "contenuto", a colori, ognuna in due misure (grande 56, piccola 18), dagli SVG esportati
# dal file Figma. Le ombre e le luci interne sono filtri SVG: le calcola rsvg-convert, qui,
# una volta per tutte. (Oggi nessun programma le usa: Files e la scrivania usano gli oggetti,
# qui sotto.)
for tipo in TIPI_FILE:
    for dimensione in ('grande', 'piccola'):
        img = Image.open(io.BytesIO(subprocess.check_output(
            ['rsvg-convert', os.path.join(FIGMA, 'file-%s-%s.svg' % (tipo, dimensione))]))).convert('RGBA')
        lato = 56 if dimensione == 'grande' else 18
        img = img.crop((0, 0, lato, lato))   # alcune sono alte mezzo pixel in piu' (l'ombra)
        scrivi_immagine('file_%s_%s' % (tipo, dimensione), img)

# GLI OGGETTI (il componente "Oggetto" del file Figma): le icone di file e cartelle del disegno
# nuovo, in due misure ottiche (grande 48, piccola 24). Sono a tre colori del tema
# (oggetto/riempimento-forte, oggetto/riempimento, oggetto/tratto), quindi cambiano tra Chiaro
# e Scuro: non si salvano i colori ma QUANTO di ognuno c'e' in ogni pixel. Si colora l'SVG con
# un colore puro per ruolo (rosso = riempimento forte, verde = riempimento, blu = tratto):
# rsvg-convert mescola i colori in modo lineare, quindi nel pixel (premoltiplicato) r, g e b
# sono proprio i pesi dei tre colori, e a la copertura. UIKit (uikit/oggetti.h) li ricolora
# con il tema in uso. I file con l'etichetta (TXT, PNG...) nella misura grande sono il foglio
# "etichetta" e la scritta, che la disegna UIKit.
RUOLI = {'#E3E3E5': '#FF0000', '#F2F2F3': '#00FF00', '#77777B': '#0000FF'}
for dimensione, elenco, lato in (('grande', OGGETTI_GRANDI, 48), ('piccola', OGGETTI_PICCOLI, 24)):
    for tipo in elenco:
        svg = open(os.path.join(FIGMA, 'oggetto-%s-%s.svg' % (tipo, dimensione)), encoding='utf-8').read()
        for colore, puro in RUOLI.items():
            svg = svg.replace(colore, puro)
        img = Image.open(io.BytesIO(subprocess.check_output(
            ['rsvg-convert', '-w', str(lato), '-h', str(lato)], input=svg.encode()))).convert('RGBA')
        scrivi_immagine('oggetto_%s_%s' % (tipo, dimensione), img)

# L'ICONA DEL TERMINALE (pagina "App - Terminale" del file Figma, sezione "Icona dell'app"): una
# piastrella grigio scuro (Chiaro #1c1c1e, Scuro #37373b) con un riflesso bianco in alto (24% ->
# 0%) e il simbolo Lucide "terminal" bianco. Il disegno e' a 96 (angoli 22, simbolo 52 a 22,22):
# qui lo si riduce a 48, la misura degli oggetti sulla scrivania.
for tema, grigio in (('chiaro', '#1c1c1e'), ('scuro', '#37373b')):
    svg = ('<svg xmlns="http://www.w3.org/2000/svg" width="96" height="96" viewBox="0 0 96 96">'
           '<defs><linearGradient id="l" x1="0" y1="0" x2="0" y2="1">'
           '<stop offset="0" stop-color="#fff" stop-opacity="0.24"/>'
           '<stop offset="1" stop-color="#fff" stop-opacity="0"/></linearGradient></defs>'
           '<rect width="96" height="96" rx="22" fill="%s"/>'
           '<rect width="96" height="96" rx="22" fill="url(#l)"/>'
           '<g transform="translate(22 22)" fill="none" stroke="#fff" stroke-width="4.5" '
           'stroke-linecap="round" stroke-linejoin="round">'
           '<path d="M8.66667 36.8333L21.6667 23.8333L8.66667 10.8333"/>'
           '<path d="M26 41.1667H43.3333"/></g></svg>') % grigio
    img = Image.open(io.BytesIO(subprocess.check_output(
        ['rsvg-convert', '-w', '48', '-h', '48'], input=svg.encode()))).convert('RGBA')
    scrivi_immagine('icona_terminale_' + tema, img)

# LO SFONDO, rimpicciolito e opaco: la scrivania lo ingrandisce a tutto lo schermo.
sfondo = svg_in_png(os.path.join(FIGMA, 'sfondo.svg'), 320, 200).convert('RGB')
formato.immagine(os.path.join(RISORSE, FILE_IMMAGINI['sfondo_piccolo']), 320, 200,
                 [0xFF000000 | r << 16 | g << 8 | b for r, g, b in sfondo.get_flattened_data()])

materiali.scrivi_elenco(os.path.join(REPO, 'utente', 'libreria', 'uikit', 'materiali_elenco.h'))
print('scritti', RISORSE, 'e materiali_elenco.h')
