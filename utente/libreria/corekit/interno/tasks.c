/* CoreKit: le funzioni per chiedere a tasks (il contratto e' in tasks.h). Il lavoro vero (leggere
 * /proc, statvfs, camminare nelle cartelle, fermare un processo) lo fa il servizio, in
 * programmi/tasks.c: qui si fa solo la richiesta. Ogni funzione apre un collegamento (dalla porta
 * D_PORTA_TASKS se l'app ce l'ha, se no per nome), fa UNA richiesta e lo chiude. */
#include "corekit/corekit.h"
#include "corekit/messaggi.h"
#include "corekit/tasks.h"
#include <string.h>

/* Una richiesta a tasks: la risposta in *r (con l'eventuale foglio in r->allegato). 0 se il
 * servizio ha risposto ok, TASKS_NEGATO se ha detto denied, -1 per il resto (non risponde, errore,
 * richiesta non valida). */
static int chiedi_a_tasks(struct messaggio *q, struct messaggio *r) {
    messaggio_inizia(r);   /* se non risponde nessuno, *r e' vuota (non resta una risposta di prima) */
    int d = collega_servizio(NOME_TASKS);
    if (d < 0) return -1;
    int e = chiedi(d, q, r);
    chiudi(d);
    if (e != 0) { if (r->allegato >= 0) chiudi(r->allegato); r->allegato = -1; return -1; }
    if (messaggio_vale(r, CHIAVE_ESITO, ESITO_OK)) return 0;
    if (r->allegato >= 0) { chiudi(r->allegato); r->allegato = -1; }
    return messaggio_vale(r, CHIAVE_ESITO, ESITO_NEGATO) ? TASKS_NEGATO : -1;
}

int elenco_processi(struct info_processo *v, int max) {
    static struct messaggio q, r;
    messaggio_inizia(&q);
    messaggio_aggiungi(&q, CHIAVE_RICHIESTA, TASKS_RICHIESTA_PROCESSI);
    int e = chiedi_a_tasks(&q, &r);
    if (e < 0) return e;
    int n = -1;
    if (r.allegato >= 0) {
        unsigned int grande = 0;
        const struct elenco_processi *foglio_p = mappa(r.allegato, &grande);
        if (foglio_p) {
            if (grande >= sizeof *foglio_p && foglio_p->quanti >= 0 && foglio_p->quanti <= MAX_PROCESSI_TASKS) {
                n = foglio_p->quanti < max ? foglio_p->quanti : max;
                memcpy(v, foglio_p->p, (size_t)n * sizeof v[0]);
            }
            togli((void *)foglio_p);
        }
        chiudi(r.allegato);
    }
    return n;
}

int termina_processo(int numero) {
    static struct messaggio q, r;
    if (numero <= 1) return -1;
    messaggio_inizia(&q);
    messaggio_aggiungi(&q, CHIAVE_RICHIESTA, TASKS_RICHIESTA_FERMA);
    messaggio_aggiungi_numero(&q, TASKS_CHIAVE_PID, numero);
    return chiedi_a_tasks(&q, &r) == 0 ? 0 : -1;
}

int stato_memoria(struct info_memoria *m) {
    static struct messaggio q, r;
    messaggio_inizia(&q);
    messaggio_aggiungi(&q, CHIAVE_RICHIESTA, TASKS_RICHIESTA_MEMORIA);
    if (chiedi_a_tasks(&q, &r) < 0) return -1;
    long t, l, c;
    if (messaggio_numero(&r, TASKS_CHIAVE_TOTALE, &t) < 0 || messaggio_numero(&r, TASKS_CHIAVE_LIBERA, &l) < 0 ||
        messaggio_numero(&r, TASKS_CHIAVE_CONDIVISA, &c) < 0)
        return -1;
    m->totale = (unsigned long long)t;
    m->libera = (unsigned long long)l;
    m->condivisa = (unsigned long long)c;
    return 0;
}

/* Le richieste che hanno un percorso: i dati della risposta in *r. 0, o un ERRORE_... */
static int chiedi_percorso(const char *richiesta, const char *percorso, struct messaggio *r) {
    static struct messaggio q;
    messaggio_inizia(&q);
    messaggio_aggiungi(&q, CHIAVE_RICHIESTA, richiesta);
    messaggio_aggiungi(&q, TASKS_CHIAVE_PERCORSO, percorso);
    if (q.guasto) return ERRORE_NOME;
    int e = chiedi_a_tasks(&q, r);
    if (e == TASKS_NEGATO) return ERRORE_PERMESSO;
    if (e < 0) return messaggio_vale(r, CHIAVE_ESITO, ESITO_NON_VALIDO) ? ERRORE_NOME : ERRORE_NON_TROVATO;
    return 0;
}

int stato_disco(const char *percorso, struct info_disco *d) {
    static struct messaggio r;
    int e = chiedi_percorso(TASKS_RICHIESTA_DISCO, percorso, &r);
    if (e < 0) return e;
    long t, l;
    if (messaggio_numero(&r, TASKS_CHIAVE_TOTALE, &t) < 0 || messaggio_numero(&r, TASKS_CHIAVE_LIBERA, &l) < 0)
        return ERRORE_NON_TROVATO;
    d->totale = (unsigned long long)t;
    d->libero = (unsigned long long)l;
    return 0;
}

int dimensione_cartella(const char *percorso, unsigned long long *byte) {
    static struct messaggio r;
    int e = chiedi_percorso(TASKS_RICHIESTA_CARTELLA, percorso, &r);
    if (e < 0) return e;
    long n;
    if (messaggio_numero(&r, TASKS_CHIAVE_PESO, &n) < 0) return ERRORE_NON_TROVATO;
    *byte = (unsigned long long)n;
    return 0;
}
