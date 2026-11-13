"""I formati su disco dei materiali gia' calcolati: font, icone, immagini (versione 1).

Li scrive genera.py; li legge GKit (gkit/interno/materiali_file.c: font_da_file, icona_da_file,
immagine_da_file), che li mette in memoria con mmap, in sola lettura: i programmi che usano
lo stesso file ne condividono le pagine. Tutti i numeri sono little-endian (come x86_64 e
arm64), e ogni parte comincia a un multiplo di 4 byte, cosi' GKit usa i dati dove stanno,
senza copiarli. Un file comincia sempre con 4 lettere (cosa e') e la versione (2 byte).

FONT (.font): le lettere gia' disegnate di un font a una misura e un peso.
    0    "MSFN"
    4    u16  versione (1)
    6    u8   altezza della riga          (struct font, gkit_disegno.h)
    7    u8   ascesa (dalla cima della riga alla linea di base)
    8    u32  quanti glifi (G)
    12   u32  quanti byte di alfa (N)
    16   u8   indice[256]: carattere (code page 437) -> glifo; 0 = non c'e'
    272  G glifi da 12 byte, come struct glifo: u32 inizio (nell'alfa), i8 x, i8 y, u8 l,
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


def _scrivi(percorso, byte):
    os.makedirs(os.path.dirname(percorso), exist_ok=True)
    with open(percorso, 'wb') as f:
        f.write(byte)


def font(percorso, altezza, ascesa, indice, glifi, alfa):
    """glifi: [(inizio, x, y, l, a, avanzo)]; alfa: byte (0-255); indice: 256 numeri."""
    assert len(indice) == 256
    b = bytearray(b'MSFN' + struct.pack('<HBBII', VERSIONE, altezza, ascesa, len(glifi), len(alfa)))
    b += bytes(indice)
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
