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
#define MFS_ERR_TROPPO_GRANDE -11   /* oltre i 16522 settori (circa 8 MB) di un file */

/* I PERMESSI, come in Unix, scritti in ottale: tre cifre, ognuna la somma di
 * 4 leggere, 2 scrivere, 1 eseguire (per una cartella: entrarci). La prima cifra vale
 * per il proprietario, l'ultima per tutti gli altri; quella di mezzo (il gruppo) per ora
 * non si usa. 0755: il proprietario fa tutto, gli altri leggono ed eseguono. */
#define MFS_FILE     1
#define MFS_CARTELLA 2
#define MFS_NOME_MAX 27             /* lettere in un nome */

/* Legge il superblocco: 1 se il disco ha mfs, 0 se no. */
int mfs_monta(void);

/* Scrive un mfs vuoto sul disco. CANCELLA TUTTO. */
int mfs_formatta(void);

/* Crea una cartella vuota, di 'proprietario' (un uid) con 'permessi'. */
int mfs_mkdir(const char *percorso, unsigned int proprietario, unsigned int permessi);

/* Crea un file con dentro 'byte' byte presi da 'dati', di 'proprietario' con 'permessi'. */
int mfs_crea(const char *percorso, const void *dati, unsigned int byte,
             unsigned int proprietario, unsigned int permessi);

/* Legge fino a 'n' byte del file, a partire dal byte 'da'. Restituisce quanti ne ha letti. */
int mfs_leggi(const char *percorso, unsigned int da, void *dove, unsigned int n);

/* Chiama 'f' per ogni voce della cartella. */
typedef void (*mfs_visita)(const char *nome, int tipo, unsigned int dimensione);
int mfs_elenca(const char *percorso, mfs_visita f);

/* Scrive 'n' byte a partire dal byte 'da' (non oltre la fine): sovrascrive, e allunga
 * il file se serve. Restituisce quanti ne ha scritti. */
int mfs_scrivi(const char *percorso, unsigned int da, const void *dati, unsigned int n);

/* Il file torna vuoto (0 byte). */
int mfs_svuota(const char *percorso);

/* Tipo (MFS_FILE o MFS_CARTELLA), dimensione in byte, proprietario e permessi (questi
 * due si possono non chiedere, passando 0). */
int mfs_info(const char *percorso, int *tipo, unsigned int *dimensione,
             unsigned int *proprietario, unsigned int *permessi);

/* Cambia proprietario e permessi. */
int mfs_permessi(const char *percorso, unsigned int proprietario, unsigned int permessi);

/* La voce numero k della cartella (0, 1, 2...): nome (MFS_NOME_MAX + 1 byte), tipo e
 * dimensione. 1 se c'e', 0 se le voci sono finite. */
int mfs_voce(const char *percorso, unsigned int k, char *nome, int *tipo, unsigned int *dimensione);

/* Rinomina o sposta (anche in un'altra cartella). Un file gia' presente all'arrivo si
 * sostituisce; una cartella no. */
int mfs_rinomina(const char *da, const char *a);

/* Cancella un file o una cartella vuota. */
int mfs_rm(const char *percorso);

/* Quante schede e quanti settori di dati sono ancora liberi. */
int mfs_spazio(unsigned int *schede_libere, unsigned int *settori_liberi);

/* Il messaggio per un codice di errore. */
const char *mfs_messaggio(int errore);

#endif
