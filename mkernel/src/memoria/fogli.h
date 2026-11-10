/* I fogli: memoria condivisa tra programmi (vedi chiamate.h). */
#ifndef FOGLI_H
#define FOGLI_H

#include "compiti.h"

struct aperto;

/* Un foglio nuovo di 'byte' byte, pieno di zeri, tra i descrittori del compito
 * attuale. Il descrittore, o -1. */
int foglio_nuovo(unsigned int byte);

/* Mette il foglio del descrittore 'd' nella memoria del compito attuale, nel primo
 * posto libero della zona dei fogli. L'indirizzo, o 0. In *byte quanto e' grande. */
unsigned int foglio_mappa(int d, unsigned int *byte);

/* Leva il foglio messo a 'indirizzo'. 0, o -1 se li' non c'e' un foglio. */
int foglio_togli(unsigned int indirizzo);

/* Lascia i fogli che erano nella memoria di 'c' (un compito finito). */
void fogli_lascia_tutti(struct compito *c);

int e_un_foglio(struct aperto *a);

#endif
