"""L'elenco dei materiali del sistema: quali font, icone e immagini ci sono, come si chiamano
in C e in quale file stanno, dentro la cartella delle risorse (/system/resources, nel
repository risorse/). Lo usa genera.py, che li disegna e scrive i file (formato.py) e
l'elenco per UIKit (utente/libreria/uikit/materiali_elenco.h)."""

# I font dell'interfaccia (dal file Figma): nome C, file TrueType, misura in pixel
FONT_USATI = [
    ('inter_13',            'Inter-Regular.ttf',  13),
    ('inter_14',            'Inter-Regular.ttf',  14),
    ('inter_medio_13',      'Inter-Medium.ttf',   13),
    ('inter_medio_14',      'Inter-Medium.ttf',   14),
    ('inter_medio_16',      'Inter-Medium.ttf',   16),
    ('inter_semigrassetto_13', 'Inter-SemiBold.ttf', 13),
    ('inter_semigrassetto_15', 'Inter-SemiBold.ttf', 15),
    # per Files: i nomi sotto le icone (medio 12), le colonne e le scorciatoie (12), i
    # titoli delle sezioni e delle colonne (semigrassetto 12), lo stato della cartella
    # (semigrassetto 14)
    ('inter_12',            'Inter-Regular.ttf',  12),
    ('inter_medio_12',      'Inter-Medium.ttf',   12),
    ('inter_semigrassetto_12', 'Inter-SemiBold.ttf', 12),
    ('inter_semigrassetto_14', 'Inter-SemiBold.ttf', 14),
]

# IBM Plex Sans (stesse regole di nome: plex_<misura>, plex_medio_<misura>, plex_semigrassetto_<misura>)
for _misura in (12, 13, 14, 15, 16):
    FONT_USATI.append(('plex_%d' % _misura, 'IBMPlexSans-Regular.ttf', _misura))
for _misura in (12, 13, 14, 15, 16, 24):
    FONT_USATI.append(('plex_medio_%d' % _misura, 'IBMPlexSans-Medium.ttf', _misura))
for _misura in (12, 13, 14, 15, 24):
    FONT_USATI.append(('plex_semigrassetto_%d' % _misura, 'IBMPlexSans-SemiBold.ttf', _misura))

ICONE = ['save', 'check', 'minus', 'plus', 'x', 'user', 'triangle-alert', 'layout-grid', 'settings', 'square', 'activity',
         'arrow-left', 'arrow-right', 'list', 'search', 'house', 'chevron-right',
         # Files: la barra laterale, i menu, gli stati della cartella
         'folder', 'folder-open', 'folder-plus', 'pencil', 'copy', 'info', 'trash-2',
         'arrow-up-down', 'image', 'file', 'terminal', 'lock',
         # la userbar (le app fissate) e il riavvio di Attivita
         'globe', 'mail', 'rotate-ccw',
         # lo Store: i permessi, le deviazioni, i ruoli, i messaggi, i pulsanti (pagina "App - Store")
         'package', 'bell', 'download', 'external-link', 'eye-off', 'layers', 'power', 'scan',
         'badge-check', 'circle-x', 'circle-check', 'chevron-left', 'volume-2',
         # Files nel disegno nuovo: indietro, le frecce dell'ordinamento, i simboli della
         # barra laterale (Scrivania, Testi, Programmi)
         'chevron-up', 'chevron-down', 'monitor', 'file-text', 'app-window']
LATI = [12, 14, 16, 18, 20, 24, 32, 48]   # in ordine: disegna_icona() prende la piu' grande che ci sta (48: lo stato vuoto)

# Le immagini a colori della scelta singola accesa: nome C, SVG di figma/
SCELTE = [('scelta_accesa', 'scelta-accesa.svg'), ('scelta_accesa_sopra', 'scelta-accesa-sopra.svg')]
# Le icone dei file (il set "Icona file"), in due misure (grande 56, piccola 18)
TIPI_FILE = ['cartella', 'home', 'file', 'immagine', 'testo', 'programma']
# Gli oggetti (il componente "Oggetto"), in due misure (grande 48, piccola 24)
OGGETTI_GRANDI = ['file', 'cartella', 'etichetta', 'home', 'scrivania', 'documenti', 'immagini',
                  'download', 'applicazioni']
OGGETTI_PICCOLI = ['file', 'cartella', 'txt', 'pdf', 'png', 'zip', 'bin', 'home', 'scrivania',
                   'documenti', 'immagini', 'download', 'applicazioni']

# --- i nomi dei file (in inglese, come le cartelle del sistema) ---

_FAMIGLIE = {'inter': 'inter', 'plex': 'ibm-plex-sans'}
_PESI = {'': 'regular', 'medio': 'medium', 'semigrassetto': 'semibold'}
_TIPI = {'cartella': 'folder', 'home': 'home', 'file': 'file', 'immagine': 'image', 'testo': 'text',
         'programma': 'program', 'etichetta': 'label', 'scrivania': 'desktop',
         'documenti': 'documents', 'immagini': 'pictures', 'download': 'downloads',
         'applicazioni': 'applications', 'txt': 'txt', 'pdf': 'pdf', 'png': 'png', 'zip': 'zip',
         'bin': 'bin'}
_MISURE = {'grande': 'large', 'piccola': 'small'}


def file_font(nome):
    """'plex_medio_14' -> 'fonts/ibm-plex-sans-medium-14.font'"""
    pezzi = nome.split('_')
    peso = pezzi[1] if len(pezzi) == 3 else ''
    return 'fonts/%s-%s-%s.font' % (_FAMIGLIE[pezzi[0]], _PESI[peso], pezzi[-1])


def file_icona(icona):
    return 'icons/lucide/%s.icon' % icona


def immagini():
    """Tutte le immagini: [(nome C, file)]. Lo sfondo per ultimo."""
    elenco = [('scelta_accesa', 'icons/controls/choice-on.image'),
              ('scelta_accesa_sopra', 'icons/controls/choice-on-hover.image')]
    for tipo in TIPI_FILE:
        for misura in ('grande', 'piccola'):
            elenco.append(('file_%s_%s' % (tipo, misura),
                           'icons/files/%s-%s.image' % (_TIPI[tipo], _MISURE[misura])))
    for misura, tipi in (('grande', OGGETTI_GRANDI), ('piccola', OGGETTI_PICCOLI)):
        for tipo in tipi:
            elenco.append(('oggetto_%s_%s' % (tipo, misura),
                           'icons/objects/%s-%s.image' % (_TIPI[tipo], _MISURE[misura])))
    elenco.append(('sfondo_piccolo', 'wallpapers/default.image'))
    return elenco


def scrivi_elenco(uscita):
    """utente/libreria/uikit/materiali_elenco.h: solo i nomi, niente dati."""
    c = ['''/* GENERATO da strumenti/grafica/genera.py (l'elenco e' in strumenti/grafica/materiali.py):
 * non modificare a mano.
 *
 * L'elenco dei materiali del sistema, di UIKit: per ognuno il nome in C e il file, dentro la
 * cartella delle risorse (/system/resources). I dati non sono qui ma nei file: li carica
 * uikit/materiali.c la prima volta che servono (vedi uikit/materiali.h). */
#ifndef MATERIALI_ELENCO_H
#define MATERIALI_ELENCO_H
''']
    c.append('/* I font: X(nome, file). */')
    c.append('#define MATERIALI_FONT(X) \\')
    c += ['    X(%s, "%s") \\' % (nome, file_font(nome)) for nome, _, _ in FONT_USATI]
    c.append('')
    c.append('/* Le icone Lucide, ognuna in tutte le sue misure: X(nome, file). Per numero: ICONA_... */')
    c.append('#define MATERIALI_ICONE(X) \\')
    c += ['    X(%s, "%s") \\' % (i.replace('-', '_'), file_icona(i)) for i in ICONE]
    c.append('')
    c.append('#define ICONA_MISURE %d   /* quante misure al massimo: %s */' % (
        len(LATI), ', '.join(str(l) for l in LATI)))
    for i, icona in enumerate(ICONE):
        c.append('#define ICONA_%s %d' % (icona.replace('-', '_').upper(), i))
    c.append('')
    c.append('/* Le immagini a colori: X(nome, file). */')
    c.append('#define MATERIALI_IMMAGINI(X) \\')
    c += ['    X(%s, "%s") \\' % (nome, file) for nome, file in immagini()]
    c.append('')
    c.append('/* Ogni materiale si chiama con il suo nome, come una variabile (&plex_14, '
             'oggetto_file_grande.l):\n * dietro c\'e\' la funzione che lo carica la prima volta. */')
    c += ['#define %s (*font_del_sistema(MATERIALE_%s))' % (n, n) for n, _, _ in FONT_USATI]
    c += ['#define %s (*immagine_del_sistema(MATERIALE_%s))' % (n, n) for n, _ in immagini()]
    c.append('\n#endif')
    with open(uscita, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(c) + '\n')
