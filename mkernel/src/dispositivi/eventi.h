/* La fila degli eventi: tastiera e mouse ci mettono cosa e' successo, un programma
 * (il compositore) li prende uno alla volta. */
#ifndef EVENTI_H
#define EVENTI_H

#include "chiamate.h"

/* Mette un evento in fondo alla fila e sveglia chi aspetta. La chiamano i gestori
 * delle interruzioni (a interruzioni spente). Se la fila e' piena l'evento si perde. */
void eventi_metti(const struct evento *e);

/* Il primo evento della fila, in *e: se la fila e' vuota il compito dorme finche' non
 * ne arriva uno. 1 se fatto, 0 se un altro compito sta gia' aspettando. */
int eventi_aspetta(struct evento *e);

#endif
