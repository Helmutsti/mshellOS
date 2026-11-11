"""mouse: il puntatore segue la tavoletta USB anche sotto sforzo, e non si blocca.

Si fa girare il puntatore in tondo per 8 secondi (120 posizioni al secondo) sulla
scrivania vuota, e poi per 4 secondi sopra una finestra (Esempio). Ogni secondo si
fotografa lo schermo e si cerca il puntatore: e' quello che cambia rispetto alla foto
senza puntatore (con il puntatore parcheggiato nell'angolo, che si copre).
Il puntatore deve:
  - esserci in ogni foto, vicino all'ultima posizione mandata (un po' di ritardo si
    accetta: il compositore ridisegna al suo ritmo);
  - essere in un posto DIVERSO da una foto all'altra (se e' fermo, si e' bloccato);
  - alla fine, mandato in un punto preciso, arrivarci esattamente.
"""

import math
import time

import scrivania as s
from qemu import OROLOGIO, riquadro_diverso

NOME = "mouse"
ORDINE = 30
DESCRIZIONE = "puntatore in tondo per 8 s: segue, non si blocca, arriva dove deve"

ANGOLO = (1230, 750, 1280, 800)     # dove e' parcheggiato il puntatore nella foto di base
RITARDO = 70                        # pixel di distanza accettati durante il giro


def trova_puntatore(base, foto):
    """L'angolo in alto a sinistra del puntatore (la punta della freccia), o None."""
    r = riquadro_diverso(base, foto, [OROLOGIO, ANGOLO], soglia=24)
    return (r[0], r[1]) if r else None


def giro(p, base, cx, cy, raggio, secondi, nome):
    m = p.m
    viste = []                      # (dove si vede il puntatore, dove l'abbiamo mandato)

    def guarda():
        viste.append((trova_puntatore(base, m.foto()), m.ultimo_puntatore))

    mandate = m.cerchio(cx, cy, raggio, secondi, ogni_tanto=(1.0, guarda))
    p.controlla(len(viste) >= secondi - 2, f"{nome}: troppe poche foto ({len(viste)})")
    for i, (visto, mandato) in enumerate(viste):
        if visto is None:
            m.foto(f"{nome}_sparito")
            p.controlla(False, f"{nome}: alla foto {i + 1} il puntatore non si vede")
        d = math.dist(visto, mandato)
        if d > RITARDO:
            m.foto(f"{nome}_lontano")
            p.controlla(False, f"{nome}: alla foto {i + 1} il puntatore e' in {visto}, "
                               f"mandato in {mandato} ({d:.0f} pixel di distanza)")
    for i in range(1, len(viste)):
        p.controlla(viste[i][0] != viste[i - 1][0],
                    f"{nome}: il puntatore e' fermo in {viste[i][0]} fra la foto {i} e la "
                    f"{i + 1}: si e' bloccato")

    # il punto finale: ci deve arrivare preciso
    arrivo = (cx + raggio // 2, cy - raggio // 3)
    m.muovi(*arrivo)
    m.aspetta_che(lambda f: (q := trova_puntatore(base, f)) is not None
                  and math.dist(q, arrivo) <= 2, 3.0,
                  cosa=f"{nome}: il puntatore arrivare in {arrivo}")
    p.nota(f"{nome}: {mandate} posizioni, {len(viste)} foto")


def esegui(p):
    m = p.m
    giro(p, p.scrivania, 640, 450, 200, 8, "scrivania")

    # sopra una finestra: Esempio (a colori pieni) nel primo posto
    m.muovi(1270, 790)
    base = s.apri(p.m, "Esempio")
    x, y = s.angolo(0)
    giro(p, base, x + 160, y + 120, 70, 4, "finestra")
    time.sleep(0.2)
