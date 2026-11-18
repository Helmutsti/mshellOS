"""wayland_ritmo: i fotogrammi di un programma Wayland vanno a 60 al secondo, e solo quando servono.

Il compositore da' il "fatto" del frame callback (il "disegna il prossimo") con un orologio a 60
tic al secondo, acceso solo se qualcuno aspetta (compositore.c, il ciclo). Prima del tic il "fatto"
partiva a ogni giro del ciclo: un programma che anima girava a tutta velocita'.
Si accende il disco wayland_ritmo: il client di prova (utente/prove/wayland.c) si chiama
wayland_anima, quindi ridisegna un quadrato che si sposta ad ogni "fatto", e ogni secondo scrive
"wayland: fotogrammi: N (tempo T ms, compositore C tick)": i fotogrammi disegnati in tutto, il suo
orologio, e i tick di processore (utime + stime, da /proc/<pid>/stat) usati dal compositore.
  1. In tre secondi i fotogrammi al secondo stanno tra 50 e 65.
  2. Nello stesso tempo il compositore usa meno del 50% di un processore (si riporta il valore).
  3. Con la finestra ridotta (il pulsante riduci della cornice) i fotogrammi si fermano: in due
     secondi non arrivano piu' di 3 "fatto".
"""

import re
import time

import scrivania as s
from qemu import riquadro_colore, PUNTO_VUOTO

NOME = "wayland_ritmo"
ORDINE = 86
DESCRIZIONE = "un client che anima gira a 60 fotogrammi al secondo, con poco processore; ridotto si ferma"
DISCO = "disco_wayland_ritmo.img"
SCHERMO_ANIMATO = True   # il client anima dall'avvio: lo schermo non si ferma mai (aspetta_scrivania non lo aspetta)

LARGHEZZA, TESTATA, QUADRATO, MARGINE, ROSSO = 320, 40, 48, 16, 0xE61E1E
RIGA = re.compile(r"wayland: fotogrammi: (\d+) \(tempo (\d+) ms, compositore (\d+) tick\)")


def _righe(m):
    """I campioni scritti dal client: (fotogrammi, ms, tick), in ordine."""
    return [tuple(int(v) for v in r) for r in RIGA.findall(m.testo_seriale())]


def _aspetta_righe(m, quante, attesa):
    fine = time.time() + attesa
    while time.time() < fine:
        r = _righe(m)
        if len(r) >= quante:
            return r
        time.sleep(0.25)
    raise AssertionError(f"il client ha scritto {len(_righe(m))} righe di fotogrammi su {quante} attese")


def esegui(p):
    m = p.m
    m.aspetta_seriale("wayland: finestra mostrata", 30.0)
    m.muovi(*PUNTO_VUOTO)
    f = m.aspetta_che(lambda f: riquadro_colore(f, ROSSO) is not None, 10.0, cosa="la finestra Wayland sulla scrivania")
    r = riquadro_colore(f, ROSSO)
    x, y = r[0] - MARGINE, r[1] - MARGINE - TESTATA

    # 1 e 2. tre secondi di misura, dopo due di avvio
    base = len(_aspetta_righe(m, 3, 30.0))      # la terza riga: l'avvio e' finito
    r = _aspetta_righe(m, base + 3, 30.0)
    (n0, t0, c0), (n1, t1, c1) = r[base - 1], r[base + 2]
    fps = (n1 - n0) * 1000.0 / (t1 - t0)
    cpu = (c1 - c0) * 1000.0 / (t1 - t0)       # i tick sono centesimi di secondo
    print(f"    wayland_ritmo: {fps:.1f} fotogrammi/s, compositore {cpu:.1f}% di un processore", flush=True)
    m.foto("anima")
    p.controlla(50 <= fps <= 65, f"i fotogrammi sono {fps:.1f} al secondo, dovevano stare tra 50 e 65")
    p.controlla(cpu < 50, f"il compositore usa il {cpu:.1f}% di un processore, doveva restare sotto il 50%")
    p.controlla(c1 > 0, "il client non ha letto i tick del compositore (/proc)")

    # 3. ridotta: i fotogrammi si fermano
    riduci, _chiudi = s.comandi(x, y, LARGHEZZA)
    m.clic(*riduci)
    m.muovi(*PUNTO_VUOTO)
    m.aspetta_che(lambda f: riquadro_colore(f, ROSSO) is None, 5.0, cosa="la finestra ridursi")
    ora = len(_righe(m))
    r = _aspetta_righe(m, ora + 3, 15.0)         # una riga di assestamento, poi due secondi
    (n0, t0, _c0), (n1, t1, _c1) = r[ora], r[ora + 2]
    p.controlla(n1 - n0 <= 3, f"a finestra ridotta i fotogrammi sono {n1 - n0} in {(t1 - t0) / 1000:.1f} s, dovevano fermarsi")
    print(f"    wayland_ritmo: ridotta, {n1 - n0} fotogrammi in {(t1 - t0) / 1000:.1f} s", flush=True)
    m.foto("ridotta")
