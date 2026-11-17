"""bloccotesto: il Blocco note e la sua area di testo (uikit/area_testo.h).

Si apre il Blocco note dalla userbar (e' la seconda app) e si prova, guardando la finestra:
  1. documento nuovo: il segnaposto "Scrivi qui..." e il cursore all'inizio;
  2. si scrive (con le lettere accentate, Invio, il tasto Maiusc per le maiuscole): il segnaposto
     sparisce e il testo c'e'; il cursore e' a fine testo;
  3. Backspace toglie una lettera intera anche se accentata (e' UTF-8: 2 byte);
  4. le frecce, Home e Fine, Ctrl+freccia, e la selezione con Maiusc (la stessa del disegno:
     da "latte" a "- uo"): lo sfondo blu tenue c'e' e il cursore e' dov'e' nel disegno;
  5. scrivere con una selezione la sostituisce; Backspace la cancella;
  6. Tab inserisce un carattere di tabulazione (largo fino alla colonna multipla di 4); con piu'
     righe selezionate (Ctrl+A) Tab le rientra tutte e Maiusc+Tab toglie il rientro;
  7. una riga lunga va a capo alla larghezza dell'area (a parola);
  8. un testo piu' lungo dell'area scorre: il cursore resta in vista, Ctrl+Home torna in cima, la
     rotella scorre;
  9. il mouse: un clic mette il cursore, trascinare seleziona, il doppio clic seleziona la parola;
 10. senza il fuoco (un clic su un pulsante della barra) il cursore sparisce e la selezione e' grigia.
Le foto della finestra restano nella cartella della prova (con --tieni).
"""

import time

import scrivania as s
from qemu import pixel_diversi

NOME = "bloccotesto"
ORDINE = 46
DESCRIZIONE = "il Blocco note: scrivere, accenti, selezione, Tab, a capo, scorrimento, mouse, fuoco"

FINESTRA = (140, 130, 140 + 760, 130 + 588)
PANNELLO = (148, 218, 148 + 744, 218 + 492)          # l'area di testo (il pannello)
TESTO_X, TESTO_Y = 164, 234                          # dove comincia il testo (16 dentro il pannello)
CASELLA = 7.8                                        # la larghezza di una lettera
RIGA = 20

# la tastiera italiana: lettera -> (tasto, con Maiusc)
ACCENTATE = {"à": ("apostrophe", False), "è": ("bracket_left", False),
             "é": ("bracket_left", True), "ì": ("equal", False),
             "ò": ("semicolon", False), "ù": ("backslash", False)}
SEGNI = {" ": "spc", "-": "slash", "'": "minus", ".": "dot", ",": "comma", "\n": "ret"}


def scrivi(m, testo, tieni=0.03):
    """Scrive 'testo' un tasto alla volta (solo i segni della tastiera italiana che servono qui)."""
    for c in testo:
        if c in ACCENTATE:
            tasto, maiuscolo = ACCENTATE[c]
        elif c in SEGNI:
            tasto, maiuscolo = SEGNI[c], False
        elif c.isalpha() and c.isascii():
            tasto, maiuscolo = c.lower(), c.isupper()
        elif c.isdigit():
            tasto, maiuscolo = c, False
        else:
            raise ValueError(f"non so scrivere {c!r}")
        if maiuscolo:
            m.tasto_giu("shift")
        m.premi(tasto, tieni)
        if maiuscolo:
            m.tasto_su("shift")


def con(m, modificatore, tasto, volte=1):
    """Un tasto con un modificatore (shift, ctrl) tenuto giu', 'volte' volte."""
    m.tasto_giu(modificatore)
    try:
        for _ in range(volte):
            m.premi(tasto, 0.03)
    finally:
        m.tasto_su(modificatore)


def premi(m, tasto, volte=1):
    for _ in range(volte):
        m.premi(tasto, 0.03)


def centro_casella(colonna, riga):
    """Il punto in mezzo alla casella (colonna, riga) del testo, sullo schermo."""
    return round(TESTO_X + (colonna + 0.5) * CASELLA), TESTO_Y + riga * RIGA + RIGA // 2


def bordo_casella(colonna, riga):
    """Il punto sul bordo sinistro della casella (colonna, riga), a mezza altezza."""
    return round(TESTO_X + colonna * CASELLA), TESTO_Y + riga * RIGA + RIGA // 2


def quanti_blu(foto, zona=PANNELLO):
    """Quanti pixel hanno lo sfondo della selezione (con il fuoco, blu tenue) o il suo testo."""
    from collections import Counter
    c = Counter(foto.crop(zona).convert("RGB").getdata())
    return sum(n for (r, g, b), n in c.items() if (r, g, b) in BLU_TENUE)


def quanti_grigi(foto, zona=PANNELLO):
    from collections import Counter
    c = Counter(foto.crop(zona).convert("RGB").getdata())
    return sum(n for rgb, n in c.items() if rgb in GRIGIO_SELEZIONE)


BLU_TENUE = {(0xDB, 0xEA, 0xFE), (0x17, 0x25, 0x54)}           # informazione/tenue, chiaro e scuro
GRIGIO_SELEZIONE = {(0xD8, 0xD8, 0xDA), (0x2F, 0x2F, 0x33)}    # controllo/sfondo-premuto


def esegui(p):
    m = p.m
    s.apri(m, "BloccoNote")
    m.muovi(1270, 790)
    foto = m.aspetta_schermo_fermo()

    def scatta(nome):
        f = m.aspetta_schermo_fermo()
        f.crop(FINESTRA).save(p.cartella / f"bloccotesto_{nome}.png")
        return f

    # 1. documento nuovo
    nuovo = scatta("nuovo")
    # (il segnaposto e il cursore sono le uniche cose scritte: pochi pixel scuri in alto a sinistra)
    scuri = sum(1 for v in nuovo.crop((TESTO_X - 4, TESTO_Y - 4, TESTO_X + 120, TESTO_Y + 26)).convert("L").getdata() if v < 200)
    p.controlla(scuri > 40, "il segnaposto o il cursore non si vedono nel documento nuovo")

    # 2. si scrive (come nel disegno: "Lista della spesa", riga vuota, tre voci)
    scrivi(m, "Lista della spesa\n\n- pane\n- latte\n- uova")
    testo = scatta("testo")
    p.controlla(pixel_diversi(testo.crop(PANNELLO), nuovo.crop(PANNELLO), None, 40) > 300, "scrivendo il pannello non e' cambiato")

    # 3. le lettere accentate e Backspace su una lettera di due byte
    scrivi(m, "\n- caffè àèéìòù")
    accenti = scatta("accenti")
    premi(m, "backspace", 7)                      # " " + 6 accentate: la riga torna "- caffè"
    meno = scatta("accenti_cancellate")
    p.controlla(pixel_diversi(meno.crop(PANNELLO), accenti.crop(PANNELLO), None, 40) > 50,
                "Backspace non ha tolto le lettere accentate")
    premi(m, "backspace", 7)                      # "- caffè" e la riga: "\n" compreso -> torna "- uova"
    premi(m, "backspace", 1)
    dopo = scatta("dopo_backspace")
    p.controlla(pixel_diversi(dopo.crop(PANNELLO), testo.crop(PANNELLO), None, 40) < 8,
                "dopo aver cancellato tutto quello che si era aggiunto il testo non e' tornato com'era")

    # 4. selezione come nel disegno: da "latte" (riga 3, colonna 2) a "- uo" (riga 4, colonna 4), righe da 0
    premi(m, "up")
    premi(m, "home")
    premi(m, "right", 2)
    con(m, "shift", "down")
    con(m, "shift", "right", 2)
    sel = scatta("selezione")
    n_blu = quanti_blu(sel)
    p.controlla(n_blu > 800, f"la selezione blu non si vede ({n_blu} pixel)")
    # lo sfondo blu comincia alla colonna 2 della riga 3 e finisce alla colonna 4 della riga 4
    x, y = centro_casella(3, 3)
    p.controlla(sel.getpixel((x, y + 7)) in BLU_TENUE, "la casella (3, riga 3) non e' selezionata")
    x, y = centro_casella(1, 3)
    p.controlla(sel.getpixel((x, y + 8)) not in BLU_TENUE, "la casella (1, riga 3) e' selezionata (non dovrebbe)")
    x, y = centro_casella(5, 4)
    p.controlla(sel.getpixel((x, y + 8)) not in BLU_TENUE, "la casella (5, riga 4) e' selezionata (non dovrebbe)")

    # 5. scrivere sostituisce la selezione; poi si rifa' e Backspace la cancella
    scrivi(m, "X")
    sost = scatta("sostituita")
    p.controlla(quanti_blu(sost) < 100, "dopo aver scritto la selezione c'e' ancora")
    premi(m, "left")
    con(m, "shift", "left", 2)
    p.controlla(quanti_blu(scatta("selezione2")) > 150, "la selezione con Maiusc+sinistra non si vede")
    premi(m, "backspace")
    scatta("cancellata")

    # Ctrl+freccia: parola per parola, e Ctrl+Home / Ctrl+Fine con Maiusc
    con(m, "ctrl", "home")
    con(m, "ctrl", "right", 2)                    # "Lista della " -> prima di "spesa"
    con(m, "shift", "end")                        # fino a fine riga
    p.controlla(quanti_blu(scatta("fine_riga")) > 200, "Maiusc+Fine non seleziona fino a fine riga")
    m.tasto_giu("ctrl"); m.tasto_giu("shift")
    m.premi("end", 0.03)
    m.tasto_su("shift"); m.tasto_su("ctrl")
    tutto = scatta("fino_in_fondo")
    p.controlla(quanti_blu(tutto) > 700, "Ctrl+Maiusc+Fine non seleziona fino in fondo")
    con(m, "ctrl", "a")
    tuttotutto = scatta("tutto")
    p.controlla(quanti_blu(tuttotutto) >= quanti_blu(tutto), "Ctrl+A non seleziona tutto")
    premi(m, "left")                              # chiude la selezione in testa

    # 6. Tab: un carattere, largo fino alla colonna multipla di 4
    con(m, "ctrl", "home")
    premi(m, "tab")
    tab = scatta("tab")
    con(m, "ctrl", "a")
    premi(m, "tab")                               # tutte le righe rientrano di un '\t'
    rientro = scatta("rientro")
    p.controlla(pixel_diversi(rientro.crop(PANNELLO), tab.crop(PANNELLO), None, 40) > 300,
                "Tab con tutto selezionato non ha rientrato le righe")
    con(m, "shift", "tab")                        # toglie un rientro a ogni riga
    meno_rientro = scatta("rientro_tolto")
    premi(m, "left")
    con(m, "shift", "tab")                        # la riga del cursore: toglie il '\t' di prima
    senza = scatta("senza_rientro")
    p.controlla(pixel_diversi(meno_rientro.crop(PANNELLO), rientro.crop(PANNELLO), None, 40) > 300,
                "Maiusc+Tab non ha tolto il rientro alle righe selezionate")
    p.controlla(pixel_diversi(senza.crop(PANNELLO), meno_rientro.crop(PANNELLO), None, 40) > 20,
                "Maiusc+Tab non ha tolto il rientro della riga del cursore")

    # 7. una riga lunga va a capo (a parola): 91 caselle per riga
    con(m, "ctrl", "end")
    scrivi(m, "\n" + "parola " * 20 + "e poi ancora un pezzo di frase che continua oltre la fine della riga dell'area")
    capo = scatta("a_capo")
    p.controlla(pixel_diversi(capo.crop(PANNELLO), senza.crop(PANNELLO), None, 40) > 500, "la riga lunga non e' stata scritta")

    # su e giu' tra le righe a capo automatico, Home e Fine di una riga visiva (il cursore a fine
    # riga resta in fondo alla riga di sopra, non salta all'inizio della prossima)
    premi(m, "up")
    scatta("capo_su")
    premi(m, "end")
    scatta("capo_fine")
    premi(m, "home")
    scatta("capo_home")
    premi(m, "down")
    scatta("capo_giu")
    con(m, "ctrl", "end")

    # 8. testo piu' lungo dell'area: scorre seguendo il cursore
    scrivi(m, "\n")
    for i in range(1, 31):
        scrivi(m, f"riga {i}\n", 0.02)
    giu = scatta("scorso")
    con(m, "ctrl", "home")
    su = scatta("in_cima")
    p.controlla(pixel_diversi(giu.crop(PANNELLO), su.crop(PANNELLO), None, 40) > 1000, "Ctrl+Home non ha riportato in cima")
    premi(m, "pgdn", 2)
    scatta("pagina_giu")
    m.muovi(*centro_casella(10, 5))
    m.rotella(-3)
    time.sleep(0.4)
    scatta("rotella")

    # 9. il mouse: clic, trascinamento, doppio clic. Si torna in cima
    con(m, "ctrl", "home")
    scatta("cima2")
    m.clic(*bordo_casella(3, 0))                  # "Lis|ta": il cursore va li'
    m.muovi(1270, 790)
    scatta("clic")
    m.trascina(*bordo_casella(3, 2), *bordo_casella(5, 3))
    m.muovi(1270, 790)
    trascinata = scatta("trascinata")
    p.controlla(quanti_blu(trascinata) > 500, "trascinando col mouse non si e' selezionato")
    m.clic(*centro_casella(2, 0))
    m.clic(*centro_casella(2, 0))                 # doppio clic: la parola "Lista"
    m.muovi(1270, 790)
    doppio = scatta("doppio_clic")
    p.controlla(quanti_blu(doppio) > 500, "il doppio clic non ha selezionato la parola")

    # 10. senza il fuoco: un clic su un pulsante della barra, e la selezione diventa grigia
    m.clic(164, 194)                              # il pulsante Nuovo (non fa niente, ma prende il clic)
    m.muovi(1270, 790)
    senza_fuoco = scatta("senza_fuoco")
    p.controlla(quanti_grigi(senza_fuoco) > 300, "senza fuoco la selezione non e' grigia")
    p.controlla(quanti_blu(senza_fuoco) < 50, "senza fuoco la selezione e' ancora blu")
    p.nota("foto del Blocco note in " + str(p.cartella))
