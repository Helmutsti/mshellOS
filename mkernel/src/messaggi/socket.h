/* I socket: i tubi tra programmi (vedi chiamate.h). */
#ifndef SOCKET_H
#define SOCKET_H

/* Ognuna mette la cosa nuova tra i descrittori del compito attuale e restituisce il
 * descrittore, o -1. 'nome' e' gia' stato copiato nel kernel. */
int socket_ascolta(const char *nome);
int socket_collega(const char *nome);
int socket_accetta(int d);   /* dorme finche' non arriva un collegamento */

#endif
