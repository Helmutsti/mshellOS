/* La memoria dei blocchi letti di recente (la "cache dei blocchi", il buffer cache di
 * Unix).
 *
 * Leggere il disco e' lento: ogni settore chiede un viaggio fino al dispositivo. Ma un
 * formato come ext4 rilegge spesso gli stessi pezzi (la descrizione del disco, le
 * tabelle dei nodi, le cartelle). Allora si tengono in RAM gli ultimi blocchi letti, e
 * la seconda volta si prendono da li'.
 *
 * E' TRASPARENTE: la cache e' a sua volta un dispositivo_blocchi, messo SOPRA il disco
 * vero. Il formato la usa come un disco qualsiasi e non sa che c'e'. (Come i
 * "decoratori": stessa forma, qualcosa in piu' dentro.)
 *
 *     ext4  ->  cache (blocchi da 4 KB in RAM)  ->  disco (virtio-blk, ATA)
 *
 * Si tengono blocchi da 4 KB (8 settori), le misure di una pagina e di un blocco ext4.
 * Quando la cache e' piena, si butta il blocco usato meno di recente (LRU: least
 * recently used). Le scritture vanno subito anche sul disco ("write-through"): la cache
 * non tiene mai dati che il disco non ha, e se si spegne non si perde niente. */
#ifndef CACHE_H
#define CACHE_H

#include "vfs.h"

#define CACHE_BLOCCO 4096         /* byte di un blocco della cache */
#define CACHE_BLOCCHI 256         /* quanti blocchi tiene il disco del sistema: 1 MB */

/* Un dispositivo nuovo che legge e scrive 'sotto' tenendo fino a 'blocchi' blocchi in
 * RAM (le pagine si prendono man mano che servono). Se non si puo' (settori che non
 * stanno in un blocco, o niente memoria) restituisce 'sotto' stesso: si va senza cache. */
struct dispositivo_blocchi *cache_crea(struct dispositivo_blocchi *sotto, unsigned int blocchi);

#endif
