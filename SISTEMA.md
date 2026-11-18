# Il sistema: chi fa cosa

Come sono divisi i compiti tra i programmi di mshell e come si parlano. Vale per chi scrive un
servizio, un comando o un'app. Le regole delle app (pacchetti, permessi, isolamento) stanno in
`APPLICAZIONI.md`; qui solo quello che serve per capire il disegno d'insieme.

## 1. I ruoli

| Ruolo | Cosa fa | Chi è oggi |
|---|---|---|
| **init** | Il processo 1. Fa partire i servizi, presta loro le risorse (schermo, nomi), li sorveglia e li rifà partire se cadono, e custodisce le porte delle app di catalog, launcher, network, tasks e picker (le riconsegna quando ripartono). Nient'altro. | `/system/init` |
| **servizi** | Ognuno **possiede** una cosa del computer ed è l'**unico** a toccarla. La offre agli altri da una **porta** e controlla i permessi di chi chiede. Possiede anche la sua configurazione. | eventi (tastiera e mouse), compositore (schermo e finestre), launcher (avvio e installazione delle app), network (rete e firewall), catalog (impostazioni della persona), tasks (processi, memoria e disco), picker (la finestra Apri/Salva), scrivania |
| **app** | Programmi grafici in un pacchetto `.app`, isolati, con i permessi del manifesto. Disegnano la loro interfaccia e la danno al compositore. Per tutto il resto chiedono ai servizi dalle porte. | Files, Attività, Impostazioni, Store, Terminale... |
| **comandi** | Programmi senza grafica, per la riga di comando. Parlano con le **stesse porte** delle app, con i permessi della persona che li lancia. Non sono app (`APPLICAZIONI.md`, sezione 2). | i nostri in `/system/bin` (sorgenti in `utente/comandi/`, uno per file: oggi `network`), `/system/bin/curl` (scarica dal web, HTTP e HTTPS) e `/system/bin/vim` (l'editor, versione minima), non nostri: compilati dai sorgenti ufficiali; e la shell |
| **shell** | Un comando come gli altri: legge righe e fa partire comandi. Non ha nessun ruolo speciale e nessuno passa da lei per parlare con il sistema. Il Terminale è solo l'app che la ospita. | `/system/bin/shell` (di prova) |

Dettagli sui servizi di oggi:
- **eventi** legge tastiera e mouse e li manda in un solo tubo, al compositore. È il compositore a passare ogni evento all'app in primo piano.
- **la shell sulla seriale** la fa partire init, solo se la seriale c'è, con `TERM=xterm-256color` (la seriale di QEMU capisce xterm). Serve per lo sviluppo. Quella del Terminale la fa partire il Terminale stesso, con lo stesso `TERM`; la shell passa ai comandi il `TERM` che ha ricevuto.
- **catalog** (deciso il 16 novembre 2026) possiede le impostazioni della persona: oggi il tema
  (chiaro o scuro) e lo sfondo; la lingua quando ci sarà. Contiene **solo impostazioni**, niente
  informazioni sul sistema o sull'hardware, e valgono **per la persona** (`/data/users/<uid>/preferences`).
  È l'unico a leggere e scrivere quei file: le app e la scrivania chiedono a lui (`get`), si fanno
  avvisare quando cambiano (`watch`) e, con il permesso `settings.system`, le cambiano (`set`).
  Gira come la persona (uid 1000), non come sistema: tocca solo i file di lei. Il nome glielo
  presta init. Contratto in `corekit/catalog.h`; prova `test/prove/catalog.py`. Dal 16 novembre
  2026 nessun altro programma tocca quei file: le app (UIKit), la scrivania e il compositore
  chiedono tema e sfondo a catalog e osservano i suoi avvisi; Impostazioni li cambia con catalog.
  Il compositore non fa più da tramite per il tema (prova `tema`).
- **picker** fa scegliere alla persona un file da aprire o dove salvarlo, e lo consegna **già
  Deciso il 17/11/2026: Un'app con sopra la finestra Apri/Salva non viene controllata (non risponde) finché la scelta è aperta. Scelta consapevole: è la più semplice; il limite è che se l'app si blocca davvero mentre aspetta, il sistema non lo vede. Scartate: risposta asincrona come Mac/Flatpak (più pulita, lavoro grosso) e libreria che risponde al posto dell'app come Windows.
  aperto** all'app che l'ha chiesto (`open`, `save`): l'app riceve solo quel file, senza nessun
  permesso sui file. Mostra la finestra "Apri/Salva" (`disegni/ApriSalva/`) sopra l'app, con il
  velo sulle sue finestre; una richiesta alla volta. Gira come la persona (uid 1000) e non esce
  mai dalla Home (`/data/users/<uid>/home`): ogni cartella e ogni file li apre con `openat2` dalla
  Home, con `RESOLVE_BENEATH`, `RESOLVE_NO_SYMLINKS` e `RESOLVE_NO_MAGICLINKS` (il kernel rifiuta
  ogni percorso che esce), consegna solo file normali e nell'elenco mostra solo file e cartelle,
  niente collegamenti. Un nome proposto con "/", "." o ".." è rifiutato (`invalid`). Il nome
  glielo presta init. Contratto in `corekit/picker.h`; il servizio in `utente/programmi/picker.c`,
  la finestra in `picker_finestra.c`; prova `test/prove/picker.py` (con una finestra finta, senza
  grafica).

## 2. Un servizio, tanti clienti

La regola: **una cosa del computer ha un solo padrone**, il suo servizio. Tutti gli altri sono
clienti e chiedono a lui. Nessuno, tranne il servizio, modifica i suoi file di configurazione o
tocca la sua parte del kernel.

Esempio, la rete:
1. il servizio network è l'unico che tocca la scheda di rete, il firewall e i file in `/system/config/network/` (e, più avanti, la configurazione personale in `/data`);
2. offre una porta, dove si chiede "com'è la rete?" o "metti questo indirizzo fisso";
3. la pagina della rete nelle Impostazioni è un cliente: chiede alla porta e mostra;
4. un comando `network` è un altro cliente: chiede alla stessa porta e scrive testo;
5. l'app scelta dalla persona per il ruolo firewall è un terzo cliente, con il permesso `network.firewall` (una sola app alla volta: `APPLICAZIONI.md`, sezione 9);
6. la porta sa chi chiede (il kernel lo dice) e decide in base ai permessi.

Così fanno anche gli altri sistemi: NetworkManager con `nmcli` e le Impostazioni di GNOME su
Linux, launchd e XPC sul Mac.

### Configurazioni

- Le **predefinite** stanno in `/system/config/<servizio>/` (sola lettura, vengono con il sistema). Formato `chiave=valore`, una per riga, `#` per i commenti.
- Le **personali** staranno in `/data` (ancora da fare). Le scrive solo il servizio, quando un cliente glielo chiede dalla porta.
- Quello che cambia a sistema acceso va in `/run` (in memoria). Esempio: `/run/network/resolv.conf`, a cui punta `/etc/resolv.conf`.

## 3. Le porte

Una **porta** è il punto dove un servizio riceve le richieste. Il meccanismo lo dà il kernel e la
nostra libreria lo usa già (CoreKit, `utente/libreria/corekit/interno/comunicazione.c`):
- una "presa" Unix con un nome, come file in `/run/mshell/<nome>` (`ascolta`, `collega`, `accetta`);
- a messaggi interi (ogni messaggio arriva tutto insieme);
- può portare con sé un descrittore aperto (un foglio di memoria, un file);
- il kernel dice al servizio chi c'è dall'altra parte: processo e utente (`chi`).

Il **launcher** usa già lo standard qui sotto (il suo contratto, cioè richieste, chiavi, risposte
e permessi, è in testa a `corekit/sistema/launcher.h`). Il **compositore** ha ancora un formato
dei messaggi suo (in `gkit/sistema/protocollo.h`, gli eventi in `corekit/chiamate.h`). Lo
standard serve a non farne uno nuovo per ogni servizio.

### Lo standard (piccolissimo)

Deciso il 13 novembre 2026: **i messaggi sono testo `chiave=valore`**, come i file di
configurazione.
- Un messaggio = righe `chiave=valore`, separate da a capo. Una chiave può ripetersi (es. più `dns=`).
- Chiavi in inglese minuscolo, come nei file di configurazione.
- Chiavi della busta, uguali per tutti: `request` (quale richiesta), `id` (numero scelto da chi chiede, la risposta lo ripete), `result` (nella risposta: `ok`, `denied`, `invalid`, `error`, e `timeout` se il servizio non risponde in tempo).
- Il resto delle chiavi lo decide ogni servizio, e lo scrive accanto al suo codice.
- Un descrittore allegato (file, foglio di memoria) viaggia accanto al messaggio, come oggi.

Due parti:
1. **la regola scritta**, qui;
2. **il codice**, nella libreria che tutti i programmi già usano (CoreKit, `utente/libreria/corekit/`): un file per la busta e una funzione per il controllo dei permessi.

Cosa deve dire:
- **la busta**, uguale per tutti: quale richiesta, un numero per riconoscere la risposta, l'esito (fatto, non hai il permesso, richiesta sbagliata...), e i dati della richiesta;
- **il controllo dei permessi**, uguale per tutti: dall'identità che dà il kernel si risale a "chi è" (un'app e i suoi permessi concessi, una persona, il sistema), e il servizio chiede solo "ha il permesso X?";
- ogni servizio aggiunge solo l'elenco delle sue richieste.

Lo usa anche catalog (`corekit/catalog.h`): è il primo servizio con collegamenti che restano
aperti, perché chi chiede `watch` riceve gli avvisi (messaggi con `notice=changed`, senza `id` né
`result`) sullo stesso collegamento.

Il primo a usarlo è il launcher (aprire le app e i file, installare e togliere i
pacchetti): i dati grandi (il resoconto di un pacchetto, l'elenco delle app) viaggiano
nel foglio allegato alla risposta. Il servizio network lo usa per la sua porta (contratto in `corekit/sistema/network.h`: `status`, `restart`, `stop`; in sola lettura, la configurazione non si cambia da lì; permessi `network.use` e `network.settings`; prova `test/prove/stato_rete.py`). Il
compositore si allineerà più avanti.

Le app non vedono i messaggi: chiamano le funzioni del servizio (`apertura.h`,
`sistema/launcher.h`), che li fanno dentro CoreKit. (Il riquadro con un avviso di UIKit si
chiama `struct messagebox`, `uikit/messagebox.h`: nessuno scontro di nomi con `messaggi.h`.)

**Dove sta il codice:** in CoreKit, `utente/libreria/corekit/messaggi.h` (pubblico: chi chiede), `corekit/sistema/servizi.h` (del sistema: il lato del servizio) e `corekit/interno/messaggi.c` (la spiegazione lunga, con un esempio di scambio, è in testa a `messaggi.c`). Le funzioni principali:
- costruire: `messaggio_inizia`, `messaggio_aggiungi`, `messaggio_aggiungi_numero`, `messaggio_risposta` (la busta della risposta: stesso `id`, `result`);
- leggere: `messaggio_valore`, `messaggio_prossimo` (per le chiavi ripetute), `messaggio_numero`, `messaggio_vale`;
- mandare e ricevere, con un descrittore allegato facoltativo: `messaggio_manda`, `messaggio_ricevi`;
- chi chiede: `chiedi` (mette l'`id` e aspetta la risposta con lo stesso `id`);
- il servizio, in `sistema/servizi.h`: `chi_chiede` (uid e programma dall'altra parte) e `ha_permesso` (sotto).

**Tempo massimo.** `chiedi` aspetta al massimo 5 secondi, uguale per tutti; per una richiesta
lenta c'è `chiedi_entro`, con un tempo dato da chi chiede. Se scade, la risposta ha
`result=timeout` (lo mette `chiedi`, non il servizio) e `chiedi` dà un valore suo. Una risposta
arrivata tardi ha l'`id` vecchio e la butta la richiesta successiva. Se il servizio cade, il
collegamento si chiude e chi chiede lo sa subito.

**Permessi.** Il servizio chiede `ha_permesso(chi chiede, "nome.permesso")` (es.
`network.settings`). La regola (decisa con la persona il 16 novembre 2026, in `sistema/servizi.h`):
- il **sistema** (uid 0): tutti i permessi;
- un'**app**, dalla sua porta: solo quelli che il launcher le ha concesso e ha detto al servizio con
  `attach` (chiave `permission`, ripetuta). Finché non c'è la domanda del primo avvio, concessi =
  dichiarati nel manifesto, e quelli di sistema solo alle app del sistema (`/system/apps`);
- un **altro programma della persona** (un comando, che non parte dal launcher): i permessi
  normali, nessuno di sistema.

Ogni rifiuto finisce nel registro: `permesso <nome> a <programma> (uid <uid>): negato`. La usano
launcher (`apps.manage`, `apps.open`), network (`network.use`,
`network.settings`) e catalog (`settings.system`): `APPLICAZIONI.md`, sezione 9.

Limite: un messaggio è al massimo 5120 byte (lo stesso di oggi per tutti i messaggi). La prova è `test/prove/messaggi.py`.

- **tasks** (deciso il 16 novembre 2026) possiede quello che serve a un task manager: i programmi in
  esecuzione, la memoria, lo spazio del disco (anche quanto pesano `/system`, `/system/apps`,
  `/data/apps` e `/data/users`) e il potere di fermare un programma. Legge `/proc`, `statvfs` e le
  cartelle al posto delle app, che nel recinto non le vedranno. Gira come sistema (uid 0: deve
  vedere tutti i processi) e dà le informazioni solo con `processes.view` (o `processes.stop`, che lo
  comprende); fermare chiede `processes.stop`. **La regola di `stop`** non la lascia al kernel:
  ferma solo i processi della persona (uid 1000), mai quelli del sistema né init, a chiunque lo
  chieda. Il nome glielo presta init. Contratto in `corekit/tasks.h`; le funzioni `elenco_processi`,
  `stato_memoria`, `stato_disco`, `dimensione_cartella` e `termina_processo` di CoreKit sono richieste
  a tasks. Prova: `attivita` (Attività funziona tramite tasks) e `applicazioni`.

### Le porte delle app

Deciso con la persona il 16 novembre 2026 (`APPLICAZIONI.md`, "Come funzionano le chiavi"). Un'app
non cerca i servizi per nome: quando la fa partire, il launcher le dà una **porta** per ogni
servizio, cioè un collegamento già aperto, a un descrittore fisso. L'altro capo lo dà al servizio
dicendo di quale app è. Così il servizio sa chi è l'app dal launcher, non dal kernel (dentro il
recinto il percorso del programma sarebbe un altro) e non da quello che l'app dice di sé.

| Descrittore | Servizio | Come |
|---|---|---|
| 0, 1, 2 | nessuno: ingresso, uscita, errori | `/dev/null` |
| 3 | compositore | il suo formato: `PORTA_APP` dal launcher, `NUOVO_COLLEGAMENTO` dall'app (`gkit/sistema/protocollo.h`) |
| 4 | catalog | lo standard: `attach` dal launcher, `connect` dall'app (`corekit/messaggi.h` e `sistema/servizi.h`) |
| 5 | launcher | lo standard, ma senza `attach`: il launcher crea la coppia e tiene lui il capo del servizio, con il programma dell'app. A tutte le app |
| 6 | network | lo standard (`attach` dal launcher, `connect` dall'app). Solo alle app con `network.use` o `network.settings` nel manifesto; alle altre `/dev/null` |
| 7 | picker | lo standard (`attach` dal launcher, `connect` dall'app). A tutte le app: scegliere un file non chiede permessi |
| 8 | tasks | lo standard. Solo alle app con `processes.view` o `processes.stop` nel manifesto; alle altre `/dev/null` |

I descrittori passati a un programma vanno da 0 a 15 (`MAX_PASSATI`); il **registro** (la console, su
cui scrive `scrivi`) sta al 16, subito dopo (era all'8: il 16 novembre 2026 si è spostato per fare
posto alla porta 8).

Per i servizi dello standard:
1. il launcher crea una coppia di socket, si collega al servizio per nome e manda `request=attach`
   con `program` (il programma dell'app) e un capo allegato; il servizio accetta `attach` solo dal
   sistema e risponde `ok`;
2. l'app, per ogni collegamento, crea una coppia sua e ne manda un capo sulla porta
   (`request=connect`, senza risposta); il servizio lo tratta come un collegamento normale, che è
   dell'app della porta. Lo fa la libreria (`collega_servizio`): chi scrive l'app non lo vede;
3. da lì app e servizio parlano da soli: il launcher fa solo da centralino all'inizio.

**Quali sono le porte lo dice il launcher**, non si indovina: nella variabile d'ambiente
`MSHELL_PORTE` dell'app scrive l'elenco delle porte date davvero, servizio e descrittore (es.
`compositore=3 catalog=4 launcher=5 network=6 picker=7 tasks=8`), come fa systemd con `LISTEN_FDS`. La libreria
(`porta_di`, `corekit/messaggi.h`) controlla prima il tipo (il servizio è nell'elenco?) e poi chi
è (all'altro capo c'è davvero il sistema?). Corretto il 16 novembre 2026: prima si guardava solo il
secondo punto, e in un programma del sistema un suo collegamento qualsiasi a quel numero veniva
scambiato per una porta (la scrivania si chiudeva quando catalog ripartiva).

I programmi del sistema e i comandi, che non partono dal launcher, non hanno la variabile: si
collegano per nome, e il servizio chiede al kernel chi sono. Oggi le porte sono sei (compositore, catalog, launcher,
network, picker e tasks): i servizi che chiedono un permesso (network, tasks) danno la porta solo alle app che lo hanno
nel manifesto. Per ogni app che fa partire, il launcher scrive nel registro una riga con le porte
date (`launcher: esempio: porte compositore catalog launcher network picker`).

**Init custodisce le porte** (deciso con la persona, come il "file descriptor store" di systemd).
Il capo "servizio" di una porta sta nel servizio, e se il servizio cade si chiude. Allora catalog,
launcher, network, tasks e picker hanno con init un canale (una coppia di socket: init tiene un capo, presta
l'altro al servizio) e a ogni porta nuova gli mandano una copia del capo (`request=keep`, con
`program` e `permission`). Init le tiene senza mai leggerle (le butta quando l'app le chiude) e,
a ogni partenza del servizio, prima di farlo partire gli riconsegna le porte (`request=restore`
per ognuna, poi `restored`): il servizio le rimette nella sua tabella e le app non si accorgono
della caduta. Il compositore no (se riparte le finestre si perdono comunque). Le funzioni comuni
sono in `corekit/sistema/servizi.h` (`custodisci_porta`, `riprendi_porte`).

### Wayland: il compositore parla il protocollo di tutti (deciso il 18 novembre 2026)

**Decisione della persona:** il compositore passa a **Wayland**, a passi, senza mai rompere
niente. Il server Wayland sta **dentro il compositore** (come ChromeOS con Exo, e come Mutter
o KWin): non è un ponte esterno. Wayland dà una cosa che oggi non abbiamo, il suo mondo di
programmi (toolkit, browser, app Linux), e il nostro protocollo (`gkit/sistema/protocollo.h`)
resta, passo dopo passo, solo per quello che è davvero nostro.
1. **Passo 1 (fatto il 18/11/2026):** il compositore impara Wayland ACCANTO al protocollo di oggi.
   Un client Wayland piccolo (`utente/prove/wayland.c`) apre una finestra sulla scrivania; prova
   `wayland`. Dettagli sotto.
2. **Passo 2 (in corso):** GKit diventa un client Wayland, un'app alla volta. **Fatto: `wl_seat`**
   (mouse e tastiera nelle finestre Wayland, vedi "Mouse e tastiera" sotto). Mancano `wl_output`,
   il socket dato dal launcher come le altre porte, e il modo di dire all'app quale finestra è sua
   (l'identità, per i permessi).
   **Appunti (fatto):** `wl_data_device_manager` v3, il copia e incolla tra programmi Wayland. La
   selezione è una sola per il seat; il programma con il fuoco della tastiera la riceve (con i tipi
   MIME) a ogni cambio e quando prende il fuoco; il contenuto non passa dal compositore (`receive` va
   alla sorgente con un pipe). Sorgente distrutta: selezione vuota; sorgente sostituita: `cancelled`.
   Il trascinamento (`start_drag`) non c'è: la sorgente riceve subito `cancelled`. Prova `wayland_appunti`.
3. **Passo 3 (da fare):** via il vecchio protocollo. Resta Wayland con le nostre estensioni
   private: la finestra di scelta (`RUOLO_SCELTA`, il picker), l'identità dell'app per i
   permessi, i pannelli della scrivania (userbar, vassoio).

**La barra del titolo la disegna l'app (deciso il 18/11/2026).** Il server offre `xdg-decoration`:
se l'app chiede `client_side` la finestra non ha la cornice del compositore (l'immagine è il suo
buffer, ombra e vetro come le finestre nostre, coordinate del puntatore senza testata); senza
oggetto decoration o con `server_side` resta la cornice nostra. Lo stesso lato server di
`xdg_toplevel.move` (come TRASCINAMI: vale solo col pulsante ancora giù), `set_minimized` (come
RIDUCIMI), `xdg_wm_base.ping/pong` (stesse attese e stessa barra di emergenza delle finestre
nostre; con sopra la finestra di scelta non si giudica) e `cursor-shape` v1 (la forma per nome, come
MOSTRA_PUNTATORE). Prova: `wayland_senza_cornice` (client `wayland_csd`).

**Sottosuperfici (`wl_subcompositor` v1).** Le usano le app vere (foot non parte senza). L'immagine
della finestra è la superficie principale con le sottosuperfici sopra e sotto, nelle loro posizioni
e tagliata ai bordi della principale (alfa premoltiplicato come il resto). Posizione e ordine
(`set_position`, `place_above/below`) valgono al commit del genitore; una sottosuperficie sincrona
(valore di partenza) tiene il suo stato in cache fino al commit del genitore, una `set_desync`
vale subito (se nessun antenato è sincrono). Il puntatore va alla superficie sotto, con le sue
coordinate; `set_input_region` vale se è una somma di rettangoli (se no, tutta la superficie). Non
fatto: scala e trasformazione del buffer (accettate, si disegna a scala 1), `set_opaque_region`.
Prova: `wayland_sottosuperfici` (client `wayland_sotto`).

**Come è fatto il passo 1.**
- **libwayland-server** (e libwayland-client, per i client di prova) si compila STATICA dai
  sorgenti ufficiali, versione fissata: wayland 1.25.0 e wayland-protocols 1.48 (le stesse di
  Alpine 3.24), scaricate in `build/scarico` e controllate con lo sha256. Lo fa
  `strumenti/compilazione/wayland.sh` (chiamato da `programmi.mk`, quindi da `build.sh` e dalla
  sandbox) senza meson: `wayland-scanner` (statico, solo per la compilazione, non va nel disco)
  genera le intestazioni e il codice dai file XML (`wayland.xml`, `xdg-shell.xml`). Il compositore
  lega anche `libffi.a` (da `libffi-dev` nella radice di Alpine, come `expat-static` per lo scanner).
- **Dentro il compositore**: il ciclo guarda anche il descrittore del server (`wl_event_loop_get_fd`)
  e basta. Il codice è in `utente/programmi/compositore_wayland.c`, con un'interfaccia piccola
  verso `compositore.c` (`compositore_wayland.h`: il ponte, "IL PONTE" in `compositore.c`).
  Globali: `wl_compositor` (superfici: attach, damage, frame, commit), `wl_shm` (ARGB8888 e
  XRGB8888), `xdg_wm_base` (finestre con titolo, configure, chiusura) e `wl_seat` (passo 2). Niente output e popup.
- **Come una finestra Wayland entra tra le finestre del compositore**: occupa un posto di
  `finestre[]` come le altre (con `wayland = 1`, senza collegamento né foglio). L'immagine è
  memoria del compositore: la cornice nostra disegnata da UIKit (`cornice_disegna`, `uikit_compositore.h`:
  vetro, testata di 40 con titolo e comandi, filo di luce del tema scuro, gli stessi della finestra
  di UIKit) e sotto la testata il contenuto del programma, copiato a ogni commit. Ombra, vetro
  smerigliato, primo piano e fuoco sono quelli di tutte le finestre. La cornice la gestisce il
  compositore sul puntatore: trascinare dalla testata, ridurre, e la X che manda
  `xdg_toplevel.close` (il programma decide).
- **Mouse e tastiera (passo 2)**: `wl_seat` "seat0", versione 7, capacità puntatore e tastiera
  (la tastiera solo se c'è la keymap). *Puntatore*: le notizie PUNTATORE, USCITO, ROTELLA e FUOCO
  che `avvisa` manda alle nostre finestre arrivano a Wayland (`wayland_notizia`): chi "sente" il
  puntatore lo decide quindi `al_mouse` come sempre (finestra sotto il puntatore, presa durante il
  clic, niente puntatore che passa a una finestra senza fuoco). Le coordinate sono del contenuto
  (sotto la testata); sulla cornice il programma non sente niente (leave), e un clic cominciato lì
  non gli arriva. Pulsanti `BTN_LEFT/RIGHT/MIDDLE`, rotella come `axis` (10 per scatto) più
  `axis_discrete`, ogni gruppo chiuso da `frame`. La rotella a una finestra senza fuoco arriva lo
  stesso (enter, axis, leave). `set_cursor` si ignora: il compositore mostra la sua freccia.
  *Tastiera*: enter/leave seguono il FUOCO (lo stesso delle nostre finestre); `key` ha il codice di
  Linux (il programma aggiunge 8 lato XKB); `modifiers` dopo ogni tasto che li cambia; ripetizione
  dal programma (`repeat_info`: 33 al secondo dopo 500 ms; `TASTI_RIPETI_*` in `chiamate.h`, gli
  stessi valori che `eventi` chiede al kernel). **I codici grezzi** arrivano al compositore con un
  evento in più, `EVENTO_TASTO_GREZZO` (`corekit/chiamate.h`): codice di Linux del tasto fisico (anche
  il tastierino con Bloc Num spento, dove il vecchio `EVENTO_TASTO_FISICO` manda il tasto di
  movimento), premuto, lasciato o ripetuto, e i modificatori DOPO il tasto (`GREZZO_*`). `eventi` lo
  manda prima degli altri eventi dello stesso tasto; le nostre app non lo vedono mai. *La keymap*:
  il testo XKB_V1 della tastiera italiana si compila a ogni compilazione con `xkbcomp`
  (`strumenti/compilazione/tastiera.sh`: `pc+it+inet(evdev)+kpdl(comma)`, pacchetti Alpine `xkbcomp`
  e `xkeyboard-config` negli attrezzi) e va in `/system/resources/keymaps/it.xkb`; il compositore la
  legge all'avvio e la manda in un memfd sigillato a ogni `wl_keyboard`. Non c'è nessuna libreria
  XKB nel compositore: i client la leggono con la loro.
- **Il socket**: `/run/mshell/wayland-prova`, aperto dal compositore stesso (`wl_display_add_socket`;
  `/run` è in memoria, niente dati scrivibili sotto `/system`). Solo per la prova: lo apre root con
  i permessi normali, un'app della persona non lo raggiunge. Le app normali NON hanno ancora Wayland.
  Per il passo 2 la strada è quella delle altre porte: il launcher apre una coppia di socket, tiene un
  capo e passa l'altro all'app al descrittore dato, che l'app usa con `WAYLAND_SOCKET` (la libreria
  lo prende come connessione già aperta: `wl_client_create` lato server, con l'identità che
  il launcher ha già, senza socket con nome per le app).

## 4. Domande aperte

- **Comandi distribuiti dalla persona.** Già deciso: i comandi della persona stanno in `/data/users/<uid>/bin`, cercati dopo `/system/bin`. Da decidere: come si distribuiscono (un'app che li dichiara nel manifesto, un pacchetto di soli comandi, o solo copia a mano) e con quali poteri girano. Oggi un comando ha tutti i permessi della persona, come su Linux e Mac: va bene per i propri, è un rischio per quelli di altri.
- **Cambiare la rete dall'interno.** Oggi la configurazione della rete e del firewall sta solo in `/system/config/network/` (sola lettura) e la porta del servizio la legge soltanto. Già previsto (sezione 1): la configurazione della persona in `/data`, scritta dal servizio su richiesta dalla porta (es. "metti questo indirizzo fisso"), con il permesso `network.settings`. Da decidere: quali richieste, dove sta il file, come si torna alle predefinite.
- **Regole del firewall e i loro permessi.** Oggi il firewall ha solo quattro interruttori per tutto il computer (`incoming`, `outgoing`, `forward`, `ping` in `firewall.conf`). In `APPLICAZIONI.md` c'erano due permessi di sistema (`network.firewall.rules`: regole per sé; `network.firewall`: controllo pieno) e i moduli aggiuntivi con il punto di aggancio "filtro della rete" (sezione 10). Le domande e le decisioni:
  1. **deciso il 14 novembre 2026**: le "regole per sé" non esistono. Un'app che vuole ricevere connessioni da fuori non scrive regole: **chiede al firewall** di accettare connessioni in entrata su una porta, e decide solo il firewall. `network.firewall.rules` è tolto. Tecnicamente la risposta "sì" diventa una regola che apre quella porta solo verso i programmi di quell'app (nftables riconosce a chi appartiene la presa che riceve);
  2. **deciso il 14 novembre 2026: la configurazione predefinita del firewall c'è SEMPRE, come via d'uscita.** Vale quando nessuna app è scelta per il ruolo, quando l'app scelta viene tolta, e quando l'app scelta si blocca o non risponde. In quei casi le richieste di aprire una porta le decide il servizio network secondo la configurazione predefinita. La configurazione predefinita **chiede alla persona** (deciso il 14 novembre 2026, come Mac e Windows): il sistema mostra "vuoi che <app> accetti connessioni in entrata?", e la risposta si ricorda per quell'app e quella porta; si potrà cambiare nella pagina della rete delle Impostazioni. Serve una finestra di domanda del sistema, che oggi non c'è;
  3. ~~il controllo pieno: permesso o modulo~~ **deciso il 14 novembre 2026**: con il permesso `network.firewall`, e lo esercita una sola app alla volta, quella scelta dalla persona per il ruolo firewall. **Deciso anche, come Windows:** quando un'app è scelta, prende il posto degli interruttori di base. Le Impostazioni non li mostrano più come modificabili: dicono "il firewall è gestito da <app>", con il pulsante per smettere di usarla. Se la persona la toglie dal ruolo (o disinstalla l'app), le sue regole spariscono e si torna agli interruttori di base com'erano prima. Non c'è scontro con le altre app: chiedono, e l'app firewall risponde;
  4. dove si salvano le regole (in `/data`, del servizio, con il nome dell'app) e quando spariscono (app tolta; regole che valgono solo finché l'app è aperta?);
  5. l'ordine: prima il controllo dei permessi vero (`ha_permesso`: fatto il 16 novembre 2026), poi la finestra di domanda del sistema, poi le richieste al servizio network.
- ~~**Permessi dei comandi.**~~ Deciso il 16 novembre 2026: un programma della persona che non è un'app ha i permessi normali, nessuno di sistema (`ha_permesso`, sezione 3).
- **La shell come sistema.** Oggi la shell del Terminale è del sistema (decisione provvisoria della persona, 17 novembre 2026, `STATO.md`): il launcher fa partire il Terminale come sistema (uid 0), unica app a cui succede (un punto solo nel launcher, `uid_dell_app`), e il Terminale fa partire la shell da sé, che ne eredita i poteri. Non c'è più una richiesta al launcher per aprire la shell, né il permesso `system.shell`. Per farla girare come la persona basta togliere quel caso. Quando le porte controlleranno i permessi, la shell girerà come la persona, e le azioni di sistema passeranno dalla porta del servizio, che chiede conferma (come `sudo` o la finestra della password sul Mac).
