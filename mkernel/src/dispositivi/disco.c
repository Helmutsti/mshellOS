/* Lo sportello dei dischi (vedi disco.h). Neutro. */
#include "kernel.h"
#include "disco.h"

static const struct disco *il_disco;

void disco_iscrivi(const struct disco *d) {
    if (!il_disco) il_disco = d;
}

int disco_prepara(void) { return il_disco != 0; }
const char *disco_modello(void) { return il_disco ? il_disco->modello : "nessuno"; }
unsigned int disco_settori(void) { return il_disco ? il_disco->settori : 0; }

int disco_leggi(unsigned int n, void *dati) {
    if (!il_disco || n >= il_disco->settori) return -1;
    return il_disco->leggi(n, dati);
}

int disco_scrivi(unsigned int n, const void *dati) {
    if (!il_disco || n >= il_disco->settori) return -1;
    return il_disco->scrivi(n, dati);
}
