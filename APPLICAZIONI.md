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

- Un'app si distribuisce come **un file unico con estensione `.app`**.
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

Prima proposta, scritta il 12 novembre 2026 su incarico della persona. È volutamente piccola: si
allarga quando arriva un'app che ha davvero bisogno di qualcosa in più.

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

Toccano tutte le app o tutto il computer. Proposta: oggi li ricevono solo le app del sistema
(in `/system/apps`); quando ci saranno le firme, anche le app firmate da uno sviluppatore
autorizzato. È la proposta delle "chiavi a due livelli", ancora da confermare (vedi sotto).

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

Il ruolo **firewall** resta fuori finché non si decide il lavoro in sottofondo: un firewall deve
restare sempre acceso e controllare la rete di tutto il sistema.

### Le app di oggi con questo elenco

| App | Permessi | Deviazioni | Ruoli |
|---|---|---|---|
| Files | `file.scrivere` | — | file |
| Attività | `processi` (di sistema) | — | — |
| Impostazioni | `impostazioni.sistema` (di sistema) | — | — |
| Doom | — | — | — (i salvataggi stanno nella sua cartella dei dati) |
| Contatore, Esempio | — | — | — |

## 10. Sul tavolo

Da decidere, più o meno in quest'ordine:

1. **Confermare l'elenco minimo** qui sopra, in particolare i permessi di sistema solo per le app
   del sistema.
2. **Il lavoro in sottofondo**: se un'app lo può dichiarare liberamente, solo per i ruoli che lo
   prevedono, o solo se firmata da uno sviluppatore autorizzato (chiavi a due livelli). Riguarda
   ruoli come "firewall", che non possono essere app che si aprono e si chiudono con la finestra.
3. **I tipi di file che un'app sa aprire**, dichiarati nel manifesto.
4. **La firma**: un registro delle firme degli sviluppatori con le loro scadenze, la verifica del
   pacchetto, e il controllo che un aggiornamento venga dallo stesso autore.
5. **L'identificativo definitivo** delle app (per esempio un indirizzo al contrario come
   `it.mep.files`).
6. **Le app la cui versione esce dal supporto**: avviso prima di aggiornare il sistema, oppure app
   che resta installata ma non si apre e spiega perché.
7. **Com'è scritto il manifesto** (oggi righe "chiave = valore") e **che tipo di archivio** è il file `.app`.
8. **Il nome `.app`**: su macOS è già usato per le app (che lì sono cartelle). Un nostro pacchetto
   copiato su Mac apparirebbe come un'app rotta.
9. **La libreria per disegnare gli SVG** (per esempio nanosvg, piccola, o resvg, completa).
10. **Cosa vede un eseguibile normale avviato dalla scrivania**: oggi ha tutti i poteri della
    persona. macOS, per esempio, avvisa al primo avvio dei file scaricati.
