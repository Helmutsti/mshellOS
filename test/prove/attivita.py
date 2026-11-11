"""attivita: la pagina Memoria di Attivita dice quanta RAM c'e', e si puo' terminare
un'applicazione.

Si apre prima Contatore (primo posto) e poi Attivita (secondo posto, angolo in 180,170).

LA MEMORIA. Nella scheda "Memoria" la prima riga dice "In uso 69,6 MB su 1972,1 MB". Il
totale e' quello che Linux dice in /proc/meminfo (MemTotal: la RAM meno quello che il
kernel tiene per se' fin dall'avvio): 1972 MB sul PC con 2 GB, 7946 con --ram 8192. Per leggerlo senza un vero lettore di testo
(OCR) si fa cosi':
  - si prende la riga, e si tengono solo i pixel chiari (il testo e' bianco);
  - si divide in PAROLE (colonne vuote larghe) e ogni parola in LETTERE (colonne vuote);
  - le ultime due parole sono il totale ("2041,7") e "MB"; del totale si tiene la parte
    prima della virgola ("2041"): la virgola e' la lettera che sta tutta in basso;
  - quel pezzo di immagine si confronta con quello salvato in riferimenti/<cpu>/
    (memoria_totale_<MB dati a QEMU>.png: memoria_totale_2048.png, _8192.png, _512.png).
    Le cifre sono disegnate sempre uguali, quindi basta questo: se il totale cambia
    (per esempio 1971 al posto di 1972), il disegno e' diverso.
Il riferimento si rifa' con --riferimenti (con --ram, per un'altra RAM), e va guardato a
occhio una volta.

TERMINARE. Nella scheda "Processi" si sceglie la riga di Contatore (il primo processo
dopo quelli del sistema: init, eventi, compositore, launcher, scrivania) e si preme
"Termina processo" (la X a destra nella barra degli strumenti): la finestra di Contatore
deve sparire, e nell'isola Contatore non deve piu' risultare aperta.
"""

import time

import scrivania as s
from qemu import ErroreProva, PUNTO_VUOTO

NOME = "attivita"
ORDINE = 60
DESCRIZIONE = "Attivita: il totale della memoria (1972 MB a 2 GB); termina Contatore"
RIFERIMENTI = True

RIGA_CONTATORE = 5        # 0 init, 1 eventi, 2 compositore, 3 launcher, 4 scrivania


def inchiostro(foto, area, soglia=170):
    """La zona in bianco e nero: 255 dove c'e' testo chiaro."""
    return foto.crop(area).convert("L").point(lambda v: 255 if v > soglia else 0)


def colonne_piene(bn):
    l, a = bn.size
    px = bn.load()
    return [any(px[x, y] for y in range(a)) for x in range(l)]


def pezzi(piene, spazio):
    """Le corse di colonne piene separate da almeno 'spazio' colonne vuote: [(x1, x2)]."""
    corse, inizio, vuote = [], None, 0
    for x, c in enumerate(piene + [False] * (spazio + 1)):
        if c:
            if inizio is None:
                inizio = x
            fine, vuote = x, 0
        elif inizio is not None:
            vuote += 1
            if vuote >= spazio:
                corse.append((inizio, fine + 1))
                inizio, vuote = None, 0
    return corse


def parte_intera_del_totale(foto, area):
    """L'immagine (bianco e nero) delle cifre prima della virgola del totale."""
    bn = inchiostro(foto, area)
    parole = pezzi(colonne_piene(bn), 4)
    if len(parole) < 2:
        raise ErroreProva(f"nella riga della memoria non trovo il totale ({len(parole)} parole)")
    x1, x2 = parole[-2]                       # il numero prima di "MB"
    numero = bn.crop((x1, 0, x2, bn.size[1]))
    a = numero.size[1]
    lettere = pezzi(colonne_piene(numero), 1)
    fine = x2 - x1
    for l1, l2 in lettere:
        box = numero.crop((l1, 0, l2, a)).getbbox()
        if box and box[1] > a * 0.55:          # tutta in basso: la virgola
            fine = l1
            break
    return numero.crop((0, 0, fine, a)).convert("RGB")


def esegui(p):
    m = p.m
    s.apri(m, "Contatore")
    s.apri(m, "Attivita")
    x, y = s.angolo(1)

    # --- la memoria ---
    m.clic(x + 125, y + 58)                    # la scheda "Memoria", nella barra degli strumenti
    m.muovi(*PUNTO_VUOTO)
    riga = (x + 20, y + 95, x + 620, y + 120)
    # la riga compare quando Attivita ha ridisegnato: si aspetta che ci sia testo
    f = m.aspetta_che(lambda f: inchiostro(f, riga).getbbox() is not None, 5.0,
                      cosa="la riga 'In uso ... su ...' nella scheda Memoria")
    time.sleep(0.3)
    f = m.foto("memoria")
    cifre = parte_intera_del_totale(f, riga)
    try:
        p.riferimento(f"memoria_totale_{p.ram}", cifre, maschera=None, tollerati=3)
    except ErroreProva as e:
        raise ErroreProva(f"il totale della memoria non e' quello atteso ({e}): guarda "
                          "memoria_totale_ottenuta.png e la foto 'memoria'") from None
    # --- terminare Contatore ---
    m.clic(x + 49, y + 58)                     # la scheda "Processi"
    time.sleep(0.5)
    m.clic(x + 150, y + 147 + 30 * RIGA_CONTATORE)
    time.sleep(0.3)
    m.clic(x + 616, y + 58)                    # "Termina processo": la X nella barra
    m.muovi(*PUNTO_VUOTO)
    cx, cy = s.angolo(0)
    # la barra di Contatore, sopra Attivita (e lontano dalla sua ombra)
    striscia = (cx, cy, cx + s.MISURE["Contatore"][0], cy + 16)
    s.aspetta_zona(m, p.scrivania, striscia, attesa=5.0, cosa="la finestra di Contatore sparire")
    m.aspetta_che(lambda f: s.segno_sotto(f, "Contatore") == 0, 5.0,
                  cosa="Contatore chiusa nell'isola")
    m.foto("terminata")
