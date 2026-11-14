"""pacchetti: le richieste sui pacchetti .app al launcher, come le fara' lo Store.

Si accende il disco delle prove dei pacchetti (disco_pacchetti.img, vedi build.sh): in
/data/tests c'e' il programma pacchetti (utente/prove/pacchetti.c), che init fa partire come
sistema. Lui, con contatore.app (sulla Scrivania della persona):
  - chiede l'elenco delle app installate: le 7 del sistema (con lo Store e il Terminale), senza Contatore;
  - chiede di aprire il file leggimi.txt: il sistema non sa aprirlo (aprire contatore.app
    farebbe partire lo Store: lo prova la prova store);
  - chiede di aprire l'app contatore: non e' installata, quindi no;
  - descrive un file che non e' un pacchetto (no) e contatore.app (si', senza errori, con le
    due icone);
  - aggiorna (no: non e' installata), installa (si'), installa di nuovo (no), aggiorna alla
    stessa versione (no);
  - chiede di nuovo l'elenco: ora c'e' contatore, tra quelle della persona;
  - togliere files (del sistema) e un id con "../": no; svuotare i dati di "..": no;
    svuotare i dati di contatore (spenta): si';
  - apre contatore, e toglierla mentre e' accesa: no ("Chiudi Contatore prima ...").
Ogni passo lo scrive nella seriale ("pacchetti: ... -> esito"). Qui si aspetta "pacchetti:
tutto bene", e poi che la finestra di Contatore compaia (primo posto, angolo in 140,130).
La logica della libreria (zip slip, permessi, icone...) si prova sul computer che compila:
test/pacchetti/prova.sh.
"""

import time

import scrivania as s
from qemu import ErroreProva

NOME = "pacchetti"
ORDINE = 80
DESCRIZIONE = "launcher: elenco, apri un .app (serve lo Store), descrivi, installa e apri contatore.app"
DISCO = "disco_pacchetti.img"


def esegui(p):
    m = p.m
    fine = time.time() + 60
    seriale = m.testo_seriale()
    while "pacchetti: tutto bene" not in seriale and "pacchetti: fallita" not in seriale:
        if time.time() > fine:
            raise ErroreProva("dopo 60 s il programma pacchetti non ha finito")
        time.sleep(0.5)
        seriale = m.testo_seriale()
    sbagliati = [r for r in seriale.splitlines() if r.startswith("pacchetti:") and "FALLITA" in r]
    p.controlla(not sbagliati, "passi falliti: " + "; ".join(sbagliati))
    p.controlla("launcher: dati svuotati di contatore" in seriale,
                "il launcher non dice 'dati svuotati di contatore'")
    p.controlla("launcher: installata contatore" in seriale, "il launcher non dice 'installata contatore'")
    x, y = s.angolo(0)
    l, _ = s.MISURE["Contatore"]
    # Non si confronta con la foto della scrivania vuota (p.scrivania): il programma pacchetti
    # parte con il sistema e, su una macchina veloce (arm64 con hvf), apre Contatore prima che
    # quella foto sia fatta, quindi la finestra ci sarebbe gia' dentro e la zona risulterebbe
    # sempre uguale. Si guarda la finestra stessa: il corpo e' una superficie chiara e piena,
    # mentre lo sfondo, in quel punto, e' blu e viola scuro.
    def finestra_aperta(foto):
        rgb = foto.convert("RGB")
        punti = [(x + 12, y + 12), (x + 12, y + s.BARRA + 40), (x + l - 12, y + s.BARRA + 40)]
        return all(min(rgb.getpixel(q)) > 170 for q in punti)
    m.aspetta_che(finestra_aperta, 15.0, cosa="la finestra di Contatore (installata) aperta")
    m.foto("contatore_installata")
