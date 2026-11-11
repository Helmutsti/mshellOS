/* I file e le cartelle aperti dai programmi (vedi chiamate.h). */
#ifndef FILE_H
#define FILE_H

/* Apre il file o la cartella 'percorso' (gia' copiato nel kernel) con 'modo'
 * (APRI_...) tra i descrittori del compito attuale. Il descrittore, o -1. */
int file_apri(const char *percorso, unsigned int modo);

/* Il compito attuale puo' fare 'cosa' (PERMESSO_...) su 'percorso'? Il sistema (uid 0)
 * si'; gli altri secondo i permessi: la prima cifra se sono il proprietario, l'ultima se
 * no. 0 anche se il file non c'e'. */
int file_puo(const char *percorso, unsigned int cosa);

/* ... e nella cartella che contiene 'percorso'? (Per creare, cancellare, rinominare.) */
int file_puo_nella_cartella(const char *percorso, unsigned int cosa);

/* Sposta la posizione di lettura e scrittura di un file aperto. La posizione nuova, o -1. */
int file_posiziona(int d, unsigned int posizione);

#endif
