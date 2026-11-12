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
- **I dettagli** (per esempio, per i file, "solo leggere" o "anche scrivere") li controlla il
  servizio, in base all'identità dell'app.
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
  d'accordo (per esempio via rete) per aggirare i permessi.

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
- **Database del sistema sull'app**: c'è sempre. Contiene i permessi concessi, se il primo avvio è
  già avvenuto, i ruoli assegnati. Sta **fuori dal mondo dell'app**, nella parte del disco che solo
  il sistema può toccare (`/data/system`). Non può stare nel file dell'app, perché l'app potrebbe
  modificarlo (per esempio concedersi i permessi da sola).
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
| Leggere tema (chiaro/scuro), lingua, ora | impostazioni, ora | solo lettura |
| Usare font e icone del sistema | (disco) | `/system/resources`, sola lettura |
| Aprire **un file scelto dalla persona** | selettore dei file | il sistema mostra la finestra "Apri"/"Salva"; l'app riceve solo quel file. Così la maggior parte delle app non ha bisogno del permesso sui file (come macOS e iPhone) |
| Chiedere al sistema di condividere o di passare una richiesta a un ruolo | sistema | è la persona a scegliere a chi va |

### Permessi normali: li concede la persona al primo avvio

| Permesso | Cosa permette |
|---|---|
| `file.leggere` | leggere tutti i file della persona (la sua Home) |
| `file.scrivere` | leggere e modificare tutti i file della persona (comprende `file.leggere`) |
| `notifiche` | mandare notifiche |
| `rete` | usare la rete e internet (oggi l'app non vede nessuna rete) |
| `audio` | riprodurre suoni (quando il sistema avrà l'audio) |

### Permessi di sistema: poteri sul sistema intero

Toccano tutte le app o tutto il computer. Oggi li ricevono solo le app del sistema
(in `/system/apps`); quando ci saranno le firme, anche le app firmate da uno sviluppatore
autorizzato ("chiavi a due livelli").

| Permesso | Cosa permette | Chi lo usa oggi |
|---|---|---|
| `processi` | vedere i programmi in esecuzione e fermarli | Attività |
| `impostazioni.sistema` | cambiare le impostazioni del sistema (tema, lingua, schermo) | Impostazioni |
| `spegnimento` | spegnere e riavviare il computer | (scrivania) |
| `app.gestire` | installare, aggiornare e togliere app | (il futuro installatore) |

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

Ruoli come **firewall**, che devono lavorare per tutto il computer, passano dai moduli aggiuntivi
(sezione 10): il ruolo è coperto da un'app che ha un modulo sul punto di aggancio giusto, più la sua
finestra come pannello di controllo. Non sono nell'elenco finché non esiste quel punto di aggancio.

### Le app di oggi con questo elenco

| App | Permessi | Deviazioni | Ruoli |
|---|---|---|---|
| Files | `file.scrivere` | — | file |
| Attività | `processi` (di sistema) | — | — |
| Impostazioni | `impostazioni.sistema` (di sistema) | — | — |
| Doom | — | — | — (i salvataggi stanno nella sua cartella dei dati) |
| Contatore, Esempio | — | — | — |

## 10. Moduli aggiuntivi (integrazione speciale con il sistema)

Deciso il 12 novembre 2026. La persona installa **solo app**. Alcune app però hanno bisogno di
un'integrazione speciale con il sistema, per esempio un firewall che deve partire con il computer e
controllare la rete di tutti. Per queste un'app può portarsi dietro un **modulo aggiuntivo**
(come le estensioni di sistema del Mac o il servizio VPN di Android).

### Lavoro in sottofondo: due casi diversi

- **Per una persona** (la posta che controlla le mail, la musica che continua a suonare): è la
  deviazione `sottofondo` dell'elenco. Gira nel mondo isolato di quella persona, finché la persona è
  dentro. Si dichiara, e la persona viene informata.
- **Per tutto il computer** (firewall, VPN, backup di tutto il disco): solo con un modulo aggiuntivo.

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
bisogno. Il primo candidato è il "filtro della rete", per il firewall.

## 11. Sul tavolo

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
