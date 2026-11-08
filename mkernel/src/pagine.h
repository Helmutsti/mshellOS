/* Il magazziniere: tiene il registro delle pagine fisiche (blocchi da 4 KB). */
#ifndef PAGINE_H
#define PAGINE_H

#include "multiboot.h"

#define PAGINA 4096   /* dimensione di una pagina in byte */

/* 1. Preparazione: legge la mappa del bootloader e protegge cio' che e' gia' usato. */
void pagine_prepara(struct multiboot_info *info);

/* 2. Chiede una pagina libera. Restituisce il suo indirizzo, o 0 se la memoria e' finita. */
unsigned int pagina_chiedi(void);

/* Chiede n pagine una dopo l'altra (memoria consecutiva). Indirizzo della prima, o 0. */
unsigned int pagine_chiedi_consecutive(unsigned int n);

/* 3. Restituisce una pagina avuta da pagina_chiedi. */
void pagina_restituisci(unsigned int indirizzo);

unsigned int pagine_libere(void);   /* quante pagine si possono ancora chiedere */
unsigned int memoria_fine(void);    /* l'indirizzo dopo l'ultima pagina di RAM */

#endif
