"""immagini: alcune scene sempre uguali, confrontate pixel per pixel con le foto di
riferimento (test/riferimenti/<cpu>/<scena>.png).

Serve ad accorgersi di un cambiamento NON voluto nel disegno (il compositore, il toolkit,
i caratteri, il vetro). L'ora dell'isola cambia e si copre (x 695..845, y 25..50); l'ora
del PC virtuale e' comunque fissa (qemu.py: -rtc base=2026-01-01T10:00:00).
Le scene, una dopo l'altra nella stessa accensione:
  scrivania     appena accesa, niente aperto
  esempio       Esempio aperta, con due quadretti e un tasto premuto (sfondo azzurro)
  impostazioni  in piu' Impostazioni
Attivita non c'e': i suoi numeri cambiano da soli. Contatore non c'e' piu': non e' un'app del
sistema (va installata dal suo pacchetto). Per questo i riferimenti di "scrivania" (nell'isola
c'era Contatore), "esempio" e "impostazioni" (dietro c'era la finestra di Contatore) vanno
rifatti, dopo averli guardati.

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
