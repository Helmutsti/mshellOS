/* I file e le cartelle aperti dai programmi (vedi chiamate.h). */
#ifndef FILE_H
#define FILE_H

/* Apre il file o la cartella 'percorso' (gia' copiato nel kernel) con 'modo'
 * (APRI_...) tra i descrittori del compito attuale. Il descrittore, o un VFS_ERR_... */
int file_apri(const char *percorso, unsigned int modo);

/* Il compito attuale puo' fare 'cosa' (PERMESSO_...) su 'percorso'? Le regole sono in
 * vfs_puo (fs/vfs_operazioni.h). 0 anche se il file non c'e'. */
int file_puo(const char *percorso, unsigned int cosa);

/* Sposta la posizione di lettura e scrittura di un file aperto (per una cartella: alla
 * voce numero 'posizione'). La posizione nuova, o un VFS_ERR_... */
long long file_posiziona(int d, unsigned long long posizione);

#endif
