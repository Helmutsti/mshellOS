/* I dischi: l'elenco dei dischi che i driver hanno trovato.
 *
 * Un disco e' una struct dispositivo_blocchi (fs/vfs.h): una fila di settori, numerati
 * da 0 con numeri a 64 bit, che si leggono e si scrivono anche tanti alla volta.
 *
 * LO SPORTELLO DEI DISCHI. I driver che trovano un disco (ATA su x86, virtio-blk su x86
 * e ARM, domani NVMe) lo ISCRIVONO qui con disco_iscrivi, nell'ordine in cui lo
 * trovano: il primo e' il numero 0, il secondo il numero 1... Chi usa i dischi (il file
 * system virtuale, all'avvio) li chiede per numero e non sa che driver c'e' sotto.
 *
 * IL DISCO DEL SISTEMA e' il primo iscritto, e oggi il suo formato (ext4) occupa TUTTO
 * il disco, dal settore 0: niente tabella delle partizioni. Va bene per i dischi di
 * QEMU che prepara build.sh.
 * DA IMPLEMENTARE: la tabella delle partizioni GPT. Un disco vero (un PC, un Mac con
 * UTM, una chiavetta) e' diviso in partizioni, e il sistema starebbe in una di esse:
 * bisognera' leggere la GPT (settore 1) e iscrivere ogni partizione come un disco a se'
 * (un dispositivo_blocchi che aggiunge il suo primo settore a ogni richiesta), come fa
 * Linux con /dev/sda1, /dev/sda2... */
#ifndef DISCO_H
#define DISCO_H

#include "vfs.h"

#define SETTORE 512      /* i byte di un settore dei nostri driver (ATA e virtio-blk) */
#define MAX_DISCHI 8

/* Un driver ha trovato un disco: lo iscrive. Il disco resta suo (la struttura non si
 * copia: deve vivere per sempre). Il numero del disco, o -1 se l'elenco e' pieno. */
int disco_iscrivi(struct dispositivo_blocchi *d);

/* Quanti dischi sono iscritti. */
int disco_quanti(void);

/* Il disco numero 'n' (0 il primo, cioe' quello del sistema), o 0 se non c'e'. */
struct dispositivo_blocchi *disco_numero(int n);

/* Legge / scrive 'quanti' settori dal numero 'primo', controllando che stiano dentro il
 * disco. 0 se e' andata bene, -1 se no. */
int disco_leggi(struct dispositivo_blocchi *d, unsigned long long primo, unsigned int quanti,
                void *dove);
int disco_scrivi(struct dispositivo_blocchi *d, unsigned long long primo, unsigned int quanti,
                 const void *da);

#endif
