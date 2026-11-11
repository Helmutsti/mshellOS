/* La memoria dei blocchi letti di recente (vedi cache.h). Neutra.
 *
 * I blocchi in RAM stanno in una FILA in ordine d'uso: in testa il piu' recente, in coda
 * quello che nessuno tocca da piu' tempo. Ogni volta che si usa un blocco lo si rimette
 * in testa; quando serve posto si ricicla quello in coda.
 *
 * Per trovare un blocco si guarda la fila dall'inizio: con qualche centinaio di blocchi
 * basta e avanza, e i blocchi cercati di solito sono quelli usati da poco, in testa.
 * (Linux, che ne tiene centinaia di migliaia, li trova con una tabella hash.)
 *
 * UN COMPITO ALLA VOLTA: ogni lettura e scrittura prende il lucchetto della cache. */
#include "kernel.h"
#include "kmalloc.h"
#include "pagine.h"
#include "lucchetto.h"
#include "disco.h"
#include "cache.h"

#define NESSUNO (~0ULL)               /* il numero di un posto che non contiene niente */

struct blocco_cache {
    unsigned long long numero;        /* quale blocco del disco (NESSUNO se vuoto) */
    unsigned char *dati;              /* CACHE_BLOCCO byte, una pagina */
    struct blocco_cache *prima, *dopo;   /* i vicini nella fila d'uso */
};

struct cache {
    struct dispositivo_blocchi d;     /* per primo: la cache E' un dispositivo */
    struct dispositivo_blocchi *sotto;
    unsigned int per_blocco;          /* settori in un blocco */
    unsigned int massimo, usati;      /* posti in tutto, e quanti hanno gia' la pagina */
    struct blocco_cache *posti;
    struct blocco_cache *recente, *vecchio;   /* la testa e la coda della fila */
    struct lucchetto lucchetto;
};

/* --- la fila d'uso --- */

static void togli_dalla_fila(struct cache *c, struct blocco_cache *b) {
    if (b->prima) b->prima->dopo = b->dopo; else c->recente = b->dopo;
    if (b->dopo) b->dopo->prima = b->prima; else c->vecchio = b->prima;
    b->prima = b->dopo = 0;
}

static void metti_in_testa(struct cache *c, struct blocco_cache *b) {
    b->prima = 0;
    b->dopo = c->recente;
    if (c->recente) c->recente->prima = b; else c->vecchio = b;
    c->recente = b;
}

static struct blocco_cache *cerca(struct cache *c, unsigned long long numero) {
    for (struct blocco_cache *b = c->recente; b; b = b->dopo)
        if (b->numero == numero) return b;
    return 0;
}

/* Il blocco 'numero', in testa alla fila: gia' in RAM, o letto adesso dal disco in un
 * posto nuovo (o in quello del blocco piu' vecchio). 0 se il disco da' errore. */
static struct blocco_cache *prendi(struct cache *c, unsigned long long numero) {
    struct blocco_cache *b = cerca(c, numero);
    if (b) {                                       /* c'e' gia': si rimette in testa */
        togli_dalla_fila(c, b);
        metti_in_testa(c, b);
        return b;
    }
    /* un posto: uno nuovo finche' ce ne sono (e c'e' memoria), se no il piu' vecchio */
    b = 0;
    if (c->usati < c->massimo) {
        indirizzo_t pagina = pagina_chiedi();
        if (pagina) {
            b = &c->posti[c->usati++];
            b->dati = virt_da_fisico(pagina);
        }
    }
    if (!b) {
        b = c->vecchio;
        if (!b) return 0;                          /* nessun posto e niente memoria */
        togli_dalla_fila(c, b);
    }
    /* l'ultimo blocco del disco puo' essere piu' corto: si legge quello che c'e' */
    unsigned long long primo = numero * c->per_blocco;
    unsigned long long restano = c->sotto->settori - primo;
    unsigned int settori = restano < c->per_blocco ? (unsigned int)restano : c->per_blocco;
    if (c->sotto->leggi(c->sotto, primo, settori, b->dati) < 0) {
        b->numero = NESSUNO;                       /* il posto resta vuoto, in coda */
        b->prima = c->vecchio;
        b->dopo = 0;
        if (c->vecchio) c->vecchio->dopo = b; else c->recente = b;
        c->vecchio = b;
        return 0;
    }
    b->numero = numero;
    metti_in_testa(c, b);
    return b;
}

/* --- il dispositivo --- */

static int leggi(struct dispositivo_blocchi *d, unsigned long long primo, unsigned int quanti,
                 void *dove) {
    struct cache *c = (struct cache *)d;
    unsigned char *qui = dove;
    int esito = 0;
    lucchetto_prendi(&c->lucchetto);
    while (quanti) {                               /* un blocco alla volta */
        unsigned int dentro = primo % c->per_blocco;
        unsigned int n = c->per_blocco - dentro;
        if (n > quanti) n = quanti;
        struct blocco_cache *b = prendi(c, primo / c->per_blocco);
        if (!b) { esito = -1; break; }
        memcpy(qui, b->dati + dentro * d->byte_settore, n * d->byte_settore);
        qui += n * d->byte_settore;
        primo += n;
        quanti -= n;
    }
    lucchetto_lascia(&c->lucchetto);
    return esito;
}

/* Si scrive sul disco; poi i blocchi che sono in RAM si aggiornano, perche' non dicano
 * il falso. Quelli che non ci sono non si caricano. */
static int scrivi(struct dispositivo_blocchi *d, unsigned long long primo, unsigned int quanti,
                  const void *da) {
    struct cache *c = (struct cache *)d;
    const unsigned char *qui = da;
    lucchetto_prendi(&c->lucchetto);
    int esito = c->sotto->scrivi ? c->sotto->scrivi(c->sotto, primo, quanti, da) : -1;
    while (esito == 0 && quanti) {
        unsigned int dentro = primo % c->per_blocco;
        unsigned int n = c->per_blocco - dentro;
        if (n > quanti) n = quanti;
        struct blocco_cache *b = cerca(c, primo / c->per_blocco);
        if (b) memcpy(b->dati + dentro * d->byte_settore, qui, n * d->byte_settore);
        qui += n * d->byte_settore;
        primo += n;
        quanti -= n;
    }
    lucchetto_lascia(&c->lucchetto);
    return esito;
}

struct dispositivo_blocchi *cache_crea(struct dispositivo_blocchi *sotto, unsigned int blocchi) {
    if (!sotto->byte_settore || CACHE_BLOCCO % sotto->byte_settore || !blocchi) return sotto;
    struct cache *c = kmalloc(sizeof *c);
    struct blocco_cache *posti = kmalloc(blocchi * sizeof *posti);
    if (!c || !posti) { kfree(c); kfree(posti); return sotto; }
    memset(c, 0, sizeof *c);
    memset(posti, 0, blocchi * sizeof *posti);
    c->d.nome = sotto->nome;
    c->d.byte_settore = sotto->byte_settore;
    c->d.settori = sotto->settori;
    c->d.leggi = leggi;
    c->d.scrivi = scrivi;
    c->d.privato = 0;
    c->sotto = sotto;
    c->per_blocco = CACHE_BLOCCO / sotto->byte_settore;
    c->massimo = blocchi;
    c->posti = posti;
    c->lucchetto = (struct lucchetto)LUCCHETTO("cache dei blocchi");
    return &c->d;
}
