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
 *     (uid 1000), con l'argomento della richiesta se c'e', e con la sua porta verso il
 *     compositore (fai_partire_app). E' per questo che il launcher e'
 *     del sistema (uid 0): solo il sistema puo' far partire un programma con un altro uid;
 *   - open_program: un programma normale di /system/bin (non un'app; per ora nessuno lo
 *     usa), come la persona (uid 1000);
 *   - open_shell: la shell di prova come SISTEMA (uid 0), sul terminale finto che chi chiede
 *     gli passa (oggi il Terminale);
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
#include "corekit/sistema/network.h"     /* NOME_NETWORK, D_PORTA_NETWORK: la porta verso la rete */

#define D_ASCOLTO 0      /* prestato da init */
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

/* LA HOME NEL RECINTO (APPLICAZIONI.md, sezione 5, "I permessi sulle cartelle"). Fuori dalle
 * sue cartelle, un'app puo' avere solo la Home della persona in uso, intera: in sola lettura
 * con "files.read", in lettura e scrittura con "files.write" (che comprende la lettura). */
#define HOME_NIENTE    0
#define HOME_LETTURA   1
#define HOME_SCRITTURA 2

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

/* Cosa dice il manifesto dell'app sulla Home. Il permesso dichiarato vale come concesso finche'
 * non c'e' la domanda del primo avvio (permessi_concessi). */
static int home_dell_app(const struct descrizione *d) {
    static struct manifesto m;    /* grande: non sulla pila (le richieste sono una alla volta) */
    char cartella[MAX_PERCORSO];
    descrizione_copia(cartella, cartelle_applicazioni[d->di_sistema ? CARTELLA_APP_SISTEMA : CARTELLA_APP_DATI],
                      sizeof cartella);
    descrizione_aggiungi(cartella, "/", sizeof cartella);
    descrizione_aggiungi(cartella, d->id, sizeof cartella);
    if (manifesto_dell_app(cartella, &m) < 0) return HOME_NIENTE;
    int home = HOME_NIENTE;
    for (int i = 0; i < m.quanti_permessi; i++) {
        if (descrizione_uguali(m.permessi[i], "files.write")) home = HOME_SCRITTURA;
        else if (descrizione_uguali(m.permessi[i], "files.read") && home == HOME_NIENTE) home = HOME_LETTURA;
    }
    return home;
}

/* Scrive nel registro cosa il recinto agganciera' all'app. Per ora solo questo: il recinto
 * nel launcher non c'e' ancora (STATO.md). */
static void registra_home(const struct descrizione *d) {
    static const char *modi[] = {"niente Home", "Home in sola lettura (files.read)",
                                 "Home in lettura e scrittura (files.write)"};
    char riga[MAX_ID + 160];
    descrizione_copia(riga, d->id, sizeof riga);
    descrizione_aggiungi(riga, ": nel recinto ", sizeof riga);
    descrizione_aggiungi(riga, modi[home_dell_app(d)], sizeof riga);
    registro(riga, " (da agganciare: il recinto non c'e' ancora)");
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
 * Poi il launcher chiude i suoi capi: app e servizi parlano da soli. Se un servizio non risponde
 * (o il permesso manca), al posto della porta c'e' /dev/null (l'app si collega per nome) e lo si
 * scrive nel registro. Per ogni app una riga dice quali porte ha avuto.
 * I descrittori 0, 1 e 2 (ingresso, uscita, errori: la convenzione di ogni programma) sono
 * /dev/null: se restassero chiusi, il primo file o collegamento che l'app apre finirebbe li', e
 * quello che l'app stampa (Doom stampa molto) ci andrebbe dentro. */
_Static_assert(D_PORTA_APP == 3 && D_PORTA_CATALOG == 4 && D_PORTA_LAUNCHER == 5 && D_PORTA_NETWORK == 6,
               "le porte delle app: 3 compositore, 4 catalog, 5 launcher, 6 network");

/* La porta verso il compositore (il suo formato): il capo dell'app, o -1. */
static int porta_compositore(const char *programma) {
    int porta[2];
    if (coppia(porta) < 0) return -1;
    int c = collega(NOME_COMPOSITORE);
    struct porta_app m = { .tipo = PORTA_APP };
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
    return porta[1];
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
    int compositore = porta_compositore(programma);
    int catalog = porta_servizio(NOME_CATALOG, &app);
    int launcher = porta_launcher(&app);
    /* la rete solo a chi ha un suo permesso: senza, la porta non c'e' (e non e' un errore); con il
     * permesso ma senza risposta del servizio si scrive come per gli altri */
    int vuole_rete = ha_concesso(&app, "network.use") || ha_concesso(&app, "network.settings");
    int rete = vuole_rete ? porta_servizio(NOME_NETWORK, &app) : -1;
    if (vuole_rete && rete < 0) registro("niente porta verso network per ", programma);
    if (compositore < 0) registro("niente porta verso il compositore per ", programma);
    if (catalog < 0) registro("niente porta verso catalog per ", programma);
    if (launcher < 0) registro("niente porta verso il launcher per ", programma);
    int passati[] = { nulla, nulla, nulla, compositore >= 0 ? compositore : nulla, catalog >= 0 ? catalog : nulla,
                      launcher >= 0 ? launcher : nulla, rete >= 0 ? rete : nulla };
    char riga[MAX_ID + 96];
    descrizione_copia(riga, d->id, sizeof riga);
    descrizione_aggiungi(riga, ": porte", sizeof riga);
    if (compositore >= 0) descrizione_aggiungi(riga, " compositore", sizeof riga);
    if (catalog >= 0) descrizione_aggiungi(riga, " catalog", sizeof riga);
    if (launcher >= 0) descrizione_aggiungi(riga, " launcher", sizeof riga);
    if (rete >= 0) descrizione_aggiungi(riga, " network", sizeof riga);
    registro(riga, "");
    int numero = avvia_app(programma, UID_APPLICAZIONI, argomento, passati, (int)(sizeof passati / sizeof passati[0]));
    chiudi(nulla);
    if (compositore >= 0) chiudi(compositore);
    if (catalog >= 0) chiudi(catalog);
    if (launcher >= 0) chiudi(launcher);
    if (rete >= 0) chiudi(rete);
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
    registra_home(&desc);
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

/* La shell come SISTEMA sul terminale (lo schiavo del pty) che chi chiede ha allegato.
 * PROVVISORIO, decisione della persona: terminale come sistema aperto a chiunque (oggi
 * ha_permesso dice sempre si'): da proteggere (STATO.md). */
static void apri_la_shell(const struct chi_chiede *c, const struct messaggio *q, struct messaggio *r) {
    if (!ha_permesso(c, PERMESSO_SHELL)) {
        risposta(r, q, ESITO_NEGATO, 0, 0);
        return;
    }
    if (q->allegato < 0) {
        risposta(r, q, ESITO_NON_VALIDO, 0, "Manca il terminale per la shell.");
        return;
    }
    int passati[3] = { q->allegato, q->allegato, q->allegato };
    partito(r, q, avvia_come_con_descrittori(PROGRAMMA_SHELL, UID_SISTEMA, passati, 3));
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
        else if (descrizione_uguali(richiesta, APRI_SHELL)) apri_la_shell(&c, &q, &r);
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

int main(void) {
    int clienti[MAX_CLIENTI];
    int da_porta_di[MAX_CLIENTI];   /* -1: chi e' lo dice il kernel; altrimenti il numero della porta */
    struct chi_chiede identita[MAX_CLIENTI];   /* per i collegamenti dalle porte, copiata alla nascita */
    int quanti = 0;
    for (int p = 0; p < MAX_PORTE; p++) porte[p].d = -1;
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
