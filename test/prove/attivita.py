"""attivita: la pagina Memoria di Attivita dice quanta RAM c'e', l'albero dei processi
nasconde e mostra i servizi del sistema e apre e chiude i rami, e si puo' terminare
un'applicazione.

Si apre prima Esempio (primo posto) e poi Attivita (secondo posto, angolo in 180,170).

LA MEMORIA. Nella scheda "Memoria" la prima riga dice "In uso 69,6 MB su 1972,1 MB". Il
totale e' quello che Linux dice in /proc/meminfo (MemTotal: la RAM meno quello che il
kernel tiene per se' fin dall'avvio): 1972 MB sul PC con 2 GB, 7946 con --ram 8192. Per leggerlo senza un vero lettore di testo
(OCR) si fa cosi':
  - si prende la riga, e si tengono solo i pixel scuri (il testo e' scuro, il tema e' chiaro);
  - si divide in PAROLE (colonne vuote larghe) e ogni parola in LETTERE (colonne vuote);
  - le ultime due parole sono il totale ("2041,7") e "MB"; del totale si tiene la parte
    prima della virgola ("2041"): la virgola e' la lettera che sta tutta in basso;
  - quel pezzo di immagine si confronta con quello salvato in riferimenti/<cpu>/
    (memoria_totale_<MB dati a QEMU>.png: memoria_totale_2048.png, _8192.png, _512.png).
    Le cifre sono disegnate sempre uguali, quindi basta questo: se il totale cambia
    (per esempio 1971 al posto di 1972), il disegno e' diverso.
Il riferimento si rifa' con --riferimenti (con --ram, per un'altra RAM), e va guardato a
occhio una volta.

L'ALBERO. Nella scheda "Processi" le righe sono ad albero (ognuno sotto chi l'ha lanciato).
Le righe si contano guardando la colonna PID (c'e' scritto qualcosa o no), la freccia del
ramo guardando il suo posto nella colonna Processo:
  - l'interruttore "Mostra i servizi di sistema" e' spento: init (PID 1), eventi, compositore
    e launcher non ci sono, la prima riga e' la scrivania (senza freccia: non ha figli);
  - acceso: le righe sono 4 di piu' e la prima e' init, con la freccia del ramo aperto;
  - un clic sulla freccia di init chiude il ramo: resta solo init (tutti gli altri
    discendono da lui); un altro clic lo riapre.

TERMINARE. Con l'interruttore acceso le righe sono init, eventi, compositore, launcher e
sotto launcher le app in ordine di PID: Esempio (aperta per prima) e' la riga 4. Si
sceglie e si preme "Termina processo" (compare a destra nella barra degli strumenti
quando c'e' una riga scelta): la finestra di Esempio deve sparire, e nella userbar Esempio
non deve piu' risultare aperta.
"""

import time

import scrivania as s
from qemu import ErroreProva, PUNTO_VUOTO

NOME = "attivita"
ORDINE = 60
DESCRIZIONE = "Attivita: il totale della memoria (1972 MB a 2 GB); l'albero; termina Esempio"
RIFERIMENTI = True

RIGA_ESEMPIO = 4        # servizi visibili: 0 init, 1 eventi, 2 compositore, 3 launcher, 4 esempio
PRIMA_RIGA = 150        # il centro della prima riga, dall'angolo della finestra
PASSO = 44              # righe alte 40, a 4 l'una dall'altra
RIGHE_IN_VISTA = 8      # quante ne stanno nella finestra (alta 526)
COLONNA_PID = (24, 72)  # x della colonna PID
FRECCIA = (96, 112)     # x del posto della freccia del ramo (livello 0)
INTERRUTTORE = (724, 502)   # il centro dell'interruttore, in basso a destra nel piede


def righe_piene(foto, x, y):
    """Quante righe hanno qualcosa scritto nella colonna PID (dall'alto, fino alla prima vuota)."""
    n = 0
    for k in range(RIGHE_IN_VISTA):
        cy = y + PRIMA_RIGA + PASSO * k
        zona = (x + COLONNA_PID[0], cy - 8, x + COLONNA_PID[1], cy + 8)
        if inchiostro(foto, zona, 200).getbbox() is None:
            break
        n += 1
    return n


def freccia_prima_riga(foto, x, y):
    """Vero se al posto della freccia della prima riga c'e' disegnato qualcosa."""
    cy = y + PRIMA_RIGA
    return inchiostro(foto, (x + FRECCIA[0], cy - 8, x + FRECCIA[1], cy + 8), 200).getbbox() is not None


def inchiostro(foto, area, soglia=120):
    """La zona in bianco e nero: 255 dove c'e' testo scuro (il tema e' chiaro)."""
    return foto.crop(area).convert("L").point(lambda v: 255 if v < soglia else 0)


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
    s.apri(m, "Esempio")
    s.apri(m, "Attivita")
    x, y = s.angolo(1)

    # --- la memoria ---
    m.clic(x + 384, y + 65)                    # la scheda "Memoria" (tre schede larghe uguali nella barra)
    m.muovi(*PUNTO_VUOTO)
    riga = (x + 20, y + 112, x + 620, y + 135)
    # la riga compare quando Attivita ha ridisegnato: si aspetta che ci sia testo
    f = m.aspetta_che(lambda f: inchiostro(f, riga).getbbox() is not None, 5.0,
                      cosa="la riga 'In uso ... su ...' nella scheda Memoria")
    time.sleep(0.3)
    f = m.foto("memoria")
    if not p.ram:
        # nella sandbox la RAM e' quella del computer, diversa da uno all'altro: il totale
        # non ha un riferimento (lo si controlla in QEMU, con la RAM data)
        p.nota("totale della memoria non controllato: senza RAM fissa (sandbox)")
    else:
        cifre = parte_intera_del_totale(f, riga)
        try:
            p.riferimento(f"memoria_totale_{p.ram}", cifre, maschera=None, tollerati=3)
        except ErroreProva as e:
            raise ErroreProva(f"il totale della memoria non e' quello atteso ({e}): guarda "
                              "memoria_totale_ottenuta.png e la foto 'memoria'") from None
    # --- l'albero: i servizi del sistema nascosti, poi visibili; un ramo chiuso e riaperto ---
    m.clic(x + 132, y + 65)                    # la scheda "Processi"
    m.muovi(*PUNTO_VUOTO)
    f = m.aspetta_che(lambda f: righe_piene(f, x, y) > 0, 5.0, cosa="le righe dei processi")
    time.sleep(0.3)
    f = m.foto("servizi_nascosti")
    nascosti = righe_piene(f, x, y)
    p.controlla(not freccia_prima_riga(f, x, y),
                "servizi nascosti: la prima riga ha una freccia (e' init, PID 1, invece della scrivania?)")
    m.clic(x + INTERRUTTORE[0], y + INTERRUTTORE[1])
    m.muovi(*PUNTO_VUOTO)
    f = m.aspetta_che(lambda f: righe_piene(f, x, y) == nascosti + 4, 5.0,
                      cosa="acceso l'interruttore, 4 righe in piu' (init, eventi, compositore, launcher)")
    time.sleep(0.3)
    f = m.foto("servizi_visibili")
    p.controlla(freccia_prima_riga(f, x, y), "servizi visibili: la prima riga (init, PID 1) non ha la freccia")
    p.nota(f"righe: {nascosti} con i servizi nascosti, {nascosti + 4} visibili")
    m.clic(x + (FRECCIA[0] + FRECCIA[1]) // 2, y + PRIMA_RIGA)     # chiude il ramo di init
    m.muovi(*PUNTO_VUOTO)
    m.aspetta_che(lambda f: righe_piene(f, x, y) == 1, 5.0, cosa="chiuso il ramo di init, resta solo lui")
    m.foto("ramo_chiuso")
    m.clic(x + (FRECCIA[0] + FRECCIA[1]) // 2, y + PRIMA_RIGA)     # e lo riapre
    m.muovi(*PUNTO_VUOTO)
    m.aspetta_che(lambda f: righe_piene(f, x, y) == nascosti + 4, 5.0, cosa="riaperto il ramo di init")
    # --- terminare Esempio ---
    m.clic(x + 300, y + PRIMA_RIGA + PASSO * RIGA_ESEMPIO)
    time.sleep(0.3)
    m.clic(x + 684, y + 65)                    # "Termina processo", in fondo a destra nella barra (compare con la riga scelta)
    m.muovi(*PUNTO_VUOTO)
    cx, cy = s.angolo(0)
    # la barra di Esempio, sopra Attivita (e lontano dalla sua ombra)
    striscia = (cx, cy, cx + s.MISURE["Esempio"][0], cy + 16)
    s.aspetta_zona(m, p.scrivania, striscia, attesa=5.0, cosa="la finestra di Esempio sparire")
    m.aspetta_che(lambda f: s.segno_sotto(f, "Esempio") == 0, 5.0,
                  cosa="Esempio chiusa nella userbar")
    m.foto("terminata")


DISCO = "disco_app.img"   # con Esempio e Contatore, le app di prova (utente/prove/applicazioni): non sono nel disco normale
