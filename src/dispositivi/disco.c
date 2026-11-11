/* Lo sportello dei dischi (vedi disco.h). Neutro. */
#include "kernel.h"
#include "disco.h"

static struct dispositivo_blocchi *dischi[MAX_DISCHI];
static int quanti_dischi;

int disco_iscrivi(struct dispositivo_blocchi *d) {
    if (quanti_dischi == MAX_DISCHI) return -1;
    dischi[quanti_dischi] = d;
    return quanti_dischi++;
}

int disco_quanti(void) { return quanti_dischi; }

struct dispositivo_blocchi *disco_numero(int n) {
    return n >= 0 && n < quanti_dischi ? dischi[n] : 0;
}

/* Dentro il disco? (Attenti al giro: primo + quanti potrebbe superare i 64 bit.) */
static int dentro(struct dispositivo_blocchi *d, unsigned long long primo, unsigned int quanti) {
    return d && quanti && primo < d->settori && quanti <= d->settori - primo;
}

int disco_leggi(struct dispositivo_blocchi *d, unsigned long long primo, unsigned int quanti,
                void *dove) {
    if (!dentro(d, primo, quanti)) return -1;
    return d->leggi(d, primo, quanti, dove);
}

int disco_scrivi(struct dispositivo_blocchi *d, unsigned long long primo, unsigned int quanti,
                 const void *da) {
    if (!dentro(d, primo, quanti) || !d->scrivi) return -1;
    return d->scrivi(d, primo, quanti, da);
}
