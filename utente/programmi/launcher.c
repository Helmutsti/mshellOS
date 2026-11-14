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
 *     (uid 1000), con l'argomento della richiesta se c'e'. E' per questo che il launcher e'
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
 * ha_permesso (servizi.h), che oggi dice sempre si' e lo scrive nel registro
 * (PROVVISORIO). Per installare, aggiornare, togliere e svuotare resta in piu' il controllo
 * vero di prima: il sistema, o un'app del sistema con "apps.manage" nel manifesto.
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

#define D_ASCOLTO 0      /* prestato da init */
#define MAX_CLIENTI 8

static void registro(const char *a, const char *b) {
    scrivi_testo("launcher: ");
    scrivi_testo(a);
    scrivi_testo(b);
    scrivi_testo("\n");
}

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
    partito(r, q, avvia_come_con(desc.programma, UID_APPLICAZIONI, argomento ? argomento : ""));
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
    partito(r, q, avvia_come_con(store.programma, UID_APPLICAZIONI, percorso));
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

/* Chi chiede puo' installare, aggiornare, togliere e svuotare i dati? Prima la domanda a
 * ha_permesso (provvisoria: oggi si', e la scrive nel registro); poi il controllo vero di
 * oggi: il sistema, o un'app del sistema con il permesso "apps.manage" nel manifesto. */
static int puo_gestire(const struct chi_chiede *c) {
    if (!ha_permesso(c, PERMESSO_GESTIRE_APP)) return 0;
    return c->uid == UID_SISTEMA || (c->programma[0] && pacchetto_puo_gestire(c->programma));
}

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
static void rispondi(int d) {
    struct messaggio q, r;
    int e = messaggio_ricevi(d, &q);
    if (e == 0 || e == -1) return;
    if (e == MESSAGGIO_NON_VALIDO) {
        messaggio_risposta(&r, 0, ESITO_NON_VALIDO);
    } else {
        struct chi_chiede c;
        if (chi_chiede(d, &c) < 0) {   /* il kernel non lo sa: nessuno, nessun permesso vero */
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

int main(void) {
    int clienti[MAX_CLIENTI];
    int quanti = 0;
    for (;;) {
        struct da_guardare g[1 + MAX_CLIENTI];
        g[0].descrittore = D_ASCOLTO;
        for (int i = 0; i < quanti; i++) g[1 + i].descrittore = clienti[i];
        if (aspetta(g, 1 + quanti, -1) < 0) return 1;

        /* le richieste: una per collegamento, poi lo si chiude */
        for (int i = quanti - 1; i >= 0; i--) {
            if (!g[1 + i].pronto) continue;
            rispondi(clienti[i]);
            chiudi(clienti[i]);
            clienti[i] = clienti[--quanti];
        }
        if (g[0].pronto) {
            int d = accetta(D_ASCOLTO);
            if (d >= 0 && quanti < MAX_CLIENTI) clienti[quanti++] = d;
            else if (d >= 0) chiudi(d);
        }
    }
}
