"""finestre_molte: con tante app aperte il compositore resta in piedi.

Prima il compositore guardava i suoi descrittori con un elenco di al massimo 16 posti; ne
servivano fino a 4 + 8 finestre + 8 porte, e con molte app l'attesa falliva (-1) e il
compositore usciva: init lo faceva ripartire e tutte le finestre si perdevano. Ora l'elenco
non ha un massimo suo: lo dimensiona il compositore sul suo massimo vero.
  1. Si aprono dalla userbar tutte le app tranne Doom: 6 app, cioe' 6 finestre piu' le 2 della
     scrivania (lo sfondo e la barra) = 8 finestre, il massimo, e 6 porte. Fanno 18 descrittori
     da guardare: col vecchio limite (16) il compositore sarebbe caduto. Ogni app e' segnata
     "aperta" nella userbar e la sua barra del titolo si vede dove la mette il compositore.
  2. Si apre anche Doom: e' la nona finestra, oltre il massimo. Il compositore la rifiuta e lo
     scrive nel registro ("troppe finestre: collegamento rifiutato"), ma non esce: la userbar
     c'e' ancora e il registro non dice che init l'abbia fatto ripartire.
"""

import time

import scrivania as s

NOME = "finestre_molte"
ORDINE = 21
DESCRIZIONE = "6 app + scrivania = 8 finestre e 6 porte (18 descrittori): il compositore resta; la nona finestra e' rifiutata"

# Doom, rifiutato dal compositore, scrive "non riesco ad aprire la finestra": e' quello che si vuole
PREVISTE = ["non riesco"]

APPS = ["Attivita", "BloccoNote", "Esempio", "Files", "Impostazioni", "Store"]


def esegui(p):
    m = p.m
    for app in APPS:
        s.apri(m, app, attesa=30.0)
    f = m.aspetta_schermo_fermo()
    m.foto("sei_app")
    for app in APPS:
        p.controlla(s.segno_sotto(f, app) > 0, f"{app} non risulta aperta nella userbar")
    p.controlla(s.userbar_visibile(f), "con sei app aperte la userbar non si vede piu'")
    seriale = m.testo_seriale()
    p.controlla("troppe finestre" not in seriale, "il compositore ha rifiutato una finestra prima del massimo")

    # la nona finestra: rifiutata, e il compositore resta
    s.clic_app(m, "Doom")
    deadline = time.time() + 30.0
    while "troppe finestre: collegamento rifiutato" not in m.testo_seriale() and time.time() < deadline:
        time.sleep(0.5)
    seriale = m.testo_seriale()
    p.controlla("compositore: troppe finestre: collegamento rifiutato" in seriale,
                "oltre il massimo il compositore non ha scritto il rifiuto nel registro")
    time.sleep(2.0)
    f = m.aspetta_schermo_fermo()
    m.foto("oltre_il_massimo")
    p.controlla(s.userbar_visibile(f), "oltre il massimo la userbar non c'e' piu' (il compositore e' caduto?)")
    for app in APPS:
        p.controlla(s.segno_sotto(f, app) > 0, f"{app} non e' piu' aperta dopo il rifiuto della nona finestra")
    p.controlla("lo faccio ripartire" not in seriale, "init ha fatto ripartire un servizio")
