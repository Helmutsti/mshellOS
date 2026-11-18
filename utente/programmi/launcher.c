/* Il launcher: il servizio del sistema che fa partire le applicazioni e le installa.
 *
 * Non ha finestre, come LaunchServices sul Mac o systemd per GNOME: l'interfaccia (le
 * icone da cliccare, lo Store) e' di altri, che gli chiedono le cose. Le richieste e le
 * risposte seguono lo standard dei messaggi (SISTEMA.md, sezione 3; messaggi.h): il
 * contratto del launcher (quali richieste, quali chiavi, quali permessi) e' in launcher.h.
 * Lui:
 *   - open_app: controlla che sia un'applicazione installata (una cartella con il suo
 *     manifest.toml in /system/apps o, se li' non c'e', in /data/apps: descrizione.h),
 *     prende dal manifesto quale programma far partire e lo fa partire come APPLICAZIONE
 *     (uid 1000), con l'argomento della richiesta se c'e', con le sue porte verso i servizi
 *     e chiusa nel suo RECINTO (prepara_recinto). E' per questo che il launcher e' del
 *     sistema (uid 0): solo il sistema puo' far partire un programma con un altro uid e
 *     costruirgli il recinto. Un'eccezione sola, provvisoria: il Terminale parte come
 *     SISTEMA e senza recinto (uid_dell_app);
 *   - open_program: un programma normale di /system/bin (non un'app; per ora nessuno lo
 *     usa), come la persona (uid 1000);
 *   - open_file: un pacchetto .app non si apre mai da solo: si fa partire lo Store su di
 *     lui, se e' installato; se no si risponde che le app vanno installate prima;
 *   - remove_app, clear_app_data: togliere un'app della persona, svuotare i dati di un'app
 *     (pacchetto_togli, pacchetto_svuota_dati), con lo stesso permesso di installare;
 *   - describe_package, install_package, update_package, list_apps: il lavoro lo fa
 *     pacchetto.c; il launcher controlla chi puo' installare, legge il file che chi chiede
 *     gli passa gia' aperto, e risponde con il resoconto in un foglio allegato. E' lui a
 *     farlo perche' /data/apps lo puo' scrivere solo il sistema.
 *
 * I permessi: per ogni richiesta che ne vuole uno (launcher.h) il launcher chiede a
 * ha_permesso (servizi.h). Per un'app valgono i permessi che il launcher stesso le ha concesso
 * quando l'ha fatta partire (permessi_concessi: dichiarati nel manifesto, quelli di sistema
 * solo alle app del sistema), e li passa anche agli altri servizi con le porte.
 *
 * Il suo nome, "launcher", lo tiene init, che glielo presta al descrittore 0: se il
 * launcher cade, nessun altro puo' prenderlo, e chi chiede nel frattempo aspetta.
 * Le richieste si servono una alla volta: un'installazione grossa fa aspettare gli altri
 * per il tempo di scompattarla. */
#include "corekit/corekit.h"
#include "corekit/messaggi.h"
#include "corekit/sistema/servizi.h"     /* chi_chiede, ha_permesso */
#include "corekit/sistema/launcher.h"
#include "corekit/sistema/privilegi.h"   /* avvia_come...: le app partono come la persona */
#include "corekit/sistema/descrizione.h"
#include "corekit/sistema/pacchetto.h"
#include "gkit/sistema/protocollo.h"     /* PORTA_APP: la porta dell'app verso il compositore */
#include "corekit/catalog.h"             /* NOME_CATALOG, D_PORTA_CATALOG: la porta verso catalog */
#include "corekit/tasks.h"                /* NOME_TASKS, D_PORTA_TASKS: la porta verso tasks */
#include "corekit/picker.h"               /* NOME_PICKER, D_PORTA_PICKER: la porta verso picker */
#include "corekit/sistema/network.h"     /* NOME_NETWORK, D_PORTA_NETWORK: la porta verso la rete */

#define D_ASCOLTO 0      /* prestato da init */
#define D_CUSTODE 1      /* prestato da init: il canale con cui init custodisce le porte delle app
                          * (servizi.h): ogni porta nostra gli si manda, e a ogni partenza lui
                          * riconsegna quelle che teneva */
#define MAX_CLIENTI 8

static void registro(const char *a, const char *b) { scrivi_riga("launcher", a, b); }

/* Finisce con 'ext'? */
static int finisce_con(const char *s, const char *ext) {
    int ls = lunghezza(s), le = lunghezza(ext);
    return ls > le && descrizione_uguali(s + ls - le, ext);
}

/* Comincia la risposta *r alla richiesta *q: la busta (id, result = 'esito'), il perche'
 * (reason, se 'motivo' non e' 0) e la frase per la persona (message, se c'e'). */
static void risposta(struct messaggio *r, const struct messaggio *q, const char *esito, const char *motivo,
                     const char *frase) {
    messaggio_risposta(r, q, esito);
    if (motivo) messaggio_aggiungi(r, CHIAVE_MOTIVO, motivo);
    if (frase && frase[0]) messaggio_aggiungi(r, CHIAVE_FRASE, frase);
}

/* La risposta a una richiesta che fa partire un compito: ok con il suo numero (task), o
 * error se non e' partito (numero < 0). */
static void partito(struct messaggio *r, const struct messaggio *q, int numero) {
    if (numero < 0) {
        risposta(r, q, ESITO_ERRORE, 0, 0);
        return;
    }
    messaggio_risposta(r, q, ESITO_OK);
    messaggio_aggiungi_numero(r, CHIAVE_COMPITO, numero);
}

/* Il valore della chiave, se c'e' ed e' piu' corto di 'massimo' byte (compreso lo 0: le
 * misure di sempre, MAX_NOME_FILE per un id, MAX_PERCORSO per un percorso); se no 0. */
static const char *dato(const struct messaggio *q, const char *chiave, int massimo) {
    const char *v = messaggio_valore(q, chiave);
    return v && lunghezza(v) < massimo ? v : 0;
}

/* I PERMESSI CONCESSI all'app 'd', in *app (che diventa il chi_chiede dei suoi collegamenti dalle
 * porte: servizi.h). La regola (decisa con la persona il 16 novembre 2026; APPLICAZIONI.md,
 * sezione 9): finche' non c'e' la domanda del primo avvio, i permessi DICHIARATI nel manifesto
 * valgono come concessi; quelli di sistema solo alle app del sistema (/system/apps); un nome che
 * il sistema non conosce non vale. Quelli tolti si scrivono nel registro. */
static void permessi_concessi(const struct descrizione *d, struct chi_chiede *app) {
    static struct manifesto m;
    app->uid = UID_APPLICAZIONI;
    descrizione_copia(app->programma, d->programma, sizeof app->programma);
    app->app = 1;
    app->quanti_permessi = 0;
    char cartella[MAX_PERCORSO];
    descrizione_copia(cartella, cartelle_applicazioni[d->di_sistema ? CARTELLA_APP_SISTEMA : CARTELLA_APP_DATI],
                      sizeof cartella);
    descrizione_aggiungi(cartella, "/", sizeof cartella);
    descrizione_aggiungi(cartella, d->id, sizeof cartella);
    if (manifesto_dell_app(cartella, &m) < 0) return;
    for (int i = 0; i < m.quanti_permessi && app->quanti_permessi < MAX_PERMESSI_APP; i++) {
        const char *p = m.permessi[i];
        if (!permesso_conosciuto(p) || lunghezza(p) >= MAX_NOME_PERMESSO) {
            registro("permesso sconosciuto, non concesso: ", p);
        } else if (permesso_di_sistema(p) && !d->di_sistema) {
            registro("permesso di sistema a un'app non del sistema, non concesso: ", p);
        } else {
            descrizione_copia(app->permessi[app->quanti_permessi++], p, MAX_NOME_PERMESSO);
        }
    }
}

static int ha_concesso(const struct chi_chiede *app, const char *nome) {
    for (int i = 0; i < app->quanti_permessi; i++)
        if (descrizione_uguali(app->permessi[i], nome)) return 1;
    return 0;
}

/* IL RECINTO DELL'APP (APPLICAZIONI.md, sezione 5, "L'app non vede niente" e "I permessi sulle
 * cartelle"; il lavoro lo fa CoreKit: sistema/recinto.h). Dentro, ogni cartella sta allo stesso
 * percorso di fuori (decisione della persona, 17 novembre 2026: cosi' le app non cambiano):
 *   - il suo pacchetto, intero, in sola lettura: l'unico posto da cui puo' eseguire programmi
 *     (/system/apps/<id> o /data/apps/<id>);
 *   - le risorse del sistema, in sola lettura: font, icone, sfondi predefiniti
 *     (/system/resources, RISORSE_CARTELLA di gkit_disegno.h);
 *   - la sua cartella dei dati, in lettura e scrittura (/data/users/<uid>/appsdata/<id>; se non
 *     c'e' la si crea, della persona). Nient'altro di /data;
 *   - la Home della persona: in sola lettura con "files.read", in lettura e scrittura con
 *     "files.write" (che comprende la lettura); senza, niente Home;
 *   - la cartella degli sfondi della persona, in sola lettura, solo con "settings.system"
 *     (/data/users/<uid>/wallpapers, fuori dalla Home: la usa Impostazioni);
 *   - la rete del computer solo con "network.use"; senza, una rete vuota. Nessuna regola del
 *     firewall per app (decisione della persona).
 * I permessi sono quelli concessi (permessi_concessi). Il Terminale resta SENZA recinto
 * (decisione della persona): gira come sistema e ci fa partire la shell (uid_dell_app), e
 * col recinto non avrebbe senso.
 * I percorsi stanno qui, perche' struct recinto ne tiene solo l'indirizzo fino all'avvio. */
#define RISORSE_DEL_SISTEMA "/system/resources"

static struct {
    char pacchetto[MAX_PERCORSO];
    char dati[MAX_PERCORSO];
    char home[MAX_PERCORSO];
    char sfondi[MAX_PERCORSO];
} cartelle;

/* Una cartella della persona, '<cartella_persone>/<uid>/<dove>', in 'p'. 0, o -1 se non ci sta. */
static int cartella_della_persona(char *p, const char *dove) {
    char numero[12];
    int k = 0;
    unsigned int n = UID_APPLICAZIONI;
    do { numero[k++] = (char)('0' + n % 10); n /= 10; } while (n);
    descrizione_copia(p, cartella_persone, MAX_PERCORSO);
    descrizione_aggiungi(p, "/", MAX_PERCORSO);
    char cifra[2] = {0, 0};
    while (k) { cifra[0] = numero[--k]; descrizione_aggiungi(p, cifra, MAX_PERCORSO); }
    descrizione_aggiungi(p, "/", MAX_PERCORSO);
    descrizione_aggiungi(p, dove, MAX_PERCORSO);
    return lunghezza(p) < MAX_PERCORSO - 1 ? 0 : -1;
}

/* Prepara in *r il recinto dell'app 'd' con i permessi concessi 'app', e scrive nel registro
 * cosa ha dentro. 0, o -1 se non si puo' (l'app allora non parte). */
static int prepara_recinto(const struct descrizione *d, const struct chi_chiede *app, struct recinto *r) {
    *r = (struct recinto){ .quanti = 0 };
    descrizione_copia(cartelle.pacchetto, cartelle_applicazioni[d->di_sistema ? CARTELLA_APP_SISTEMA : CARTELLA_APP_DATI],
                      sizeof cartelle.pacchetto);
    descrizione_aggiungi(cartelle.pacchetto, "/", sizeof cartelle.pacchetto);
    descrizione_aggiungi(cartelle.pacchetto, d->id, sizeof cartelle.pacchetto);
    /* la cartella dei dati: se non c'e' si crea; deve essere una cartella vera della persona,
     * non un collegamento (il launcher aggancia quello che trova, e un collegamento messo li'
     * porterebbe l'app altrove) */
    if (pacchetto_cartella_dati(d->id, cartelle.dati, sizeof cartelle.dati) < 0) {
        registro("la cartella dei dati non e' una cartella della persona: ", cartelle.dati);
        return -1;
    }
    r->agganci[r->quanti++] = (struct aggancio){ cartelle.pacchetto, AGGANCIO_PROGRAMMA };
    r->agganci[r->quanti++] = (struct aggancio){ RISORSE_DEL_SISTEMA, AGGANCIO_LETTURA };
    r->agganci[r->quanti++] = (struct aggancio){ cartelle.dati, AGGANCIO_SCRITTURA };
    const char *home = "niente Home";
    if (ha_concesso(app, "files.write") || ha_concesso(app, "files.read")) {
        int scrive = ha_concesso(app, "files.write");
        if (cartella_della_persona(cartelle.home, "home") < 0) return -1;
        r->agganci[r->quanti++] = (struct aggancio){ cartelle.home, scrive ? AGGANCIO_SCRITTURA : AGGANCIO_LETTURA };
        home = scrive ? "Home in lettura e scrittura (files.write)" : "Home in sola lettura (files.read)";
    }
    int sfondi = ha_concesso(app, "settings.system");
    if (sfondi) {
        if (cartella_della_persona(cartelle.sfondi, "wallpapers") < 0) return -1;
        r->agganci[r->quanti++] = (struct aggancio){ cartelle.sfondi, AGGANCIO_LETTURA };
    }
    r->rete = ha_concesso(app, "network.use");
    char riga[MAX_ID + 2 * MAX_PERCORSO + 200];
    descrizione_copia(riga, d->id, sizeof riga);
    descrizione_aggiungi(riga, ": nel recinto il pacchetto ", sizeof riga);
    descrizione_aggiungi(riga, cartelle.pacchetto, sizeof riga);
    descrizione_aggiungi(riga, ", i dati ", sizeof riga);
    descrizione_aggiungi(riga, cartelle.dati, sizeof riga);
    descrizione_aggiungi(riga, ", le risorse, ", sizeof riga);
    descrizione_aggiungi(riga, home, sizeof riga);
    if (sfondi) descrizione_aggiungi(riga, ", gli sfondi (settings.system)", sizeof riga);
    registro(riga, r->rete ? ", la rete del computer (network.use)" : ", una rete vuota");
    return 0;
}

/* LE PORTE VERSO IL LAUNCHER STESSO. Una porta per app: il launcher tiene lui il capo del
 * servizio (non c'e' attach: la crea lui) e sa di chi e' (il programma dell'app). Un connect con
 * un capo allegato su una porta diventa un cliente con quell'identita', al posto di chiederla al
 * kernel. Una porta si chiude quando l'app la chiude (o finisce). */
#define MAX_PORTE 32
static struct {
    int d;                  /* -1: posto libero */
    int usata;              /* e' gia' arrivato un connect (per scriverlo nel registro una volta) */
    struct chi_chiede chi;
} porte[MAX_PORTE];

/* FAR PARTIRE UN'APP CON LE SUE PORTE (SISTEMA.md, sezione 3, "Le porte delle app"). Per ogni
 * servizio una coppia di socket: un capo al servizio, con il programma dell'app (cosi' il
 * servizio sa di chi e' senza chiederlo al kernel), l'altro all'app, a un descrittore fisso:
 *     3  il compositore (D_PORTA_APP, protocollo.h: il suo formato, PORTA_APP)
 *     4  catalog (D_PORTA_CATALOG, catalog.h: lo standard dei messaggi, attach)
 *     5  il launcher stesso (D_PORTA_LAUNCHER, launcher.h): il capo del servizio lo tiene lui
 *     6  network (D_PORTA_NETWORK, network.h), solo con network.use o network.settings nel manifesto
 *     7  picker (D_PORTA_PICKER, picker.h): la finestra Apri/Salva, di base a tutte le app
 *     8  tasks (D_PORTA_TASKS, tasks.h), solo con processes.view o processes.stop nel manifesto
 *     9  la connessione Wayland (D_PORTA_WAYLAND, protocollo.h: PORTA_WAYLAND), a tutte: il
 *        compositore ne fa un client Wayland di quell'app; l'app la trova con WAYLAND_SOCKET=9,
 *        la variabile di libwayland-client per una connessione gia' aperta (wl_display_connect)
 * Poi il launcher chiude i suoi capi: app e servizi parlano da soli. Se un servizio non risponde
 * (o il permesso manca), al posto della porta c'e' /dev/null e lo si scrive nel registro. All'app
 * dice quali porte ha con la variabile d'ambiente VARIABILE_PORTE (messaggi.h): l'elenco delle
 * sole porte date. Per ogni app una riga del registro dice quali porte ha avuto.
 * I descrittori 0, 1 e 2 (ingresso, uscita, errori: la convenzione di ogni programma) sono
 * /dev/null: se restassero chiusi, il primo file o collegamento che l'app apre finirebbe li', e
 * quello che l'app stampa (Doom stampa molto) ci andrebbe dentro. */
_Static_assert(D_PORTA_APP == 3 && D_PORTA_CATALOG == 4 && D_PORTA_LAUNCHER == 5 && D_PORTA_NETWORK == 6 &&
               D_PORTA_PICKER == 7 && D_PORTA_TASKS == 8 && D_PORTA_WAYLAND == 9,
               "le porte delle app: 3 compositore, 4 catalog, 5 launcher, 6 network, 7 picker, 8 tasks, 9 Wayland");

/* La porta verso il compositore (il suo formato): 'tipo' PORTA_APP (il protocollo nostro) o
 * PORTA_WAYLAND (la connessione Wayland). Il capo dell'app, o -1. */
static int porta_compositore(unsigned int tipo, const char *programma) {
    int porta[2];
    if (coppia(porta) < 0) return -1;
    int c = collega(NOME_COMPOSITORE);
    struct porta_app m = { .tipo = tipo };
    descrizione_copia(m.programma, programma, sizeof m.programma);
    int data = c >= 0 && manda_foglio(c, &m, sizeof m, porta[1]) >= 0;
    if (c >= 0) chiudi(c);
    chiudi(porta[1]);
    if (data) return porta[0];
    chiudi(porta[0]);
    return -1;
}

/* La porta verso un servizio dello standard dei messaggi (attacca_porta): il capo dell'app, o -1. */
static int porta_servizio(const char *nome, const struct chi_chiede *app) {
    int porta[2];
    if (coppia(porta) < 0) return -1;
    int data = attacca_porta(nome, app, porta[1]) == 0;
    chiudi(porta[1]);
    if (data) return porta[0];
    chiudi(porta[0]);
    return -1;
}

/* La porta verso il launcher stesso: il capo dell'app, o -1. Il capo del servizio resta in porte[]. */
static int porta_launcher(const struct chi_chiede *app) {
    int p = 0;
    while (p < MAX_PORTE && porte[p].d >= 0) p++;
    if (p == MAX_PORTE) return -1;
    int porta[2];
    if (coppia(porta) < 0) return -1;
    porte[p].d = porta[0];
    porte[p].usata = 0;
    porte[p].chi = *app;
    custodisci_porta(D_CUSTODE, app, porta[0]);   /* init ne tiene una copia: se cado, la riavro' */
    return porta[1];
}

/* CHI E' L'APP, per il kernel: ogni app parte come la persona (UID_APPLICAZIONI), tranne UNA.
 *
 * IL TERMINALE PARTE COME SISTEMA (uid 0). PROVVISORIO, decisione della persona del 17/11/2026:
 * la shell di prova serve come super amministratore, e la fa partire il Terminale stesso (con
 * fork ed exec, sul suo terminale finto): la shell eredita i poteri di chi la fa partire, quindi
 * e' il Terminale a dover essere sistema. Prima c'era una richiesta apposta al launcher (la
 * shell come sistema su un terminale allegato), che qualunque app poteva fare; ora non c'e' piu':
 * questo e' l'unico punto del sistema in cui un'app diventa sistema, e vale solo per l'app
 * "terminale" di /system/apps (un'app installata dalla persona non puo' avere quell'id, e se
 * capitasse non e' di_sistema). Il Terminale resta senza recinto; tutte le altre app ce l'hanno.
 * Le porte verso i servizi restano quelle di un'app con i suoi permessi (permessi_concessi):
 * cambia solo l'uid del processo.
 *
 * Per far girare il Terminale (e la sua shell) come la persona basta togliere questo caso; per
 * le azioni da sistema servira' poi un comando come sudo (STATO.md, "la shell del Terminale"). */
#define ID_TERMINALE "terminale"
static unsigned int uid_dell_app(const struct descrizione *d) {
    if (d->di_sistema && descrizione_uguali(d->id, ID_TERMINALE)) return UID_SISTEMA;
    return UID_APPLICAZIONI;
}

/* LE APP LINUX (manifesto linux = true; APPLICAZIONI.md, "Le app Linux"). Decisioni della persona
 * (18/11/2026): come sul Mac, ogni app Linux porta nel pacchetto le sue librerie, in un albero
 * tipo Alpine (<pacchetto>/linux/lib, linux/usr/lib, linux/usr/share, linux/etc); il caricatore
 * di musl invece e' del SISTEMA, come dyld sul Mac (/lib/ld-musl-<cpu>.so.1, il percorso che i
 * programmi hanno scritto dentro, collegamento al file vero in /system/lib: build.sh). Quindi
 * l'app parte come un programma Linux qualsiasi: il kernel fa partire il caricatore, che cerca
 * le librerie in LD_LIBRARY_PATH (le due cartelle del pacchetto). LD_LIBRARY_PATH passa anche ai
 * programmi Linux che l'app fa partire, che trovano lo stesso caricatore.
 * Le altre variabili d'ambiente standard dicono all'app dove sono le sue cose: i caratteri
 * (FONTCONFIG_FILE, il fonts.conf del pacchetto, che punta a cartelle relative a se'), i dati
 * condivisi (XDG_DATA_DIRS), le descrizioni dei terminali (TERMINFO), e le sue cartelle
 * scrivibili, tutte nella sua cartella dei dati (XDG_CONFIG_HOME, XDG_CACHE_HOME, XDG_DATA_HOME,
 * XDG_STATE_HOME, XDG_RUNTIME_DIR). HOME e' la Home della persona, SHELL e PATH la nostra shell
 * e i nostri comandi.
 * PER ORA SENZA RECINTO (decisione della persona: il sistema deve funzionare anche senza). */
#define MAX_VARIABILE (32 + 2 * MAX_PERCORSO)

static int avvia_app_linux(const struct descrizione *d, unsigned int uid, const char *argomento, const int *passati,
                           int quanti, const char *elenco, int wayland) {
    static char pacchetto[MAX_PERCORSO], dati[MAX_PERCORSO], home[MAX_PERCORSO];
    static char v[12][MAX_VARIABILE];
    descrizione_copia(pacchetto, cartelle_applicazioni[d->di_sistema ? CARTELLA_APP_SISTEMA : CARTELLA_APP_DATI],
                      sizeof pacchetto);
    descrizione_aggiungi(pacchetto, "/", sizeof pacchetto);
    descrizione_aggiungi(pacchetto, d->id, sizeof pacchetto);
    if (pacchetto_cartella_dati(d->id, dati, sizeof dati) < 0 || cartella_della_persona(home, "home") < 0) {
        registro("la cartella dei dati o la Home non vanno bene per ", d->id);
        return -1;
    }
    /* una variabile: NOME=<base><resto> */
    int k = 0;
#define VARIABILE(nome, base, resto)                                         do {                                                                         descrizione_copia(v[k], nome "=", MAX_VARIABILE);                        descrizione_aggiungi(v[k], base, MAX_VARIABILE);                         descrizione_aggiungi(v[k], resto, MAX_VARIABILE);                        k++;                                                                 } while (0)
    VARIABILE("LD_LIBRARY_PATH", pacchetto, "/linux/lib:");          /* le librerie del pacchetto */
    descrizione_aggiungi(v[k - 1], pacchetto, MAX_VARIABILE);
    descrizione_aggiungi(v[k - 1], "/linux/usr/lib", MAX_VARIABILE);
    VARIABILE("FONTCONFIG_FILE", pacchetto, "/linux/etc/fonts/fonts.conf");
    VARIABILE("XDG_DATA_DIRS", pacchetto, "/linux/usr/share");
    VARIABILE("TERMINFO", pacchetto, "/linux/usr/share/terminfo");   /* le descrizioni dei terminali */
    VARIABILE("XDG_CONFIG_HOME", dati, "/config");
    VARIABILE("XDG_CACHE_HOME", dati, "/cache");
    VARIABILE("XDG_DATA_HOME", dati, "/data");
    VARIABILE("XDG_STATE_HOME", dati, "/state");
    VARIABILE("XDG_RUNTIME_DIR", dati, "");
    VARIABILE("HOME", home, "");
#undef VARIABILE
    const char *ambiente[20];
    int n = 0;
    ambiente[n++] = elenco;
    if (wayland) ambiente[n++] = "WAYLAND_SOCKET=9";
    for (int i = 0; i < k; i++) ambiente[n++] = v[i];
    ambiente[n++] = "SHELL=/system/bin/shell";
    ambiente[n++] = "PATH=/system/bin";
    ambiente[n++] = "LANG=C.UTF-8";
    ambiente[n] = 0;
    registro(d->id, ": app Linux, con le librerie del suo pacchetto e il caricatore del sistema, senza recinto");
    return avvia_app(d->programma, uid, argomento, passati, quanti, ambiente);
}

static int fai_partire_app(const struct descrizione *d, const char *argomento) {
    const char *programma = d->programma;
    int nulla = apri_nulla();
    if (nulla < 0) {
        registro("non riesco ad aprire /dev/null per ", programma);
        return -1;
    }
    static struct chi_chiede app;
    permessi_concessi(d, &app);
    /* il recinto prima delle porte: se non si puo' fare, l'app non parte e non resta niente
     * di aperto a suo nome */
    static struct recinto recinto;
    unsigned int uid = uid_dell_app(d);
    /* fuori dal recinto: il Terminale del sistema e, per ora, le app Linux */
    int con_recinto = uid != UID_SISTEMA && !d->app_linux;
    if (d->app_linux) {
        /* (lo dice avvia_app_linux) */
    } else if (!con_recinto) {
        registro(d->id, ": senza recinto (il Terminale, decisione della persona)");
    } else if (prepara_recinto(d, &app, &recinto) < 0) {
        registro("niente recinto, l'app non parte: ", programma);
        chiudi(nulla);
        return -1;
    }
    int compositore = porta_compositore(PORTA_APP, programma);
    int wayland = porta_compositore(PORTA_WAYLAND, programma);
    int catalog = porta_servizio(NOME_CATALOG, &app);
    int launcher = porta_launcher(&app);
    int picker = porta_servizio(NOME_PICKER, &app);   /* di base: scegliere un file non chiede permessi */
    /* la rete solo a chi ha un suo permesso: senza, la porta non c'e' (e non e' un errore); con il
     * permesso ma senza risposta del servizio si scrive come per gli altri */
    int vuole_rete = ha_concesso(&app, "network.use") || ha_concesso(&app, "network.settings");
    int rete = vuole_rete ? porta_servizio(NOME_NETWORK, &app) : -1;
    if (vuole_rete && rete < 0) registro("niente porta verso network per ", programma);
    /* i processi, la memoria e il disco: stessa regola (processes.stop comprende processes.view) */
    int vuole_tasks = ha_concesso(&app, "processes.view") || ha_concesso(&app, "processes.stop");
    int tasks = vuole_tasks ? porta_servizio(NOME_TASKS, &app) : -1;
    if (vuole_tasks && tasks < 0) registro("niente porta verso tasks per ", programma);
    if (compositore < 0) registro("niente porta verso il compositore per ", programma);
    if (catalog < 0) registro("niente porta verso catalog per ", programma);
    if (launcher < 0) registro("niente porta verso il launcher per ", programma);
    if (picker < 0) registro("niente porta verso picker per ", programma);
    if (wayland < 0) registro("niente connessione Wayland per ", programma);
    int passati[] = { nulla, nulla, nulla, compositore >= 0 ? compositore : nulla, catalog >= 0 ? catalog : nulla,
                      launcher >= 0 ? launcher : nulla, rete >= 0 ? rete : nulla,
                      picker >= 0 ? picker : nulla, tasks >= 0 ? tasks : nulla, wayland >= 0 ? wayland : nulla };
    /* L'ELENCO DELLE PORTE (messaggi.h, VARIABILE_PORTE): solo quelle date davvero, cosi' l'app
     * non deve indovinare quali descrittori sono porte. Es. "MSHELL_PORTE=compositore=3 catalog=4" */
    char elenco[128];
    descrizione_copia(elenco, VARIABILE_PORTE "=", sizeof elenco);
    if (compositore >= 0) descrizione_aggiungi(elenco, NOME_COMPOSITORE "=3 ", sizeof elenco);
    if (catalog >= 0) descrizione_aggiungi(elenco, NOME_CATALOG "=4 ", sizeof elenco);
    if (launcher >= 0) descrizione_aggiungi(elenco, NOME_LAUNCHER "=5 ", sizeof elenco);
    if (rete >= 0) descrizione_aggiungi(elenco, NOME_NETWORK "=6 ", sizeof elenco);
    if (picker >= 0) descrizione_aggiungi(elenco, NOME_PICKER "=7 ", sizeof elenco);
    if (tasks >= 0) descrizione_aggiungi(elenco, NOME_TASKS "=8 ", sizeof elenco);
    /* senza connessione, niente WAYLAND_SOCKET: al 9 c'e' /dev/null */
    const char *ambiente[] = { elenco, wayland >= 0 ? "WAYLAND_SOCKET=9" : 0, 0 };
    char riga[MAX_ID + 96];
    descrizione_copia(riga, d->id, sizeof riga);
    descrizione_aggiungi(riga, ": porte", sizeof riga);
    if (compositore >= 0) descrizione_aggiungi(riga, " compositore", sizeof riga);
    if (catalog >= 0) descrizione_aggiungi(riga, " catalog", sizeof riga);
    if (launcher >= 0) descrizione_aggiungi(riga, " launcher", sizeof riga);
    if (rete >= 0) descrizione_aggiungi(riga, " network", sizeof riga);
    if (picker >= 0) descrizione_aggiungi(riga, " picker", sizeof riga);
    if (tasks >= 0) descrizione_aggiungi(riga, " tasks", sizeof riga);
    if (wayland >= 0) descrizione_aggiungi(riga, " wayland", sizeof riga);
    registro(riga, "");
    int quanti = (int)(sizeof passati / sizeof passati[0]);
    int numero = d->app_linux    ? avvia_app_linux(d, uid, argomento, passati, quanti, elenco, wayland >= 0)
                 : con_recinto ? avvia_app_nel_recinto(programma, uid, argomento, passati, quanti, ambiente, &recinto)
                               : avvia_app(programma, uid, argomento, passati, quanti, ambiente);
    if (numero < 0) registro("l'app non e' partita: ", programma);
    chiudi(nulla);
    if (compositore >= 0) chiudi(compositore);
    if (catalog >= 0) chiudi(catalog);
    if (launcher >= 0) chiudi(launcher);
    if (rete >= 0) chiudi(rete);
    if (picker >= 0) chiudi(picker);
    if (tasks >= 0) chiudi(tasks);
    if (wayland >= 0) chiudi(wayland);
    return numero;
}

static void apri_app(const struct messaggio *q, struct messaggio *r) {
    const char *app = dato(q, CHIAVE_APP, MAX_NOME_FILE);
    const char *argomento = messaggio_valore(q, CHIAVE_ARGOMENTO);   /* facoltativo */
    if (!app || (argomento && lunghezza(argomento) >= MAX_PERCORSO)) {
        risposta(r, q, ESITO_NON_VALIDO, 0, 0);
        return;
    }
    struct descrizione desc;
    if (leggi_descrizione(app, &desc) < 0) {
        risposta(r, q, ESITO_ERRORE, MOTIVO_NON_INSTALLATA, "Quest'app non \xc3\xa8 installata.");
        return;
    }
    partito(r, q, fai_partire_app(&desc, argomento ? argomento : ""));
}

static void apri_un_file(const struct messaggio *q, struct messaggio *r) {
    const char *percorso = dato(q, CHIAVE_PERCORSO, MAX_PERCORSO);
    if (!percorso) {
        risposta(r, q, ESITO_NON_VALIDO, 0, 0);
        return;
    }
    if (!finisce_con(percorso, ESTENSIONE_PACCHETTO)) {
        risposta(r, q, ESITO_ERRORE, MOTIVO_NON_SUPPORTATO, "Per ora il sistema non sa aprire questo tipo di file.");
        return;
    }
    struct descrizione store;
    if (leggi_descrizione(ID_STORE, &store) < 0) {
        risposta(r, q, ESITO_ERRORE, MOTIVO_SERVE_STORE, "Le app vanno installate prima di aprirle.");
        registro("le app vanno installate prima di aprirle: ", percorso);
        return;
    }
    partito(r, q, fai_partire_app(&store, percorso));
}

/* Un programma normale di /system/bin (non un'app), come la persona. Il percorso deve stare
 * proprio li': niente ".." per uscirne. */
static void apri_un_programma(const struct chi_chiede *c, const struct messaggio *q, struct messaggio *r) {
    if (!ha_permesso(c, PERMESSO_PROGRAMMI)) {
        risposta(r, q, ESITO_NEGATO, 0, 0);
        return;
    }
    const char *p = dato(q, CHIAVE_PERCORSO, MAX_PERCORSO), *cartella = CARTELLA_PROGRAMMI;
    if (!p) {
        risposta(r, q, ESITO_NON_VALIDO, 0, 0);
        return;
    }
    int k = 0;
    while (cartella[k] && p[k] == cartella[k]) k++;
    int dentro = !cartella[k] && p[k];
    for (const char *s = p + k; dentro && *s; s++)
        if (*s == '/') dentro = 0;           /* solo i file di /system/bin, non piu' giu' */
    if (!dentro || descrizione_uguali(p + k, "..") || descrizione_uguali(p + k, ".")) {
        risposta(r, q, ESITO_NEGATO, 0, "Si possono aprire solo i programmi del sistema.");
        return;
    }
    partito(r, q, avvia_come(p, UID_APPLICAZIONI));
}

/* Chi chiede puo' installare, aggiornare, togliere e svuotare i dati? "apps.manage" e' un permesso
 * di sistema: ce l'ha il sistema, e un'app del sistema che lo dichiara (lo dice permessi_concessi,
 * quando le da' la porta). */
static int puo_gestire(const struct chi_chiede *c) { return ha_permesso(c, PERMESSO_GESTIRE_APP); }

/* Le richieste sui pacchetti: il lavoro in un foglio, che si allega alla risposta (anche
 * quando non e' andata: e' il resoconto a dire perche'). */
static void pacchetti(const struct chi_chiede *c, const struct messaggio *q, struct messaggio *r,
                      const char *richiesta) {
    int elenco = descrizione_uguali(richiesta, ELENCO_APP);
    unsigned int byte = elenco ? sizeof(struct elenco_app) : sizeof(struct resoconto);
    int f = foglio(byte);
    void *dove = f >= 0 ? mappa(f, 0) : 0;
    if (!dove) {
        if (f >= 0) chiudi(f);
        risposta(r, q, ESITO_ERRORE, 0, 0);
        return;
    }
    if (elenco) {
        pacchetto_elenco(dove);
        messaggio_risposta(r, q, ESITO_OK);
    } else if (q->allegato < 0) {
        risposta(r, q, ESITO_NON_VALIDO, 0, "Manca il file del pacchetto.");
    } else if (descrizione_uguali(richiesta, DESCRIVI_PACCHETTO)) {
        if (pacchetto_descrivi(q->allegato, PACCHETTO_DESCRIVI, dove) == 0) messaggio_risposta(r, q, ESITO_OK);
        else risposta(r, q, ESITO_ERRORE, MOTIVO_PACCHETTO, 0);
    } else if (!puo_gestire(c)) {
        risposta(r, q, ESITO_NEGATO, 0, "Solo lo Store pu\xc3\xb2 installare o aggiornare le app.");
    } else {
        int cosa = descrizione_uguali(richiesta, INSTALLA_PACCHETTO) ? PACCHETTO_INSTALLA : PACCHETTO_AGGIORNA;
        struct resoconto *res = dove;
        int fatto = pacchetto_installa(q->allegato, cosa, res) == 0;
        if (fatto) messaggio_risposta(r, q, ESITO_OK);
        else risposta(r, q, ESITO_ERRORE, MOTIVO_PACCHETTO, 0);
        if (res->manifesto_letto)
            registro(fatto ? (cosa == PACCHETTO_INSTALLA ? "installata " : "aggiornata ") : "pacchetto rifiutato: ",
                     res->m.id);
    }
    togli(dove);
    r->allegato = f;     /* lo chiude rispondi, dopo averlo mandato */
}

/* Togliere un'app o svuotarne i dati: lo stesso permesso di installare. */
static void togli_o_svuota(const struct chi_chiede *c, const struct messaggio *q, struct messaggio *r, int togliere) {
    if (!puo_gestire(c)) {
        risposta(r, q, ESITO_NEGATO, 0, "Solo lo Store pu\xc3\xb2 togliere le app o svuotarne i dati.");
        return;
    }
    const char *app = messaggio_valore(q, CHIAVE_APP);
    if (!app) {
        risposta(r, q, ESITO_NON_VALIDO, 0, 0);
        return;
    }
    char id[MAX_ID];
    descrizione_copia(id, app, MAX_ID);
    if (lunghezza(app) >= MAX_ID) id[0] = 0;   /* troppo lungo: non valido */
    int tieni_dati = messaggio_vale(q, CHIAVE_TIENI_DATI, "1");
    char frase[MAX_FRASE_LAUNCHER] = { 0 };
    int esito = togliere ? pacchetto_togli(id, tieni_dati, frase, MAX_FRASE_LAUNCHER)
                         : pacchetto_svuota_dati(id, frase, MAX_FRASE_LAUNCHER);
    switch (esito) {
    case TOGLI_FATTO:          risposta(r, q, ESITO_OK, 0, frase); break;
    case TOGLI_ID_SBAGLIATO:   risposta(r, q, ESITO_NON_VALIDO, 0, frase); break;
    case TOGLI_DEL_SISTEMA:    risposta(r, q, ESITO_ERRORE, MOTIVO_DEL_SISTEMA, frase); break;
    case TOGLI_NON_INSTALLATA: risposta(r, q, ESITO_ERRORE, MOTIVO_NON_INSTALLATA, frase); break;
    case TOGLI_IN_USO:         risposta(r, q, ESITO_ERRORE, MOTIVO_IN_USO, frase); break;
    default:                   risposta(r, q, ESITO_ERRORE, 0, frase); break;
    }
    if (esito == TOGLI_FATTO)
        registro(togliere ? (tieni_dati ? "tolta (dati tenuti) " : "tolta ") : "dati svuotati di ", id);
}

/* Una richiesta e la sua risposta, sul collegamento 'd'. */
static void rispondi(int d, const struct chi_chiede *dalla_porta) {
    struct messaggio q, r;
    int e = messaggio_ricevi(d, &q);
    if (e == 0 || e == -1) return;
    if (e == MESSAGGIO_NON_VALIDO) {
        messaggio_risposta(&r, 0, ESITO_NON_VALIDO);
    } else {
        struct chi_chiede c;
        if (dalla_porta) {             /* da una porta: lo sa il launcher, non il kernel */
            c = *dalla_porta;
        } else if (chi_chiede(d, &c) < 0) {   /* il kernel non lo sa: nessuno, nessun permesso vero */
            c.uid = -1;
            c.programma[0] = 0;
        }
        const char *richiesta = messaggio_valore(&q, CHIAVE_RICHIESTA);
        if (!richiesta) richiesta = "";
        if (descrizione_uguali(richiesta, APRI_APPLICAZIONE)) apri_app(&q, &r);
        else if (descrizione_uguali(richiesta, APRI_FILE)) apri_un_file(&q, &r);
        else if (descrizione_uguali(richiesta, APRI_PROGRAMMA)) apri_un_programma(&c, &q, &r);
        else if (descrizione_uguali(richiesta, RIMUOVI_PACCHETTO)) togli_o_svuota(&c, &q, &r, 1);
        else if (descrizione_uguali(richiesta, SVUOTA_DATI)) togli_o_svuota(&c, &q, &r, 0);
        else if (descrizione_uguali(richiesta, DESCRIVI_PACCHETTO) || descrizione_uguali(richiesta, INSTALLA_PACCHETTO) ||
                 descrizione_uguali(richiesta, AGGIORNA_PACCHETTO) || descrizione_uguali(richiesta, ELENCO_APP))
            pacchetti(&c, &q, &r, richiesta);
        else risposta(&r, &q, ESITO_NON_VALIDO, 0, 0);   /* una richiesta che il launcher non conosce */
        if (q.allegato >= 0) chiudi(q.allegato);
    }
    /* Chi chiede non ci blocca: il collegamento e' nuovo e porta solo questa risposta, che
     * ci sta sempre nella casella del kernel, quindi messaggio_manda non dorme mai. */
    messaggio_manda(d, &r);
    if (r.allegato >= 0) chiudi(r.allegato);
}

/* Un messaggio sulla porta p: un connect con il capo allegato da' il collegamento nuovo (chi c'e'
 * lo dice il launcher, porte[p].chi); l'app chiusa o un messaggio sbagliato chiudono la porta.
 * Ritorna il collegamento nuovo, o -1. */
static int da_porta(int p) {
    static struct messaggio m;
    int n = messaggio_ricevi(porte[p].d, &m);
    if (n == 1 && m.allegato >= 0 && messaggio_vale(&m, CHIAVE_RICHIESTA, RICHIESTA_COLLEGA)) {
        if (!porte[p].usata) registro("collegamento dalla porta di ", porte[p].chi.programma);
        porte[p].usata = 1;
        return m.allegato;
    }
    if (n == 1 && m.allegato >= 0) chiudi(m.allegato);
    chiudi(porte[p].d);
    porte[p].d = -1;
    return -1;
}

/* Una porta che init ci riconsegna dopo una ripartenza: va nella tabella com'era, e il collegamento
 * e' gia' stato scritto nel registro da chi c'era prima. */
static void riprendi(const struct chi_chiede *app, int capo, void *dato) {
    (void)dato;
    int p = 0;
    while (p < MAX_PORTE && porte[p].d >= 0) p++;
    if (p == MAX_PORTE) { chiudi(capo); return; }
    porte[p].d = capo;
    porte[p].usata = 1;
    porte[p].chi = *app;
}

int main(void) {
    int clienti[MAX_CLIENTI];
    int da_porta_di[MAX_CLIENTI];   /* -1: chi e' lo dice il kernel; altrimenti il numero della porta */
    struct chi_chiede identita[MAX_CLIENTI];   /* per i collegamenti dalle porte, copiata alla nascita */
    int quanti = 0;
    for (int p = 0; p < MAX_PORTE; p++) porte[p].d = -1;
    riprendi_porte(D_CUSTODE, riprendi, 0);   /* le porte delle app che teneva init, se sono ripartito */
    for (;;) {
        struct da_guardare g[1 + MAX_CLIENTI + MAX_PORTE];
        int numero_porta[MAX_PORTE];
        int n = 1 + quanti, inizio_porte = n;
        g[0].descrittore = D_ASCOLTO;
        for (int i = 0; i < quanti; i++) g[1 + i].descrittore = clienti[i];
        for (int p = 0; p < MAX_PORTE; p++)
            if (porte[p].d >= 0) { numero_porta[n - inizio_porte] = p; g[n++].descrittore = porte[p].d; }
        if (aspetta(g, n, -1) < 0) return 1;

        /* le richieste: una per collegamento, poi lo si chiude */
        for (int i = quanti - 1; i >= 0; i--) {
            if (!g[1 + i].pronto) continue;
            rispondi(clienti[i], da_porta_di[i] >= 0 ? &identita[i] : 0);
            chiudi(clienti[i]);
            quanti--;
            clienti[i] = clienti[quanti];
            da_porta_di[i] = da_porta_di[quanti];
            identita[i] = identita[quanti];
        }
        /* i collegamenti arrivati dalle porte: un cliente con l'identita' dell'app */
        for (int k = inizio_porte; k < n; k++) {
            int p = numero_porta[k - inizio_porte];
            if (!g[k].pronto || porte[p].d != g[k].descrittore) continue;
            int d = da_porta(p);
            if (d < 0) continue;
            if (quanti < MAX_CLIENTI) {
                clienti[quanti] = d;
                da_porta_di[quanti] = p;
                identita[quanti] = porte[p].chi;
                quanti++;
            } else {
                chiudi(d);
            }
        }
        if (g[0].pronto) {
            int d = accetta(D_ASCOLTO);
            if (d >= 0 && quanti < MAX_CLIENTI) { da_porta_di[quanti] = -1; clienti[quanti++] = d; }
            else if (d >= 0) chiudi(d);
        }
    }
}
