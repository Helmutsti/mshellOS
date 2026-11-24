# puntatore.py: PREPARA LE IMMAGINI DEL PUNTATORE per xcursorgen (lo usa costruisci.sh, negli
# attrezzi: Python con Pillow).
#   python3 strumenti/puntatore.py <cartella>
# Le immagini disegnate in Figma (risorse/puntatore/, 32x32 con l'ombra) si rimpiccioliscono
# perche' il disegno (la parte piena, senza l'ombra) sia alto ALTEZZA pixel, uguale per tutti i
# puntatori (la persona, 24/11/2026: "il cursore normale ridotto ancora un paio di px e tutti gli
# altri allineati"; il normale era alto 19). Il punto caldo si rimpicciolisce con l'immagine.
# Scrive in <cartella> le immagini e, per ogni puntatore, la descrizione per xcursorgen (misura
# nominale, punto caldo, immagine, e per l'attesa i millesimi di ogni fotogramma).
import sys
from pathlib import Path
from PIL import Image

ALTEZZA = 17
NOMINALE = 24  # la misura che chiedono il compositore e le app
ORIGINE = Path("risorse/puntatore")

# nome standard Xcursor: immagini, punto caldo x e y nel disegno a 32 (dal vecchio
# disegni/Puntatore/descrizione.md), millesimi per fotogramma (0: fermo)
PUNTATORI = {
    "default": (["normale"], 6, 3, 0),
    "text": (["testo"], 14, 12, 0),
    "pointer": (["collegamento"], 11, 4, 0),
    "not-allowed": (["divieto"], 6, 3, 0),
    "wait": ([f"attesa-{i:02d}" for i in range(12)], 14, 12, 83),
}


def alto(im):
    """L'altezza della parte piena (senza l'ombra, che e' semitrasparente)."""
    scatola = im.getchannel("A").point(lambda v: 255 if v > 200 else 0).getbbox()
    return scatola[3] - scatola[1]


uscita = Path(sys.argv[1])
uscita.mkdir(parents=True, exist_ok=True)
for nome, (immagini, x, y, ms) in PUNTATORI.items():
    prima = Image.open(ORIGINE / f"{immagini[0]}.png").convert("RGBA")
    scala = ALTEZZA / alto(prima)  # dalla prima immagine: i fotogrammi restano uguali fra loro
    righe = []
    for n in immagini:
        im = Image.open(ORIGINE / f"{n}.png").convert("RGBA")
        lato = round(im.width * scala)
        im.resize((lato, lato), Image.LANCZOS).save(uscita / f"{n}.png")
        riga = f"{NOMINALE} {round(x * scala)} {round(y * scala)} {uscita / n}.png"
        righe.append(riga + (f" {ms}" if ms else ""))
    (uscita / nome).write_text("\n".join(righe) + "\n")
