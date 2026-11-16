# mshell: le applicazioni

Questo documento raccoglie cosa è un'applicazione in mshell: com'è fatto il pacchetto, come si
installa, come chiede i permessi e come resta isolata. Sono decisioni di progetto prese il
12 novembre 2026, prima di scrivere il codice. Niente di questo è ancora costruito.

## 1. Lo scopo

Il sistema delle app serve a dare alle persone **uno standard di usabilità e di sicurezza**.
Ogni app segue le stesse regole. Tutto quello che si allontana dallo standard va dichiarato,
e la persona ne viene informata.

## 2. Cosa è un'app e cosa non lo è

- **App**: un pacchetto installato, isolato, con i permessi che ha dichiarato e che la persona
  gli ha concesso. Si apre solo se è installata.
- **Eseguibili normali** (i comandi in `bin`): non sono app. Funzionano come qualsiasi
  eseguibile, anche avviati dalla scrivania, come su Mac.

## 3. Il pacchetto

- Un'app si distribuisce come **un file unico con estensione `.app`**. L'estensione resta `.app`
  anche se su macOS è già usata (lì le app sono cartelle): un nostro pacchetto copiato su Mac
  apparirà come un'app rotta, e lo accettiamo.
- Il file è un **archivio zip** (come i pacchetti Android). Ha un indice, quindi durante
  l'installazione il sistema legge prima il manifesto e le icone, mostra la richiesta di
  installazione con permessi e avvisi, e solo dopo scompatta il resto. Si crea e si apre con
  qualsiasi programma per gli zip.
- Durante lo scompattamento il sistema **rifiuta i percorsi che escono dalla cartella dell'app**
  (per esempio `../`), un trucco noto per scrivere file dove non si dovrebbe.
- Contiene il manifesto, le icone chiare e scure e i dati dell'app.
- **Un pacchetto per processore**: il formato è lo stesso, ma il programma si compila una volta
  per PC (x86_64) e una per ARM (arm64). Il manifesto dice per quale processore è fatto.
- Il manifesto contiene la **versione minima del sistema** di cui l'app ha bisogno. È il sistema
  a dichiarare quali versioni non supporta più.

### Identificativo

- Ogni app ha un identificativo fisso, diverso dal nome che si vede (che può cambiare o essere
  tradotto). Serve per gli aggiornamenti, per la cartella dei dati di ogni persona e per i permessi.
- **Per ora è un nome semplice**, scelto da chi sviluppa l'app. La forma definitiva si decide dopo.
- Se l'identificativo cambia, le cartelle dei dati e i permessi già legati al vecchio nome vanno spostati.

### Icone

- **Un SVG per tema** (chiaro e scuro), più delle **PNG facoltative per le misure piccole**
  (dove un disegno vettoriale rimpicciolito viene male).
- Se manca una PNG piccola, il sistema rimpicciolisce l'SVG.

## 4. Installazione

- Installare vuol dire **scompattare il file `.app` in una cartella dentro la cartella delle app**
  (`/data/apps/<app>`; le app del sistema stanno in `/system/apps/<app>`). Da lì l'app gira come oggi.
- **Nessuna app si avvia se non è installata**: se si prova ad aprire un file `.app`, compare un
  avviso che dice che va prima installato.
- **Processore sbagliato**: l'installazione si ferma con un avviso che spiega il problema.
- **Icone mancanti** (una misura o un tema): l'installazione va avanti lo stesso e il sistema usa
  la più adatta tra quelle che ci sono (la misura più vicina, l'altro tema). Un avviso spiega
  cosa manca e cosa significa (per esempio "in tema scuro vedrai l'icona chiara").

## 5. Permessi e chiavi

### Come si chiedono

- Il manifesto elenca i permessi di cui l'app ha bisogno.
- **Al primo avvio** il sistema li chiede tutti insieme alla persona.
- Dopo, le **Impostazioni** mostrano i permessi di ogni app e permettono di spegnerli.
- **Spegnere un permesso chiude subito l'app**, se è aperta, con un avviso (deciso il 14 novembre
  2026, come Android): i file e le connessioni che ha già ricevuto non si possono togliere uno per
  uno, quindi si chiude l'app e alla prossima apertura riparte senza quel permesso.
- Se la persona dice no, l'app parte lo stesso senza quel permesso. **Ogni app deve funzionare,
  o almeno spiegare cosa non va, senza i permessi che non ha.**

### Chiavi anche per le deviazioni dallo standard

- Le chiavi non servono solo a *permettere* azioni, ma anche a *dichiarare* quando un'app non fa
  una cosa raccomandata. Per esempio: nessuna interfaccia grafica, oppure un lavoro che resta
  acceso in sottofondo senza finestre.
- Non c'è niente di male, ma il sistema lo deve sapere e la persona ne viene informata.
- Alcune deviazioni il sistema le può controllare da solo (un'app che non apre nessuna finestra,
  o che resta accesa dopo la chiusura). Altre si possono verificare solo con la revisione, quindi
  con la firma e il registro degli sviluppatori.

### Come funzionano le chiavi

- **Un canale per ogni servizio del sistema, aperto apposta per l'app.** Chi avvia l'app legge il
  manifesto e i permessi concessi, crea un canale verso ogni servizio permesso, dice al servizio
  quale app c'è dall'altra parte e passa i canali all'app.
- L'app non può aprirsi canali da sola: nel suo mondo isolato i servizi non si vedono.
- **L'identità sta nel canale** (16 novembre 2026, con la persona). Il launcher crea una coppia di
  socket, ne dà un capo all'app e manda l'altro al servizio dicendo di quale app è: tutto quello che
  arriva da quel canale è di quell'app. Il servizio non chiede al kernel chi c'è dall'altra parte e
  non crede a quello che l'app dice di sé; si fida solo del launcher. Dopo, app e servizio si
  parlano da soli: il launcher fa da centralino solo all'inizio. I figli dell'app ereditano il
  canale e parlano come l'app. I programmi del sistema, che non partono dal launcher, si collegano
  ancora per nome e il servizio chiede al kernel chi sono.
- **Fatto per il compositore** (16 novembre 2026): ogni app parte con la sua porta verso il
  compositore (al descrittore 3; 0, 1 e 2 sono /dev/null), e per ogni finestra apre un collegamento
  dalla porta (`gkit/sistema/protocollo.h`, "LA PORTA DELL'APP"). **E per catalog** (stesso
  giorno): la porta al descrittore 4, con lo standard dei messaggi (`SISTEMA.md`, sezione 3, "Le
  porte delle app"). **E per launcher e network**: la porta al 5 (verso il launcher, a tutte le app)
  e al 6 (verso la rete, solo con `network.use` o `network.settings` nel manifesto).
- **Le impostazioni comuni** arrivano dal servizio **catalog** (deciso il 16 novembre 2026): solo
  impostazioni (oggi tema e sfondo), della persona, in lettura per tutte le app; avvisa le app
  aperte quando cambiano; cambiarle chiede `settings.system`. Le preferenze dell'app stanno nel
  suo database, nella sua cartella dei dati (sezione 8). Il servizio c'è (`SISTEMA.md`, sezione
  1) e dal 16 novembre 2026 le app lo usano (lo fa UIKit): nessuna app legge più i file delle
  preferenze.
- **I dettagli** (per esempio, per i file, "solo leggere" o "anche scrivere") li controlla il
  servizio, in base all'identità dell'app.
- **I canali sono uno per servizio, non uno per permesso** (14 novembre 2026): i permessi sono
  tanti, i servizi pochi. Ogni servizio decide quale permesso chiede per ogni sua richiesta
  (per la rete, "dimmi lo stato" chiede `network.use`, "fermati" chiede `network.settings`) e lo
  controlla nel database del sistema (sezione 8), con un solo controllo uguale per tutti.

### L'app non vede niente: il kernel fa i muri, i servizi fanno le porte

Deciso il 14 novembre 2026.

- **I muri (il kernel).** Il launcher fa partire ogni app in un recinto fatto con gli
  strumenti di Linux (namespaces, seccomp, cgroups). Dentro l'app vede **solo** la sua cartella
  del pacchetto, la sua cartella dei dati e, in sola lettura, font, icone e sfondi del sistema
  (`/system/resources`, come l'iPhone). Non vede il disco della persona, la rete, gli altri
  programmi né gli indirizzi dei servizi.
- **Le porte (i servizi).** Tutto il resto, anche i file della persona e la rete, si chiede dai
  canali. Il servizio controlla il permesso e **non fa da tramite per i dati**: apre il file o la
  connessione e la consegna all'app già aperta (Linux passa un file aperto da un programma
  all'altro). Da lì l'app legge, scrive e trasmette alla velocità normale; il costo è un messaggio
  solo all'apertura. Come la finestra "Apri" del Mac e i portali di Flatpak.
- **Rischi che restano e come si chiudono:**
  1. *un servizio che si fa ingannare* (esempio: chiede "Home/../../data/system/..." e il servizio
     apre il file con i suoi poteri): i percorsi si aprono con il modo di Linux che rifiuta da sé
     ogni percorso che esce dalla cartella, anche tramite collegamenti (`openat2` con
     `RESOLVE_BENEATH`); ogni servizio gira con i poteri minimi (il servizio dei file di una
     persona gira come quella persona, non come sistema); prove automatiche con richieste sbagliate
     o maligne per ogni servizio;
  2. *il file consegnato con poteri sbagliati o che dura troppo*: il servizio apre sempre con i
     poteri esatti del permesso (solo lettura se il permesso è `files.read`); per la revoca, l'app
     si chiude (sezione "Come si chiedono");
  3. *la rete*: dentro il recinto l'app non ha nessuna rete sua e usa solo le connessioni che
     riceve; il firewall riconosce l'app dal suo gruppo (cgroup) e applica le regole per app anche
     se i dati non passano dal servizio;
  4. *il launcher è il pezzo più potente* (recinti, canali, identità): va tenuto piccolo
     e semplice, e solo lui fa questo lavoro;
  5. *un'app che si prende tutte le risorse*: limiti per gruppo su memoria, processore e spazio
     dei suoi dati;
  6. *un errore nel kernel che fa uscire dal recinto*: all'app si vietano le chiamate al kernel
     che non le servono, e il kernel si tiene aggiornato (aggiornamenti A/B);
  7. *due app d'accordo* che hanno tutte e due la rete e si parlano via internet: rischio accettato,
     come su Mac e Android.

### I permessi sulle cartelle: il kernel aggancia le cartelle concesse

Deciso dalla persona il 16 novembre 2026.

- **Un permesso sui file è un permesso su una cartella**, in uno di due modi: **lettura** oppure
  **lettura e scrittura**. Vale per la cartella, per tutti i file e per tutte le sottocartelle
  dentro di lei. Non ci sono permessi su singoli file.
- **Basta dichiararlo e il kernel lo fa rispettare.** L'app scrive nel manifesto quali cartelle le
  servono e in che modo; la persona le concede (come ogni permesso, sezione "Come si chiedono").
  Non serve nessun servizio in mezzo, e non conta se l'app usa CoreKit o no.
- **Come lo fa il kernel: un aggancio (bind mount) per ogni cartella concessa.** Quando il launcher
  costruisce il recinto, aggancia dentro il mondo dell'app ogni cartella concessa, con il segno di
  sola lettura se il modo è "lettura". L'app la vede e la sfoglia come una cartella normale, alla
  velocità normale. Dettagli tecnici:
  1. ogni processo ha il suo elenco dei montaggi (mount namespace): il launcher ne fa una copia
     privata per l'app (`unshare(CLONE_NEWNS)`, montaggi resi privati), e quello che aggancia lì
     lo vede solo l'app e i suoi figli;
  2. un aggancio è "quel disco, da quella cartella in giù": seguendo i nomi il kernel non sale mai
     sopra la cartella di partenza, e ".." dalla cima dell'aggancio torna nel mondo dell'app, non
     nel disco vero;
  3. il segno di sola lettura è dell'aggancio: il kernel rifiuta ogni scrittura, creazione,
     cancellazione e cambio di nome lì dentro, anche se il file in sé sarebbe scrivibile. Sotto
     restano i permessi normali di Unix (proprietario, lettura/scrittura): due muri;
  4. il mount che vedeva tutto il disco viene staccato dal mondo dell'app (`pivot_root`, poi via la
     vecchia radice), e l'app non può montare niente da sola (ha perso i poteri e il disco come
     dispositivo nel suo mondo non c'è). Lo mostra la prova `recinto` (`STATO.md`).
- **Mai cartelle consegnate aperte.** Una cartella vera già aperta, data all'app, permette di
  risalire con ".." nel disco vero: i servizi consegnano solo file. Le cartelle passano solo
  dagli agganci del launcher.
- **I collegamenti** dentro una cartella concessa che puntano fuori si seguono nel mondo dell'app:
  non portano da nessuna parte. Va bene così.

**Per ora solo la Home** (decisione della persona, 16 novembre 2026): tra le cartelle fuori da
quelle dell'app (pacchetto e dati), il launcher può agganciare **soltanto la Home della persona che
sta usando il computer**, intera, in lettura oppure in lettura e scrittura. Nessun'altra cartella.
Si chiede con i permessi di sempre (deciso dalla persona lo stesso giorno): `files.read` = Home in
lettura, `files.write` = Home in lettura e scrittura (sezione 9). Dentro il recinto la Home si vede allo stesso percorso di fuori (`/data/users/<uid>/home`), così le
app non cambiano. I punti qui sotto restano per quando si allargherà.

**Da decidere** (16 novembre 2026):
1. come si scrivono nel manifesto le cartelle e il modo (per esempio relative alla Home:
   "Documents", oppure tutta la Home), e se questi permessi prendono il posto di `files.read` e
   `files.write` (sezione 9);
2. cartelle una dentro l'altra con modi diversi (per esempio la Home in lettura e "Documents" in
   scrittura): si aggancia prima la più grande e poi la più piccola sopra, e vince la più interna;
   funziona (prova `recinto`, 16 novembre 2026: Home in lettura con una sottocartella in
   scrittura; ".." non esce, nemmeno da una cartella aperta dentro l'aggancio);
3. una cartella dichiarata che non c'è: il launcher la crea o la salta;
4. una cartella concessa mentre l'app è aperta: vale dal prossimo avvio, oppure si aggancia dal
   vivo (il kernel lo permette con un punto preparato apposta nel recinto, come fa il Mac con la
   finestra "Apri");
5. un secondo muro con Landlock (regole del kernel sui percorsi, che non si tolgono più): da
   aggiungere dopo, se il nostro kernel lo ha acceso.

### Problemi che l'idea porta con sé (da risolvere, 14 novembre 2026)

Oggi il recinto non c'è: le app vedono tutto il disco (il controllo dei permessi nei servizi e le
porte invece ci sono, dal 16 novembre 2026). Passare al recinto tocca quasi ogni pezzo:

1. ~~**Le cartelle consegnate aperte non sono sicure.**~~ Deciso il 16 novembre 2026: i servizi
   consegnano solo file; le cartelle concesse le aggancia il launcher dentro il recinto (sezione
   "I permessi sulle cartelle" qui sopra). Il servizio dei file resta per le app senza permesso
   (la finestra "Apri").
2. **La finestra "Apri"/"Salva" del sistema non esiste.** Senza, un'app senza permessi sui file non
   può aprire niente.
3. **La finestra di domanda del sistema non esiste** (serve anche al firewall e al primo avvio
   per i permessi).
4. **Come si trovano i servizi.** Oggi ogni programma si collega da sé agli indirizzi in
   `/run/mshell`. Col recinto li apre il launcher e li passa all'app all'avvio: cambiano
   il launcher, CoreKit (come trova i canali) e il compositore (anche il canale verso il
   compositore va passato così).
5. **Come il servizio sa chi è l'app.** Oggi lo ricava dal kernel (uid e percorso del programma).
   Dentro il recinto il percorso visto è diverso: l'identità la dice il launcher quando
   apre il canale, e il servizio si fida solo di lui.
6. **Le app del sistema che oggi guardano tutto.** Attività legge l'elenco dei programmi da `/proc`
   (nel recinto non c'è: serve un servizio dei processi); lo Store scrive in `/data/apps`; il
   Terminale apre una shell come sistema. Vanno rifatte sulle porte, o tenute fuori dal recinto
   (da decidere: niente eccezioni sarebbe meglio).
7. **La rete dentro l'app.** Senza rete propria l'app non legge i server DNS né apre connessioni:
   il servizio di rete deve sapere "collegami a questo nome e porta" e consegnare la connessione;
   per ricevere connessioni da fuori c'è la richiesta al firewall (sezione 7).
8. **I comandi non sono nel recinto.** Un comando lanciato dalla persona ha tutti i suoi poteri
   (come su Linux e Mac): `curl` usa la rete direttamente. Due modelli diversi convivono; come si
   distribuiscono e con quali poteri girano i comandi degli altri è già una domanda aperta
   (`SISTEMA.md`).
9. **I permessi `apps.open` e `system.shell`**: da capire come gestirli (sezione 12).
10. **SQLite dentro i servizi**: i programmi sono statici, circa 1 MB in più per ogni programma che
    lo apre; dipende da chi apre il database (sezione 8).
11. **Prove.** La sandbox Docker deve poter creare i recinti (namespaces dentro Docker chiedono
    permessi in più); servono prove che un'app non veda davvero niente e che le richieste maligne
    vengano rifiutate.
12. **Fare debug di un'app** diventa più difficile (non vede `/proc`, non scrive dove vuole): i suoi
    messaggi devono andare nel registro del sistema.

**Ordine dei lavori proposto:** il database del sistema e il controllo vero dei permessi
(`ha_permesso`); poi il launcher che passa i canali; poi il recinto; poi le finestre del
sistema ("Apri", domande). Il firewall che risponde alle app viene dopo il controllo dei permessi
(`SISTEMA.md`, sezione 4).
- **Niente token.** Un token è un testo che si può copiare e rubare. Il canale invece è un oggetto
  del kernel e non si copia. I token servirebbero solo per servizi su internet.
- **Il toolkit nasconde tutto questo**: chi sviluppa scrive "leggi questo file" e il toolkit trova
  il canale. Se il permesso manca, riceve un errore chiaro, non un crash.
- Il toolkit **non protegge** niente: è codice dentro l'app e si può aggirare. La protezione viene
  dal sistema (canali e identità) e dall'isolamento.

### La catena della sicurezza

**firma legata al codice → identità dell'app → canali solo per quello che è dichiarato e concesso
→ isolamento che impedisce alle app di passarsi i canali.**

- La firma garantisce che "l'app X" sia davvero quel codice. Le app non se la possono scambiare.
- Un'app non può eseguire codice che non stava nel suo pacchetto. Questo chiude la strada dei
  programmi figli che ereditano i canali.
- Rischi che restano (nessun sistema li elimina del tutto): un errore nel kernel che permette di
  uscire dall'isolamento, un servizio che non controlla bene le richieste, due app che si mettono
  d'accordo (per esempio via rete) per aggirare i permessi. Come si chiudono: sezione "L'app non
  vede niente" qui sopra.

## 6. Le app tra loro

- **Le app non si parlano mai direttamente.** Chiedono al sistema (per esempio "condividi questo
  file" o "manda una mail"), e il sistema consegna il dato all'app del ruolo giusto o a quella che
  sceglie la persona.

## 7. Ruoli

- Un ruolo (per esempio "mail" o "firewall") **non** riguarda le persone e i loro permessi: è un
  insieme di funzioni che un'app può offrire.
- Ogni ruolo ha un **minimo di funzioni obbligatorie**. Un'app dichiara quali funzioni offre; se
  non copre tutte quelle obbligatorie, non viene proposta per quel ruolo.
- La persona può installare un'app e dedicarla a un ruolo.
- Nel manifesto, **i permessi** (cosa l'app chiede al sistema) e **le funzioni** (cosa l'app offre)
  sono due elenchi separati.
- Il sistema non può controllare che una funzione ci sia davvero: controlla che sia dichiarata e
  che l'app risponda alle richieste nel modo stabilito. Per questo ogni funzione va definita con
  precisione.

## 8. Dati

- **Database dell'app**: ogni app ha a disposizione un piccolo database SQLite, uno per ogni persona
  che la usa, nella sua cartella dei dati (`/data/users/<uid>/appsdata/<app>`). È **offerto, non
  obbligatorio**: l'app può anche salvare file normali.
- **Database del sistema**: uno solo per tutto il sistema (deciso il 14 novembre 2026, al posto di
  un database del sistema per ogni app; come il Mac e Android), SQLite. Contiene per tutte le app i
  permessi concessi, se il primo avvio è già avvenuto, i ruoli assegnati, le risposte "questa app
  può ricevere connessioni su questa porta"; forse anche l'elenco delle app installate. Sta **fuori
  dal mondo dell'app**, nella parte del disco che solo il sistema può toccare (`/data/system`).
  Non può stare nella cartella dell'app, perché l'app potrebbe modificarlo (per esempio
  concedersi i permessi da sola). Contro i guasti: SQLite regge gli spegnimenti a metà scrittura,
  più una copia di riserva ogni tanto. Da decidere: se i permessi valgono per ogni persona o per
  tutto il computer, e chi apre il database (un custode solo a cui gli altri servizi chiedono,
  oppure ogni servizio lo legge da sé e scrive solo il launcher).
- Il toolkit mostra le due cose come una sola: l'app può chiedere "che permessi ho?", ma non può
  cambiarli.

## 9. Elenco minimo di servizi, permessi, deviazioni e ruoli

Scritta il 12 novembre 2026 su incarico della persona e **confermata** lo stesso giorno, compresi i
permessi di sistema riservati per ora alle app del sistema. È volutamente piccola: si allarga
quando arriva un'app che ha davvero bisogno di qualcosa in più.

**Principio:** quello che fa ogni app normale non chiede permessi. Si chiede il permesso solo per
quello che tocca le cose della persona o il sistema intero.

### La base: ogni app ce l'ha, senza chiedere

| Cosa | Servizio | Note |
|---|---|---|
| Aprire finestre, disegnarci, ricevere tastiera e mouse nelle proprie finestre | compositore | lo standard di ogni app |
| La propria cartella dei dati e il proprio database | (disco) | una per persona, nel suo mondo isolato |
| Leggere tema (chiaro/scuro), sfondo, lingua, ora | catalog (impostazioni), ora | solo lettura |
| Usare font e icone del sistema | (disco) | `/system/resources`, sola lettura |
| Aprire **un file scelto dalla persona** | selettore dei file | il sistema mostra la finestra "Apri"/"Salva"; l'app riceve solo quel file. Così la maggior parte delle app non ha bisogno del permesso sui file (come macOS e iPhone) |
| Chiedere al sistema di condividere o di passare una richiesta a un ruolo | sistema | è la persona a scegliere a chi va |

### Permessi normali: li concede la persona al primo avvio

| Permesso | Cosa permette |
|---|---|
| `files.read` | leggere tutti i file della persona: il launcher aggancia la sua Home nel recinto in sola lettura (sezione 5, "I permessi sulle cartelle"; deciso il 16 novembre 2026) |
| `files.write` | leggere e modificare tutti i file della persona (comprende `files.read`): il launcher aggancia la sua Home in lettura e scrittura |
| `notifications` | mandare notifiche |
| `network.use` | usare la rete e internet (oggi l'app non vede nessuna rete) |
| `audio` | riprodurre suoni (quando il sistema avrà l'audio) |

### Permessi di sistema: poteri sul sistema intero

Toccano tutte le app o tutto il computer. Oggi li ricevono solo le app del sistema
(in `/system/apps`); quando ci saranno le firme, anche le app firmate da uno sviluppatore
autorizzato ("chiavi a due livelli").

| Permesso | Cosa permette | Chi lo usa oggi |
|---|---|---|
| `processes.view` | vedere i programmi in esecuzione | — |
| `processes.stop` | vederli e fermarli (comprende `processes.view`) | Attività |
| `network.settings` | riavviare o fermare il servizio di rete | — |
| `network.firewall` | controllo pieno del firewall. Lo esercita **una sola app alla volta**: quella che la persona sceglie per il ruolo firewall (sezione 7); le altre app con questo permesso non possono usarlo finché non vengono scelte | — |
| `settings.system` | cambiare le impostazioni del sistema (tema, lingua, schermo) | Impostazioni |
| `system.power` | spegnere e riavviare il computer | (scrivania) |
| `apps.manage` | installare, aggiornare e togliere app | Store |
| `apps.open` | far partire un programma di `/system/bin` come la persona | — (da capire come gestirlo: sezione 12) |
| `system.shell` | aprire la shell come sistema | Terminale (da capire come gestirlo: sezione 12) |

### Permessi che i servizi chiedono dalla porta

Il controllo è vero dal 16 novembre 2026 (deciso con la persona). I servizi che usano lo standard
dei messaggi (`SISTEMA.md`, sezione 3) chiedono a `ha_permesso` se chi chiede ha un permesso:
- il sistema (uid 0) li ha tutti;
- un'app ha quelli che il launcher le concede quando la fa partire e passa ai servizi con le
  porte. **Finché non c'è la domanda del primo avvio, concessi = dichiarati nel manifesto**;
  quelli di sistema solo alle app del sistema (`/system/apps`), e un nome sconosciuto non vale
  (il launcher scrive nel registro quelli che toglie);
- un altro programma della persona (un comando) ha i permessi normali, nessuno di sistema.
Ogni rifiuto si scrive nel registro (`permesso <nome> a <programma> (uid <uid>): negato`). La
prova `applicazioni` controlla i rifiuti con Esempio, che non ha `settings.system` né
`network.settings`.

| Permesso | Servizio | Cosa permette | Chi ce l'ha oggi |
|---|---|---|---|
| `apps.manage` | launcher | installare, aggiornare e togliere app, svuotarne i dati | lo Store |
| `apps.open` | launcher | far partire un programma di `/system/bin` come la persona | nessuna app |
| `system.shell` | launcher | aprire la shell come sistema (il Terminale) | il Terminale (provvisorio: `STATO.md`) |
| `settings.system` | catalog | cambiare tema e sfondo | Impostazioni |
| `network.use` | network | chiedere lo stato della rete (e avere la porta verso network) | Esempio (prova) |
| `network.settings` | network | riavviare e fermare la rete | nessuna app |

### Deviazioni dallo standard: si dichiarano, la persona viene informata

| Deviazione | Cosa vuol dire | Cosa vede la persona |
|---|---|---|
| `senza_finestra` | l'app non mostra nessuna interfaccia | un avviso all'installazione; l'app compare tra quelle attive in Attività |
| `sottofondo` | l'app resta accesa dopo che chiudi la finestra | un avviso all'installazione e un segno sempre visibile finché è accesa |
| `avvio_automatico` | l'app parte da sola quando la persona entra | un avviso all'installazione; si spegne dalle Impostazioni |
| `schermo_intero` | l'app può coprire tutto lo schermo, barra compresa | un avviso all'installazione; c'è sempre un tasto per uscire |

### Ruoli e funzioni obbligatorie

Ogni funzione dice cosa riceve l'app e cosa deve fare. Un'app viene proposta per un ruolo solo se
offre **tutte** le funzioni obbligatorie.

| Ruolo | Funzioni obbligatorie | Esempio oggi |
|---|---|---|
| **file** (gestore dei file) | `file.apri_cartella` (riceve un percorso, mostra la cartella) · `file.mostra` (riceve un file, lo mostra selezionato nella sua cartella) | Files |
| **browser** | `web.apri` (riceve un indirizzo http/https e lo apre) | — |
| **mail** | `mail.scrivi` (riceve destinatari, oggetto, testo e allegati facoltativi, e apre una mail nuova pronta da mandare; non la manda da sola) | — |
| **immagini** | `immagine.apri` (riceve un file immagine e lo mostra) | — |
| **testo** | `testo.apri` (riceve un file di testo e lo apre per modificarlo) · `testo.nuovo` (apre un testo vuoto) | — |
| **terminale** | `terminale.apri` (apre un terminale in una cartella data) | — |
| **firewall** | `firewall.porta` (riceve "<app> vuole accettare connessioni sulla porta X" e risponde sì o no, eventualmente chiedendo alla persona). In più il permesso `network.firewall` | — (senza app vale la configurazione predefinita) |

Il ruolo **firewall** (deciso il 14 novembre 2026) non passa da un modulo aggiuntivo ma dal
permesso `network.firewall`: l'app scelta dalla persona chiede le regole al servizio network, che le
controlla, le applica e le conserva anche ad app chiusa (come firewalld con polkit su Linux). Una sola
app alla volta, perché due app con il controllo pieno si scontrerebbero (come Windows, che spegne il
suo firewall quando un altro prende il posto, o Android, che tiene attiva una sola VPN). Le funzioni
L'app scelta prende il posto degli interruttori di base del firewall; tolta l'app, si torna a quelli
(come Windows).

**Le app che vogliono ricevere connessioni da fuori** (per esempio un gioco in rete che accoglie altri
giocatori) non scrivono regole e non decidono niente: **chiedono al firewall** di poter accettare
connessioni in entrata su una porta. Decide solo il firewall (l'app scelta per il ruolo, o il servizio
network con gli interruttori di base se nessuna app è scelta), così non c'è mai scontro. Deciso il 14
novembre 2026; come il Mac e Windows, che chiedono "vuoi che questa app accetti connessioni in
entrata?". Senza app scelta vale sempre la configurazione predefinita, che chiede alla persona e
ricorda la risposta (`SISTEMA.md`, sezione 4).

**Funzione obbligatoria del ruolo firewall** (deciso il 14 novembre 2026): rispondere alle richieste
di aprire una porta (riceve "<app> vuole accettare connessioni sulla porta X", risponde sì o no,
eventualmente chiedendo alla persona). Un'app che non la offre non viene proposta per il ruolo.
Altri ruoli che devono lavorare per tutto il computer passano dai moduli aggiuntivi (sezione 10).

### Le app di oggi con questo elenco

| App | Permessi | Deviazioni | Ruoli |
|---|---|---|---|
| Files | `files.write` | — | file |
| Attività | `processes.stop` (di sistema) | — | — |
| Impostazioni | `settings.system` (di sistema) | — | — |
| Store | `apps.manage` (di sistema) | — | — |
| Doom | — | — | — (i salvataggi stanno nella sua cartella dei dati) |
| Contatore, Esempio (solo per le prove: `utente/prove/applicazioni/`) | — | — | — |

## 10. Moduli aggiuntivi (integrazione speciale con il sistema)

Deciso il 12 novembre 2026. La persona installa **solo app**. Alcune app però hanno bisogno di
un'integrazione speciale con il sistema, per esempio un firewall che deve partire con il computer e
controllare la rete di tutti. Per queste un'app può portarsi dietro un **modulo aggiuntivo**
(come le estensioni di sistema del Mac o il servizio VPN di Android).

### Lavoro in sottofondo: due casi diversi

- **Per una persona** (la posta che controlla le mail, la musica che continua a suonare): è la
  deviazione `sottofondo` dell'elenco. Gira nel mondo isolato di quella persona, finché la persona è
  dentro. Si dichiara, e la persona viene informata.
- **Per tutto il computer** (VPN, backup di tutto il disco): solo con un modulo aggiuntivo. Il firewall no: passa dal permesso `network.firewall` e dal ruolo (sezione 9).

### Come funziona

1. **Il sistema decide i punti di aggancio**: un elenco fisso. Ogni punto dice cosa riceve il modulo,
   cosa può fare e cosa non può fare. Un modulo si aggancia solo a uno di questi punti.
2. **Il modulo sta dentro il pacchetto `.app`**, accanto all'app, e il manifesto lo dichiara. Per la
   persona resta un solo file e una sola icona.
3. **Il modulo gira da solo, separato dall'app.** Lo avvia il supervisore dei servizi all'accensione,
   con una sua identità, senza finestra e con i soli poteri del suo punto di aggancio.
4. **L'app è il pannello di controllo del modulo.** I due si parlano solo attraverso il sistema.
5. **Installare un modulo richiede un'approvazione a parte**, con un avviso chiaro (per esempio
   "questa app vuole controllare la rete di tutto il computer"). Per ora solo le app del sistema;
   più avanti anche quelle firmate da uno sviluppatore autorizzato (chiavi a due livelli).
6. **Togli l'app e se ne va anche il modulo.**

### Condizioni di sicurezza

Il sistema si fida di codice esterno, ma in modo limitato e controllato:

1. **Il modulo fa solo quello che il suo punto di aggancio permette**: isolato, senza accesso ai file,
   allo schermo o alle altre app, con limiti di memoria e processore e un filtro delle chiamate al kernel.
2. **Il modulo propone, il sistema decide.** Per esempio, il modulo del firewall dà un elenco di
   regole; il sistema controlla che siano valide e le applica lui. Il modulo non tocca mai la rete.
3. **Il danno massimo è limitato** a quello che il punto di aggancio permette (per il firewall:
   bloccare troppo o lasciar passare troppo, non rubare dati).
4. **Via d'uscita sicura**: se il modulo si blocca o non risponde, il sistema torna a un comportamento
   di base; la persona può spegnerlo dalle Impostazioni.
5. **Si sa chi lo ha fatto**: firma, registro con le scadenze, possibilità di revocare uno sviluppatore.
6. **La persona approva** con un avviso a parte rispetto ai permessi normali.

Rimandato: un modulo che vede ogni singola connessione e decide caso per caso. È più potente ma
vede informazioni private (con chi parla ogni app); si valuterà solo dopo le firme.

### Punti di aggancio

**Nessuno per ora.** Si aggiungono uno alla volta, solo quando arriva un'app che ne ha davvero
bisogno. Il firewall non ne ha bisogno (passa dal permesso, sezione 9): gli esempi su firewall qui sopra restano solo per spiegare le condizioni.

## 11. Lo Store

Un'app del sistema (`store`, con il permesso `apps.manage`) per **installare** un'app aprendo un file
`.app`, **aggiornarla** aprendo un file `.app` più nuovo, **vedere** le app installate con la loro
scheda e **togliere** un'app installata dalla persona (le app del sistema non si possono togliere).
Il disegno è sulla pagina "App – Store" di Figma.

- **Togli app** (solo app installate dalla persona): una finestra di conferma, come su iPhone, chiede
  se togliere **l'app e i suoi dati** oppure **solo l'app**; in questo caso i dati restano e una
  reinstallazione li ritrova.
- **Svuota dati** (per ogni app, anche del sistema): un pulsante a parte, con conferma, che cancella i
  dati salvati dall'app per la persona; l'app resta installata e riparte da zero.
- Un'app aperta non si toglie e non si svuota: prima va chiusa.
- Nello Store le deviazioni dallo standard si chiamano **"Cosa fa di diverso"**.
- Per ora lo Store non cerca da solo gli aggiornamenti e non ha un pulsante "Installa da file…":
  si installa e si aggiorna solo aprendo il file.

## 12. Sul tavolo

Da decidere, più o meno in quest'ordine:

1. **I tipi di file che un'app sa aprire**, dichiarati nel manifesto.
2. **La firma**: un registro delle firme degli sviluppatori con le loro scadenze, la verifica del
   pacchetto, e il controllo che un aggiornamento venga dallo stesso autore.
3. **L'identificativo definitivo** delle app (per esempio un indirizzo al contrario come
   `it.mep.files`).
4. **Le app la cui versione esce dal supporto**: avviso prima di aggiornare il sistema, oppure app
   che resta installata ma non si apre e spiega perché.
5. **Le librerie** per disegnare gli SVG (per esempio nanosvg, piccola, o resvg, completa) e per
   leggere gli zip (per esempio miniz).
6. **Cosa vede un eseguibile normale avviato dalla scrivania**: oggi ha tutti i poteri della
    persona. macOS, per esempio, avvisa al primo avvio dei file scaricati.
7. **I permessi `apps.open` e `system.shell`** (aggiunti all'elenco il 14 novembre 2026): capire
   come gestirli. Chi li riceve (solo le app del sistema? anche le altre, chiedendo alla persona?),
   se `system.shell` deve restare un permesso di un'app o diventare una conferma della persona
   ogni volta (come `sudo` o la finestra della password del Mac), e se `apps.open` serve davvero.
