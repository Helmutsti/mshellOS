/* Il magazziniere: tiene il registro delle pagine fisiche (blocchi da 4 KB).
 * Qui c'e' anche la PIANTA DEGLI INDIRIZZI: dove il kernel vede la RAM. */
#ifndef PAGINE_H
#define PAGINE_H

#include "kernel.h"   /* indirizzo_t */

#define PAGINA 4096   /* dimensione di una pagina in byte */

/* LA PIANTA DEGLI INDIRIZZI VIRTUALI, uguale sul PC e su ARM64 (indirizzi da 48 bit:
 * 256 TB in tutto, divisi in due meta' da 128 TB). Come in Linux:
 *
 *   da 0x0000000000000000 a 0x00007FFFFFFFFFFF   META' BASSA: i PROGRAMMI (chiamate.h).
 *                                                Ogni mappa ha la sua; nella mappa del
 *                                                kernel e' vuota.
 *   da 0xFFFF800000000000                        META' ALTA: il KERNEL, uguale in tutte
 *                                                le mappe; i programmi non la possono
 *                                                toccare (lo impedisce la CPU).
 *     MAPPA_DIRETTA     0xFFFF800000000000       tutta la RAM, a MAPPA_DIRETTA + il suo
 *                                                indirizzo fisico (c'e' posto per 64 TB)
 *     ZONA_DISPOSITIVI  0xFFFFC00000000000       i registri dei dispositivi (MMIO), senza
 *                                                cache: pag_mappa_dispositivo (512 GB)
 *     KERNEL_IMMAGINE   0xFFFFFFFF80000000       il kernel: codice, costanti, dati, .bss
 *                                                (gli ultimi 2 GB; linker.ld lo mette qui)
 * (In mezzo agli indirizzi bassi e alti c'e' un buco che la CPU non accetta: gli indirizzi
 * da 48 bit si allungano a 64 ripetendo il bit 47.)
 *
 * FISICO E VIRTUALE. Il magazziniere da' indirizzi FISICI: quelli che si scrivono nelle
 * tabelle delle pagine e che si danno ai dispositivi (DMA: una scheda legge e scrive la RAM
 * da sola, e non sa niente delle mappe). Il kernel invece usa indirizzi VIRTUALI: per
 * leggere o scrivere una pagina avuta dal magazziniere passa dalla mappa diretta.
 *
 * DA IMPLEMENTARE:
 *   - la mappa diretta copre la RAM fino a 64 GB sul PC (MAX_GB) e fino a 512 GB su ARM
 *     (una sola tabella L1): oltre, la RAM non si vede;
 *   - togliere un pezzo dalla zona dei dispositivi (oggi si mappa e basta: va bene finche'
 *     i dispositivi si preparano una volta sola, all'avvio);
 *   - SMAP sul PC e PAN su ARM: la CPU che vieta anche al kernel di toccare la meta' bassa,
 *     tranne quando lo dice lui (nelle chiamate di sistema). Oggi un errore nel kernel
 *     potrebbe leggere o scrivere la memoria di un programma senza accorgersene. */
#define MAPPA_DIRETTA         0xFFFF800000000000ul
#define ZONA_DISPOSITIVI      0xFFFFC00000000000ul
#define ZONA_DISPOSITIVI_FINE 0xFFFFC08000000000ul
#define KERNEL_IMMAGINE       0xFFFFFFFF80000000ul

/* Dove sta, nella RAM, il primo byte del kernel (kernel_start). Lo scrive kernel_head.s
 * all'avvio: sul PC e' sempre 2 MB, su ARM dipende da dove l'ha messo chi ci avvia. */
extern indirizzo_t kernel_fisico;

/* Dove il kernel vede la pagina fisica 'fisico': nella mappa diretta. Solo per la RAM. */
static inline void *virt_da_fisico(indirizzo_t fisico) {
    return (void *)(fisico + MAPPA_DIRETTA);
}

/* L'indirizzo fisico di un indirizzo del kernel: della mappa diretta (la RAM data dal
 * magazziniere) o del kernel stesso (le sue variabili). Non vale per la zona dei
 * dispositivi (chi la usa sa gia' l'indirizzo fisico: e' lui che l'ha mappato). */
static inline indirizzo_t fisico_da_virt(const void *virt) {
    extern char kernel_start[];
    indirizzo_t v = (indirizzo_t)virt;
    if (v >= KERNEL_IMMAGINE) return v - (indirizzo_t)kernel_start + kernel_fisico;
    return v - MAPPA_DIRETTA;
}

/* Una zona di RAM, [inizio, fine) in byte, indirizzi fisici. Chi avvia il kernel (arch:
 * il bootloader, il firmware) dice quali sono: la RAM libera E quella dove sta il kernel
 * (anche lui e' RAM: conta nel totale; le sue pagine le rioccupa pagine_prepara). */
struct zona_ram {
    unsigned long long inizio, fine;
};

/* 1. Preparazione: libera le zone di RAM che gli si danno (e le conta: pagine_ram), poi
 * rioccupa il kernel. */
void pagine_prepara(const struct zona_ram *zone, int quante);

/* Occupa per sempre la zona fisica [inizio, fine): cio' che e' gia' usato da altri (il
 * BIOS, le informazioni del bootloader...). Si chiama dopo pagine_prepara. */
void pagine_proteggi(indirizzo_t inizio, indirizzo_t fine);

/* 2. Chiede una pagina libera. Restituisce il suo indirizzo FISICO (per usarla:
 * virt_da_fisico), o 0 se la memoria e' finita. */
indirizzo_t pagina_chiedi(void);

/* Chiede n pagine una dopo l'altra (memoria fisica consecutiva). L'indirizzo fisico
 * della prima, o 0. */
indirizzo_t pagine_chiedi_consecutive(unsigned int n);

/* 3. Restituisce una pagina avuta da pagina_chiedi (il suo indirizzo fisico). */
void pagina_restituisci(indirizzo_t indirizzo);

unsigned int pagine_libere(void);   /* quante pagine si possono ancora chiedere */
indirizzo_t memoria_fine(void);     /* l'indirizzo fisico dopo l'ultima pagina di RAM */
unsigned int pagine_ram(void);      /* quante pagine di RAM ci sono davvero: kernel compreso,
                                     * buchi esclusi */

#endif
