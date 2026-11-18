"""puntatore_manina: la manina su tutto quello che si clicca, la freccia sul resto, la barra a I dove si scrive.

Regola della persona (18/11/2026). Si porta il puntatore su vari punti e dalla foto si riconosce la
forma: si confronta l'immagine del puntatore (utente/programmi/puntatori.h, i pixel pieni) con quello
che c'e' nella foto, intorno alla posizione mandata; vince la forma che coincide di piu'.
  - userbar: un'icona (Files) e il pulsante App: la manina; lo spazio tra due icone: la freccia;
  - vassoio delle app: una voce: la manina; lo spazio vuoto del vassoio: la freccia;
  - Files: un pulsante della barra degli strumenti, una voce della barra laterale, un file nella
    griglia, il pulsante chiudi: la manina; lo spazio vuoto della barra laterale, quello della
    griglia, la testata (vuota): la freccia; il campo di ricerca: la barra a I.
"""

import re
import time
from pathlib import Path

import scrivania as s
from qemu import PUNTO_VUOTO

NOME = "puntatore_manina"
ORDINE = 66
DESCRIZIONE = "il puntatore: manina su quello che si clicca, freccia sul resto, barra a I sui campi"

NORMALE, MANINA, TESTO = "freccia", "manina", "barra a I"
INTESTO = Path(__file__).resolve().parents[2] / "utente" / "programmi" / "puntatori.h"

# dentro la finestra di Files (dall'angolo in alto a sinistra), come files.py
A_ELENCO = (308, 64)               # (indietro e avanti, sulla casa, sono disabilitati: freccia)
RICERCA = (700, 64)
HOME = (60, 96)
BARRA_LATERALE_VUOTA = (60, 420)
TESTATA_VUOTA = (300, 18)
GRIGLIA_VUOTO = (520, 420)
PRIMO_FILE = (256, 128)


def _forme():
    """Le immagini delle tre forme da puntatori.h: nome -> (l, a, px, py, [(x, y, rgb)] dei pixel pieni)."""
    testo = INTESTO.read_text(encoding="utf-8")
    misure = re.findall(r"\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*&forma_(\w+)_0", testo)
    risultato = {}
    for nome, forma in (("freccia", "normale"), ("manina", "collegamento"), ("barra a I", "testo")):
        l, a, px, py = next(tuple(int(v) for v in m[:4]) for m in misure if m[4] == forma)
        corpo = re.search(r"forma_%s_0\[\d+\]\[\d+\] = \{(.*?)\n\};" % forma, testo, re.S).group(1)
        valori = [int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]{8})", corpo)]
        pieni = []
        for i, v in enumerate(valori):
            if v >> 24 == 0xFF:
                pieni.append((i % l, i // l, ((v >> 16) & 255, (v >> 8) & 255, v & 255)))
        risultato[nome] = (l, a, px, py, pieni)
    return risultato


def forma_sotto(foto, x, y, forme, raggio=14):
    """Quale forma e' disegnata nella foto con la punta vicino a (x, y) (entro 'raggio' pixel: la
    sandbox mette il puntatore con qualche pixel di scarto): (nome, punteggio di ognuna)."""
    punteggi = {}
    for nome, (l, a, px, py, pieni) in forme.items():
        migliore = 0.0
        for cx in range(x - raggio, x + raggio + 1):
            for cy in range(y - raggio, y + raggio + 1):
                giusti = 0
                for dx, dy, c in pieni[::3]:                      # un pixel pieno su tre: basta
                    q = foto.getpixel((cx - px + dx, cy - py + dy))
                    giusti += max(abs(q[0] - c[0]), abs(q[1] - c[1]), abs(q[2] - c[2])) <= 20
                migliore = max(migliore, giusti / len(pieni[::3]))
        punteggi[nome] = migliore
    migliore = max(punteggi, key=punteggi.get)
    return (migliore if punteggi[migliore] > 0.9 else None), punteggi


def esegui(p):
    m = p.m
    forme = _forme()
    wx, wy = s.angolo(0)
    l, a = s.MISURE["Files"]

    def prova(cosa, punto, atteso):
        m.muovi(*punto)
        time.sleep(0.6)
        foto = m.foto("puntatore_" + re.sub(r"\W+", "_", cosa))
        visto, punteggi = forma_sotto(foto, *punto, forme)
        p.controlla(visto == atteso, f"{cosa}: la forma e' {visto}, doveva essere {atteso} "
                                      f"(punteggi: { {k: round(v, 2) for k, v in punteggi.items()} })")

    # --- la userbar e il vassoio (la finestra di Files non c'e' ancora) ---
    ax, ay = s.APP_PULSANTE
    prova("userbar: il pulsante App", (ax, ay), MANINA)
    prova("userbar: un'icona (Files)", s.APP["Files"], MANINA)
    prova("userbar: lo spazio tra due icone", (ax, s.APP["Files"][1] + 20), NORMALE)
    m.clic(ax, ay)                                          # apre il vassoio
    m.muovi(*PUNTO_VUOTO)
    m.aspetta_che(lambda f: s.zona_uguale(f, f, s.VASSOIO_RETTANGOLO) and True, 5.0, cosa="il vassoio")
    time.sleep(1.0)
    prova("vassoio: una voce (Files)", s.VASSOIO["Files"], MANINA)
    vx, vy = s.VASSOIO_ANGOLO
    prova("vassoio: lo spazio vuoto", (vx + 6, vy + 6), NORMALE)
    m.clic(ax, ay)                                          # lo richiude
    m.muovi(*PUNTO_VUOTO)
    time.sleep(1.0)

    # --- Files ---
    s.apri(m, "Files")
    m.aspetta_schermo_fermo()
    prova("Files: il pulsante 'elenco' della barra degli strumenti", (wx + A_ELENCO[0], wy + A_ELENCO[1]), MANINA)
    prova("Files: una voce della barra laterale", (wx + HOME[0], wy + HOME[1]), MANINA)
    prova("Files: lo spazio vuoto della barra laterale", (wx + BARRA_LATERALE_VUOTA[0], wy + BARRA_LATERALE_VUOTA[1]), NORMALE)
    prova("Files: un file nella griglia", (wx + PRIMO_FILE[0], wy + PRIMO_FILE[1]), MANINA)
    prova("Files: lo spazio vuoto della griglia", (wx + GRIGLIA_VUOTO[0], wy + GRIGLIA_VUOTO[1]), NORMALE)
    prova("Files: la testata (vuota)", (wx + TESTATA_VUOTA[0], wy + TESTATA_VUOTA[1]), NORMALE)
    riduci, chiudi = s.comandi(wx, wy, l)
    prova("Files: il pulsante chiudi", chiudi, MANINA)
    prova("Files: il pulsante riduci", riduci, MANINA)
    prova("Files: il campo di ricerca", (wx + RICERCA[0], wy + RICERCA[1]), TESTO)
