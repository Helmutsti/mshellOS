/* I file e le cartelle aperti dai programmi (vedi chiamate.h). */
#ifndef FILE_H
#define FILE_H

/* Apre il file o la cartella 'percorso' (gia' copiato nel kernel) con 'modo'
 * (APRI_...) tra i descrittori del compito attuale. Il descrittore, o -1. */
int file_apri(const char *percorso, unsigned int modo);

/* Sposta la posizione di lettura e scrittura di un file aperto. La posizione nuova, o -1. */
int file_posiziona(int d, unsigned int posizione);

#endif
