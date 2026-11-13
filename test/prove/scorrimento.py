"""scorrimento: la rotella fa scorrere la tabella dei processi di Attivita, e la barra di
scorrimento compare, si allarga sotto il puntatore, si trascina e poi sparisce.

I PROCESSI IN PIU'. Questa prova accende il sistema con il disco delle prove
(DISCO = "disco_prove.img", vedi collaudo.py e build.sh): init fa partire anche 6
"riempitivi" (da /data/tests), processi che dormono e basta (righe 5..10, subito dopo
quelli del sistema), e la tabella ha abbastanza righe da traboccare. Le applicazioni si aprono una sola volta
ciascuna: senza i riempitivi le righe sarebbero 9, e nella tabella ne stanno 9 giuste.

Si aprono Files, Esempio e Impostazioni, e per ultima Attivita (quarto posto, angolo
in 260,250). Le righe sono 15: 0..4 il sistema (init, eventi, compositore, launcher,
scrivania), 5..10 i riempitivi, 11 Files, 12 Esempio, 13 Impostazioni, 14 Attivita.
Sono alte 40 pixel piu' 4 di spazio (passo 44; la prima ha il centro a 150 dall'angolo della
finestra); la vista e' alta 348 (8 righe), il contenuto 15 * 44 - 4 = 656: si puo' scorrere
di 308 pixel. La
rotella accelera come sul Mac (stile.h): uno scatto da solo fa 10 pixel; due scatti di
fila, veloci, 10 + 20 = 30.

Dove sono le cose, dall'angolo della finestra di Attivita (il tema e' chiaro: il testo e'
scuro, si guardano i pixel scuri):
  - la tabella (nel pannello, da x 16, y 97): la testata a y 113 (il puntatore sta li': la
    rotella vale lo stesso e nessuna riga si illumina), le righe cominciano a y 130;
  - la striscia della barra di scorrimento: x 746..760, y 130..478, nel padding destro del
    pannello.

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
     pixel): 30. Due scatti veloci in su le riportano in cima. Poi tanti scatti portano
     le righe in fondo (308 pixel, non di piu'), e tanti in su di nuovo in cima;
  5. TRASCINARE: uno scatto fa ricomparire la barra; il puntatore sopra il cursore lo
     allarga, e finche' resta sopra la barra non svanisce (anche dopo il secondo);
     premuto e trascinato fino in fondo, le righe sono come scorse in fondo con la rotella;
  6. CLIC DOPO LO SCORRIMENTO: scorse le righe in fondo, un clic a y 414 cade sulla riga
     13, Impostazioni (senza scorrere sarebbe la 4, scrivania). "Termina processo" (la
     X nella barra degli strumenti) deve chiudere proprio Impostazioni.
Le foto 'visibile', 'sopra' e 'trascinata' mostrano la barra nei suoi stati.
"""

import time

import scrivania as s
from qemu import pixel_diversi

NOME = "scorrimento"
ORDINE = 65
DESCRIZIONE = "Attivita: la rotella scorre i processi; la barra compare, si trascina, sparisce"
DISCO = "disco_prove.img"             # con i riempitivi: vedi sopra

STRISCIA = (746, 130, 760, 478)       # la barra di scorrimento (dentro la finestra)
RIGHE = (24, 130, 80, 478)           # le righe, ma solo la colonna dei numeri: le altre
                                      # (stato, memoria) Attivita le aggiorna da sola
DENTRO = (300, 113)                   # un punto sulla testata della tabella: la rotella vale
                                      # lo stesso, e nessuna riga si illumina sotto il puntatore
CURSORE = (753, 224)                  # il centro del cursore, quando si e' in cima
RIGA_IMPOSTAZIONI = 414               # la y della riga 13 quando si e' scorso in fondo
SCATTO = 10                           # i pixel di uno scatto da solo (SCORRIMENTO_RIGA)
RAFFICA = 30                          # due scatti veloci: 10 + 20
IN_FONDO = 308                        # fin dove si puo' scorrere: il contenuto meno la vista
ULTIMA_RIGA = 463                     # dove finisce il testo dell'ultima riga (y dall'angolo della finestra) quando si e' in fondo
TERMINA = (611, 61)                   # "Termina processo", la X nella barra degli strumenti


def zona(x, y, r):
    return (x + r[0], y + r[1], x + r[2], y + r[3])


def uguali(a, b, r):
    return pixel_diversi(a.crop(r), b.crop(r), None, 6) == 0


def scritte(foto, r):
    """Dove c'e' scritto qualcosa nella zona: i pixel scuri (il tema e' chiaro, il testo
    e' grigio scuro). Lo sfondo della tabella e' di vetro e lascia vedere lo sfondo della
    scrivania, che non scorre: per questo si confrontano solo le scritte."""
    return foto.crop(r).convert("L").point(lambda v: 255 if v < 150 else 0)


def fine_testo(foto, r):
    """La y (dentro la zona r) dove finisce l'ultima scritta della colonna dei numeri."""
    bn = scritte(foto, r)
    ys = [y for y in range(bn.size[1]) if any(bn.getpixel((x, y)) for x in range(bn.size[0]))]
    return ys[-1] if ys else None


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


def salite_di(dopo, prima, r, d):
    """Vero se le righe di 'dopo' sono quelle di 'prima' salite di d pixel esatti (come
    spostamento, per una d sola, anche grande: si confronta il pezzo che resta in vista
    tutte e due le volte)."""
    a, b = scritte(dopo, r), scritte(prima, r)
    l, h = a.size
    return pixel_diversi(a.crop((0, 0, l, h - d)), b.crop((0, d, l, h)), None, 0) <= 20


def esegui(p):
    m = p.m
    for app in ("Files", "Esempio", "Impostazioni", "Attivita"):
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
    m.aspetta_che(lambda f: spostamento(f, prima, righe) == RAFFICA, 3.0,
                  cosa=f"due scatti veloci spostare le righe di {RAFFICA} pixel "
                       f"(piu' dei {2 * SCATTO} di due scatti da soli)")
    p.nota(f"due scatti veloci: {RAFFICA} pixel")
    m.rotella(-2, pausa=0.02)
    m.aspetta_che(lambda f: uguali(f, prima, striscia) and uguali(f, prima, righe), 4.0,
                  pausa=0.1, cosa="due scatti veloci in su riportare le righe in cima")

    # in fondo: 45 scatti fanno almeno 450 pixel (anche se Attivita, su una macchina
    # emulata lenta, li vedesse tutti da soli, 10 pixel ciascuno), piu' dei 308 che si
    # possono scorrere: le righe devono essere salite di 308 esatti, non di piu'
    # (la vista e' piu' bassa del contenuto: fra la foto in cima e quella in fondo non
    # c'e' piu' niente in comune, quindi si controlla dove arriva l'ultima riga)
    m.rotella(45, pausa=0.02)
    m.aspetta_che(lambda f: fine_testo(f, righe) is not None
                  and abs(fine_testo(f, righe) - (ULTIMA_RIGA - RIGHE[1])) <= 1, 8.0,
                  cosa=f"tanti scatti portare le righe in fondo ({IN_FONDO} pixel)")
    in_fondo = m.aspetta_schermo_fermo()
    m.foto("in_fondo")
    m.rotella(-45, pausa=0.02)
    m.aspetta_che(lambda f: uguali(f, prima, striscia) and uguali(f, prima, righe), 8.0,
                  pausa=0.1, cosa="tanti scatti in su riportare le righe in cima")

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
    for i in range(1, 13):            # 180 pixel: piu' dei 160 circa che il cursore puo' fare
        m.muovi(x + CURSORE[0], y + CURSORE[1] + 15 * i)
        time.sleep(0.05)
    # Attivita ridisegna dopo ogni spostamento del puntatore: su una macchina emulata
    # resta un po' indietro. Si aspetta che arrivi in fondo (con il pulsante ancora giu')
    m.aspetta_che(lambda f: uguali(f, in_fondo, righe), 3.0,
                  cosa="trascinato il cursore in fondo, le righe come scorse in fondo con la rotella")
    trascinata = m.foto("trascinata")
    m.pulsante(False)
    p.controlla(not uguali(trascinata, sopra, striscia),
                "trascinata, la barra e' uguale a quando ci si era solo sopra")
    m.muovi(x + DENTRO[0], y + DENTRO[1])
    m.aspetta_che(lambda f: uguali(f, prima, striscia), 4.0, cosa="la barra sparire dopo il trascinamento")

    # --- 6. un clic sulla riga dopo lo scorrimento ---
    m.clic(x + DENTRO[0], y + RIGA_IMPOSTAZIONI)
    time.sleep(0.3)
    m.clic(x + TERMINA[0], y + TERMINA[1])
    m.muovi(x + DENTRO[0], y + DENTRO[1])
    m.aspetta_che(lambda f: s.segno_sotto(f, "Impostazioni") == 0, 5.0,
                  cosa="Impostazioni chiusa (il clic dopo lo scorrimento ha scelto un'altra riga?)")
    f = m.foto("terminata")
    p.controlla(s.segno_sotto(f, "Esempio") > 0, "e' stata chiusa Esempio invece di Impostazioni")
    p.controlla(s.segno_sotto(f, "Files") > 0, "e' stata chiusa Files")
