/* I descrittori (vedi descrittori.h) e l'attesa su piu' cose insieme.
 *
 * TUTTO A INTERRUZIONI SPENTE. Le chiamate di sistema girano gia' cosi', e i gestori
 * delle interruzioni (la tastiera, il mouse) svegliano chi aspetta: tra "non c'e'
 * niente" e "mi metto a dormire" non deve poter arrivare niente, se no la sveglia
 * andrebbe persa e si dormirebbe con un evento gia' arrivato. */
#include "kernel.h"
#include "descrittori.h"
#include "timer.h"

/* --- dormire e svegliarsi su una cosa aperta --- */

static void aggiungi(struct aperto *a, struct attesa *n) {
    n->dopo = a->attese;
    a->attese = n;
}

static void togli(struct aperto *a, struct attesa *n) {
    for (struct attesa **p = &a->attese; *p; p = &(*p)->dopo)
        if (*p == n) { *p = n->dopo; return; }
}

void aperto_aspetta(struct aperto *a) {
    struct attesa n = { .chi = compito_attuale() };
    aggiungi(a, &n);
    compito_aspetta();   /* dorme qui; al risveglio il nodo si toglie */
    togli(a, &n);
    compito_fine_se_terminato();
}

void aperto_sveglia(struct aperto *a) {
    for (struct attesa *n = a->attese; n; n = n->dopo) compito_sveglia(n->chi);
}

/* --- gli usi --- */

void aperto_tieni(struct aperto *a) { a->usi++; }

void aperto_lascia(struct aperto *a) {
    if (--a->usi == 0) a->op->chiudi(a);
}

/* --- la tabella delle cose aperte, una per compito --- */

int descrittore_nuovo(struct aperto *a) {
    struct compito *c = compito_attuale();
    for (int d = 0; d < MAX_APERTI; d++)
        if (!c->aperti[d]) { c->aperti[d] = a; aperto_tieni(a); return d; }
    return -1;
}

struct aperto *descrittore(int d) {
    if (d < 0 || d >= MAX_APERTI) return 0;
    return compito_attuale()->aperti[d];
}

int descrittore_chiudi(int d) {
    struct aperto *a = descrittore(d);
    if (!a) return -1;
    compito_attuale()->aperti[d] = 0;
    aperto_lascia(a);
    return 0;
}

void descrittori_chiudi_tutti(struct compito *c) {
    for (int d = 0; d < MAX_APERTI; d++)
        if (c->aperti[d]) {
            struct aperto *a = c->aperti[d];
            c->aperti[d] = 0;
            aperto_lascia(a);
        }
}

/* --- aspettare su piu' cose insieme (poll) ---
 *
 * Si guarda se qualcuno e' gia' pronto. Se no, ci si mette in attesa su TUTTI, e il
 * primo che si sveglia ci sveglia. Al risveglio ci si toglie da tutti e si riguarda. */
int descrittori_aspetta(struct da_guardare *elenco, int quanti, int ms) {
    struct aperto *cose[MAX_GUARDATI];
    struct attesa nodi[MAX_GUARDATI];
    if (quanti < 1 || quanti > MAX_GUARDATI) return -1;
    for (int i = 0; i < quanti; i++)
        if (!(cose[i] = descrittore(elenco[i].descrittore))) return -1;

    /* il tick a cui smettere di aspettare (1 tick = 10 ms, arrotondato in su) */
    unsigned int scadenza = 0;
    if (ms >= 0) scadenza = timer_tick() + (ms + 9) / 10;

    for (;;) {
        int pronti = 0;
        for (int i = 0; i < quanti; i++) {
            elenco[i].pronto = cose[i]->op->pronto(cose[i]);
            pronti += elenco[i].pronto;
        }
        if (pronti || (ms >= 0 && timer_tick() >= scadenza)) return pronti;

        for (int i = 0; i < quanti; i++) {
            nodi[i].chi = compito_attuale();
            aggiungi(cose[i], &nodi[i]);
        }
        compito_aspetta_fino(scadenza ? scadenza : 0);
        for (int i = 0; i < quanti; i++) togli(cose[i], &nodi[i]);
        compito_fine_se_terminato();
    }
}
