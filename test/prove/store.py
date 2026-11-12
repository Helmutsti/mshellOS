"""store: lo Store, dall'apertura di contatore.app fino a toglierla.

Come farebbe una persona, sul disco normale (contatore.app sta sulla Scrivania della
persona, build.sh):
  - Files dall'isola (primo posto, angolo in (140, 130)); nella barra laterale la Scrivania,
    e il doppio clic su contatore.app (il primo elemento): il launcher fa partire lo Store
    con il file, e lo Store (secondo posto, angolo in (180, 170), 600 x 600) mostra la
    richiesta di installazione ("store: richiesta di installazione: contatore");
  - "Installa": il launcher la installa, lo Store dice "Contatore e' installata";
  - "Apri": parte Contatore (al posto dello Store, che si chiude); la si chiude;
  - di nuovo il doppio clic su contatore.app: ora e' gia' installata, con la stessa versione
    ("store: gia' installata: contatore"); "Chiudi";
  - lo Store dall'isola: l'elenco, con Contatore tra le tue app (7 app, 1 tua); un clic su
    Contatore mostra il dettaglio;
  - "Svuota dati" e la conferma: il launcher svuota i dati;
  - "Togli app" e "Togli app e dati": il launcher la toglie, e l'elenco torna a 6 app.
Ogni passo si controlla nella seriale (lo Store e il launcher scrivono cosa fanno) e se ne fa
una foto. I posti dei pulsanti sono presi dallo schermo vero (1280x800).
"""

import time

import scrivania as s
from qemu import PUNTO_VUOTO

NOME = "store"
ORDINE = 85
DESCRIZIONE = "Store: richiesta di installazione, installa, apri, gia' installata, elenco, svuota dati, togli"

FILES = s.angolo(0)
SCRIVANIA_FILES = (FILES[0] + 72, FILES[1] + 177)      # la Scrivania nella barra laterale di Files
PACCHETTO = (FILES[0] + 263, FILES[1] + 133)           # contatore.app, il primo elemento

# La richiesta di installazione: lo Store nel secondo posto, i pulsanti in basso a destra
INSTALLA = (699, 726)           # "Installa" (con "Annulla" a sinistra), in fondo a destra
APRI, CHIUDI = (690, 726), (578, 726)   # "Apri Contatore" e, a sinistra, "Chiudi"
CHIUDI_CONTATORE = (393, 190)   # la X di Contatore, aperta nel secondo posto

# L'elenco: lo Store (760 x 600) nel secondo posto; il dettaglio con la barra degli strumenti
RIGA_CONTATORE = (266, 289)     # la prima riga di "Le tue app"
SVUOTA, TOGLI = (760, 315), (869, 315)   # nell'intestazione dell'app, a destra
CONFERMA_SVUOTA = (684, 526)    # nell'avviso di UIKit: il pulsante "Svuota dati" (Annulla a sinistra)
CONFERMA_TOGLI = (705, 516)     # nell'avviso a tre scelte: "Togli app e dati", il piu' a destra


def doppio_clic(m, x, y):
    m.muovi(x, y)
    time.sleep(0.15)
    for _ in range(2):
        m.pulsante(True)
        time.sleep(0.05)
        m.pulsante(False)
        time.sleep(0.05)
    time.sleep(0.1)
    m.muovi(*PUNTO_VUOTO)


def esegui(p):
    m = p.m

    def passo(azione, seriale, foto, attesa=30.0):
        """Fa l'azione, aspetta la riga nella seriale, poi lo schermo fermo, e la foto."""
        azione()
        m.muovi(*PUNTO_VUOTO)
        if seriale:
            m.aspetta_seriale(seriale, attesa=attesa)
        time.sleep(0.5)
        m.aspetta_schermo_fermo()
        m.foto(foto)

    s.apri(m, "Files")
    passo(lambda: m.clic(*SCRIVANIA_FILES), None, "scrivania_in_files")
    passo(lambda: doppio_clic(m, *PACCHETTO), "store: richiesta di installazione: contatore", "richiesta")
    passo(lambda: m.clic(*INSTALLA), "store: installata contatore", "installata")
    p.controlla("launcher: installata contatore" in m.testo_seriale(), "il launcher non dice 'installata contatore'")
    passo(lambda: m.clic(*APRI), "store: apro contatore", "contatore_aperta")
    time.sleep(2)                                    # che Contatore abbia la sua finestra
    passo(lambda: m.clic(*CHIUDI_CONTATORE), None, "contatore_chiusa")

    passo(lambda: doppio_clic(m, *PACCHETTO), "store: gia' installata: contatore", "gia_installata")
    passo(lambda: m.clic(*CHIUDI), None, "store_chiuso")

    passo(lambda: m.clic(s.APP_X["Store"], s.ISOLA_Y), "store: elenco: 7 app, 1 tue", "elenco")
    passo(lambda: m.clic(*RIGA_CONTATORE), None, "dettaglio")
    passo(lambda: m.clic(*SVUOTA), None, "conferma_svuota")
    passo(lambda: m.clic(*CONFERMA_SVUOTA), "store: dati svuotati di contatore", "dati_svuotati")
    p.controlla("launcher: dati svuotati di contatore" in m.testo_seriale(), "il launcher non ha svuotato i dati")
    passo(lambda: m.clic(*TOGLI), None, "conferma_togli")
    passo(lambda: m.clic(*CONFERMA_TOGLI), "store: elenco: 6 app, 0 tue", "tolta")
    p.controlla("launcher: tolta contatore" in m.testo_seriale(), "il launcher non dice 'tolta contatore'")
