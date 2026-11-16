/* CoreKit: le funzioni per chiedere a catalog (il contratto e' in catalog.h). */
#include "corekit/corekit.h"
#include "corekit/messaggi.h"
#include "corekit/catalog.h"
#include <string.h>

void catalog_predefiniti(struct catalog_valori *v) {
    strcpy(v->tema, CATALOG_TEMA_CHIARO);
    v->sfondo[0] = 0;
}

/* Copia theme e wallpaper da un messaggio di catalog. 0, o -1 se mancano o sono sbagliati. */
static int valori_da(const struct messaggio *m, struct catalog_valori *v) {
    const char *t = messaggio_valore(m, CATALOG_CHIAVE_TEMA);
    const char *s = messaggio_valore(m, CATALOG_CHIAVE_SFONDO);
    if (!t || !s || (strcmp(t, CATALOG_TEMA_CHIARO) && strcmp(t, CATALOG_TEMA_SCURO)) ||
        strlen(s) >= CATALOG_MAX_SFONDO)
        return -1;
    strcpy(v->tema, t);
    strcpy(v->sfondo, s);
    return 0;
}

/* Una richiesta senza dati sul collegamento 'd'; la risposta in *r. 0 se ok. */
static int chiedi_semplice(int d, const char *richiesta, struct messaggio *r) {
    static struct messaggio q;
    messaggio_inizia(&q);
    messaggio_aggiungi(&q, CHIAVE_RICHIESTA, richiesta);
    return chiedi(d, &q, r) == 0 && messaggio_vale(r, CHIAVE_ESITO, ESITO_OK) ? 0 : -1;
}

int catalog_leggi(struct catalog_valori *v) {
    static struct messaggio r;
    catalog_predefiniti(v);
    int d = collega(NOME_CATALOG);
    if (d < 0) return -1;
    int fatto = chiedi_semplice(d, CATALOG_RICHIESTA_LEGGI, &r) == 0 && valori_da(&r, v) == 0;
    chiudi(d);
    if (!fatto) catalog_predefiniti(v);
    return fatto ? 0 : -1;
}

int catalog_cambia(const char *tema, const char *sfondo) {
    static struct messaggio q, r;
    int d = collega(NOME_CATALOG);
    if (d < 0) return -1;
    messaggio_inizia(&q);
    messaggio_aggiungi(&q, CHIAVE_RICHIESTA, CATALOG_RICHIESTA_CAMBIA);
    if (tema) messaggio_aggiungi(&q, CATALOG_CHIAVE_TEMA, tema);
    if (sfondo) messaggio_aggiungi(&q, CATALOG_CHIAVE_SFONDO, sfondo);
    int esito = -1;
    if (q.guasto) esito = CATALOG_NON_VALIDO;          /* un valore che non sta in un messaggio */
    else if (chiedi(d, &q, &r) == 0) {
        if (messaggio_vale(&r, CHIAVE_ESITO, ESITO_OK)) esito = 0;
        else if (messaggio_vale(&r, CHIAVE_ESITO, ESITO_NEGATO)) esito = CATALOG_NEGATO;
        else if (messaggio_vale(&r, CHIAVE_ESITO, ESITO_NON_VALIDO)) esito = CATALOG_NON_VALIDO;
        else if (messaggio_vale(&r, CHIAVE_ESITO, ESITO_ERRORE)) esito = CATALOG_ERRORE;
    }
    chiudi(d);
    return esito;
}

int catalog_osserva(struct catalog_valori *v) {
    static struct messaggio r;
    catalog_predefiniti(v);
    int d = collega(NOME_CATALOG);
    if (d < 0) return -1;
    if (chiedi_semplice(d, CATALOG_RICHIESTA_OSSERVA, &r) < 0 || valori_da(&r, v) < 0) {
        chiudi(d);
        catalog_predefiniti(v);
        return -1;
    }
    return d;
}

int catalog_avviso(int d, struct catalog_valori *v) {
    static struct messaggio m;
    int n = messaggio_ricevi(d, &m);
    if (n == 0) return 0;
    if (n < 0) return -1;
    if (m.allegato >= 0) { chiudi(m.allegato); m.allegato = -1; }
    if (!messaggio_vale(&m, CATALOG_CHIAVE_AVVISO, CATALOG_AVVISO_CAMBIATE)) return -1;
    return valori_da(&m, v) == 0 ? 1 : -1;
}
