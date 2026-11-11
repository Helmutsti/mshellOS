/* Il disco: una fila di settori da 512 byte, numerati da 0.
 *
 * LO SPORTELLO DEI DISCHI. Chi usa il disco (mfs) chiama disco_leggi, disco_scrivi... e
 * non sa che disco c'e' sotto. I driver che trovano un disco (ATA su x86, virtio-blk su
 * x86 e ARM) si ISCRIVONO con disco_iscrivi: il primo che si iscrive diventa IL disco. */
#ifndef DISCO_H
#define DISCO_H

#define SETTORE 512   /* byte in un settore */

/* Un disco, come lo descrive il suo driver. */
struct disco {
    const char *modello;
    unsigned int settori;
    int (*leggi)(unsigned int n, void *dati);          /* 0 bene, -1 errore */
    int (*scrivi)(unsigned int n, const void *dati);
};

/* Un driver ha trovato un disco: lo iscrive. Conta solo il primo. */
void disco_iscrivi(const struct disco *d);

/* C'e' un disco? (Qualche driver si e' iscritto.) 1 se c'e', 0 se no. */
int disco_prepara(void);

/* Informazioni sul disco trovato (valide solo se disco_prepara ha dato 1). */
const char *disco_modello(void);
unsigned int disco_settori(void);

/* Legge / scrive il settore numero 'n'. 'dati' deve avere 512 byte.
 * Restituiscono 0 se e' andata bene, -1 se c'e' stato un errore. */
int disco_leggi(unsigned int n, void *dati);
int disco_scrivi(unsigned int n, const void *dati);

#endif
