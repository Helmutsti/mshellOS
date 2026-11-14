# Il sistema: chi fa cosa

Come sono divisi i compiti tra i programmi di mshell e come si parlano. Vale per chi scrive un
servizio, un comando o un'app. Le regole delle app (pacchetti, permessi, isolamento) stanno in
`APPLICAZIONI.md`; qui solo quello che serve per capire il disegno d'insieme.

## 1. I ruoli

| Ruolo | Cosa fa | Chi è oggi |
|---|---|---|
| **init** | Il processo 1. Fa partire i servizi, presta loro le risorse (schermo, nomi), li sorveglia e li rifà partire se cadono. Nient'altro. | `/system/init` |
| **servizi** | Ognuno **possiede** una cosa del computer ed è l'**unico** a toccarla. La offre agli altri da una **porta** e controlla i permessi di chi chiede. Possiede anche la sua configurazione. | eventi (tastiera e mouse), compositore (schermo e finestre), launcher (avvio e installazione delle app), network (rete e firewall), scrivania |
| **app** | Programmi grafici in un pacchetto `.app`, isolati, con i permessi del manifesto. Disegnano la loro interfaccia e la danno al compositore. Per tutto il resto chiedono ai servizi dalle porte. | Files, Attività, Impostazioni, Store, Terminale... |
| **comandi** | Programmi senza grafica, per la riga di comando. Parlano con le **stesse porte** delle app, con i permessi della persona che li lancia. Non sono app (`APPLICAZIONI.md`, sezione 2). | `/system/bin/webask` (il primo: chiede una pagina web, solo HTTP; sorgenti in `utente/comandi/`, uno per file), e la shell |
| **shell** | Un comando come gli altri: legge righe e fa partire comandi. Non ha nessun ruolo speciale e nessuno passa da lei per parlare con il sistema. Il Terminale è solo l'app che la ospita. | `/system/bin/shell` (di prova) |

Dettagli sui servizi di oggi:
- **eventi** legge tastiera e mouse e li manda in un solo tubo, al compositore. È il compositore a passare ogni evento all'app in primo piano.
- **la shell sulla seriale** la fa partire init, solo se la seriale c'è. Serve per lo sviluppo.

## 2. Un servizio, tanti clienti

La regola: **una cosa del computer ha un solo padrone**, il suo servizio. Tutti gli altri sono
clienti e chiedono a lui. Nessuno, tranne il servizio, modifica i suoi file di configurazione o
tocca la sua parte del kernel.

Esempio, la rete:
1. il servizio network è l'unico che tocca la scheda di rete, il firewall e i file in `/system/config/network/` (e, più avanti, la configurazione personale in `/data`);
2. offre una porta, dove si chiede "com'è la rete?" o "metti questo indirizzo fisso";
3. la pagina della rete nelle Impostazioni è un cliente: chiede alla porta e mostra;
4. un comando `network` è un altro cliente: chiede alla stessa porta e scrive testo;
5. un'app firewall (con il suo modulo aggiuntivo, `APPLICAZIONI.md` sezione 10) è un terzo cliente, con il suo permesso;
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

Il primo a usarlo è il launcher (aprire le app e i file, installare e togliere i pacchetti,
aprire la shell): i dati grandi (il resoconto di un pacchetto, l'elenco delle app) viaggiano
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

**Permessi (provvisori).** Il servizio chiede `ha_permesso(chi chiede, "nome.permesso")` (es.
`network.settings`). **Oggi risponde sempre sì** e scrive la domanda nel registro, con la parola
"provvisorio". Quando sarà vera riconoscerà sistema, app (manifesto e permessi concessi) e
comando della persona: il commento in `messaggi.c` dice come. La usa il launcher
(`apps.manage`, `programs.open`, `system.shell`: `APPLICAZIONI.md`, sezione 9).

Limite: un messaggio è al massimo 5120 byte (lo stesso di oggi per tutti i messaggi). La prova è `test/prove/messaggi.py`.

## 4. Domande aperte

- **Comandi distribuiti dalla persona.** Già deciso: i comandi della persona stanno in `/data/users/<uid>/bin`, cercati dopo `/system/bin`. Da decidere: come si distribuiscono (un'app che li dichiara nel manifesto, un pacchetto di soli comandi, o solo copia a mano) e con quali poteri girano. Oggi un comando ha tutti i permessi della persona, come su Linux e Mac: va bene per i propri, è un rischio per quelli di altri.
- **Permessi dei comandi.** Un'app ha il manifesto, un comando no: si parte dai permessi della persona che lo lancia.
- **La shell come sistema.** Oggi il Terminale apre una shell come sistema, e chiunque può chiederla (decisione provvisoria, `STATO.md`). Quando le porte controlleranno i permessi, la shell girerà come la persona, e le azioni di sistema passeranno dalla porta del servizio, che chiede conferma (come `sudo` o la finestra della password sul Mac).
