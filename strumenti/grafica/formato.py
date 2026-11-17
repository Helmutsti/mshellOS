"""I formati su disco dei materiali gia' calcolati: font, icone, immagini (versione 1).

Li scrive genera.py; li legge GKit (gkit/interno/materiali_file.c: font_da_file, icona_da_file,
immagine_da_file), che li mette in memoria con mmap, in sola lettura: i programmi che usano
lo stesso file ne condividono le pagine. Tutti i numeri sono little-endian (come x86_64 e
arm64), e ogni parte comincia a un multiplo di 4 byte, cosi' GKit usa i dati dove stanno,
senza copiarli. Un file comincia sempre con 4 lettere (cosa e') e la versione (2 byte).

FONT (.font): le lettere gia' disegnate di un font a una misura e un peso. I caratteri sono
numeri Unicode (il testo e' UTF-8); i glifi stanno in ordine di numero, e una tabella di
INTERVALLI dice dove sta ogni gruppo di numeri consecutivi (ricerca binaria: gkit/interno/testo.c).
    0    "MSFN"
    4    u16  versione (2)
    6    u8   altezza della riga          (struct font, gkit_disegno.h)
    7    u8   ascesa (dalla cima della riga alla linea di base)
    8    u32  quanti glifi (G)
    12   u32  quanti byte di alfa (N)
    16   u32  quanti intervalli (R)
    20   u32  il glifo SOSTITUTIVO (il segno per un carattere che il font non ha)
    24   R intervalli da 8 byte, in ordine di numero: u32 primo carattere, u16 quanti
         (consecutivi), u16 primo glifo (i glifi dell'intervallo si seguono)
    ...  G glifi da 12 byte, come struct glifo: u32 inizio (nell'alfa), i8 x, i8 y, u8 l,
         u8 a, u8 avanzo, 3 byte a zero
    ...  N byte di alfa (0-255): le lettere una dopo l'altra, riga per riga

ICONA (.icon): un'icona dell'interfaccia in tutte le sue misure, solo alfa (il colore lo
sceglie chi disegna).
    0    "MSIC"
    4    u16  versione (1)
    6    u16  quante misure (M), dalla piu' piccola alla piu' grande
    8    M volte: u32 lato, u32 dove comincia la sua alfa (dall'inizio del file)
    ...  le alfa: lato x lato byte ciascuna

IMMAGINE (.image): un'immagine a colori.
    0    "MSIM"
    4    u16  versione (1)
    6    u16  0
    8    u32  larghezza
    12   u32  altezza
    16   larghezza x altezza pixel u32: 0xAARRGGBB premoltiplicati, riga dopo riga
"""
import os
import struct

VERSIONE = 1
VERSIONE_FONT = 2


def _scrivi(percorso, byte):
    os.makedirs(os.path.dirname(percorso), exist_ok=True)
    with open(percorso, 'wb') as f:
        f.write(byte)


def font(percorso, altezza, ascesa, intervalli, sostitutivo, glifi, alfa):
    """intervalli: [(primo carattere, quanti, primo glifo)] in ordine di carattere; glifi:
    [(inizio, x, y, l, a, avanzo)]; alfa: byte (0-255); sostitutivo: l'indice di un glifo."""
    assert 0 <= sostitutivo < len(glifi) and len(glifi) < 65536
    b = bytearray(b'MSFN' + struct.pack('<HBBIIII', VERSIONE_FONT, altezza, ascesa, len(glifi),
                                         len(alfa), len(intervalli), sostitutivo))
    prec = -1
    for primo, quanti, g in intervalli:
        assert primo > prec and quanti < 65536 and g + quanti <= len(glifi)
        prec = primo + quanti - 1
        b += struct.pack('<IHH', primo, quanti, g)
    for g in glifi:
        b += struct.pack('<IbbBBB3x', *g)
    b += bytes(alfa)
    _scrivi(percorso, b)


def icona(percorso, misure):
    """misure: [(lato, alfa)], dalla piu' piccola; alfa: lato*lato byte."""
    b = bytearray(b'MSIC' + struct.pack('<HH', VERSIONE, len(misure)))
    dove = 8 + 8 * len(misure)
    for lato, alfa in misure:
        assert len(alfa) == lato * lato
        b += struct.pack('<II', lato, dove)
        dove += len(alfa)
    for _, alfa in misure:
        b += bytes(alfa)
    _scrivi(percorso, b)


def immagine(percorso, l, a, pixel):
    """pixel: l*a numeri 0xAARRGGBB premoltiplicati."""
    assert len(pixel) == l * a
    _scrivi(percorso, b'MSIM' + struct.pack('<HHII', VERSIONE, 0, l, a) +
            struct.pack('<%dI' % len(pixel), *pixel))
