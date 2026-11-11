/* Il magazziniere delle pagine fisiche.
 *
 * La RAM e' divisa in pagine da 4 KB, numerate da 0. Il registro ha un bit per pagina:
 *   0 = libera, 1 = occupata.
 * Il numero di una pagina e il suo indirizzo si ricavano l'uno dall'altro:
 *   indirizzo = numero * 4096        numero = indirizzo / 4096
 *
 * Il registro copre i primi 64 GB di indirizzi: 16M pagine = 16M bit = 2 MB. E' una
 * globale a zero, quindi sta nel .bss: lo spazio lo riserva il linker e kernel_end lo
 * comprende gia'. La RAM oltre i 64 GB non si usa (sui PC di oggi la RAM sopra i 4 GB e'
 * quasi tutta; una CPU a 64 bit la vede, e la mappa diretta del kernel la copre fino a 64 GB).
 * Gli indirizzi del registro sono FISICI: per usare una pagina, virt_da_fisico (pagine.h).
 * DA IMPLEMENTARE: un registro grande quanto la RAM che c'e', messo in RAM libera
 * all'avvio invece che nella .bss. */
#include "kernel.h"
#include "pagine.h"

#define LIMITE     0x1000000000ull           /* 64 GB: oltre, la RAM non si vede */
#define MAX_PAGINE (LIMITE / PAGINA)          /* 16M pagine */
#define BIT_GRUPPO 32                       /* un gruppo = un unsigned int = 32 caselle */
#define TUTTO_PIENO 0xFFFFFFFF

extern char kernel_start[], kernel_end[];   /* messi dal linker script */

static unsigned int registro[MAX_PAGINE / BIT_GRUPPO];
static unsigned int n_pagine;   /* pagine fino alla fine della RAM: oltre non si cerca */
static unsigned int ram_pagine;  /* pagine di RAM vera: quelle delle zone, kernel compreso, buchi esclusi */
static unsigned int libere;     /* quante caselle sono a 0 */
static unsigned int ripresa;    /* gruppo da cui ripartire a cercare */

/* --- le singole caselle --- */

static int e_occupata(unsigned int p) {
    return registro[p / BIT_GRUPPO] & (1u << (p % BIT_GRUPPO));
}

static void segna_occupata(unsigned int p) {
    if (!e_occupata(p)) {
        registro[p / BIT_GRUPPO] |= 1u << (p % BIT_GRUPPO);
        libere--;
    }
}

static void segna_libera(unsigned int p) {
    if (e_occupata(p)) {
        registro[p / BIT_GRUPPO] &= ~(1u << (p % BIT_GRUPPO));
        libere++;
    }
}

/* Occupa tutte le pagine toccate dalla zona [inizio, fine), anche solo in parte. */
void pagine_proteggi(indirizzo_t inizio, indirizzo_t fine) {
    for (unsigned int p = (unsigned int)(inizio / PAGINA); p < (fine + PAGINA - 1) / PAGINA; p++)
        segna_occupata(p);
}

/* --- 1. Preparazione --- */

void pagine_prepara(const struct zona_ram *zone, int quante) {
    /* Mossa 1: tutto occupato. Le caselle partono a 0 (sono nel .bss), le mettiamo a 1. */
    for (unsigned int g = 0; g < MAX_PAGINE / BIT_GRUPPO; g++)
        registro[g] = TUTTO_PIENO;
    libere = 0;
    n_pagine = 0;

    /* Mossa 2: libera le zone di RAM che ci danno. Fra queste c'e' anche il kernel:
     * per un attimo risulta libero, e la mossa 3 lo rioccupa. */
    for (int z = 0; z < quante; z++) {
        unsigned long long inizio_z = zone[z].inizio;
        unsigned long long fine_z   = zone[z].fine;
        if (inizio_z >= LIMITE) continue;                   /* oltre 64 GB: non la vediamo */
        if (fine_z   >  LIMITE) fine_z = LIMITE;

        /* solo pagine TUTTE dentro la zona: l'inizio si arrotonda in su, la fine in giu' */
        unsigned int prima  = (unsigned int)((inizio_z + PAGINA - 1) / PAGINA);
        unsigned int ultima = (unsigned int)(fine_z / PAGINA);   /* esclusa */
        for (unsigned int p = prima; p < ultima; p++)
            segna_libera(p);
        if (ultima > n_pagine) n_pagine = ultima;
    }
    /* La RAM vera e' quella appena liberata, kernel compreso: il totale e' la RAM della
     * macchina e non cambia se il kernel cresce. Non e' n_pagine: le pagine si contano da
     * 0, e prima della RAM possono esserci buchi (su ARM "virt" la RAM parte da 1 GB). */
    ram_pagine = libere;

    /* Mossa 3: rioccupa cio' che e' gia' nostro: il kernel, registro compreso. (Quello
     * del BIOS e del bootloader lo protegge chi ci ha avviato: pagine_proteggi.) Il
     * registro parla di pagine FISICHE: del kernel servono i suoi indirizzi fisici, non
     * quelli in alto dove lo vede la CPU. */
    pagine_proteggi(fisico_da_virt(kernel_start), fisico_da_virt(kernel_end));

    ripresa = 0;
}

/* --- 2. Chiedere una pagina --- */

/* Le funzioni per chiedere e restituire sono SEZIONI CRITICHE, come in kmalloc: le
 * chiamano sia i compiti (es. avvia, che crea un programma) sia lo scheduler, dentro
 * un'interruzione (quando libera la pila di un programma finito). Se un tick arrivasse
 * a meta' di una ricerca, la stessa pagina potrebbe finire a due. */

static indirizzo_t pagina_chiedi_dentro(void) {
    unsigned int gruppi = (n_pagine + BIT_GRUPPO - 1) / BIT_GRUPPO;

    for (unsigned int g = ripresa; g < gruppi; g++) {
        if (registro[g] == TUTTO_PIENO) continue;          /* 32 caselle piene: salta */

        /* la prima casella a 0 nel gruppo: e' il primo bit a 1 di ~registro[g] */
        unsigned int p = g * BIT_GRUPPO + __builtin_ctz(~registro[g]);
        if (p >= n_pagine) break;                           /* oltre la fine della RAM */

        segna_occupata(p);
        ripresa = g;
        return (indirizzo_t)p * PAGINA;
    }
    return 0;   /* memoria finita (la pagina 0 non viene mai consegnata) */
}

indirizzo_t pagina_chiedi(void) {
    unsigned int s = interruzioni_spegni();
    indirizzo_t pagina = pagina_chiedi_dentro();
    interruzioni_ripristina(s);
    return pagina;
}

/* Cerca n caselle libere di fila: scorre il registro contando quante libere ha visto
 * una dopo l'altra; una occupata azzera il conto. Arrivato a n, le occupa tutte. */
static indirizzo_t pagine_chiedi_consecutive_dentro(unsigned int n) {
    unsigned int di_fila = 0;
    for (unsigned int p = 0; p < n_pagine; p++) {
        if (e_occupata(p)) { di_fila = 0; continue; }
        if (++di_fila == n) {
            unsigned int prima = p - n + 1;
            for (unsigned int q = prima; q <= p; q++)
                segna_occupata(q);
            return (indirizzo_t)prima * PAGINA;
        }
    }
    return 0;
}

indirizzo_t pagine_chiedi_consecutive(unsigned int n) {
    unsigned int s = interruzioni_spegni();
    indirizzo_t prima = pagine_chiedi_consecutive_dentro(n);
    interruzioni_ripristina(s);
    return prima;
}

/* --- 3. Restituire una pagina --- */

void pagina_restituisci(indirizzo_t indirizzo) {
    unsigned int p = (unsigned int)(indirizzo / PAGINA);   /* il numero della pagina */

    if (indirizzo % PAGINA)
        panico("pagina_restituisci: %x non e' l'inizio di una pagina", indirizzo);
    if (p >= n_pagine)
        panico("pagina_restituisci: %x e' oltre la fine della RAM", indirizzo);
    if (indirizzo < 0x100000 ||
        (indirizzo >= fisico_da_virt(kernel_start) && indirizzo < fisico_da_virt(kernel_end)))
        panico("pagina_restituisci: %x e' una pagina protetta", indirizzo);
    if (!e_occupata(p))
        panico("pagina_restituisci: %x restituita due volte", indirizzo);

    unsigned int s = interruzioni_spegni();
    segna_libera(p);
    if (p / BIT_GRUPPO < ripresa)
        ripresa = p / BIT_GRUPPO;   /* c'e' un posto libero prima: la prossima ricerca parte da qui */
    interruzioni_ripristina(s);
}

unsigned int pagine_libere(void) { return libere; }
indirizzo_t memoria_fine(void) { return (indirizzo_t)n_pagine * PAGINA; }
unsigned int pagine_ram(void) { return ram_pagine; }
