/* Le cose che i pezzi della libreria (libreria.c, file.c, comunicazione.c, schermo.c)
 * si dicono tra loro. I programmi non lo includono: per loro c'e' libreria.h. */
#ifndef INTERNO_H
#define INTERNO_H

/* IL REGISTRO: il descrittore su cui scrive scrivi. E' la console del kernel (la
 * seriale): lo apre avvio, il primo programma, e passa a init, che lo lascia a tutti i
 * programmi che fa partire, e cosi' via. Sta sempre al posto 8, appena dopo quelli che
 * si prestano con avvia_con (da 0 a MAX_PASSATI - 1). */
#define REGISTRO 8

/* Un percorso dei programmi come lo vuole Linux: controlla che sia assoluto e non troppo
 * lungo, e traduce /home nella casa di chi chiede (/Utenti/<uid>). 0, o ERRORE_NOME. */
#define MAX_VERO (MAX_PERCORSO + 16)
int percorso_vero(const char *percorso, char *vero);

/* Da un errore di Linux (errno) a un ERRORE_... */
int errore_file(int numero);

/* ricevi su un descrittore che non e' un socket: una cartella, un file, un processo
 * (file.c e libreria.c). */
int ricevi_non_socket(int d, void *dove, unsigned int byte);

/* Se d e' una cartella aperta con apri, la dimentica (chiudi). */
void dimentica_cartella(int d);

#endif
