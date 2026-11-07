/* La shell del kernel: una riga di comando per provare il kernel mentre gira.
 * Non e' un processo: e' codice del kernel, chiamato dal ciclo principale. */
#ifndef SHELL_H
#define SHELL_H

/* Stampa il benvenuto e il primo prompt. */
void shell_avvia(void);

/* Riceve un tasto: lo aggiunge alla riga, o esegue la riga se e' Invio. */
void shell_tasto(char c);

#endif
