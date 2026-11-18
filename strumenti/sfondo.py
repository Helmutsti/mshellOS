# sfondo.py: PORTA LA FOTO DELLO SFONDO ALLA MISURA DELLO SCHERMO, in pixel pronti per il
# compositore. Gira negli attrezzi (python3-pil), lo chiama costruisci.sh:
#   python3 strumenti/sfondo.py <foto> <larghezza> <altezza> <uscita>
# La foto si ingrandisce (o rimpicciolisce) quanto basta per COPRIRE tutto lo schermo, senza
# deformarla, e si taglia al centro quello che avanza.
#
# IL FILE CHE ESCE (formato di mshell, letto da mshell-compositore all'avvio):
#   - larghezza: numero intero senza segno a 32 bit, little-endian (4 byte)
#   - altezza:   numero intero senza segno a 32 bit, little-endian (4 byte)
#   - i pixel:   larghezza x altezza, riga per riga dall'alto, 4 byte ciascuno in formato
#                XRGB8888 (quello di DRM/wlroots): in memoria blu, verde, rosso, 255.
# Niente compressione: il compositore lo legge cosi' com'e', senza librerie per le immagini.
import struct
import sys

from PIL import Image, ImageOps

foto, larghezza, altezza, uscita = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
immagine = Image.open(foto).convert("RGB")
immagine = ImageOps.fit(immagine, (larghezza, altezza), Image.LANCZOS, centering=(0.5, 0.5))
pixel = immagine.convert("RGBA").tobytes("raw", "BGRA")   # la foto non ha trasparenza: A = 255
with open(uscita, "wb") as f:
    f.write(struct.pack("<II", larghezza, altezza))
    f.write(pixel)
