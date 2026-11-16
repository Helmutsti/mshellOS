"""scelta: la finestra di scelta del sistema (RUOLO_SCELTA) sopra una finestra dell'app.

Il picker provvisorio (utente/programmi/picker.c), acceso dalla cartella /data/tests/scelta di
disco_scelta.img, apre ogni tanto (15 secondi aperta, 10 chiusa) una finestra di scelta
480 x 421 vuota e chiara sopra Files (/system/apps/files/code) e scrive nel registro quando
l'apre e quando la chiude. (Il disegno dice Esempio, ma Esempio, 320 x 220, sta tutta sotto
una finestra di scelta di 480 x 421: non si vedrebbero ne' il velo ne' il clic. Files,
800 x 480 nel primo posto, in (140, 130), esce da tutti i lati.)

(Le attese sono lunghe: in QEMU l'orologio del guest e' piu' lento di quello del PC, e il picker
conta i secondi del guest.)

La prova apre Files, aspetta la scelta e controlla:
  (a) la scelta compare centrata sopra Files: il centro di Files e' in (540, 370), quindi la
      scelta sta in x 300..780, y 160..581;
  (b) Files e' velata: la parte che resta fuori dalla scelta (la barra laterale, a sinistra,
      x 150..296) e' piu' scura di prima;
  (c) un clic su Files (sulla voce "Scrivania" della barra laterale, +60 +132) non arriva:
      la barra laterale non cambia;
  (d) quando la scelta si chiude, Files torna come prima (pixel per pixel) e ha il fuoco.
"""

import scrivania as s
from qemu import colore_in, PUNTO_VUOTO

NOME = "scelta"
ORDINE = 52
DESCRIZIONE = "la finestra di scelta sopra Files: posto, velo, clic che non arrivano, fine"

FILES = s.angolo(0)                       # (140, 130)
L, A = s.MISURE["Files"]
SCELTA = (FILES[0] + L // 2 - 240, FILES[1] + A // 2 - 210, 480, 421)    # x, y, l, a
BIANCO_SCELTA = 0xF4F4F6                  # il colore della finestra di picker.c
LATERALE = (FILES[0] + 10, FILES[1] + 100, FILES[0] + 156, FILES[1] + 420)   # fuori dalla scelta
VOCE = (FILES[0] + 60, FILES[1] + 132)    # "Scrivania" nella barra laterale di Files


def _luce(foto):
    return s._luce(foto, LATERALE)


def _aperta(foto):
    """La scelta si vede: dentro il suo bordo sinistro c'e' il suo colore."""
    return colore_in(foto, SCELTA[0] + 3, SCELTA[1] + SCELTA[3] // 2) == BIANCO_SCELTA


def esegui(p):
    m = p.m
    s.apri(m, "Files")
    # la scelta puo' essere aperta adesso (sul centro dello schermo, se Files non c'era): si
    # aspetta che si chiuda, e a quel punto Files e' li' da sola. (Si guarda lo schermo, non il
    # registro: il registro arriva un po' dopo.)
    for _ in range(6):    # chiusa e con lo schermo fermo: se nel frattempo si riapre, da capo
        m.aspetta_che(lambda f: not _aperta(f), 120.0, cosa="la scelta chiusa")
        prima = m.aspetta_schermo_fermo()
        if not _aperta(prima):
            break
    p.controlla(not _aperta(prima), "la scelta e' ancora aperta")
    m.foto("files_prima")
    m.aspetta_che(_aperta, 120.0, cosa="la scelta aprirsi sopra Files")
    dopo = m.aspetta_schermo_fermo()
    m.foto("scelta_sopra")

    # (a) centrata sopra Files: dentro il suo bordo c'e' il suo colore, appena fuori no
    x, y, l, a = SCELTA
    cx, cy = x + l // 2, y + a // 2
    for nome, (px, py), dentro in [("sinistra", (x + 3, cy), True), ("a sinistra", (x - 6, cy), False),
                                   ("destra", (x + l - 4, cy), True), ("a destra", (x + l + 6, cy), False),
                                   ("sopra", (cx, y + 3), True), ("sotto", (cx, y + a - 4), True)]:
        p.controlla((colore_in(dopo, px, py) == BIANCO_SCELTA) == dentro,
                    f"la scelta non e' centrata sopra Files: bordo {nome} in ({px}, {py})")
    p.controlla(colore_in(dopo, cx, y - 6) != BIANCO_SCELTA, "sopra la scelta c'e' ancora il suo colore")

    # (b) Files velata: la barra laterale e' piu' scura (il velo e' nero al 20%)
    chiara, scura = _luce(prima), _luce(dopo)
    p.controlla(scura < chiara * 0.9, f"Files non e' velata (luce {chiara:.0f} -> {scura:.0f})")

    # (c) un clic su Files non arriva: la barra laterale resta com'e'
    m.clic(*VOCE)
    m.muovi(*PUNTO_VUOTO)
    m.aspetta_schermo_fermo()
    clic = m.foto("scelta_dopo_clic")
    p.controlla(s.zona_uguale(clic, dopo, LATERALE),
                "il clic su Files velata e' arrivato a Files (la barra laterale e' cambiata)")
    p.controlla(colore_in(clic, x + 3, cy) == BIANCO_SCELTA, "dopo il clic la scelta non e' piu' davanti")

    # (d) la scelta si chiude: il velo sparisce e Files torna come prima
    m.aspetta_che(lambda f: not _aperta(f), 120.0, cosa="la scelta chiudersi")
    fine = m.aspetta_schermo_fermo()
    m.foto("scelta_chiusa")
    p.controlla(s.zona_uguale(fine, prima, (FILES[0], FILES[1], FILES[0] + L, FILES[1] + A)),
                "chiusa la scelta, Files non e' tornata come prima")
    p.controlla(s.zona_uguale(fine, prima, (FILES[0] - 40, FILES[1] - 40, FILES[0] + L + 40, FILES[1] + A + 60)),
                "chiusa la scelta, l'ombra di Files non e' tornata come prima (Files non ha il fuoco?)")


DISCO = "disco_scelta.img"   # come disco_app.img, con la cartella /data/tests/scelta: accende il picker provvisorio
