/* CoreKit, interno: le cose che i pezzi di CoreKit (corekit.c, file.c, comunicazione.c) si
 * dicono tra loro. Nessun programma lo include, e nemmeno GKit e UIKit: per loro c'e'
 * corekit.h. */
#ifndef INTERNO_H
#define INTERNO_H

/* IL REGISTRO: il descrittore su cui scrive scrivi. E' la console del kernel (la
 * seriale): lo apre avvio, il primo programma, e passa a init, che lo lascia a tutti i
 * programmi che fa partire, e cosi' via. Sta sempre al posto 8, appena dopo quelli che
 * si prestano con avvia_con (da 0 a MAX_PASSATI - 1). */
#define REGISTRO 8

/* Un percorso dei programmi come lo vuole Linux: controlla che sia assoluto e non troppo
 * lungo, e traduce /home nella casa di chi chiede (/data/users/<uid>/home). 0, o
 * ERRORE_NOME. */
#define MAX_VERO (MAX_PERCORSO + 32)
int percorso_vero(const char *percorso, char *vero);

/* Da un errore di Linux (errno) a un ERRORE_... */
int errore_file(int numero);

/* ricevi su un descrittore che non e' un socket: una cartella, un file, un processo
 * (file.c e corekit.c). */
int ricevi_non_socket(int d, void *dove, unsigned int byte);

/* Se d e' una cartella aperta con apri, la dimentica (chiudi). */
void dimentica_cartella(int d);

/* IL LAUNCHER, dalla parte di chi chiede (launcher.c, e apertura.c che lo usa): si collega
 * al launcher, manda 'richiesta' con chiedi_entro (al massimo 'ms' millisecondi), chiude il
 * collegamento e traduce la risposta in un LAUNCHER_... (sistema/launcher.h). La risposta
 * resta in *risposta, con il suo allegato se c'e' (lo chiude chi chiama); se non e'
 * arrivata, *risposta ha result=error (o timeout) e niente allegato. */
struct messaggio;
int launcher_chiedi(struct messaggio *richiesta, struct messaggio *risposta, int ms);

/* Copia la frase per la persona (message) della risposta in 'perche' (al massimo 'byte'
 * byte, compreso lo 0; "" se non c'e'). Con 'perche' 0 o 'byte' 0 non fa niente. */
void launcher_frase(const struct messaggio *risposta, char *perche, unsigned int byte);

#endif
