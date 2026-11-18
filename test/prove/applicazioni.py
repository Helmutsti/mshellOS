"""applicazioni: ogni applicazione si apre dalla userbar; il vassoio delle app.

Si cliccano una dopo l'altra le applicazioni della userbar, tranne Doom (pesante, e la sua
finestra non e' di UIKit), con Files per ultima (la sua finestra, 800 x 480, al quarto
posto sta ancora tutta nello schermo). Per ognuna:
  - a sinistra della sua icona deve comparire il segno "aperta" (il puntino o la lineetta);
  - la sua finestra deve comparire dove la mette il compositore (a scala: la prima in
    (140, 130), le altre 40 pixel piu' in basso e a destra), cioe' quella zona dello
    schermo deve cambiare rispetto a prima.
Alla fine il pulsante App (la griglia in cima alla userbar) apre il vassoio delle app; un
clic su Esempio nel vassoio lo chiude e riporta davanti Esempio (gia' aperta). Poi il
vassoio si riapre e un clic sullo sfondo lo chiude.
Il launcher, per ogni app aperta, scrive nel registro cosa ha nel suo recinto
(APPLICAZIONI.md, "I permessi sulle cartelle"): il pacchetto e i dati sempre; Files
(files.write) la Home in lettura e scrittura, Attivita', Esempio e Impostazioni niente Home;
gli sfondi solo Impostazioni (settings.system); la rete del computer solo Esempio
(network.use). E a ogni app consegna la sua porta verso il
compositore (gkit/sistema/protocollo.h, LA PORTA DELL'APP): il compositore lo scrive nel registro.
"""

import time

import scrivania as s
from qemu import PUNTO_VUOTO

NOME = "applicazioni"
ORDINE = 20
DESCRIZIONE = "ogni applicazione si apre dalla userbar; il vassoio delle app apre, si chiude"

# La parte sinistra del vassoio (fino a x 112): li' sotto ci sono solo lo sfondo e la
# userbar, mai una finestra o la sua ombra (la prima finestra comincia a x 140, la sua
# ombra 56 pixel prima). Basta per sapere se il vassoio c'e'.
VASSOIO_SINISTRA = (s.VASSOIO_RETTANGOLO[0], s.VASSOIO_RETTANGOLO[1], 112, s.VASSOIO_RETTANGOLO[3])


def esegui(p):
    m = p.m
    prima = p.scrivania
    for posto, app in enumerate(["Attivita", "Esempio", "Impostazioni", "Files"]):
        p.controlla(s.segno_sotto(prima, app) == 0, f"{app} risulta gia' aperta nella userbar")
        dopo = s.apri(m, app)
        x, y = s.angolo(posto)
        l, a = s.MISURE[app]
        # la barra del titolo della nuova finestra: deve essere cambiata
        barra = (x, y, x + l, y + s.BARRA)
        p.controlla(not s.zona_uguale(prima, dopo, barra),
                    f"{app}: la finestra non compare in ({x}, {y})")
        m.foto(f"aperta_{app}")
        prima = dopo

    # il vassoio: si apre con il pulsante App, a destra della userbar
    m.clic(*s.APP_PULSANTE)
    m.muovi(*PUNTO_VUOTO)
    s.aspetta_zona(m, prima, VASSOIO_SINISTRA, uguale=False, cosa="il vassoio delle app")
    aperto = m.aspetta_schermo_fermo()
    m.foto("vassoio")
    p.controlla(s.userbar_visibile(aperto), "con il vassoio aperto la userbar non si vede piu'")
    # un clic su Esempio (gia' aperta) nel vassoio: il vassoio si chiude, Esempio torna davanti
    m.clic(*s.VASSOIO["Esempio"])
    m.muovi(*PUNTO_VUOTO)
    x, y = s.angolo(1)
    s.aspetta_zona(m, aperto, (x, y, x + s.MISURE["Esempio"][0], y + s.BARRA), uguale=False,
                   cosa="Esempio tornare davanti dal vassoio")
    chiuso = m.aspetta_schermo_fermo()
    p.controlla(s.zona_uguale(chiuso, prima, VASSOIO_SINISTRA),
                "dopo il clic nel vassoio, il vassoio non si e' chiuso")
    # di nuovo aperto, e un clic sullo sfondo (in basso a destra) lo chiude
    m.clic(*s.APP_PULSANTE)
    m.muovi(*PUNTO_VUOTO)
    s.aspetta_zona(m, chiuso, VASSOIO_SINISTRA, uguale=False, cosa="il vassoio riaperto")
    m.clic(1200, 760)
    m.muovi(*PUNTO_VUOTO)
    s.aspetta_zona(m, chiuso, VASSOIO_SINISTRA, uguale=True, cosa="il vassoio chiuso dallo sfondo")

    # il recinto di ogni app, come lo scrive il launcher: il pacchetto, i dati, la Home secondo
    # files.*, gli sfondi solo con settings.system, la rete solo con network.use
    seriale = m.testo_seriale()
    righe = {r.split(": nel recinto ", 1)[0][len("launcher: "):]: r for r in seriale.splitlines()
             if r.startswith("launcher: ") and ": nel recinto " in r}
    for app, cose in [("files", ["Home in lettura e scrittura (files.write)", "una rete vuota"]),
                      ("attivita", ["niente Home", "una rete vuota"]),
                      ("esempio", ["niente Home", "la rete del computer (network.use)"]),
                      ("impostazioni", ["niente Home", "gli sfondi (settings.system)", "una rete vuota"])]:
        riga = righe.get(app, "")
        p.controlla(f"il pacchetto /system/apps/{app}, i dati /data/users/1000/appsdata/{app}," in riga,
                    f"manca nel registro il recinto di {app} (pacchetto e dati)")
        for cosa in cose:
            p.controlla(cosa in riga, f"nel recinto di {app} manca '{cosa}'")
    p.controlla("sfondi" not in righe.get("files", "x sfondi"), "Files ha avuto gli sfondi")
    # la porta verso il compositore, data dal launcher a ogni app
    for app in ["attivita", "esempio", "impostazioni", "files"]:
        p.controlla(f"compositore: porta dell'app /system/apps/{app}/code" in seriale,
                    f"il compositore non ha ricevuto la porta di {app}")
    p.controlla("niente porta verso il compositore" not in seriale, "un'app e' partita senza porta")
    # la porta verso catalog: Esempio chiede il tema, e catalog sa che e' Esempio dalla porta
    p.controlla("niente porta verso catalog" not in seriale, "un'app e' partita senza la porta verso catalog")
    p.controlla("catalog: collegamento dalla porta di /system/apps/esempio/code" in seriale,
                "catalog non ha ricevuto il collegamento di Esempio dalla sua porta")
    p.controlla("esempio: tema da catalog: light" in seriale, "Esempio non ha avuto il tema da catalog")
    # le porte date a ogni app, una riga per app: network solo a chi ha network.use (Esempio),
    # picker a tutte (e' di base)
    p.controlla("launcher: attivita: porte compositore catalog launcher picker tasks wayland\n" in seriale,
                "Attivita non ha avuto le porte compositore, catalog, launcher, picker, tasks e Wayland (e basta)")
    p.controlla("launcher: esempio: porte compositore catalog launcher network picker wayland\n" in seriale,
                "Esempio non ha avuto le porte compositore, catalog, launcher, network, picker e Wayland")
    # la porta verso network: Esempio chiede lo stato della rete, e network sa che e' Esempio
    p.controlla("network: collegamento dalla porta di /system/apps/esempio/code" in seriale,
                "network non ha ricevuto il collegamento di Esempio dalla sua porta")
    p.controlla("esempio: rete risponde\n" in seriale, "Esempio non ha avuto risposta dalla rete")
    # il controllo vero dei permessi: Esempio non ha settings.system ne' network.settings
    p.controlla("esempio: cambiare il tema: negato\n" in seriale, "a Esempio non e' stato negato di cambiare il tema")
    p.controlla("esempio: riavviare la rete: negato\n" in seriale, "a Esempio non e' stato negato di riavviare la rete")
    # la porta verso tasks: solo a chi ha processes.view o processes.stop (Attivita'), non a Esempio
    p.controlla("tasks: collegamento dalla porta di /system/apps/attivita/code" in seriale,
                "tasks non ha ricevuto il collegamento di Attivita dalla sua porta")
    p.controlla("launcher: esempio: porte" in seriale and " tasks" not in
                seriale.split("launcher: esempio: porte", 1)[1].split("\n", 1)[0],
                "Esempio ha avuto la porta verso tasks")
    # Esempio chiede l'elenco dei processi senza porta (per nome): nel recinto gli indirizzi dei
    # servizi non ci sono, quindi tasks non si raggiunge. Solo dove le app hanno il recinto: nella
    # sandbox partono senza (decisione della persona, strumenti/sandbox/recinto_sandbox.c)
    if m.recinto:
        p.controlla("esempio: elenco dei processi: irraggiungibile\n" in seriale,
                    "Esempio, nel recinto, ha raggiunto tasks per nome")
    # la connessione Wayland, data dal launcher a ogni app: il compositore sa di quale app e'
    for app in ["attivita", "esempio", "impostazioni", "files"]:
        p.controlla(f"compositore: connessione Wayland dell'app /system/apps/{app}/code" in seriale,
                    f"il compositore non ha ricevuto la connessione Wayland di {app}")
    p.controlla("niente connessione Wayland" not in seriale, "un'app e' partita senza connessione Wayland")
    for permesso, servizio in [("settings.system", "catalog"), ("network.settings", "network")]:
        p.controlla(f"permesso {permesso} a /system/apps/esempio/code (uid 1000): negato" in seriale,
                    f"{servizio}: manca nel registro il rifiuto di {permesso} a Esempio")


DISCO = "disco_app.img"   # con Esempio e Contatore, le app di prova (utente/prove/applicazioni): non sono nel disco normale
