/* Il descrittore di un processo, per sapere quando finisce (vedi chiamate.h, processo). */
#ifndef PROCESSO_H
#define PROCESSO_H

/* Un descrittore che diventa "pronto" quando il compito 'numero' finisce, tra quelli del
 * compito attuale. -1 se quel compito non c'e'. */
int processo_apri(int numero);

/* Il compito 'numero' e' finito: chi lo stava aspettando si sveglia. La chiama lo
 * scheduler quando libera un compito. */
void processo_finito(int numero);

#endif
