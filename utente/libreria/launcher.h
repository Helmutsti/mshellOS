/* Il protocollo del launcher: aprire le applicazioni, aprire un file, e (per lo Store)
 * descrivere, installare e aggiornare i pacchetti .app.
 *
 * Il launcher e' il servizio del sistema che fa partire le applicazioni (come
 * LaunchServices sul Mac, o systemd per GNOME). Non ha finestre: chi vuole qualcosa (la
 * scrivania, Files, domani lo Store) si collega al nome "launcher", manda UNA richiesta
 * (struct richiesta_launcher) e riceve UNA risposta (struct risposta_launcher); poi il
 * collegamento si chiude. Il launcher gira come sistema (uid 0): e' l'unico che puo' far
 * partire un programma come applicazione (uid 1000) e scrivere in /data/apps.
 *
 * LE RICHIESTE (il campo tipo):
 *
 *   APRI_APPLICAZIONE   applicazione = il suo id (la sua cartella in /system/apps o
 *                       /data/apps); argomento = un testo per lei ("": niente), che trova
 *                       in main (argomenti[1], vedi avvia_come_con in libreria.h): per Files
 *                       e' la cartella da aprire. Risposta: esito 0 e il numero del compito,
 *                       o LAUNCHER_NON_INSTALLATA se non c'e'. Si aprono SOLO le app
 *                       installate.
 *
 *   APRI_FILE           argomento = il percorso del file. Per ora l'unico tipo di file che
 *                       si apre cosi' e' il pacchetto .app: il launcher fa partire l'app
 *                       "store" con il percorso come argomento (e' lo Store a mostrare cosa
 *                       c'e' dentro e a chiedere se installarla). Se lo Store non e'
 *                       installato: LAUNCHER_SERVE_STORE, con il messaggio "Le app vanno
 *                       installate prima di aprirle". Un altro tipo di file:
 *                       LAUNCHER_NON_SUPPORTATO. Un pacchetto .app non parte MAI da solo.
 *
 *   DESCRIVI_PACCHETTO  allegato (manda_foglio) il file .app APERTO IN LETTURA da chi chiede:
 *                       cosi' il launcher legge solo i file che chi chiede puo' gia' leggere.
 *                       Lo controlla senza scompattarlo (pacchetto.h). Risposta: esito 0 se
 *                       si puo' installare (o aggiornare, se l'app c'e' gia' piu' vecchia),
 *                       LAUNCHER_PACCHETTO_NON_VALIDO se no; in tutti e due i casi allegato
 *                       un FOGLIO (memoria condivisa, chiamate.h) con una struct resoconto:
 *                       il manifesto, quali icone ci sono, se e' gia' installata e con quale
 *                       versione, gli errori e gli avvisi con le frasi per la persona.
 *                       Chiunque puo' chiederlo.
 *
 *   INSTALLA_PACCHETTO  come DESCRIVI_PACCHETTO, e se non ci sono errori lo installa in
 *                       /data/apps/<id>. L'app non deve essere gia' installata. Risposta:
 *                       esito 0 se fatto; il foglio con il resoconto sempre.
 *
 *   AGGIORNA_PACCHETTO  lo stesso, per un'app gia' installata con una versione piu'
 *                       vecchia: la sostituisce tutta insieme; i dati della persona
 *                       (/data/users/<uid>/appsdata/<id>) restano.
 *
 *   ELENCO_APP          nessun dato. Risposta: esito 0 e un foglio con una struct
 *                       elenco_app: le app installate (prima del sistema, poi di /data/apps)
 *                       con il loro manifesto. Chiunque puo' chiederlo.
 *
 *   RIMUOVI_PACCHETTO   applicazione = l'id; opzioni = LAUNCHER_TIENI_DATI o 0. Toglie
 *                       un'app installata dalla persona (/data/apps/<id>): quelle del sistema
 *                       no (LAUNCHER_DEL_SISTEMA). Senza LAUNCHER_TIENI_DATI cancella anche i
 *                       dati delle persone (/data/users/<uid>/appsdata/<id>); con, li lascia,
 *                       e reinstallando la stessa app si ritrovano. pacchetto_togli in
 *                       pacchetto.h. Risposta: l'esito e la frase per la persona in messaggio.
 *
 *   SVUOTA_DATI         applicazione = l'id. Per ogni app installata, anche del sistema:
 *                       svuota la cartella dei dati di ogni persona (la cartella resta).
 *                       L'app resta installata. pacchetto_svuota_dati in pacchetto.h.
 *
 *   APRI_PROGRAMMA      argomento = il percorso di un PROGRAMMA normale, non un'app (per
 *                       ora solo quelli di /system/bin: APPLICAZIONI.md, sezione 2). Il
 *                       launcher lo fa partire come la persona (uid 1000), senza argomenti.
 *                       Oggi lo usa la scrivania per il terminale (l'icona "Shell"). Un
 *                       percorso fuori da /system/bin: LAUNCHER_NON_PERMESSO.
 *
 *   APRI_SHELL          allegato = il capo "schiavo" di un terminale finto (pty) di chi
 *                       chiede. Il launcher fa partire /system/bin/shell come SISTEMA (uid 0),
 *                       con quel terminale come 0, 1 e 2. Oggi lo usa il terminale della
 *                       scrivania. PROVVISORIO, decisione della persona: lo puo' chiedere
 *                       chiunque (terminale come sistema aperto a chiunque: da proteggere).
 *
 *   Se l'app e' accesa, RIMUOVI_PACCHETTO e SVUOTA_DATI rispondono LAUNCHER_IN_USO ("Chiudi
 *   <nome> prima di ..."): il launcher non la ferma.
 *
 * CHI PUO' INSTALLARE, AGGIORNARE, TOGLIERE E SVUOTARE I DATI: il sistema (uid 0), oppure
 * un'app DEL SISTEMA che nel manifesto chiede il permesso "app.gestire" (lo Store). Il
 * launcher lo controlla chiedendo al kernel quale programma c'e' all'altro capo
 * (programma_di). Gli altri ricevono LAUNCHER_NON_PERMESSO.
 *
 * Una nuova app installata la scrivania non la vede finche' non riparte (per ora legge le
 * app una volta, all'avvio). */
#ifndef LAUNCHER_H
#define LAUNCHER_H

#include "libreria.h"
#include "pacchetto.h"

#define NOME_LAUNCHER "launcher"

/* i tipi di richiesta */
#define APRI_APPLICAZIONE  1
#define DESCRIVI_PACCHETTO 2
#define INSTALLA_PACCHETTO 3
#define AGGIORNA_PACCHETTO 4
#define ELENCO_APP         5
#define APRI_FILE          6
#define RIMUOVI_PACCHETTO  7
#define SVUOTA_DATI        8
#define APRI_PROGRAMMA     9
#define APRI_SHELL        10

/* la shell che APRI_SHELL fa partire */
#define PROGRAMMA_SHELL "/system/bin/shell"

/* dove stanno i programmi che APRI_PROGRAMMA fa partire */
#define CARTELLA_PROGRAMMI "/system/bin/"

/* le opzioni di RIMUOVI_PACCHETTO */
#define LAUNCHER_TIENI_DATI 1

/* gli esiti della risposta */
#define LAUNCHER_FATTO                 0
#define LAUNCHER_ERRORE               -1   /* richiesta sbagliata, o non si e' riusciti */
#define LAUNCHER_NON_INSTALLATA       -2   /* l'app chiesta non e' installata */
#define LAUNCHER_SERVE_STORE          -3   /* un .app: va installato, e lo Store non c'e' */
#define LAUNCHER_NON_SUPPORTATO       -4   /* un file che il sistema non sa ancora aprire */
#define LAUNCHER_PACCHETTO_NON_VALIDO -5   /* il resoconto (nel foglio) dice perche' */
#define LAUNCHER_NON_PERMESSO         -6   /* chi chiede non puo' installare */
#define LAUNCHER_IN_USO               -7   /* l'app e' accesa: va chiusa prima */
#define LAUNCHER_DEL_SISTEMA          -8   /* un'app del sistema non si toglie */

#define ID_STORE "store"

struct richiesta_launcher {
    unsigned int tipo;
    char applicazione[MAX_NOME_FILE];   /* APRI_APPLICAZIONE: l'id, per esempio "files" */
    char argomento[MAX_PERCORSO];       /* per l'applicazione, o il file da aprire; "" niente */
    unsigned int opzioni;               /* RIMUOVI_PACCHETTO: LAUNCHER_TIENI_DATI o 0 */
};

#define MAX_MESSAGGIO_LAUNCHER 160

struct risposta_launcher {
    int esito;                          /* LAUNCHER_... */
    int numero;                         /* il numero del compito appena partito, o -1 */
    char messaggio[MAX_MESSAGGIO_LAUNCHER];  /* quando non si puo': perche', per la persona (UTF-8) */
};

/* --- per chi chiede: una richiesta e la sua risposta --- */

static inline void launcher_copia(char *dove, const char *da, int massimo) {
    for (int i = 0; da && i < massimo - 1 && da[i]; i++) dove[i] = da[i];
}

/* Manda la richiesta (con 'allegato', un descrittore da passare, o -1) e aspetta la
 * risposta; se la risposta porta un foglio, lo copia in 'dove' (al massimo 'byte'). */
static inline int launcher_chiedi(const struct richiesta_launcher *r, int allegato, struct risposta_launcher *risposta,
                                  void *dove, unsigned int byte) {
    *risposta = (struct risposta_launcher){ LAUNCHER_ERRORE, -1, { 0 } };
    int d = collega(NOME_LAUNCHER);
    if (d < 0) return LAUNCHER_ERRORE;
    int foglio_risposta = -1;
    int ok = (allegato >= 0 ? manda_foglio(d, r, sizeof *r, allegato) : manda(d, r, sizeof *r)) >= 0 &&
             ricevi_foglio(d, risposta, sizeof *risposta, &foglio_risposta) == sizeof *risposta;
    chiudi(d);
    if (!ok) risposta->esito = LAUNCHER_ERRORE;
    if (foglio_risposta >= 0) {
        unsigned int grande = 0;
        unsigned char *p = mappa(foglio_risposta, &grande);
        if (p && dove) {
            unsigned char *q = dove;
            for (unsigned int i = 0; i < byte && i < grande; i++) q[i] = p[i];
        }
        if (p) togli(p);
        chiudi(foglio_risposta);
    }
    risposta->messaggio[MAX_MESSAGGIO_LAUNCHER - 1] = 0;
    return risposta->esito;
}

/* Chiede al launcher di aprire un'applicazione, con un argomento per lei (0 o "":
 * nessuno). Il numero del compito, o -1. */
static inline int apri_applicazione_con(const char *id, const char *argomento) {
    struct richiesta_launcher r = { .tipo = APRI_APPLICAZIONE };
    struct risposta_launcher risposta;
    launcher_copia(r.applicazione, id, MAX_NOME_FILE);
    launcher_copia(r.argomento, argomento, MAX_PERCORSO);
    return launcher_chiedi(&r, -1, &risposta, 0, 0) < 0 ? -1 : risposta.numero;
}

/* Chiede al launcher di aprire un'applicazione: il numero del compito, o -1. */
static inline int apri_applicazione(const char *id) { return apri_applicazione_con(id, 0); }

/* Chiede al launcher di far partire il programma 'percorso' (in /system/bin) come la
 * persona. Il numero del compito, o -1. */
static inline int apri_programma(const char *percorso) {
    struct richiesta_launcher r = { .tipo = APRI_PROGRAMMA };
    struct risposta_launcher risposta;
    launcher_copia(r.argomento, percorso, MAX_PERCORSO);
    return launcher_chiedi(&r, -1, &risposta, 0, 0) < 0 ? -1 : risposta.numero;
}

/* Chiede al launcher di far partire la shell come SISTEMA su un terminale finto: 'schiavo' e'
 * il suo capo schiavo (/dev/pts/N, aperto da chi chiede). PROVVISORIO: aperto a chiunque. Il
 * numero del compito, o -1. */
static inline int apri_shell(int schiavo) {
    struct richiesta_launcher r = { .tipo = APRI_SHELL };
    struct risposta_launcher risposta;
    return launcher_chiedi(&r, schiavo, &risposta, 0, 0) < 0 ? -1 : risposta.numero;
}

/* Chiede al launcher di aprire un file (per ora: un pacchetto .app, con lo Store).
 * LAUNCHER_FATTO, o un altro esito con il perche' in risposta->messaggio. */
static inline int apri_file(const char *percorso, struct risposta_launcher *risposta) {
    struct richiesta_launcher r = { .tipo = APRI_FILE };
    launcher_copia(r.argomento, percorso, MAX_PERCORSO);
    return launcher_chiedi(&r, -1, risposta, 0, 0);
}

/* Descrive (DESCRIVI_PACCHETTO), installa (INSTALLA_PACCHETTO) o aggiorna
 * (AGGIORNA_PACCHETTO) il pacchetto nel file 'percorso': lo apre chi chiede, con i suoi
 * permessi. Il resoconto va in *r (e' grande: meglio non metterlo nello stack). L'esito. */
static inline int launcher_pacchetto(unsigned int tipo, const char *percorso, struct resoconto *r) {
    struct richiesta_launcher q = { .tipo = tipo };
    struct risposta_launcher risposta;
    int f = apri(percorso, APRI_LEGGERE);
    if (f < 0) return LAUNCHER_ERRORE;
    int esito = launcher_chiedi(&q, f, &risposta, r, sizeof *r);
    chiudi(f);
    return esito;
}

/* Toglie l'app 'id' (RIMUOVI_PACCHETTO), tenendo i dati delle persone se 'tieni_dati'.
 * L'esito; la frase per la persona in risposta->messaggio. */
static inline int togli_app(const char *id, int tieni_dati, struct risposta_launcher *risposta) {
    struct richiesta_launcher q = { .tipo = RIMUOVI_PACCHETTO, .opzioni = tieni_dati ? LAUNCHER_TIENI_DATI : 0 };
    launcher_copia(q.applicazione, id, MAX_NOME_FILE);
    return launcher_chiedi(&q, -1, risposta, 0, 0);
}

/* Svuota i dati dell'app 'id' (SVUOTA_DATI). L'esito; la frase in risposta->messaggio. */
static inline int svuota_dati_app(const char *id, struct risposta_launcher *risposta) {
    struct richiesta_launcher q = { .tipo = SVUOTA_DATI };
    launcher_copia(q.applicazione, id, MAX_NOME_FILE);
    return launcher_chiedi(&q, -1, risposta, 0, 0);
}

/* Le app installate, in *e. L'esito. */
static inline int elenco_app_installate(struct elenco_app *e) {
    struct richiesta_launcher q = { .tipo = ELENCO_APP };
    struct risposta_launcher risposta;
    e->quante = 0;
    return launcher_chiedi(&q, -1, &risposta, e, sizeof *e);
}

#endif
