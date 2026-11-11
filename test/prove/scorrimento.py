"""scorrimento: la rotella fa scorrere la tabella dei processi di Attivita, e la barra di
scorrimento compare, si allarga sotto il puntatore, si trascina e poi sparisce.

Si aprono Contatore, Esempio e Impostazioni, e per ultima Attivita (quarto posto, angolo
in 260,250): con i 5 processi del sistema le righe sono 9, e nella tabella ne stanno 8.
Le righe sono alte 30 pixel; nella vista ne entrano 244, il contenuto ne e' alto 272:
si puo' scorrere di 28 pixel. La rotella accelera come sul Mac (stile.h): uno scatto da
solo fa 10 pixel; due scatti di fila, veloci, 10 + 20, cioe' (fermandosi in fondo) 28.

Dove sono le cose, dall'angolo della finestra di Attivita:
  - la tabella: x 20..620, y 108..390; la testata e' alta 34, le righe cominciano a y 144;
  - la striscia della barra di scorrimento: x 606..620, y 142..390, sopra le righe.

Le righe si confrontano solo nella colonna dei numeri dei processi: lo stato e la
memoria li aggiorna Attivita ogni secondo, e possono cambiare da una foto all'altra.

Cosa si controlla:
  1. ROTELLA IN BASSO, uno scatto da solo, con il puntatore sopra la tabella: le righe si
     spostano di 10 pixel esatti, e nella striscia compare la barra;
  2. dopo l'ultimo scatto la barra resta (almeno un secondo) e poi sparisce: la striscia
     torna com'era. Le righe restano dove sono;
  3. ROTELLA IN SU: le righe tornano al loro posto, e sparita la barra la striscia e' di
     nuovo identica, pixel per pixel, a prima di scorrere (a riposo la barra non lascia
     segni);
  4. UNA RAFFICA: due scatti veloci spostano le righe piu' di due scatti da soli (20
     pixel): fino in fondo, 28. Due scatti veloci in su le riportano in cima;
  5. TRASCINARE: uno scatto fa ricomparire la barra; il puntatore sopra il cursore lo
     allarga, e finche' resta sopra la barra non svanisce (anche dopo il secondo);
     premuto e trascinato in giu', le righe scorrono come con la rotella;
  6. CLIC DOPO LO SCORRIMENTO: scorse le righe in fondo, un clic a y 340 cade sulla riga
     7, Impostazioni (senza scorrere sarebbe la 6, Esempio). "Termina processo" deve
     chiudere proprio Impostazioni.
Le foto 'visibile', 'sopra' e 'trascinata' mostrano la barra nei suoi stati.
"""

import time

import scrivania as s
from qemu import pixel_diversi

NOME = "scorrimento"
ORDINE = 65
DESCRIZIONE = "Attivita: la rotella scorre i processi; la barra compare, si trascina, sparisce"

STRISCIA = (606, 143, 620, 388)       # la barra di scorrimento (dentro la finestra)
RIGHE = (21, 144, 100, 386)           # le righe, ma solo la colonna dei numeri: le altre
                                      # (stato, memoria) Attivita le aggiorna da sola
DENTRO = (300, 250)                   # un punto sopra le righe, lontano dalla barra
CURSORE = (613, 253)                  # il centro del cursore, quando si e' in cima
RIGA_IMPOSTAZIONI = 340               # la y della riga 7 quando si e' scorso in fondo
SCATTO = 10                           # i pixel di uno scatto da solo (SCORRIMENTO_RIGA)
IN_FONDO = 28                         # fin dove si puo' scorrere


def zona(x, y, r):
    return (x + r[0], y + r[1], x + r[2], y + r[3])


def uguali(a, b, r):
    return pixel_diversi(a.crop(r), b.crop(r), None, 6) == 0


def scritte(foto, r):
    """Dove c'e' scritto qualcosa nella zona: i pixel chiari (il testo e' bianco). Lo
    sfondo della tabella e' di vetro e lascia vedere lo sfondo della scrivania, che non
    scorre: per questo si confrontano solo le scritte."""
    return foto.crop(r).convert("L").point(lambda v: 255 if v > 160 else 0)


def spostamento(dopo, prima, r):
    """Di quanti pixel le righe di 'dopo' sono salite rispetto a 'prima': la d (da 0 a 40)
    per cui le scritte di 'dopo' sono quelle di 'prima' spostate in su di d (a meno di
    qualche pixel sul bordo delle lettere). None se nessuna d va bene."""
    a, b = scritte(dopo, r), scritte(prima, r)
    l, h = a.size
    diversi = {d: pixel_diversi(a.crop((0, 0, l, h - d)), b.crop((0, d, l, h)), None, 0)
               for d in range(41)}
    d = min(diversi, key=diversi.get)
    return d if diversi[d] <= 20 else None


def esegui(p):
    m = p.m
    for app in ("Contatore", "Esempio", "Impostazioni", "Attivita"):
        s.apri(m, app)
    x, y = s.angolo(3)
    striscia, righe = zona(x, y, STRISCIA), zona(x, y, RIGHE)

    # il puntatore sopra le righe; aspettare che Attivita sia ferma (si aggiorna ogni
    # secondo: i numeri della memoria possono cambiare, quindi si confrontano solo le
    # zone che servono, e prima di ogni confronto si rifa' la foto "di prima")
    m.muovi(x + DENTRO[0], y + DENTRO[1])
    prima = m.aspetta_schermo_fermo()

    # --- 1. la rotella in basso, uno scatto da solo ---
    m.rotella(1)
    inizio = time.time()
    visibile = m.aspetta_che(lambda f: not uguali(f, prima, striscia), 2.0,
                             cosa="la barra di scorrimento comparire")
    m.foto("visibile")
    p.controlla(not uguali(visibile, prima, righe), "la rotella non ha spostato le righe")
    scorsa = m.aspetta_che(lambda f: spostamento(f, prima, righe) not in (0, None), 2.0,
                           cosa="le righe spostarsi")
    d = spostamento(scorsa, prima, righe)
    p.controlla(d == SCATTO, f"uno scatto da solo ha spostato le righe di {d} pixel, non {SCATTO}")
    p.nota(f"uno scatto da solo: {d} pixel")

    # --- 2. la barra resta un secondo, poi sparisce ---
    sparita = m.aspetta_che(lambda f: uguali(f, prima, striscia), 4.0, pausa=0.1,
                            cosa="la barra di scorrimento sparire")
    dopo = time.time() - inizio
    p.controlla(dopo >= 0.9, f"la barra e' sparita troppo presto ({dopo:.2f} s)")
    p.nota(f"barra sparita dopo {dopo:.2f} s")
    p.controlla(not uguali(sparita, prima, righe), "sparita la barra, le righe sono tornate su")

    # --- 3. la rotella in su: tutto come prima ---
    m.rotella(-1)
    m.aspetta_che(lambda f: not uguali(f, prima, striscia), 2.0, cosa="la barra ricomparire")
    m.aspetta_che(lambda f: uguali(f, prima, striscia) and uguali(f, prima, righe), 4.0,
                  pausa=0.1, cosa="le righe e la striscia tornare identiche a prima di scorrere")

    # --- 4. una raffica: due scatti veloci scorrono piu' di due scatti da soli ---
    m.rotella(2, pausa=0.02)          # uno ogni 40 ms: ben sotto la pausa che chiude la raffica
    in_fondo = m.aspetta_che(lambda f: spostamento(f, prima, righe) == IN_FONDO, 3.0,
                             cosa=f"due scatti veloci portare le righe in fondo ({IN_FONDO} "
                                  f"pixel, piu' dei {2 * SCATTO} di due scatti da soli)")
    p.nota(f"due scatti veloci: {IN_FONDO} pixel (in fondo)")
    m.rotella(-2, pausa=0.02)
    m.aspetta_che(lambda f: uguali(f, prima, striscia) and uguali(f, prima, righe), 4.0,
                  pausa=0.1, cosa="due scatti veloci in su riportare le righe in cima")

    # --- 5. trascinare il cursore ---
    m.rotella(-1)                     # in cima gia': non scorre, ma la barra si mostra
    m.aspetta_che(lambda f: not uguali(f, prima, striscia), 2.0, cosa="la barra ricomparire")
    m.muovi(x + CURSORE[0], y + CURSORE[1])
    time.sleep(0.4)                   # il cursore si allarga in 150 ms (su ARM emulato
    precedente = [m.foto()]           # anche di piu': si aspetta che sia fermo)

    def ferma(f):
        uguale = uguali(f, precedente[0], striscia)
        precedente[0] = f
        return uguale
    m.aspetta_che(ferma, 3.0, pausa=0.2, cosa="il cursore finire di allargarsi")
    sopra = m.foto("sopra")
    p.controlla(not uguali(sopra, prima, striscia), "con il puntatore sopra la barra e' sparita")
    time.sleep(1.2)                   # passato il secondo: con il puntatore sopra resta
    p.controlla(uguali(m.foto("ancora_sopra"), sopra, striscia),
                "con il puntatore sopra, dopo un secondo la barra e' cambiata (svanisce?)")
    m.pulsante(True)
    time.sleep(0.2)
    for i in range(1, 11):
        m.muovi(x + CURSORE[0], y + CURSORE[1] + 4 * i)
        time.sleep(0.05)
    time.sleep(0.3)
    trascinata = m.foto("trascinata")
    m.pulsante(False)
    p.controlla(not uguali(trascinata, sopra, striscia),
                "trascinata, la barra e' uguale a quando ci si era solo sopra")
    p.controlla(uguali(trascinata, in_fondo, righe),
                "trascinato il cursore in fondo, le righe non sono come scorse con la rotella")
    m.muovi(x + DENTRO[0], y + DENTRO[1])
    m.aspetta_che(lambda f: uguali(f, prima, striscia), 4.0, cosa="la barra sparire dopo il trascinamento")

    # --- 6. un clic sulla riga dopo lo scorrimento ---
    m.clic(x + DENTRO[0], y + RIGA_IMPOSTAZIONI)
    time.sleep(0.3)
    m.clic(x + 533, y + 421)          # "Termina processo"
    m.muovi(x + DENTRO[0], y + DENTRO[1])
    m.aspetta_che(lambda f: s.segno_sotto(f, "Impostazioni") == 0, 5.0,
                  cosa="Impostazioni chiusa (il clic dopo lo scorrimento ha scelto un'altra riga?)")
    f = m.foto("terminata")
    p.controlla(s.segno_sotto(f, "Esempio") > 0, "e' stata chiusa Esempio invece di Impostazioni")
    p.controlla(s.segno_sotto(f, "Contatore") > 0, "e' stata chiusa Contatore")
