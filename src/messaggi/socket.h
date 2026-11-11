/* I socket: i tubi tra programmi (vedi chiamate.h). */
#ifndef SOCKET_H
#define SOCKET_H

/* Ognuna mette la cosa nuova tra i descrittori del compito attuale e restituisce il
 * descrittore, o -1. 'nome' e' gia' stato copiato nel kernel. */
int socket_ascolta(const char *nome);
int socket_collega(const char *nome);
int socket_accetta(int d);   /* dorme finche' non arriva un collegamento */

struct compito;
/* Il compito che sta dall'altro capo del collegamento 'd' (del compito attuale),
 * o 0 se 'd' non e' un collegamento o l'altro ha chiuso. */
struct compito *socket_altro(int d);

struct aperto;
/* E' un capo di un collegamento? (Quelli non si passano ad altri programmi: chi() deve
 * sapere chi lo tiene.) */
int e_un_capo(struct aperto *a);

#endif
