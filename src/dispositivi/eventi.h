/* La fila degli eventi: tastiera e mouse ci mettono cosa e' successo, un programma
 * (il compositore) la apre e li riceve uno alla volta, come da un socket. */
#ifndef EVENTI_H
#define EVENTI_H

#include "chiamate.h"

struct aperto;

/* Mette un evento in fondo alla fila e sveglia chi aspetta. La chiamano i gestori
 * delle interruzioni (a interruzioni spente). Se la fila e' piena l'evento si perde. */
void eventi_metti(const struct evento *e);

/* La fila, come cosa aperta da mettere tra i descrittori. 0 se e' gia' aperta da un
 * altro: gli eventi vanno a uno solo. */
struct aperto *eventi_apri(void);

#endif
