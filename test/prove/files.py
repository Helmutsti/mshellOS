"""files: Files, il gestore dei file, e la cartella Home sulla scrivania.

Files si apre dall'isola (la sua finestra, 800 x 480, nel primo posto: angolo in (140, 130))
sulla casa, /home, dove build.sh ha copiato la cartella file/ del repository:
    Documenti/   appunti.txt, elenco della spesa.txt e una cartella con un nome lungo
    Immagini/    due .png
    Scrivania/   contatore.app (il pacchetto di Contatore, da installare: build.sh)
    dati.bin, leggimi.txt
Nella vista a icone gli elementi stanno in righe da 6, larghi 84 a 92 pixel l'uno
dall'altro: il primo con l'angolo in (+200, +98) dalla finestra, l'icona al centro in
(+242, +134). Prima le cartelle, poi i file, per nome.

Si prova, una cosa dopo l'altra:
  - la casa a icone e a elenco (la pillola nella barra: +562 e +588, y +58), confrontate
    con le foto di riferimento;
  - il doppio clic su Documenti ci entra, "indietro" (+24, +58) torna alla casa;
  - nella Scrivania (barra laterale) c'e' contatore.app: il doppio clic NON lo avvia (non
    e' installato): si apre lo Store con la richiesta di installazione, e lo si chiude;
  - in Documenti, la cartella col nome lungo non si apre (il suo percorso supera i 63
    caratteri della libreria): lo stato "errore";
  - /data/system (del sistema, 700) non si puo' aprire: lo stato "permesso negato". Ci si
    arriva come farebbe una persona: Apps nella barra laterale, "Disco" nel percorso (la
    radice), e poi la ricerca per trovare "data" e "system";
  - il tasto destro su un file apre il menu del file; sul vuoto il menu della cartella,
    e sopra "Ordina per" il suo sottomenu;
  - Nuova cartella: compare "Nuova cartella" con il nome da scrivere; si scrive "Prova",
    poi la si rinomina "Esempi" (dal menu) e la si elimina: la casa torna com'era;
  - eliminare Documenti (non e' vuota) non si puo': compare l'avviso in basso;
  - il doppio clic sulla Home della scrivania apre un'altra finestra di Files (secondo
    posto) sulla casa.
Le foto di riferimento sono in riferimenti/<cpu>/files_*.png (l'ora coperta).
"""

import time

import scrivania as s
from qemu import PUNTO_VUOTO

NOME = "files"
ORDINE = 65
DESCRIZIONE = "Files: viste, cartelle, stati, menu, nuova/rinomina/elimina; la Home sulla scrivania"
RIFERIMENTI = True

# dentro la finestra di Files (dall'angolo in alto a sinistra)
INDIETRO = (24, 58)
A_ICONE, A_ELENCO = (562, 58), (588, 58)
RICERCA = (700, 58)
RADICE = (88, 58)               # il primo nome del percorso fuori dalla casa ("Disco")
HOME, SCRIVANIA, APPS = (60, 141), (60, 177), (60, 213)
VUOTO = (520, 420)             # un punto del contenuto dove non c'e' niente (sotto i file)


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


def esegui(p):
    m = p.m
    wx, wy = s.angolo(0)
    l, a = s.MISURE["Files"]
    finestra = (wx, wy, wx + l, wy + a)
    corpo = (wx + 1, wy + 79, wx + l - 1, wy + a - 1)     # sotto le barre

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

    # --- dall'isola: la casa, a icone e a elenco ---
    s.apri(m, "Files")
    casa = ferma("casa_icone")
    p.riferimento("files_icone", casa)
    clic(A_ELENCO)
    elenco = cambia(casa, "la vista a elenco")
    m.foto("casa_elenco")
    p.riferimento("files_elenco", elenco)
    clic(A_ICONE)
    s.aspetta_zona(m, casa, corpo, attesa=10.0, cosa="di nuovo la vista a icone")

    # --- dentro Documenti, e indietro ---
    doppio_clic(m, wx + elemento(0)[0], wy + elemento(0)[1])          # Documenti
    documenti = cambia(casa, "Documenti aperta (doppio clic)")
    m.foto("documenti")
    clic(INDIETRO)
    s.aspetta_zona(m, casa, corpo, attesa=10.0, cosa="la casa dopo 'indietro'")

    # --- la Scrivania, con il pacchetto contatore.app: aprirlo non lo avvia ---
    # (prima era vuota e serviva per lo stato "vuota": ora lo stato vuoto non si prova,
    # finche' non si sceglie un'altra cartella vuota da mettere nella casa)
    clic(SCRIVANIA)
    scrivania_files = cambia(casa, "la Scrivania (con contatore.app)")
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
    # (non si confronta il corpo con com'era: dopo il doppio clic contatore.app resta scelto)
    m.aspetta_che(lambda f: s.segno_sotto(f, "Store") == 0, 10.0, cosa="lo Store chiuso")
    p.controlla(s.segno_sotto(m.aspetta_schermo_fermo(), "Files") > 0, "Files non e' piu' aperta")

    # --- errore: la cartella dal nome troppo lungo, in Documenti ---
    clic(HOME)
    s.aspetta_zona(m, casa, corpo, attesa=10.0, cosa="la casa dalla barra laterale")
    doppio_clic(m, wx + elemento(0)[0], wy + elemento(0)[1])
    s.aspetta_zona(m, documenti, corpo, attesa=10.0, cosa="di nuovo Documenti")
    doppio_clic(m, wx + elemento(0)[0], wy + elemento(0)[1])          # la cartella lunga
    errore = cambia(documenti, "lo stato 'errore'")
    m.foto("errore")
    p.riferimento("files_errore", errore)

    # --- permesso negato: /data/system, da Apps -> Disco -> data -> system ---
    clic(APPS)
    apps = cambia(errore, "Apps")
    m.foto("apps")
    clic(RADICE)
    radice = cambia(apps, "la radice (Disco)")
    m.foto("radice")
    clic(RICERCA)
    scrivi(m, "data")
    trovata = cambia(radice, "la ricerca di 'data'")
    doppio_clic(m, wx + elemento(0)[0], wy + elemento(0)[1])
    dati = cambia(trovata, "/data")
    m.foto("data")
    clic(RICERCA)
    scrivi(m, "system")
    trovata = cambia(dati, "la ricerca di 'system'")
    doppio_clic(m, wx + elemento(0)[0], wy + elemento(0)[1])
    negato = cambia(trovata, "lo stato 'permesso negato'")
    m.foto("permesso")
    p.riferimento("files_permesso", negato)

    # --- i menu del tasto destro ---
    clic(HOME)
    s.aspetta_zona(m, casa, corpo, attesa=10.0, cosa="la casa dalla barra laterale")
    x, y = elemento(4)                                                  # leggimi.txt
    clic_destro(m, wx + x, wy + y)
    menu_file = cambia(casa, "il menu del file", finestra)
    m.foto("menu_file")
    p.riferimento("files_menu_file", menu_file)
    clic(VUOTO)                     # un clic fuori lo chiude (e finisce li': leggimi.txt resta scelto)
    clic(VUOTO)                     # un altro clic sul vuoto: niente e' scelto
    s.aspetta_zona(m, casa, corpo, attesa=10.0, cosa="il menu chiuso (clic fuori)")

    mx, my = 250, 300                                                   # sul vuoto (sotto la prima riga)
    clic_destro(m, wx + mx, wy + my)
    menu_cartella = cambia(casa, "il menu della cartella", finestra)
    m.muovi(wx + mx + 100, wy + my + 8 + 40 + 9 + 19)           # sopra "Ordina per"
    time.sleep(0.3)
    m.muovi(wx + mx + 100, wy + my + 8 + 40 + 9 + 20)
    sottomenu = m.aspetta_schermo_fermo()
    m.foto("menu_cartella")
    p.controlla(not s.zona_uguale(menu_cartella, sottomenu, finestra), "il sottomenu 'Ordina per' non si apre")
    p.riferimento("files_menu_cartella", sottomenu, maschera=[(695, 25, 845, 50)])

    # --- nuova cartella, rinomina, elimina ---
    m.clic(wx + mx + 100, wy + my + 8 + 20)                             # "Nuova cartella"
    m.muovi(*PUNTO_VUOTO)
    nuova = cambia(casa, "la cartella nuova")
    m.foto("nuova_cartella")
    cancella(m, len("Nuova cartella"))
    scrivi(m, "Prova")
    m.premi("ret")
    time.sleep(0.5)
    prova = ferma("prova")
    p.controlla(not s.zona_uguale(nuova, prova, corpo), "il nome della cartella nuova non cambia")
    # Documenti, Immagini, Prova, Scrivania: Prova e' la terza
    x, y = elemento(2)
    clic_destro(m, wx + x, wy + y)
    cambia(prova, "il menu su Prova", finestra)
    m.clic(wx + x + 80, wy + y + 8 + 40 + 9 + 20)               # "Rinomina"
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
    m.clic(wx + x + 80, wy + y + 8 + 4 * 40 + 2 * 9 + 20)       # "Elimina"
    m.muovi(*PUNTO_VUOTO)
    s.aspetta_zona(m, casa, corpo, attesa=10.0, cosa="la casa com'era, dopo Elimina")

    # --- eliminare una cartella piena: l'avviso ---
    x, y = elemento(0)                                                  # Documenti
    clic_destro(m, wx + x, wy + y)
    cambia(casa, "il menu su Documenti", finestra)
    m.clic(wx + x + 80, wy + y + 8 + 4 * 40 + 2 * 9 + 20)
    m.muovi(*PUNTO_VUOTO)
    avviso = cambia(casa, "l'avviso 'solo le cartelle vuote'")
    m.foto("avviso")
    p.riferimento("files_avviso", avviso)

    # --- le informazioni di un file (dal menu), poi un clic le chiude ---
    clic(VUOTO)                                                         # via l'avviso
    clic(VUOTO)                                                         # niente scelto
    s.aspetta_zona(m, casa, corpo, attesa=10.0, cosa="la casa senza l'avviso")
    x, y = elemento(4)                                                  # leggimi.txt
    clic_destro(m, wx + x, wy + y)
    cambia(casa, "il menu su leggimi.txt", finestra)
    m.clic(wx + x + 80, wy + y + 8 + 3 * 40 + 9 + 20)           # "Informazioni"
    m.muovi(*PUNTO_VUOTO)
    scheda = cambia(casa, "la scheda delle informazioni", finestra)
    m.foto("informazioni")
    p.riferimento("files_informazioni", scheda)
    clic(VUOTO)
    clic(VUOTO)
    s.aspetta_zona(m, casa, corpo, attesa=10.0, cosa="la scheda chiusa")

    # --- un filtro (Testi): resta solo leggimi.txt; un altro clic lo spegne ---
    clic((60, 317))
    testi = cambia(casa, "il filtro Testi")
    m.foto("filtro_testi")
    clic((60, 317))
    s.aspetta_zona(m, casa, corpo, attesa=10.0, cosa="il filtro spento")

    # --- la tastiera: Documenti scelta, Invio la apre ---
    clic(elemento(0))
    m.premi("ret")
    s.aspetta_zona(m, documenti, corpo, attesa=10.0, cosa="Documenti aperta con Invio")
    clic(HOME)
    s.aspetta_zona(m, casa, corpo, attesa=10.0, cosa="la casa dalla barra laterale")

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
