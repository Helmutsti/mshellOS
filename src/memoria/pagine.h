/* Il magazziniere: tiene il registro delle pagine fisiche (blocchi da 4 KB). */
#ifndef PAGINE_H
#define PAGINE_H

#include "kernel.h"   /* indirizzo_t */

#define PAGINA 4096   /* dimensione di una pagina in byte */

/* Una zona di RAM libera, [inizio, fine) in byte. Chi avvia il kernel (arch: il
 * bootloader, il firmware) dice quali sono. */
struct zona_ram {
    unsigned long long inizio, fine;
};

/* 1. Preparazione: libera le zone di RAM che gli si danno, e protegge il kernel. */
void pagine_prepara(const struct zona_ram *zone, int quante);

/* Occupa per sempre la zona [inizio, fine): cio' che e' gia' usato da altri (il BIOS,
 * le informazioni del bootloader...). Si chiama dopo pagine_prepara. */
void pagine_proteggi(indirizzo_t inizio, indirizzo_t fine);

/* 2. Chiede una pagina libera. Restituisce il suo indirizzo, o 0 se la memoria e' finita. */
indirizzo_t pagina_chiedi(void);

/* Chiede n pagine una dopo l'altra (memoria consecutiva). Indirizzo della prima, o 0. */
indirizzo_t pagine_chiedi_consecutive(unsigned int n);

/* 3. Restituisce una pagina avuta da pagina_chiedi. */
void pagina_restituisci(indirizzo_t indirizzo);

unsigned int pagine_libere(void);   /* quante pagine si possono ancora chiedere */
indirizzo_t memoria_fine(void);     /* l'indirizzo dopo l'ultima pagina di RAM */

#endif
