/* I fogli: memoria condivisa tra programmi.
 *
 * Ogni programma ha la sua mappa, e due mappe di solito portano a pagine fisiche
 * diverse: e' questo che li separa. Un foglio e' l'eccezione voluta: un gruppo di
 * pagine fisiche che il kernel mette nella mappa di PIU' programmi. Quello che uno ci
 * scrive, l'altro lo legge subito, senza passare dal kernel.
 *
 *     mappa dell'applicazione          pagine fisiche          mappa del compositore
 *     0x60000000  ------------------>  [ p1 p2 p3 ... ]  <----------  0x61000000
 *
 * Il kernel interviene solo all'inizio (creare, mettere nelle mappe) e alla fine.
 * Lo scambio vero, i pixel della finestra, non costa niente: non si copia nulla.
 *
 * DI CHI SONO LE PAGINE. Non di un programma: del foglio. Nelle mappe sono segnate
 * PRESTATE, cosi' quando un programma finisce la sua mappa non le restituisce al
 * magazziniere. Lo fa il foglio, quando l'ultimo uso (descrittore, messaggio o mappa)
 * lo lascia. */
#include "kernel.h"
#include "kmalloc.h"
#include "pagine.h"
#include "paginazione.h"
#include "descrittori.h"
#include "fogli.h"

struct foglio {
    struct aperto aperto;      /* per primo: un foglio E' una cosa aperta */
    unsigned int pagine;
    indirizzo_t fisiche[];     /* l'indirizzo fisico di ogni pagina */
};

static unsigned int pagine_dei_fogli;   /* quante pagine usano tutti i fogli insieme */

unsigned int fogli_pagine(void) { return pagine_dei_fogli; }

/* --- le operazioni: un foglio non si riceve e non si manda, si mappa --- */

static int ricevi(struct aperto *a, void *dove, unsigned int byte, struct aperto **allegato) {
    (void)a; (void)dove; (void)byte;
    *allegato = 0;
    return -1;
}

static int manda(struct aperto *a, const void *da, unsigned int byte, struct aperto *allegato,
                 int subito) {
    (void)a; (void)da; (void)byte; (void)allegato; (void)subito;
    return -1;
}

static int pronto(struct aperto *a) { (void)a; return 0; }

static void chiudi(struct aperto *a) {   /* nessuno lo tiene piu': le pagine tornano libere */
    struct foglio *f = (struct foglio *)a;
    for (unsigned int i = 0; i < f->pagine; i++) pagina_restituisci(f->fisiche[i]);
    pagine_dei_fogli -= f->pagine;
    kfree(f);
}

static const struct operazioni operazioni = { ricevi, manda, pronto, chiudi };

int e_un_foglio(struct aperto *a) { return a->op == &operazioni; }

/* --- le chiamate --- */

int foglio_nuovo(unsigned int byte) {
    if (byte == 0 || byte > FOGLIO_MAX) return -1;
    unsigned int pagine = (byte + PAGINA - 1) / PAGINA;
    struct foglio *f = kmalloc(sizeof *f + pagine * sizeof f->fisiche[0]);
    if (!f) return -1;
    memset(f, 0, sizeof *f);
    f->aperto.op = &operazioni;
    for (; f->pagine < pagine; f->pagine++) {
        indirizzo_t p = pagina_chiedi();
        if (!p) { chiudi(&f->aperto); return -1; }   /* memoria finita: si rende quel che si e' preso */
        memset((void *)p, 0, PAGINA);   /* mai dare a un programma i resti di un altro */
        f->fisiche[f->pagine] = p;
        pagine_dei_fogli++;
    }
    int d = descrittore_nuovo(&f->aperto);
    if (d < 0) chiudi(&f->aperto);
    return d;
}

static indirizzo_t indirizzo_del_posto(int i) { return FOGLI_INIZIO + (indirizzo_t)i * FOGLIO_MAX; }

indirizzo_t foglio_mappa(int d, unsigned int *byte) {
    struct aperto *a = descrittore(d);
    if (!a || !e_un_foglio(a)) return 0;
    struct foglio *f = (struct foglio *)a;
    struct compito *c = compito_attuale();

    int i = 0;
    while (i < MAX_MAPPATI && c->mappati[i]) i++;
    if (i == MAX_MAPPATI) return 0;   /* la zona dei fogli e' piena */

    indirizzo_t inizio = indirizzo_del_posto(i);
    for (unsigned int p = 0; p < f->pagine; p++)
        if (!pag_mappa_programma(c->mappa, inizio + p * PAGINA, f->fisiche[p],
                                 PAG_UTENTE | PAG_SCRITTURA | PAG_PRESTATA)) {
            while (p--) pag_togli_programma(c->mappa, inizio + p * PAGINA);
            return 0;                 /* memoria finita per le tabelle */
        }
    c->mappati[i] = a;
    *byte = f->pagine * PAGINA;
    aperto_tieni(a);                  /* la mappa e' un uso: il foglio resta anche se si chiude d */
    return inizio;
}

/* Leva il posto i dalla mappa di c e lascia il foglio. */
static void togli_posto(struct compito *c, int i) {
    struct foglio *f = (struct foglio *)c->mappati[i];
    indirizzo_t inizio = indirizzo_del_posto(i);
    for (unsigned int p = 0; p < f->pagine; p++) pag_togli_programma(c->mappa, inizio + p * PAGINA);
    c->mappati[i] = 0;
    aperto_lascia(&f->aperto);
}

int foglio_togli(indirizzo_t indirizzo) {
    struct compito *c = compito_attuale();
    if (indirizzo < FOGLI_INIZIO || (indirizzo - FOGLI_INIZIO) % FOGLIO_MAX) return -1;
    unsigned int i = (unsigned int)((indirizzo - FOGLI_INIZIO) / FOGLIO_MAX);
    if (i >= MAX_MAPPATI || !c->mappati[i]) return -1;
    togli_posto(c, i);
    return 0;
}

/* Un compito finito: la sua mappa sta per essere cancellata (le pagine PRESTATE non le
 * restituisce), quindi basta lasciare i fogli. */
void fogli_lascia_tutti(struct compito *c) {
    for (int i = 0; i < MAX_MAPPATI; i++)
        if (c->mappati[i]) {
            aperto_lascia(c->mappati[i]);
            c->mappati[i] = 0;
        }
}
