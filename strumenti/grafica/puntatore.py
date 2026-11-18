"""Converte le immagini del puntatore esportate da Figma in utente/programmi/puntatori.h.

    uv run --with pillow strumenti/grafica/puntatore.py

Le immagini sono in risorse/puntatore/: normale, collegamento, testo, divieto e i dodici
fotogrammi dell'attesa (attesa-00 ... attesa-11). Vengono dal disegno ("Componenti base",
Puntatore) e si usano COSI' COME SONO: nessun tracciato rifatto qui, nessuna ombra rifatta.
Sono PNG RGBA 32 x 32, pixel 1:1 (il componente e' gia' a 24 x 24, decisione della persona),
con il componente a x = +2, y = 0 su una tela trasparente piu' grande, perche' l'ombra esce
dai 24 pixel. L'alfa vero e' stato ricavato esportando su fondo nero e su fondo bianco.
IL PUNTATORE NON CAMBIA COL TEMA (deciso dalla persona): un'immagine sola per forma.

Nell'attesa gira solo l'arco: anello e ombra stanno fermi, e i 12 fotogrammi (un giro al
secondo, un fotogramma ogni 1/12 s) sono gia' nelle immagini.

Ogni forma e' ritagliata sul rettangolo che contiene tutti i suoi pixel non trasparenti
(per l'attesa, di tutti i fotogrammi, cosi' hanno la stessa misura): niente ombra tagliata.
Ogni pixel e' scritto 0xAARRGGBB PREMOLTIPLICATO (i colori gia' moltiplicati per l'alfa),
il formato che il compositore mescola con la scena (sopra_a).

PUNTI CALDI, nelle immagini a 32 (cioe' sul disegno: x del disegno + 2): freccia e divieto
(6, 3), la punta della freccia; mano (11, 4), la punta dell'indice; barra a I e attesa (14, 12),
il centro. Il compositore sottrae il margine del ritaglio.
"""
from pathlib import Path

from PIL import Image

RADICE = Path(__file__).resolve().parent.parent.parent
CARTELLA = RADICE / "risorse" / "puntatore"
FOTOGRAMMI = 12

# (nome nel .h, file, punto caldo nell'immagine a 32)
FORME = [("normale", ["normale"], (6, 3)),
         ("collegamento", ["collegamento"], (11, 4)),
         ("testo", ["testo"], (14, 12)),
         ("non_disponibile", ["divieto"], (6, 3)),
         ("attesa", [f"attesa-{k:02d}" for k in range(FOTOGRAMMI)], (14, 12))]


def premoltiplica(r, g, b, a):
    return (a << 24) | (((r * a + 127) // 255) << 16) | (((g * a + 127) // 255) << 8) | ((b * a + 127) // 255)


def scrivi_c():
    righe = ["/* Le forme del puntatore (Normale, Collegamento, Testo, Non disponibile, Attesa).",
             " * FILE GENERATO da strumenti/grafica/puntatore.py dalle immagini di risorse/puntatore/",
             " * (esportate da Figma, \"Componenti base\", Puntatore): non si modifica a mano.",
             " * Ogni pixel e' 0xAARRGGBB premoltiplicato, ombra compresa (vedi il compositore). */",
             "#ifndef PUNTATORI_H", "#define PUNTATORI_H", "",
             f"#define FOTOGRAMMI_ATTESA {FOTOGRAMMI}", ""]
    elenco = []
    for nome, file, caldo in FORME:
        imgs = [Image.open(CARTELLA / (f + ".png")).convert("RGBA") for f in file]
        x0 = y0 = 10 ** 9
        x1 = y1 = -1
        for im in imgs:
            b = im.getchannel("A").getbbox()
            x0, y0, x1, y1 = min(x0, b[0]), min(y0, b[1]), max(x1, b[2]), max(y1, b[3])
        l, a = x1 - x0, y1 - y0
        for k, im in enumerate(imgs):
            righe.append(f"static const unsigned int forma_{nome}_{k}[{a}][{l}] = {{")
            for y in range(y0, y1):
                righe.append("    {" + ",".join(f"0x{premoltiplica(*im.getpixel((x, y))):08X}" for x in range(x0, x1)) + "},")
            righe.append("};")
        elenco.append((nome, l, a, caldo[0] - x0, caldo[1] - y0))
        righe.append("")
    righe.append("struct forma_puntatore_immagine { int l, a, px, py; const unsigned int *pixel; };")
    righe.append("")
    righe.append("/* una per forma, nell'ordine di FORMA_... (gkit.h); i fotogrammi dell'attesa sono a parte */")
    righe.append("static const struct forma_puntatore_immagine forme_puntatore[] = {")
    for nome, l, a, px, py in elenco:
        righe.append(f"    {{ {l}, {a}, {px}, {py}, &forma_{nome}_0[0][0] }},   /* {nome} */")
    righe.append("};")
    righe.append("")
    righe.append("static const unsigned int *const fotogrammi_attesa[FOTOGRAMMI_ATTESA] = {")
    righe.append("    " + ", ".join(f"&forma_attesa_{k}[0][0]" for k in range(FOTOGRAMMI)))
    righe.append("};")
    righe.append("")
    righe.append("#endif")
    return "\n".join(righe) + "\n"


if __name__ == "__main__":
    dest = RADICE / "utente" / "programmi" / "puntatori.h"
    dest.write_text(scrivi_c())
    print("scritto", dest)
