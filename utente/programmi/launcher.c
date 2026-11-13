/* Il launcher: il servizio del sistema che fa partire le applicazioni e le installa.
 *
 * Non ha finestre, come LaunchServices sul Mac o systemd per GNOME: l'interfaccia (le
 * icone da cliccare, lo Store) e' di altri, che gli chiedono le cose (il protocollo e'
 * in launcher.h). Lui:
 *   - APRI_APPLICAZIONE: controlla che sia un'applicazione installata (una cartella con
 *     il suo manifest.toml in /system/apps o, se li' non c'e', in /data/apps:
 *     descrizione.h), prende dal manifesto quale programma far partire e lo fa partire
 *     come APPLICAZIONE (uid 1000), con l'argomento della richiesta se c'e'. E' per questo
 *     che il launcher e' del sistema (uid 0): solo il sistema puo' far partire un
 *     programma con un altro uid;
 *   - APRI_PROGRAMMA: un programma normale di /system/bin (non un'app; per ora nessuno lo usa),
 *     come la persona (uid 1000);
 *   - APRI_SHELL: la shell di prova come SISTEMA (uid 0), sul terminale finto che chi chiede
 *     gli passa (oggi il Terminale);
 *   - APRI_FILE: un pacchetto .app non si apre mai da solo: si fa partire lo Store su di
 *     lui, se e' installato; se no si risponde che le app vanno installate prima;
 *   - RIMUOVI_PACCHETTO, SVUOTA_DATI: togliere un'app della persona, svuotare i dati di
 *     un'app (pacchetto_togli, pacchetto_svuota_dati), con lo stesso permesso di installare;
 *   - DESCRIVI_PACCHETTO, INSTALLA_PACCHETTO, AGGIORNA_PACCHETTO, ELENCO_APP: il lavoro lo
 *     fa pacchetto.c; il launcher controlla chi puo' installare, legge il file che chi
 *     chiede gli passa gia' aperto, e risponde con il resoconto in un foglio. E' lui a
 *     farlo perche' /data/apps lo puo' scrivere solo il sistema.
 *
 * Il suo nome, "launcher", lo tiene init, che glielo presta al descrittore 0: se il
 * launcher cade, nessun altro puo' prenderlo, e chi chiede nel frattempo aspetta.
 * Le richieste si servono una alla volta: un'installazione grossa fa aspettare gli altri
 * per il tempo di scompattarla. */
#include "corekit/corekit.h"
#include "corekit/sistema/launcher.h"
#include "corekit/sistema/descrizione.h"
#include "corekit/sistema/pacchetto.h"

#define D_ASCOLTO 0      /* prestato da init */
#define MAX_CLIENTI 8

static void messaggio(struct risposta_launcher *r, const char *testo) {
    descrizione_copia(r->messaggio, testo, MAX_MESSAGGIO_LAUNCHER);
}

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

static void apri_app(struct richiesta_launcher *r, struct risposta_launcher *risposta) {
    struct descrizione desc;
    risposta->esito = LAUNCHER_NON_INSTALLATA;
    if (leggi_descrizione(r->applicazione, &desc) < 0) {
        messaggio(risposta, "Quest'app non \xc3\xa8 installata.");
        return;
    }
    risposta->numero = avvia_come_con(desc.programma, UID_APPLICAZIONI, r->argomento);
    risposta->esito = risposta->numero >= 0 ? LAUNCHER_FATTO : LAUNCHER_ERRORE;
}

static void apri_un_file(struct richiesta_launcher *r, struct risposta_launcher *risposta) {
    if (!finisce_con(r->argomento, ESTENSIONE_PACCHETTO)) {
        risposta->esito = LAUNCHER_NON_SUPPORTATO;
        messaggio(risposta, "Per ora il sistema non sa aprire questo tipo di file.");
        return;
    }
    struct descrizione store;
    if (leggi_descrizione(ID_STORE, &store) < 0) {
        risposta->esito = LAUNCHER_SERVE_STORE;
        messaggio(risposta, "Le app vanno installate prima di aprirle.");
        registro("le app vanno installate prima di aprirle: ", r->argomento);
        return;
    }
    risposta->numero = avvia_come_con(store.programma, UID_APPLICAZIONI, r->argomento);
    risposta->esito = risposta->numero >= 0 ? LAUNCHER_FATTO : LAUNCHER_ERRORE;
}

/* Un programma normale di /system/bin (non un'app), come la persona. Il percorso deve stare
 * proprio li': niente ".." per uscirne. */
static void apri_un_programma(struct richiesta_launcher *r, struct risposta_launcher *risposta) {
    const char *p = r->argomento, *cartella = CARTELLA_PROGRAMMI;
    int k = 0;
    while (cartella[k] && p[k] == cartella[k]) k++;
    int dentro = !cartella[k] && p[k];
    for (const char *s = p + k; dentro && *s; s++)
        if (*s == '/') dentro = 0;           /* solo i file di /system/bin, non piu' giu' */
    if (!dentro || descrizione_uguali(p + k, "..") || descrizione_uguali(p + k, ".")) {
        risposta->esito = LAUNCHER_NON_PERMESSO;
        messaggio(risposta, "Si possono aprire solo i programmi del sistema.");
        return;
    }
    risposta->numero = avvia_come(p, UID_APPLICAZIONI);
    risposta->esito = risposta->numero >= 0 ? LAUNCHER_FATTO : LAUNCHER_ERRORE;
}

/* La shell come SISTEMA sul terminale (lo schiavo del pty) che chi chiede ha allegato.
 * PROVVISORIO, decisione della persona: terminale come sistema aperto a chiunque (non si
 * guarda chi chiede): da proteggere (STATO.md). */
static void apri_la_shell(int schiavo, struct risposta_launcher *risposta) {
    if (schiavo < 0) {
        messaggio(risposta, "Manca il terminale per la shell.");
        return;
    }
    int passati[3] = { schiavo, schiavo, schiavo };
    risposta->numero = avvia_come_con_descrittori(PROGRAMMA_SHELL, UID_SISTEMA, passati, 3);
    risposta->esito = risposta->numero >= 0 ? LAUNCHER_FATTO : LAUNCHER_ERRORE;
}

/* Chi c'e' all'altro capo puo' installare? Il sistema, o un'app del sistema con il
 * permesso "app.gestire". */
static int puo_installare(int d) {
    if (chi(d) == UID_SISTEMA) return 1;
    char programma[MAX_PERCORSO_PROGRAMMA];
    return programma_di(d, programma) == 0 && pacchetto_puo_gestire(programma);
}

/* Le richieste sui pacchetti: il lavoro in un foglio, che si allega alla risposta. */
static void pacchetti(int d, struct richiesta_launcher *r, int file, struct risposta_launcher *risposta) {
    unsigned int byte = r->tipo == ELENCO_APP ? sizeof(struct elenco_app) : sizeof(struct resoconto);
    int f = foglio(byte);
    void *dove = f >= 0 ? mappa(f, 0) : 0;
    if (!dove) {
        if (f >= 0) chiudi(f);
        manda_subito(d, risposta, sizeof *risposta);
        return;
    }
    if (r->tipo == ELENCO_APP) {
        pacchetto_elenco(dove);
        risposta->esito = LAUNCHER_FATTO;
    } else if (file < 0) {
        messaggio(risposta, "Manca il file del pacchetto.");
    } else if (r->tipo == DESCRIVI_PACCHETTO) {
        risposta->esito = pacchetto_descrivi(file, PACCHETTO_DESCRIVI, dove) == 0 ? LAUNCHER_FATTO
                                                                                  : LAUNCHER_PACCHETTO_NON_VALIDO;
    } else if (!puo_installare(d)) {
        risposta->esito = LAUNCHER_NON_PERMESSO;
        messaggio(risposta, "Solo lo Store pu\xc3\xb2 installare o aggiornare le app.");
    } else {
        int cosa = r->tipo == INSTALLA_PACCHETTO ? PACCHETTO_INSTALLA : PACCHETTO_AGGIORNA;
        struct resoconto *res = dove;
        risposta->esito = pacchetto_installa(file, cosa, res) == 0 ? LAUNCHER_FATTO : LAUNCHER_PACCHETTO_NON_VALIDO;
        if (res->manifesto_letto)
            registro(risposta->esito == LAUNCHER_FATTO ? (cosa == PACCHETTO_INSTALLA ? "installata " : "aggiornata ")
                                                       : "pacchetto rifiutato: ", res->m.id);
    }
    togli(dove);
    manda_foglio(d, risposta, sizeof *risposta, f);
    chiudi(f);
}

/* Togliere un'app o svuotarne i dati: lo stesso permesso di installare. */
static void togli_o_svuota(int d, struct richiesta_launcher *r, struct risposta_launcher *risposta) {
    if (!puo_installare(d)) {
        risposta->esito = LAUNCHER_NON_PERMESSO;
        messaggio(risposta, "Solo lo Store pu\xc3\xb2 togliere le app o svuotarne i dati.");
        return;
    }
    char id[MAX_ID];
    descrizione_copia(id, r->applicazione, MAX_ID);
    if (lunghezza(r->applicazione) >= MAX_ID) id[0] = 0;   /* troppo lungo: non valido */
    int esito = r->tipo == RIMUOVI_PACCHETTO
                    ? pacchetto_togli(id, (r->opzioni & LAUNCHER_TIENI_DATI) != 0, risposta->messaggio, MAX_MESSAGGIO_LAUNCHER)
                    : pacchetto_svuota_dati(id, risposta->messaggio, MAX_MESSAGGIO_LAUNCHER);
    switch (esito) {
    case TOGLI_FATTO:          risposta->esito = LAUNCHER_FATTO; break;
    case TOGLI_DEL_SISTEMA:    risposta->esito = LAUNCHER_DEL_SISTEMA; break;
    case TOGLI_NON_INSTALLATA: risposta->esito = LAUNCHER_NON_INSTALLATA; break;
    case TOGLI_IN_USO:         risposta->esito = LAUNCHER_IN_USO; break;
    default:                   risposta->esito = LAUNCHER_ERRORE; break;
    }
    if (esito == TOGLI_FATTO)
        registro(r->tipo == RIMUOVI_PACCHETTO ? (r->opzioni & LAUNCHER_TIENI_DATI ? "tolta (dati tenuti) " : "tolta ")
                                              : "dati svuotati di ", id);
}

static void rispondi(int d) {
    struct richiesta_launcher r;
    int file = -1;
    int n = ricevi_foglio(d, &r, sizeof r, &file);
    struct risposta_launcher risposta = { LAUNCHER_ERRORE, -1, { 0 } };
    if (n == sizeof r) {
        r.applicazione[MAX_NOME_FILE - 1] = 0;
        r.argomento[MAX_PERCORSO - 1] = 0;
        switch (r.tipo) {
        case APRI_APPLICAZIONE: apri_app(&r, &risposta); break;
        case APRI_FILE: apri_un_file(&r, &risposta); break;
        case APRI_PROGRAMMA: apri_un_programma(&r, &risposta); break;
        case APRI_SHELL: apri_la_shell(file, &risposta); break;
        case RIMUOVI_PACCHETTO: case SVUOTA_DATI: togli_o_svuota(d, &r, &risposta); break;
        case DESCRIVI_PACCHETTO: case INSTALLA_PACCHETTO: case AGGIORNA_PACCHETTO: case ELENCO_APP:
            pacchetti(d, &r, file, &risposta);
            if (file >= 0) chiudi(file);
            return;
        default: break;
        }
    }
    if (file >= 0) chiudi(file);
    manda_subito(d, &risposta, sizeof risposta);   /* chi chiede non ci blocca mai */
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
