/* Il magazziniere delle pagine fisiche.
 *
 * La RAM e' divisa in pagine da 4 KB, numerate da 0. Il registro ha un bit per pagina:
 *   0 = libera, 1 = occupata.
 * Il numero di una pagina e il suo indirizzo si ricavano l'uno dall'altro:
 *   indirizzo = numero * 4096        numero = indirizzo / 4096
 *
 * Il registro copre 4 GB (tutto quello che una CPU a 32 bit puo' indirizzare):
 * 1M pagine = 1M bit = 128 KB. E' una globale a zero, quindi sta nel .bss:
 * lo spazio lo riserva il linker e kernel_end lo comprende gia'. */
#include "kernel.h"
#include "pagine.h"

#define MAX_PAGINE (1024 * 1024)            /* 4 GB / 4 KB */
#define BIT_GRUPPO 32                       /* un gruppo = un unsigned int = 32 caselle */
#define TUTTO_PIENO 0xFFFFFFFF

extern char kernel_start[], kernel_end[];   /* messi dal linker script */

static unsigned int registro[MAX_PAGINE / BIT_GRUPPO];
static unsigned int n_pagine;   /* pagine fino alla fine della RAM: oltre non si cerca */
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
static void proteggi(unsigned int inizio, unsigned int fine) {
    for (unsigned int p = inizio / PAGINA; p < (fine + PAGINA - 1) / PAGINA; p++)
        segna_occupata(p);
}

/* --- 1. Preparazione --- */

void pagine_prepara(struct multiboot_info *info) {
    /* Mossa 1: tutto occupato. Le caselle partono a 0 (sono nel .bss), le mettiamo a 1. */
    for (unsigned int g = 0; g < MAX_PAGINE / BIT_GRUPPO; g++)
        registro[g] = TUTTO_PIENO;
    libere = 0;
    n_pagine = 0;

    /* Mossa 2: libera solo le zone che la mappa dice libere. */
    unsigned int a = info->mmap_addr, fine = info->mmap_addr + info->mmap_length;
    while (a < fine) {
        struct multiboot_mmap *r = (struct multiboot_mmap *)a;
        a += r->size + 4;
        if (r->type != MB_MMAP_LIBERA) continue;

        unsigned long long inizio_z = r->base;
        unsigned long long fine_z   = r->base + r->len;
        if (inizio_z >= 0x100000000ull) continue;          /* oltre 4 GB: non la vediamo */
        if (fine_z   >  0x100000000ull) fine_z = 0x100000000ull;

        /* solo pagine TUTTE dentro la zona: l'inizio si arrotonda in su, la fine in giu' */
        unsigned int prima  = (unsigned int)((inizio_z + PAGINA - 1) / PAGINA);
        unsigned int ultima = (unsigned int)(fine_z / PAGINA);   /* esclusa */
        for (unsigned int p = prima; p < ultima; p++)
            segna_libera(p);
        if (ultima > n_pagine) n_pagine = ultima;
    }

    /* Mossa 3: rioccupa cio' che e' gia' nostro o del BIOS. */
    proteggi(0, 0x100000);                                        /* il primo MB */
    proteggi((unsigned int)kernel_start, (unsigned int)kernel_end); /* il kernel, registro compreso */
    proteggi((unsigned int)info, (unsigned int)info + sizeof *info);       /* le info del bootloader */
    proteggi(info->mmap_addr, info->mmap_addr + info->mmap_length);        /* la mappa */

    ripresa = 0;
}

/* --- 2. Chiedere una pagina --- */

unsigned int pagina_chiedi(void) {
    unsigned int gruppi = (n_pagine + BIT_GRUPPO - 1) / BIT_GRUPPO;

    for (unsigned int g = ripresa; g < gruppi; g++) {
        if (registro[g] == TUTTO_PIENO) continue;          /* 32 caselle piene: salta */

        /* la prima casella a 0 nel gruppo: e' il primo bit a 1 di ~registro[g] */
        unsigned int p = g * BIT_GRUPPO + __builtin_ctz(~registro[g]);
        if (p >= n_pagine) break;                           /* oltre la fine della RAM */

        segna_occupata(p);
        ripresa = g;
        return p * PAGINA;
    }
    return 0;   /* memoria finita (la pagina 0 non viene mai consegnata) */
}

/* Cerca n caselle libere di fila: scorre il registro contando quante libere ha visto
 * una dopo l'altra; una occupata azzera il conto. Arrivato a n, le occupa tutte. */
unsigned int pagine_chiedi_consecutive(unsigned int n) {
    unsigned int di_fila = 0;
    for (unsigned int p = 0; p < n_pagine; p++) {
        if (e_occupata(p)) { di_fila = 0; continue; }
        if (++di_fila == n) {
            unsigned int prima = p - n + 1;
            for (unsigned int q = prima; q <= p; q++)
                segna_occupata(q);
            return prima * PAGINA;
        }
    }
    return 0;
}

/* --- 3. Restituire una pagina --- */

void pagina_restituisci(unsigned int indirizzo) {
    unsigned int p = indirizzo / PAGINA;

    if (indirizzo % PAGINA)
        panico("pagina_restituisci: %x non e' l'inizio di una pagina", indirizzo);
    if (p >= n_pagine)
        panico("pagina_restituisci: %x e' oltre la fine della RAM", indirizzo);
    if (indirizzo < 0x100000 ||
        (indirizzo >= (unsigned int)kernel_start && indirizzo < (unsigned int)kernel_end))
        panico("pagina_restituisci: %x e' una pagina protetta", indirizzo);
    if (!e_occupata(p))
        panico("pagina_restituisci: %x restituita due volte", indirizzo);

    segna_libera(p);
    if (p / BIT_GRUPPO < ripresa)
        ripresa = p / BIT_GRUPPO;   /* c'e' un posto libero prima: la prossima ricerca parte da qui */
}

unsigned int pagine_libere(void) { return libere; }
