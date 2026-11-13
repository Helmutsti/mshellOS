/* CoreKit: le richieste al launcher, dalla parte di chi chiede (sistema/launcher.h, dove
 * c'e' il contratto: le richieste, le chiavi, le risposte, i permessi).
 *
 * Ogni funzione e' UNA richiesta: si fa il messaggio (messaggi.h), launcher_chiedi si
 * collega al nome "launcher", lo manda con chiedi_entro, aspetta la risposta e chiude il
 * collegamento (il launcher serve una richiesta per collegamento). Poi la risposta si
 * traduce in un numero, LAUNCHER_..., cosi' chi chiama non vede i messaggi: result=ok e'
 * LAUNCHER_FATTO, denied LAUNCHER_NON_PERMESSO, error con un reason il LAUNCHER_ di quel
 * reason, tutto il resto (invalid, timeout, error senza reason, il launcher che non c'e')
 * LAUNCHER_ERRORE.
 *
 * I dati grandi (il resoconto di un pacchetto, l'elenco delle app) arrivano in un foglio
 * allegato alla risposta, con la struct cosi' com'e': si copiano dove vuole chi chiama. */
#include <string.h>

#include "../messaggi.h"
#include "../sistema/launcher.h"
#include "interno.h"

/* Il LAUNCHER_... di una risposta. */
static int esito_di(const struct messaggio *r) {
    static const struct { const char *motivo; int esito; } motivi[] = {
        { MOTIVO_NON_INSTALLATA, LAUNCHER_NON_INSTALLATA },
        { MOTIVO_SERVE_STORE, LAUNCHER_SERVE_STORE },
        { MOTIVO_NON_SUPPORTATO, LAUNCHER_NON_SUPPORTATO },
        { MOTIVO_PACCHETTO, LAUNCHER_PACCHETTO_NON_VALIDO },
        { MOTIVO_IN_USO, LAUNCHER_IN_USO },
        { MOTIVO_DEL_SISTEMA, LAUNCHER_DEL_SISTEMA },
    };
    if (messaggio_vale(r, CHIAVE_ESITO, ESITO_OK)) return LAUNCHER_FATTO;
    if (messaggio_vale(r, CHIAVE_ESITO, ESITO_NEGATO)) return LAUNCHER_NON_PERMESSO;
    if (messaggio_vale(r, CHIAVE_ESITO, ESITO_ERRORE))
        for (unsigned int i = 0; i < sizeof motivi / sizeof motivi[0]; i++)
            if (messaggio_vale(r, CHIAVE_MOTIVO, motivi[i].motivo)) return motivi[i].esito;
    return LAUNCHER_ERRORE;
}

int launcher_chiedi(struct messaggio *richiesta, struct messaggio *risposta, int ms) {
    int d = collega(NOME_LAUNCHER);
    int e = d >= 0 ? chiedi_entro(d, richiesta, risposta, ms) : -1;
    if (d >= 0) chiudi(d);
    if (e == -1) {
        /* nessuna risposta: il launcher non c'e', ha chiuso, o la richiesta era guasta (un
         * valore con un a capo, troppo lungo...) */
        messaggio_risposta(risposta, 0, ESITO_ERRORE);
        return LAUNCHER_ERRORE;
    }
    return esito_di(risposta);   /* anche CHIEDI_SCADUTO: result=timeout, cioe' LAUNCHER_ERRORE */
}

void launcher_frase(const struct messaggio *risposta, char *perche, unsigned int byte) {
    if (!perche || byte == 0) return;
    const char *f = messaggio_valore(risposta, CHIAVE_FRASE);
    unsigned int n = 0;
    for (; f && n < byte - 1 && f[n]; n++) perche[n] = f[n];
    perche[n] = 0;
}

/* Il numero del compito (task) di una risposta, o -1. */
static int compito(const struct messaggio *r, int esito) {
    long n;
    if (esito != LAUNCHER_FATTO || messaggio_numero(r, CHIAVE_COMPITO, &n) < 0 || n < 0) return -1;
    return (int)n;
}

/* Copia il foglio allegato alla risposta in 'dove' (al massimo 'byte'), e lo chiude. */
static void leggi_foglio(struct messaggio *r, void *dove, unsigned int byte) {
    if (r->allegato < 0) return;
    unsigned int grande = 0;
    unsigned char *p = mappa(r->allegato, &grande);
    if (p) {
        memcpy(dove, p, byte < grande ? byte : grande);
        togli(p);
    }
    chiudi(r->allegato);
    r->allegato = -1;
}

int apri_programma(const char *percorso) {
    struct messaggio q, r;
    messaggio_inizia(&q);
    messaggio_aggiungi(&q, CHIAVE_RICHIESTA, APRI_PROGRAMMA);
    messaggio_aggiungi(&q, CHIAVE_PERCORSO, percorso ? percorso : "");
    int esito = launcher_chiedi(&q, &r, TEMPO_CHIEDI);
    return compito(&r, esito);
}

int apri_shell(int schiavo) {
    struct messaggio q, r;
    messaggio_inizia(&q);
    messaggio_aggiungi(&q, CHIAVE_RICHIESTA, APRI_SHELL);
    q.allegato = schiavo;     /* resta di chi chiama: il launcher ne riceve una copia */
    int esito = launcher_chiedi(&q, &r, TEMPO_CHIEDI);
    return compito(&r, esito);
}

int launcher_pacchetto(const char *richiesta, const char *percorso, struct resoconto *res) {
    int f = apri(percorso, APRI_LEGGERE);
    if (f < 0) return LAUNCHER_ERRORE;
    struct messaggio q, r;
    messaggio_inizia(&q);
    messaggio_aggiungi(&q, CHIAVE_RICHIESTA, richiesta);
    q.allegato = f;
    int esito = launcher_chiedi(&q, &r, TEMPO_LAUNCHER_PACCHETTI);
    chiudi(f);
    leggi_foglio(&r, res, sizeof *res);
    return esito;
}

/* remove_app e clear_app_data: la stessa forma. */
static int togli_o_svuota(const char *richiesta, const char *id, int tieni_dati, char *perche, unsigned int byte) {
    struct messaggio q, r;
    messaggio_inizia(&q);
    messaggio_aggiungi(&q, CHIAVE_RICHIESTA, richiesta);
    messaggio_aggiungi(&q, CHIAVE_APP, id ? id : "");
    if (tieni_dati) messaggio_aggiungi_numero(&q, CHIAVE_TIENI_DATI, 1);
    int esito = launcher_chiedi(&q, &r, TEMPO_LAUNCHER_PACCHETTI);
    if (r.allegato >= 0) chiudi(r.allegato);   /* non ne aspetta nessuno */
    launcher_frase(&r, perche, byte);
    return esito;
}

int togli_app(const char *id, int tieni_dati, char *perche, unsigned int byte) {
    return togli_o_svuota(RIMUOVI_PACCHETTO, id, tieni_dati, perche, byte);
}

int svuota_dati_app(const char *id, char *perche, unsigned int byte) {
    return togli_o_svuota(SVUOTA_DATI, id, 0, perche, byte);
}

int elenco_app_installate(struct elenco_app *e) {
    struct messaggio q, r;
    e->quante = 0;
    messaggio_inizia(&q);
    messaggio_aggiungi(&q, CHIAVE_RICHIESTA, ELENCO_APP);
    int esito = launcher_chiedi(&q, &r, TEMPO_CHIEDI);
    leggi_foglio(&r, e, sizeof *e);
    return esito;
}
