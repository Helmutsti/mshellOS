/* CoreKit, del sistema: il launcher e le richieste che gli si fanno. Aprire le
 * applicazioni, aprire un file, e (per lo Store) descrivere, installare, aggiornare e
 * togliere i pacchetti .app. Lo usano i programmi e le app del sistema; non e' per le app
 * normali (vedi corekit.h): un'app che vuole solo aprire un'altra app o un file usa
 * apertura.h, che e' pubblico e fa le richieste open_app e open_file qui sotto.
 *
 * Il launcher e' il servizio del sistema che fa partire le applicazioni (come
 * LaunchServices sul Mac, o systemd per GNOME). Non ha finestre: chi vuole qualcosa (la
 * scrivania, Files, lo Store, il Terminale) si collega al nome "launcher", manda UNA
 * richiesta e riceve UNA risposta; poi il collegamento si chiude. Il launcher gira come
 * sistema (uid 0): e' l'unico che puo' far partire un programma come applicazione
 * (uid 1000) e scrivere in /data/apps.
 *
 * I messaggi seguono lo STANDARD di SISTEMA.md (sezione 3; messaggi.h): testo
 * chiave=valore, con la busta (request, id, result) e al massimo un descrittore allegato.
 * Qui sotto il CONTRATTO del servizio: le richieste, le loro chiavi, le risposte, i
 * permessi. Le funzioni per chi chiede (in fondo) lo nascondono: un programma chiama
 * apri_shell o togli_app e riceve un numero, senza vedere i messaggi; stanno in
 * interno/launcher.c (e quelle di apertura.h in interno/apertura.c).
 *
 * LE RICHIESTE (il valore di request) E LE LORO CHIAVI:
 *
 *   open_app          app = l'id dell'app (la sua cartella in /system/apps o
 *                     /data/apps), argument = un testo per lei (facoltativo), che trova in
 *                     main (argomenti[1], vedi avvia_come_con in corekit.h): per Files e'
 *                     la cartella da aprire. Si aprono SOLO le app installate.
 *                     Risposta: ok con task; error con reason=not_installed.
 *
 *   open_file         path = il percorso del file. Per ora l'unico tipo di file che si
 *                     apre cosi' e' il pacchetto .app: il launcher fa partire l'app "store"
 *                     con il percorso come argomento (e' lo Store a mostrare cosa c'e'
 *                     dentro e a chiedere se installarla). Un pacchetto .app non parte MAI
 *                     da solo. Risposta: ok con task; error con reason=needs_store (lo
 *                     Store non e' installato: "Le app vanno installate prima di aprirle")
 *                     o reason=unsupported (un altro tipo di file).
 *
 *   describe_package  ALLEGATO il file .app APERTO IN LETTURA da chi chiede: cosi' il
 *                     launcher legge solo i file che chi chiede puo' gia' leggere. Lo
 *                     controlla senza scompattarlo (pacchetto.h). Risposta: ok se si puo'
 *                     installare (o aggiornare, se l'app c'e' gia' piu' vecchia), error con
 *                     reason=package_rejected se no; in tutti e due i casi ALLEGATO un
 *                     foglio (memoria condivisa, chiamate.h) con una struct resoconto
 *                     (pacchetto.h): il manifesto, quali icone ci sono, se e' gia'
 *                     installata e con quale versione, gli errori e gli avvisi con le frasi
 *                     per la persona. Senza allegato: invalid. Nessun permesso.
 *
 *   install_package   come describe_package, e se non ci sono errori lo installa in
 *                     /data/apps/<id>. L'app non deve essere gia' installata. Permesso
 *                     apps.manage. Risposta: ok se fatto; il foglio con il resoconto sempre.
 *
 *   update_package    lo stesso, per un'app gia' installata con una versione piu' vecchia:
 *                     la sostituisce tutta insieme; i dati della persona
 *                     (/data/users/<uid>/appsdata/<id>) restano. Permesso apps.manage.
 *
 *   list_apps         nessun dato. Risposta: ok e ALLEGATO un foglio con una struct
 *                     elenco_app (pacchetto.h): le app installate (prima del sistema, poi di
 *                     /data/apps) con il loro manifesto. Nessun permesso.
 *
 *   remove_app        app = l'id; keep_data = 1 per tenere i dati delle persone (0, o
 *                     senza la chiave: si cancellano). Toglie un'app installata dalla
 *                     persona (/data/apps/<id>): quelle del sistema no. Con keep_data=1 i
 *                     dati (/data/users/<uid>/appsdata/<id>) restano, e reinstallando la
 *                     stessa app si ritrovano. pacchetto_togli in pacchetto.h. Permesso
 *                     apps.manage. Risposta: ok; error con reason=system_app,
 *                     not_installed o in_use; invalid se l'id non va bene (es. con "/").
 *
 *   clear_app_data    app = l'id. Per ogni app installata, anche del sistema: svuota la
 *                     cartella dei dati di ogni persona (la cartella resta). L'app resta
 *                     installata. pacchetto_svuota_dati in pacchetto.h. Permesso
 *                     apps.manage. Risposte come remove_app.
 *
 *   open_program      path = il percorso di un PROGRAMMA normale, non un'app (per ora solo
 *                     quelli di /system/bin: APPLICAZIONI.md, sezione 2). Il launcher lo fa
 *                     partire come la persona (uid 1000), senza argomenti. Oggi non lo usa
 *                     nessuno (il Terminale e' un'app). Permesso apps.open. Risposta:
 *                     ok con task; denied per un percorso fuori da /system/bin.
 *
 *   open_shell        ALLEGATO il capo "schiavo" di un terminale finto (pty) di chi chiede.
 *                     Il launcher fa partire /system/bin/shell come SISTEMA (uid 0), con
 *                     quel terminale come 0, 1 e 2. Oggi lo usa il Terminale. Permesso
 *                     system.shell. Risposta: ok con task; invalid senza allegato.
 *
 * LE RISPOSTE. La busta dello standard: id e result. result e' ok, denied (chi chiede non
 * ha il permesso), invalid (manca un dato, o un dato non va bene: un id con "/", un
 * percorso troppo lungo, una richiesta che il launcher non conosce), error (la richiesta
 * era giusta ma non si e' potuta fare). Quando result=error il PERCHE' sta in una chiave
 * del launcher, reason, che un programma puo' confrontare:
 *     not_installed     l'app chiesta non e' installata
 *     needs_store       un .app va installato, e lo Store non c'e'
 *     unsupported       un file che il sistema non sa ancora aprire
 *     package_rejected  il pacchetto non si puo' installare (il resoconto dice perche')
 *     in_use            l'app e' accesa: va chiusa prima ("Chiudi <nome> prima di ...")
 *     system_app        un'app del sistema non si toglie
 * Senza reason, error vuol dire un errore qualsiasi (es. il programma non e' partito).
 * Perche' una chiave e non altri valori di result: result resta quello di tutti i servizi
 * (chi non conosce il launcher capisce lo stesso "non e' andata"), e i perche' del
 * launcher restano suoi.
 * Le altre chiavi della risposta:
 *     task      il numero del compito appena partito (open_app, open_file, open_program,
 *               open_shell)
 *     message   quando non si puo': la frase per la persona (UTF-8)
 *
 * I DATI GRANDI. Un messaggio e' al massimo MAX_MESSAGGIO byte (5 KB): il resoconto di un
 * pacchetto e l'elenco delle app sono piu' grandi, e viaggiano come allegato della
 * risposta, in un foglio di memoria condivisa che contiene la struct cosi' com'e'
 * (resoconto, elenco_app: pacchetto.h). Sono struct C, non testo: le leggono solo i
 * programmi del sistema, compilati con lo stesso pacchetto.h. Il pacchetto da descrivere o
 * installare e il terminale di open_shell sono allegati della richiesta.
 *
 * I PERMESSI. Il launcher chiede a chi_chiede chi c'e' dall'altra parte, e a ha_permesso
 * se ha il permesso (sistema/servizi.h). Oggi ha_permesso e' PROVVISORIA: dice sempre si'
 * e scrive la domanda nel registro ("permesso apps.manage a ... (provvisorio)").
 *     apps.manage    install_package, update_package, remove_app, clear_app_data
 *     apps.open  open_program
 *     system.shell   open_shell
 * open_app, open_file, describe_package e list_apps li puo' chiedere chiunque.
 * Per apps.manage, oltre a ha_permesso, resta il controllo vero di prima: lo concede solo
 * il sistema (uid 0) o un'app DEL SISTEMA che nel manifesto chiede il permesso
 * "apps.manage" (lo Store; pacchetto_puo_gestire). Gli altri ricevono denied. Quando
 * ha_permesso sara' vera, prendera' il suo posto.
 *
 * IL TEMPO MASSIMO. Chi chiede aspetta la risposta al massimo TEMPO_CHIEDI (5 secondi,
 * messaggi.h); per le richieste sui pacchetti (describe_package, install_package,
 * update_package, remove_app, clear_app_data), che leggono, scompattano o cancellano
 * molti file, TEMPO_LAUNCHER_PACCHETTI. Il launcher serve le richieste una alla volta:
 * mentre installa, chi apre un'app aspetta (al massimo il suo tempo). Se il tempo scade
 * il launcher finisce comunque il lavoro: chi chiede sa solo che non ha saputo l'esito.
 *
 * Una nuova app installata la scrivania non la vede finche' non riparte (per ora legge le
 * app una volta, all'avvio). */
#ifndef LAUNCHER_H
#define LAUNCHER_H

#include "../corekit.h"
#include "../apertura.h"
#include "pacchetto.h"
/* messaggi.h NO: lo includono solo il launcher e interno/launcher.c. Chi usa queste
 * funzioni non vede i messaggi: li fa CoreKit per lei. */

#define NOME_LAUNCHER "launcher"

/* le richieste (il valore di request) */
#define APRI_APPLICAZIONE  "open_app"
#define APRI_FILE          "open_file"
#define DESCRIVI_PACCHETTO "describe_package"
#define INSTALLA_PACCHETTO "install_package"
#define AGGIORNA_PACCHETTO "update_package"
#define ELENCO_APP         "list_apps"
#define RIMUOVI_PACCHETTO  "remove_app"
#define SVUOTA_DATI        "clear_app_data"
#define APRI_PROGRAMMA     "open_program"
#define APRI_SHELL         "open_shell"

/* le chiavi del launcher (oltre alla busta: messaggi.h) */
#define CHIAVE_APP        "app"
#define CHIAVE_ARGOMENTO  "argument"
#define CHIAVE_PERCORSO   "path"
#define CHIAVE_TIENI_DATI "keep_data"
#define CHIAVE_COMPITO    "task"
#define CHIAVE_MOTIVO     "reason"
#define CHIAVE_FRASE      "message"

/* i perche' di result=error (il valore di reason) */
#define MOTIVO_NON_INSTALLATA "not_installed"
#define MOTIVO_SERVE_STORE    "needs_store"
#define MOTIVO_NON_SUPPORTATO "unsupported"
#define MOTIVO_PACCHETTO      "package_rejected"
#define MOTIVO_IN_USO         "in_use"
#define MOTIVO_DEL_SISTEMA    "system_app"

/* i permessi che il launcher chiede (ha_permesso, sistema/servizi.h) */
#define PERMESSO_GESTIRE_APP "apps.manage"
#define PERMESSO_PROGRAMMI   "apps.open"
#define PERMESSO_SHELL       "system.shell"

/* il tempo massimo delle richieste sui pacchetti, in millisecondi */
#define TEMPO_LAUNCHER_PACCHETTI 60000

/* la shell che open_shell fa partire */
#define PROGRAMMA_SHELL "/system/bin/shell"

/* dove stanno i programmi che open_program fa partire */
#define CARTELLA_PROGRAMMI "/system/bin/"

#define ID_STORE "store"

/* --- per chi chiede ---
 * Le funzioni qui sotto rispondono con un numero, LAUNCHER_...: il result e il reason
 * della risposta, tradotti. I primi sono gli stessi di apertura.h. */
#define LAUNCHER_FATTO                APERTURA_FATTO           /* ok */
#define LAUNCHER_ERRORE               APERTURA_ERRORE          /* invalid, error, timeout, o il launcher non c'e' */
#define LAUNCHER_NON_INSTALLATA       APERTURA_NON_INSTALLATA  /* reason=not_installed */
#define LAUNCHER_SERVE_STORE          APERTURA_SERVE_STORE     /* reason=needs_store */
#define LAUNCHER_NON_SUPPORTATO       APERTURA_NON_SUPPORTATO  /* reason=unsupported */
#define LAUNCHER_PACCHETTO_NON_VALIDO -5                       /* reason=package_rejected */
#define LAUNCHER_NON_PERMESSO         -6                       /* denied */
#define LAUNCHER_IN_USO               -7                       /* reason=in_use */
#define LAUNCHER_DEL_SISTEMA          -8                       /* reason=system_app */

/* Quanto e' lunga, al piu', la frase per la persona che il launcher manda (message),
 * compreso lo 0 finale: la misura giusta per il 'perche'' delle funzioni qui sotto. */
#define MAX_FRASE_LAUNCHER 160

/* Aprire un'applicazione o un file (open_app, open_file): apri_applicazione,
 * apri_applicazione_con e apri_file, in apertura.h (incluso qui sopra). */

/* Chiede al launcher di far partire il programma 'percorso' (in /system/bin) come la
 * persona. Il numero del compito, o -1. */
int apri_programma(const char *percorso);

/* Chiede al launcher di far partire la shell come SISTEMA su un terminale finto: 'schiavo' e'
 * il suo capo schiavo (/dev/pts/N, aperto da chi chiede). Il numero del compito, o -1. */
int apri_shell(int schiavo);

/* Descrive (DESCRIVI_PACCHETTO), installa (INSTALLA_PACCHETTO) o aggiorna
 * (AGGIORNA_PACCHETTO) il pacchetto nel file 'percorso': lo apre chi chiede, con i suoi
 * permessi. Il resoconto va in *r (e' grande: meglio non metterlo nello stack); se il
 * launcher non lo manda, *r non cambia. L'esito (LAUNCHER_...). */
int launcher_pacchetto(const char *richiesta, const char *percorso, struct resoconto *r);

/* Toglie l'app 'id' (remove_app), tenendo i dati delle persone se 'tieni_dati'. L'esito;
 * se 'perche' non e' 0, ci va la frase per la persona (al massimo 'byte' byte compreso lo
 * 0; "" se non c'e'). */
int togli_app(const char *id, int tieni_dati, char *perche, unsigned int byte);

/* Svuota i dati dell'app 'id' (clear_app_data). L'esito; la frase come in togli_app. */
int svuota_dati_app(const char *id, char *perche, unsigned int byte);

/* Le app installate, in *e (e->quante 0 se non si e' potuto). L'esito. */
int elenco_app_installate(struct elenco_app *e);

#endif
