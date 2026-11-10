/* Il disco: una fila di settori da 512 byte, numerati da 0.
 * L'interfaccia e' la stessa su ogni CPU; il driver no: su x86 e' il disco ATA
 * (arch/x86/dispositivi/disco.c). */
#ifndef DISCO_H
#define DISCO_H

#define SETTORE 512   /* byte in un settore */

/* Cerca il disco principale e ne legge le informazioni. 1 se c'e', 0 se no. */
int disco_prepara(void);

/* Informazioni sul disco trovato (valide solo se disco_prepara ha dato 1). */
const char *disco_modello(void);
unsigned int disco_settori(void);

/* Legge / scrive il settore numero 'n'. 'dati' deve avere 512 byte.
 * Restituiscono 0 se e' andata bene, -1 se c'e' stato un errore. */
int disco_leggi(unsigned int n, void *dati);
int disco_scrivi(unsigned int n, const void *dati);

#endif
