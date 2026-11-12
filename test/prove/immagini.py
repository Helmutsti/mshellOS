"""immagini: alcune scene sempre uguali, confrontate pixel per pixel con le foto di
riferimento (test/riferimenti/<cpu>/<scena>.png).

Serve ad accorgersi di un cambiamento NON voluto nel disegno (il compositore, UIKit,
i caratteri, il vetro). La scrivania non mostra l'ora (la vecchia isola si'): niente da
coprire; l'ora del PC virtuale e' comunque fissa (qemu.py: -rtc base=2026-01-01T10:00:00).
Con la userbar al posto dell'isola (e senza la cartella Home) i riferimenti vanno rifatti,
dopo averli guardati.
Le scene, una dopo l'altra nella stessa accensione:
  scrivania     appena accesa, niente aperto
  esempio       Esempio aperta, con due quadretti e un tasto premuto (sfondo azzurro)
  impostazioni  in piu' Impostazioni
Attivita non c'e': i suoi numeri cambiano da soli. Contatore non c'e' piu': non e' un'app del
sistema (va installata dal suo pacchetto).

Quando un cambiamento del disegno E' voluto, si rifanno i riferimenti:
    uv run test/collaudo.py --riferimenti
e si guardano le foto nuove prima di salvarle nel repo.
"""

import scrivania as s
from qemu import PUNTO_VUOTO

NOME = "immagini"
ORDINE = 70
DESCRIZIONE = "scene fisse confrontate pixel per pixel con i riferimenti"
RIFERIMENTI = True


def esegui(p):
    m = p.m
    p.riferimento("scrivania", p.scrivania)

    s.apri(m, "Esempio")
    x, y = s.angolo(0)
    m.clic(x + 60, y + 80)
    m.clic(x + 200, y + 150)
    m.premi("a")
    m.muovi(*PUNTO_VUOTO)
    p.riferimento("esempio", m.aspetta_schermo_fermo())

    s.apri(m, "Impostazioni")
    p.riferimento("impostazioni", m.aspetta_schermo_fermo())
