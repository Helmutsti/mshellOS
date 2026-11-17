"""attivita: la pagina Memoria di Attivita dice quanta RAM c'e', l'albero dei processi
nasconde e mostra i servizi del sistema e apre e chiude i rami, e si puo' terminare
un'applicazione.

Si apre prima Esempio (primo posto) e poi Attivita (secondo posto, angolo in 180,170).

LA MEMORIA. Nella scheda "Memoria" la prima riga dice "In uso 109,1 MB su 1,9 GB". Il
totale e' quello che Linux dice in /proc/meminfo (MemTotal: la RAM meno quello che il
kernel tiene per se' fin dall'avvio): circa 1972 MB sul PC con 2 GB, 7946 con --ram 8192,
464 su ARM64 con 512 MB (TOTALE_MB, sotto). Attivita lo scrive con testo_byte di UIKit:
un decimale TRONCATO (non arrotondato, e senza ",0"), in MB fino a 1024 MB e in GB oltre.
Quindi "1,9 GB" vuol dire 1,9 GB <= totale < 2,0 GB, cioe' 1945,6 MB <= totale < 2048 MB.
Il testo si legge senza un vero lettore di testo (OCR), con le lettere del font stesso:
  - si prende la riga, e si tengono solo i pixel scuri (il testo e' scuro, il tema e' chiaro);
  - si divide in PAROLE (colonne vuote larghe) e ogni parola in LETTERE (colonne vuote);
  - le ultime due parole sono il totale e l'unita'. Ogni lettera si confronta con le lettere
    gia' disegnate del font che usa Attivita per quella riga (risorse/fonts/
    ibm-plex-sans-semibold-15.font, lo stesso file che legge GKit), passate in bianco e nero
    allo stesso modo: vince quella con meno pixel diversi;
  - il numero letto ("1,9") e l'unita' ("GB") devono contenere il totale atteso, con la
    tolleranza del troncamento: numero <= totale < numero + 0,1 (nell'unita' letta).
Nella sandbox la RAM e' quella del computer: il totale si legge e si scrive nelle note, ma
non si controlla.

L'ALBERO. Nella scheda "Processi" le righe sono ad albero (ognuno sotto chi l'ha lanciato).
Le righe si contano guardando la colonna PID (c'e' scritto qualcosa o no), la freccia del
ramo guardando il suo posto nella colonna Processo:
  - l'interruttore "Mostra i servizi di sistema" e' spento: init (PID 1), catalog, eventi,
    compositore e launcher non ci sono, la prima riga e' la scrivania (senza freccia: non ha figli);
  - acceso: le righe sono SERVIZI di piu' e la prima e' init, con la freccia del ramo aperto;
  - un clic sulla freccia di init chiude il ramo: resta solo init (tutti gli altri
    discendono da lui); un altro clic lo riapre.

TERMINARE. Con l'interruttore acceso le righe sono init, catalog, eventi, compositore, launcher
e sotto launcher le app in ordine di PID: Esempio (aperta per prima) e' la riga 5. Si
sceglie e si preme "Termina processo" (compare a destra nella barra degli strumenti
quando c'e' una riga scelta): la finestra di Esempio deve sparire, e nella userbar Esempio
non deve piu' risultare aperta.
"""

import re
import struct
import time
from pathlib import Path

from PIL import Image, ImageChops

import scrivania as s
from qemu import ErroreProva, PUNTO_VUOTO

NOME = "attivita"
ORDINE = 60
DESCRIZIONE = "Attivita: il totale della memoria (1972 MB a 2 GB); l'albero; termina Esempio"

# Il MemTotal di Linux, in MB interi, per la RAM data a QEMU (--ram): lo stesso kernel e la
# stessa macchina danno sempre lo stesso numero. Se cambia (kernel nuovo), la prova lo dice
# e il numero si aggiorna qui, dopo averlo guardato nella foto 'memoria'.
TOTALE_MB = {2048: 1972, 8192: 7946, 512: 464}
# il font della riga "In uso ... su ...": FONT_SEZIONE_NUOVO di UIKit (stile.h), Plex semigrassetto 15
FONT_RIGA = Path(__file__).resolve().parents[2] / "risorse" / "fonts" / "ibm-plex-sans-semibold-15.font"

SERVIZI = 5             # le righe che l'interruttore aggiunge: init, catalog, eventi, compositore, launcher
RIGA_ESEMPIO = 5        # servizi visibili: 0 init, 1 catalog, 2 eventi, 3 compositore, 4 launcher, 5 esempio
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


def lettere_del_font(percorso, caratteri, soglia=180):
    """Le lettere 'caratteri' del font gia' disegnato (formato in strumenti/grafica/formato.py),
    in bianco e nero e ritagliate sul disegno: {carattere: immagine "1"}. Un pixel e' pieno se
    la sua alfa supera 'soglia': il testo (grigio 66) sul pannello (grigio 250) e' "scuro"
    (inchiostro, sotto 120 di grigio) quando copre piu' di sette decimi del pixel."""
    dati = percorso.read_bytes()
    if dati[:4] != b"MSFN":
        raise ErroreProva(f"{percorso.name} non e' un font di mshell")
    quanti, = struct.unpack_from("<I", dati, 8)
    indice = dati[16:272]
    alfa = 272 + 12 * quanti
    lettere = {}
    for c in caratteri:
        g = indice[ord(c)]
        if not g:
            continue
        inizio, _x, _y, l, a, _avanzo = struct.unpack_from("<IbbBBB", dati, 272 + 12 * g)
        im = Image.frombytes("L", (l, a), dati[alfa + inizio: alfa + inizio + l * a])
        im = im.point(lambda v: 255 if v > soglia else 0)
        box = im.getbbox()
        if box:
            lettere[c] = im.crop(box)
    return lettere


def piu_simile(pezzo, lettere):
    """La lettera del font piu' simile al pezzo di immagine (bianco e nero, ritagliato):
    si contano i pixel diversi, allineati in alto a sinistra, su un riquadro grande quanto
    tutti e due (cosi' anche la differenza di misura conta)."""
    meglio, costo_meglio = None, None
    for c, im in lettere.items():
        l, a = max(im.size[0], pezzo.size[0]), max(im.size[1], pezzo.size[1])
        x = Image.new("L", (l, a)); x.paste(im, (0, 0))
        y = Image.new("L", (l, a)); y.paste(pezzo, (0, 0))
        costo = ImageChops.difference(x, y).histogram()[255]
        if costo_meglio is None or costo < costo_meglio:
            meglio, costo_meglio = c, costo
    return meglio


def leggi_totale(foto, area):
    """Il totale scritto in fondo alla riga 'In uso ... su <numero> <unita'>': (testo del
    numero, unita'), per esempio ("1,9", "GB")."""
    bn = inchiostro(foto, area)
    parole = pezzi(colonne_piene(bn), 4)
    if len(parole) < 2:
        raise ErroreProva(f"nella riga della memoria non trovo il totale ({len(parole)} parole)")
    lettere = lettere_del_font(FONT_RIGA, "0123456789,KMGTB")
    letto = []
    for x1, x2 in parole[-2:]:
        parola = bn.crop((x1, 0, x2, bn.size[1]))
        testo = ""
        for l1, l2 in pezzi(colonne_piene(parola), 1):
            pezzo = parola.crop((l1, 0, l2, parola.size[1]))
            pezzo = pezzo.crop(pezzo.getbbox())
            testo += piu_simile(pezzo, lettere)
        letto.append(testo)
    return letto[0], letto[1]


def totale_giusto(numero, unita, atteso_mb):
    """Vero se "<numero> <unita'>" (un decimale troncato, come testo_byte di UIKit) puo' essere
    il totale atteso, dato in MB interi (il totale vero sta tra atteso_mb e atteso_mb + 1)."""
    scala = {"MB": 1, "GB": 1024, "TB": 1024 * 1024}.get(unita)
    if scala is None or not re.fullmatch(r"\d+(,\d)?", numero):
        return False
    valore = float(numero.replace(",", "."))
    # [atteso_mb, atteso_mb + 1) e [valore, valore + 0,1) * scala devono toccarsi
    return valore * scala < atteso_mb + 1 and atteso_mb < (valore + 0.1) * scala


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
    # per leggere serve la riga intera: dal pannello (y + 89) fin sopra la barra (y + 133)
    numero, unita = leggi_totale(f, (x + 20, y + 100, x + 620, y + 131))
    if not p.ram:
        # nella sandbox la RAM e' quella del computer, diversa da uno all'altro: il totale
        # si legge ma non si controlla (lo si controlla in QEMU, con la RAM data)
        p.nota(f"totale della memoria letto: {numero} {unita}, non controllato: senza RAM fissa (sandbox)")
    else:
        atteso = TOTALE_MB.get(p.ram)
        p.controlla(atteso is not None,
                    f"non so quale totale aspettarmi con {p.ram} MB di RAM: aggiungilo a TOTALE_MB")
        p.controlla(totale_giusto(numero, unita, atteso),
                    f"il totale della memoria dice {numero} {unita}, ma con {p.ram} MB di RAM "
                    f"Linux ne da' {atteso} (guarda la foto 'memoria')")
        p.nota(f"totale della memoria: {numero} {unita} ({atteso} MB)")
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
    f = m.aspetta_che(lambda f: righe_piene(f, x, y) == min(nascosti + SERVIZI, RIGHE_IN_VISTA), 5.0,
                      cosa=f"acceso l'interruttore, {SERVIZI} righe in piu' (init, catalog, eventi, compositore, launcher)")
    time.sleep(0.3)
    f = m.foto("servizi_visibili")
    p.controlla(freccia_prima_riga(f, x, y), "servizi visibili: la prima riga (init, PID 1) non ha la freccia")
    p.nota(f"righe: {nascosti} con i servizi nascosti, {nascosti + SERVIZI} visibili")
    m.clic(x + (FRECCIA[0] + FRECCIA[1]) // 2, y + PRIMA_RIGA)     # chiude il ramo di init
    m.muovi(*PUNTO_VUOTO)
    m.aspetta_che(lambda f: righe_piene(f, x, y) == 1, 5.0, cosa="chiuso il ramo di init, resta solo lui")
    m.foto("ramo_chiuso")
    m.clic(x + (FRECCIA[0] + FRECCIA[1]) // 2, y + PRIMA_RIGA)     # e lo riapre
    m.muovi(*PUNTO_VUOTO)
    m.aspetta_che(lambda f: righe_piene(f, x, y) == min(nascosti + SERVIZI, RIGHE_IN_VISTA), 5.0, cosa="riaperto il ramo di init")
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
