/* mfs: il file system di mkernel. File e cartelle sopra i settori del disco. */
#ifndef MFS_H
#define MFS_H

/* Tutte le funzioni restituiscono 0 (o un numero di byte) se va bene,
 * oppure uno di questi errori, sempre negativi. */
#define MFS_ERR_DISCO          -1   /* il disco ha dato errore */
#define MFS_ERR_NON_FORMATTATO -2   /* sul disco non c'e' mfs */
#define MFS_ERR_NOME           -3   /* percorso scritto male, o nome troppo lungo */
#define MFS_ERR_NON_TROVATO    -4   /* non esiste */
#define MFS_ERR_ESISTE         -5   /* esiste gia' */
#define MFS_ERR_NON_CARTELLA   -6   /* serviva una cartella */
#define MFS_ERR_E_CARTELLA     -7   /* serviva un file */
#define MFS_ERR_NON_VUOTA      -8   /* la cartella non e' vuota */
#define MFS_ERR_PIENO          -9   /* niente settori liberi */
#define MFS_ERR_SCHEDE        -10   /* niente schede libere */
#define MFS_ERR_TROPPO_GRANDE -11   /* oltre i 140 settori di un file */

#define MFS_FILE     1
#define MFS_CARTELLA 2
#define MFS_NOME_MAX 27             /* lettere in un nome */

/* Legge il superblocco: 1 se il disco ha mfs, 0 se no. */
int mfs_monta(void);

/* Scrive un mfs vuoto sul disco. CANCELLA TUTTO. */
int mfs_formatta(void);

/* Crea una cartella vuota. */
int mfs_mkdir(const char *percorso);

/* Crea un file con dentro 'byte' byte presi da 'dati'. */
int mfs_crea(const char *percorso, const void *dati, unsigned int byte);

/* Legge fino a 'n' byte del file, a partire dal byte 'da'. Restituisce quanti ne ha letti. */
int mfs_leggi(const char *percorso, unsigned int da, void *dove, unsigned int n);

/* Chiama 'f' per ogni voce della cartella. */
typedef void (*mfs_visita)(const char *nome, int tipo, unsigned int dimensione);
int mfs_elenca(const char *percorso, mfs_visita f);

/* Cancella un file o una cartella vuota. */
int mfs_rm(const char *percorso);

/* Quante schede e quanti settori di dati sono ancora liberi. */
int mfs_spazio(unsigned int *schede_libere, unsigned int *settori_liberi);

/* Il messaggio per un codice di errore. */
const char *mfs_messaggio(int errore);

#endif
