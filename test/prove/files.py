"""files: Files, il gestore dei file, e la cartella Home sulla scrivania.

Files si apre dall'isola (la sua finestra, 800 x 480, nel primo posto: angolo in (140, 130))
sulla casa, /home, dove build.sh ha copiato la cartella file/ del repository:
    Documenti/   appunti.txt, elenco della spesa.txt e una cartella con un nome lungo
    Immagini/    due .png
    Scrivania/   contatore.app (il pacchetto di Contatore, da installare: build.sh)
    dati.bin, leggimi.txt
La finestra e' come nel disegno di Figma: la barra del titolo (40 + 1), la barra degli
strumenti (48: indietro/avanti, icone/elenco, il percorso, la ricerca, che cominciano dove
finisce la barra laterale, a x +208), sotto la barra laterale (larga 200) e il pannello chiaro
(da x +208, y +89). Nella vista a icone i tile sono larghi 96 a 8 l'uno dall'altro, in righe da
5: l'icona del k-esimo al centro in (+264 + 104 * (k % 5), +133 + 124 * (k // 5)). Prima le
cartelle, poi i file, per nome.

Si prova, una cosa dopo l'altra:
  - la casa a icone e a elenco (il gruppo nella barra: +282 e +308, y +65), confrontate
    con le foto di riferimento; nell'elenco un clic sull'intestazione "Nome" gira l'ordine
    (la freccia in giu'), un altro lo rimette;
  - il doppio clic su Documenti ci entra, "indietro" (+216, +65) torna alla casa;
  - nella Scrivania (barra laterale) c'e' contatore.app: il doppio clic NON lo avvia (non
    e' installato): si apre lo Store con la richiesta di installazione, e lo si chiude;
  - in Documenti, la cartella col nome lungo si apre (i percorsi arrivano a 4096
    caratteri, come in Linux) e mostra leggimi.txt;
  - il tasto destro su un file apre il menu del file; sul vuoto il menu della cartella,
    e sopra "Ordina per" il suo sottomenu;
  - Nuova cartella: compare "Nuova cartella" con il nome da scrivere; si scrive "Prova",
    Invio, e un altro Invio la apre: lo stato "vuota"; indietro, la si rinomina "Esempi" (dal
    menu) e la si elimina: la casa torna com'era;
  - eliminare Documenti (non e' vuota) non si puo': compare l'avviso in basso;
  - le informazioni di un file (dal menu) si aprono in una mini finestra; Esc la chiude;
  - un filtro (Testi) e la tastiera: Invio apre la cartella scelta; le scorciatoie scritte
    nei menu (Ctrl+Maiusc+N, F2, Ctrl+D, Ctrl+I, Canc, Esc);
  - il doppio clic sulla Home della scrivania apre un'altra finestra di Files (secondo
    posto) sulla casa.
Lo stato "permesso negato" non si prova piu': il disegno non ha la scorciatoia Apps nella
barra laterale, e dalla casa non si arriva alle cartelle del sistema.
Le foto di riferimento sono in riferimenti/<cpu>/files_*.png (l'ora coperta).
"""

import time

import scrivania as s
from qemu import PUNTO_VUOTO

NOME = "files"
ORDINE = 65
DESCRIZIONE = "Files: viste, ordine, cartelle, stati, menu, scorciatoie, informazioni; la Home sulla scrivania"
RIFERIMENTI = True

# dentro la finestra di Files (dall'angolo in alto a sinistra)
INDIETRO = (216, 65)
A_ICONE, A_ELENCO = (282, 65), (308, 65)
RICERCA = (700, 65)
HOME, SCRIVANIA = (60, 145), (60, 181)
TESTI = (60, 285)
INTESTAZIONE_NOME = (240, 105)  # l'intestazione della colonna Nome, nella vista a elenco
VUOTO = (520, 420)              # un punto del pannello dove non c'e' niente (sotto i file)
CORPO_Y = 89                    # sotto le barre

# il menu (voci S da 32, padding 10 sopra): il centro della voce n (contando i separatori, 9)
def voce(n, separatori=0):
    return 10 + n * 32 + separatori * 9 + 16


def elemento(k):
    """Il centro dell'icona del k-esimo elemento (vista a icone)."""
    return 264 + (k % 5) * 104, 133 + (k // 5) * 124


def doppio_clic(m, x, y):
    """Due clic veloci (il doppio clic e' entro mezzo secondo); poi il puntatore va
    nell'angolo, perche' niente resti acceso sotto di lui."""
    m.muovi(x, y)
    time.sleep(0.15)
    for _ in range(2):
        m.pulsante(True)
        time.sleep(0.05)
        m.pulsante(False)
        time.sleep(0.05)
    time.sleep(0.1)
    m.muovi(*PUNTO_VUOTO)


def clic_destro(m, x, y):
    m.muovi(x, y)
    time.sleep(0.15)
    m.pulsante(True, "right")
    time.sleep(0.15)
    m.pulsante(False, "right")
    time.sleep(0.15)


def scrivi(m, testo):
    """Scrive un testo (lettere e spazi; le maiuscole con il tasto maiuscole)."""
    for c in testo:
        if c.isupper():
            m.tasto_giu("shift")
            m.premi(c.lower())
            m.tasto_su("shift")
        else:
            m.premi({" ": "spc"}.get(c, c))
        time.sleep(0.03)


def cancella(m, quante):
    for _ in range(quante):
        m.premi("backspace")
        time.sleep(0.03)


def insieme(m, *tasti):
    """Una scorciatoia: i modificatori giu', il tasto, poi su."""
    for t in tasti[:-1]:
        m.tasto_giu(t)
        time.sleep(0.05)
    m.premi(tasti[-1])
    time.sleep(0.05)
    for t in reversed(tasti[:-1]):
        m.tasto_su(t)
        time.sleep(0.05)


def esegui(p):
    m = p.m
    wx, wy = s.angolo(0)
    l, a = s.MISURE["Files"]
    finestra = (wx, wy, wx + l, wy + a)
    corpo = (wx + 1, wy + CORPO_Y, wx + l - 1, wy + a - 1)
    schermo = (0, 70, 1280, 800)                           # tutto, tranne l'isola (l'ora)

    def clic(punto):
        """Un clic nella finestra; poi il puntatore va nell'angolo (niente resta acceso)."""
        m.clic(wx + punto[0], wy + punto[1])
        m.muovi(*PUNTO_VUOTO)

    def ferma(nome):
        m.muovi(*PUNTO_VUOTO)
        foto = m.aspetta_schermo_fermo()
        m.foto(nome)
        return foto

    def cambia(prima, cosa, zona=corpo):
        """Aspetta che la zona cambi rispetto a 'prima', poi che lo schermo si fermi."""
        m.muovi(*PUNTO_VUOTO)
        s.aspetta_zona(m, prima, zona, uguale=False, attesa=10.0, cosa=cosa)
        return m.aspetta_schermo_fermo()

    def torna(foto, cosa, zona=corpo):
        return s.aspetta_zona(m, foto, zona, attesa=10.0, cosa=cosa)

    # --- dall'isola: la casa, a icone e a elenco; l'ordine dall'intestazione ---
    s.apri(m, "Files")
    casa = ferma("casa_icone")
    p.riferimento("files_icone", casa)
    clic(A_ELENCO)
    elenco = cambia(casa, "la vista a elenco")
    m.foto("casa_elenco")
    p.riferimento("files_elenco", elenco)
    clic(INTESTAZIONE_NOME)
    girato = cambia(elenco, "l'ordine per nome girato (freccia in giu')")
    m.foto("elenco_nome_decrescente")
    clic(INTESTAZIONE_NOME)
    torna(elenco, "l'ordine per nome di nuovo crescente")
    p.controlla(not s.zona_uguale(girato, elenco, corpo), "girare l'ordine non cambia niente")
    clic(A_ICONE)
    torna(casa, "di nuovo la vista a icone")

    # --- dentro Documenti, e indietro ---
    doppio_clic(m, wx + elemento(0)[0], wy + elemento(0)[1])          # Documenti
    documenti = cambia(casa, "Documenti aperta (doppio clic)")
    m.foto("documenti")
    clic(INDIETRO)
    torna(casa, "la casa dopo 'indietro'")

    # --- la Scrivania, con il pacchetto contatore.app: aprirlo non lo avvia ---
    clic(SCRIVANIA)
    cambia(casa, "la Scrivania (con contatore.app)")
    m.foto("scrivania")
    doppio_clic(m, wx + elemento(0)[0], wy + elemento(0)[1])           # contatore.app
    # si apre lo Store con la richiesta di installazione (nel secondo posto, 560 x 480): lo
    # si chiude con la sua X (la prova store lo prova tutto)
    m.aspetta_seriale("store: richiesta di installazione: contatore", attesa=20.0)
    m.muovi(*PUNTO_VUOTO)
    m.aspetta_che(lambda f: s.segno_sotto(f, "Store") > 0, 10.0, cosa="lo Store aperto nell'isola")
    time.sleep(0.5)
    m.aspetta_schermo_fermo()
    m.foto("pacchetto_nello_store")
    sx, sy = s.angolo(1)
    m.clic(*s.comandi(sx, sy, 560)[1])
    m.muovi(*PUNTO_VUOTO)
    m.aspetta_che(lambda f: s.segno_sotto(f, "Store") == 0, 10.0, cosa="lo Store chiuso")
    p.controlla(s.segno_sotto(m.aspetta_schermo_fermo(), "Files") > 0, "Files non e' piu' aperta")

    # --- la cartella dal nome lungo (percorso oltre i 63 caratteri di una volta), in Documenti ---
    clic(HOME)
    torna(casa, "la casa dalla barra laterale")
    doppio_clic(m, wx + elemento(0)[0], wy + elemento(0)[1])
    torna(documenti, "di nuovo Documenti")
    doppio_clic(m, wx + elemento(0)[0], wy + elemento(0)[1])          # la cartella lunga
    cambia(documenti, "la cartella dal nome lungo aperta (c'e' leggimi.txt)")
    m.foto("cartella_lunga")

    # --- i menu del tasto destro ---
    clic(HOME)
    torna(casa, "la casa dalla barra laterale")
    x, y = elemento(4)                                                  # leggimi.txt
    clic_destro(m, wx + x, wy + y)
    menu_file = cambia(casa, "il menu del file", finestra)
    m.foto("menu_file")
    p.riferimento("files_menu_file", menu_file)
    m.premi("esc")                  # Esc lo chiude (leggimi.txt resta scelto)
    time.sleep(0.3)
    clic(VUOTO)                     # un clic sul vuoto: niente e' scelto
    torna(casa, "il menu chiuso (Esc)")

    mx, my = 250, 300                                                   # sul vuoto (sotto la prima riga)
    clic_destro(m, wx + mx, wy + my)
    menu_cartella = cambia(casa, "il menu della cartella", finestra)
    m.muovi(wx + mx + 100, wy + my + voce(1, 1) - 1)            # sopra "Ordina per"
    time.sleep(0.3)
    m.muovi(wx + mx + 100, wy + my + voce(1, 1))
    sottomenu = m.aspetta_schermo_fermo()
    m.foto("menu_cartella")
    p.controlla(not s.zona_uguale(menu_cartella, sottomenu, finestra), "il sottomenu 'Ordina per' non si apre")
    p.riferimento("files_menu_cartella", sottomenu, maschera=[(695, 25, 845, 50)])

    # --- nuova cartella, la cartella vuota, rinomina, elimina ---
    m.clic(wx + mx + 100, wy + my + voce(0))                           # "Nuova cartella"
    m.muovi(*PUNTO_VUOTO)
    nuova = cambia(casa, "la cartella nuova")
    m.foto("nuova_cartella")
    cancella(m, len("Nuova cartella"))
    scrivi(m, "Prova")
    m.premi("ret")
    time.sleep(0.5)
    prova = ferma("prova")
    p.controlla(not s.zona_uguale(nuova, prova, corpo), "il nome della cartella nuova non cambia")
    m.premi("ret")                                                      # Invio la apre: e' vuota
    vuota = cambia(prova, "la cartella vuota (Invio)")
    m.foto("vuota")
    p.riferimento("files_vuota", vuota)
    clic(INDIETRO)
    prova = cambia(vuota, "la casa con Prova, dopo 'indietro'")
    # Documenti, Immagini, Prova, Scrivania: Prova e' la terza
    x, y = elemento(2)
    clic_destro(m, wx + x, wy + y)
    cambia(prova, "il menu su Prova", finestra)
    m.clic(wx + x + 80, wy + y + voce(1))                               # "Rinomina"
    m.muovi(*PUNTO_VUOTO)
    time.sleep(0.5)
    cancella(m, len("Prova"))
    scrivi(m, "Esempi")
    m.premi("ret")
    time.sleep(0.5)
    esempi = ferma("esempi")
    p.controlla(not s.zona_uguale(prova, esempi, corpo), "Rinomina non cambia il nome")
    # Documenti, Esempi, Immagini, Scrivania: Esempi e' la seconda
    x, y = elemento(1)
    clic_destro(m, wx + x, wy + y)
    cambia(esempi, "il menu su Esempi", finestra)
    m.clic(wx + x + 80, wy + y + voce(4, 1))                            # "Elimina"
    m.muovi(*PUNTO_VUOTO)
    torna(casa, "la casa com'era, dopo Elimina")

    # --- eliminare una cartella piena: l'avviso ---
    x, y = elemento(0)                                                  # Documenti
    clic_destro(m, wx + x, wy + y)
    cambia(casa, "il menu su Documenti", finestra)
    m.clic(wx + x + 80, wy + y + voce(4, 1))
    m.muovi(*PUNTO_VUOTO)
    avviso = cambia(casa, "l'avviso 'solo le cartelle vuote'")
    m.foto("avviso")
    p.riferimento("files_avviso", avviso)

    # --- le informazioni di un file (dal menu): una mini finestra; Esc la chiude ---
    clic(VUOTO)                                                         # via l'avviso, niente scelto
    torna(casa, "la casa senza l'avviso")
    x, y = elemento(4)                                                  # leggimi.txt
    clic_destro(m, wx + x, wy + y)
    menu = cambia(casa, "il menu su leggimi.txt", finestra)
    m.clic(wx + x + 80, wy + y + voce(3))                               # "Informazioni"
    m.muovi(*PUNTO_VUOTO)
    informazioni = cambia(menu, "la mini finestra delle informazioni", schermo)
    m.foto("informazioni")
    p.riferimento("files_informazioni", informazioni)
    m.premi("esc")
    m.muovi(*PUNTO_VUOTO)
    s.aspetta_zona(m, informazioni, schermo, uguale=False, attesa=10.0, cosa="la mini finestra chiusa (Esc)")
    clic(VUOTO)
    torna(casa, "la casa dopo le informazioni")

    # --- un filtro (Testi): resta solo leggimi.txt; un altro clic lo spegne ---
    clic(TESTI)
    cambia(casa, "il filtro Testi")
    m.foto("filtro_testi")
    clic(TESTI)
    torna(casa, "il filtro spento")

    # --- la tastiera: Documenti scelta, Invio la apre ---
    clic(elemento(0))
    m.premi("ret")
    torna(documenti, "Documenti aperta con Invio")
    clic(HOME)
    torna(casa, "la casa dalla barra laterale")

    # --- le scorciatoie dei menu ---
    insieme(m, "ctrl", "shift", "n")                                    # Ctrl+Maiusc+N: una cartella nuova
    nuova = cambia(casa, "la cartella nuova con Ctrl+Maiusc+N")
    m.premi("esc")                                                      # Esc: il nome resta quello
    time.sleep(0.3)
    clic(elemento(2))                                                   # Documenti, Immagini, Nuova cartella
    m.premi("f2")                                                       # F2: si rinomina
    time.sleep(0.3)
    cancella(m, len("Nuova cartella"))
    scrivi(m, "Tasti")
    m.premi("ret")
    tasti = cambia(nuova, "il nome nuovo con F2")
    m.foto("scorciatoie_f2")
    m.premi("delete")                                                   # Canc: via (e' vuota)
    torna(casa, "la cartella eliminata con Canc")
    clic(elemento(4))                                                   # leggimi.txt
    scelto = m.aspetta_schermo_fermo()
    insieme(m, "ctrl", "d")                                             # Ctrl+D: "leggimi copia.txt"
    copia = cambia(scelto, "la copia con Ctrl+D")
    m.foto("scorciatoie_ctrl_d")
    m.premi("delete")                                                   # la copia e' scelta: Canc la toglie
    torna(casa, "la copia eliminata con Canc")
    clic(elemento(4))
    insieme(m, "ctrl", "i")                                             # Ctrl+I: le informazioni
    informazioni = cambia(scelto, "le informazioni con Ctrl+I", schermo)
    m.premi("esc")
    s.aspetta_zona(m, informazioni, schermo, uguale=False, attesa=10.0, cosa="la mini finestra chiusa")
    clic(elemento(4))                                                   # di nuovo in Files, leggimi.txt scelto
    m.premi("esc")                                                      # Esc: niente e' scelto
    torna(casa, "la scelta tolta con Esc")
    p.controlla(not s.zona_uguale(tasti, copia, corpo), "le scorciatoie non hanno fatto niente")

    # --- dalla scrivania: il doppio clic sulla Home apre Files (un'altra finestra) ---
    prima = m.aspetta_schermo_fermo()
    doppio_clic(m, *s.HOME)
    x2, y2 = s.angolo(1)
    m.muovi(*PUNTO_VUOTO)
    s.aspetta_zona(m, prima, (x2, y2, x2 + l, y2 + s.BARRA), uguale=False, attesa=15.0,
                   cosa="Files aperta dalla Home della scrivania")
    dalla_scrivania = m.aspetta_schermo_fermo()
    m.foto("dalla_scrivania")
    p.riferimento("files_dalla_scrivania", dalla_scrivania)
