"""tema: Impostazioni cambia il tema di tutto il sistema, dal vivo.

Si aprono Files (primo posto, angolo in 140,130, 800 x 480) e Impostazioni (secondo posto,
180,170, 800 x 520). Di Files resta scoperta la striscia a sinistra (x 140..180), che e' il
vetro del suo corpo: chiaro nel tema chiaro, scuro nello scuro. Di Impostazioni si guarda il
pannello del contenuto (FAFAFA nel chiaro, scuro nello scuro), della userbar il vetro tra il
pulsante App e la prima app.
  1. Un clic su "Scuro" nella scelta del tema di Impostazioni: diventano scuri il pannello di
     Impostazioni, il vetro di Files e quello della userbar (il compositore lo dice a tutti).
  2. Un clic su "Chiaro": tutto torna com'era, pixel per pixel (icone delle app comprese).
"""

import scrivania as s
from qemu import PUNTO_VUOTO

NOME = "tema"
ORDINE = 55
DESCRIZIONE = "Impostazioni cambia il tema dal vivo: Impostazioni, Files e userbar"


def _luce(foto, rettangolo):
    zona = foto.crop(rettangolo).convert("L")
    valori = list(zona.getdata())
    return sum(valori) / len(valori)


def esegui(p):
    m = p.m
    s.apri(m, "Files")
    s.apri(m, "Impostazioni")
    ix, iy = s.angolo(1)
    scuro = (ix + 738, iy + 84)              # "Scuro" nel gruppo della riga Tema
    chiaro = (ix + 666, iy + 84)
    pannello = (ix + 300, iy + 300, ix + 500, iy + 450)     # il pannello di Impostazioni, vuoto
    files = (142, 250, 176, 500)                             # la striscia scoperta di Files
    ax, ay = s.APP_PULSANTE
    userbar = (ax - 4, ay + 18, ax + 4, ay + 22)              # il vetro tra App e la prima app

    m.muovi(*PUNTO_VUOTO)
    prima = m.aspetta_schermo_fermo()
    m.foto("chiaro")
    p.controlla(_luce(prima, pannello) > 240, "il pannello di Impostazioni non e' chiaro all'inizio")
    luce_files, luce_barra = _luce(prima, files), _luce(prima, userbar)

    # 1. scuro
    m.clic(*scuro)
    m.muovi(*PUNTO_VUOTO)
    f = m.aspetta_che(lambda f: _luce(f, pannello) < 70, 5.0, cosa="il pannello di Impostazioni scuro")
    f = m.aspetta_schermo_fermo()
    m.foto("scuro")
    p.controlla(_luce(f, files) < luce_files - 60, "Files non e' diventata scura")
    p.controlla(_luce(f, userbar) < luce_barra - 60, "la userbar non e' diventata scura")

    # 2. di nuovo chiaro: com'era all'inizio
    m.clic(*chiaro)
    m.muovi(*PUNTO_VUOTO)
    m.aspetta_che(lambda f: _luce(f, pannello) > 240, 5.0, cosa="il pannello di Impostazioni di nuovo chiaro")
    f = m.aspetta_schermo_fermo()
    m.foto("di_nuovo_chiaro")
    p.controlla(s.zona_uguale(f, prima, (0, 0, 1280, 800)), "tornati al chiaro, lo schermo non e' come prima")
