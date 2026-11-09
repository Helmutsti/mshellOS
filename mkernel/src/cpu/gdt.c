/* La GDT (Global Descriptor Table).
 *
 * La forma la decide Intel: ogni riga e' di 8 byte, con i campi spezzettati
 * in posizioni fisse (per ragioni storiche: i 286 avevano righe piu' corte e i
 * 386 hanno aggiunto i pezzi mancanti in fondo).
 *
 * Il contenuto lo scegliamo noi. Usiamo il modello "piatto": ogni segmento
 * copre tutta la memoria (da 0 a 4 GB), quindi i segmenti non dividono niente.
 * Servono solo a dire alla CPU a che livello di privilegio sta girando il codice. */
#include "kernel.h"
#include "gdt.h"

struct riga_gdt {
    unsigned short limite_basso;   /* bit 0-15 della lunghezza */
    unsigned short base_bassa;     /* bit 0-15 dell'inizio */
    unsigned char  base_media;     /* bit 16-23 dell'inizio */
    unsigned char  accesso;        /* presente, ring, tipo (vedi sotto) */
    unsigned char  limite_flag;    /* bit 16-19 della lunghezza + 4 bit di flag */
    unsigned char  base_alta;      /* bit 24-31 dell'inizio */
} __attribute__((packed));

/* Quello che l'istruzione lgdt vuole: quanto e' lunga la tabella (meno 1) e dov'e'. */
struct registro_gdt {
    unsigned short dimensione;
    unsigned int   indirizzo;
} __attribute__((packed));

/* Il byte "accesso", bit per bit:
 *   7    presente: 1 = la riga e' valida
 *   6-5  ring: 0 = kernel, 3 = programma
 *   4    1 = segmento di codice o dati (0 = segmento speciale di sistema, es. TSS)
 *   3    1 = codice (eseguibile), 0 = dati
 *   1    codice: si puo' leggere / dati: si puo' scrivere
 *   0    "usato": lo mette la CPU */
#define PRESENTE    0x80
#define RING(n)     ((n) << 5)
#define CODICE_DATI 0x10
#define ESEGUIBILE  0x08
#define LEGGI_SCRIVI 0x02

/* I 4 bit di flag (nella parte alta di limite_flag):
 *   G = 1: la lunghezza si conta in pagine da 4 KB invece che in byte
 *   D = 1: codice a 32 bit */
#define FLAG_4K_32BIT 0xC0
#define FLAG_BYTE     0x00   /* G = 0: la lunghezza e' in byte (serve per la TSS) */

/* Il "tipo" di una riga di sistema (bit 4 a zero): 9 = TSS a 32 bit, libera */
#define TIPO_TSS 0x09

/* LA TSS (Task State Segment).
 *
 * Quando arriva un'interruzione mentre gira codice in ring 3, la CPU deve passare
 * al ring 0, e non puo' salvare i registri sulla pila del programma (non e' affidabile
 * e il programma la vede). Prende allora una pila del kernel: la trova QUI, nei campi
 * esp0 (la cima) e ss0 (il suo segmento).
 *
 * La forma e' di Intel: 104 byte, con tanti campi. Quasi tutti servivano ai vecchi
 * sistemi, che facevano cambiare compito alla CPU da sola; noi i compiti li cambia lo
 * scheduler, quindi restano a zero. Ci servono solo esp0 e ss0. Lo scheduler aggiorna
 * esp0 a ogni cambio di turno: deve indicare la pila del kernel di chi sta per girare. */
struct tss {
    unsigned int precedente;
    unsigned int esp0, ss0;              /* la pila per il ring 0: gli unici che usiamo */
    unsigned int esp1, ss1, esp2, ss2;   /* ring 1 e 2: nessuno li usa */
    unsigned int cr3, eip, eflags;
    unsigned int eax, ecx, edx, ebx, esp, ebp, esi, edi;
    unsigned int es, cs, ss, ds, fs, gs;
    unsigned int ldt;
    unsigned short trappola;
    unsigned short mappa_porte;          /* dove inizia il permesso delle porte di I/O */
} __attribute__((packed));

static struct tss tss;

#define RIGHE 6
static struct riga_gdt gdt[RIGHE];
static struct registro_gdt gdtr;

static void scrivi_riga(int i, unsigned int base, unsigned int limite, unsigned char accesso,
                        unsigned char flag) {
    gdt[i].limite_basso = limite & 0xFFFF;
    gdt[i].base_bassa   = base & 0xFFFF;
    gdt[i].base_media   = (base >> 16) & 0xFF;
    gdt[i].accesso      = accesso;
    gdt[i].limite_flag  = ((limite >> 16) & 0x0F) | flag;
    gdt[i].base_alta    = (base >> 24) & 0xFF;
}

void tss_pila_kernel(unsigned int cima) { tss.esp0 = cima; }

void gdt_prepara(void) {
    /* limite 0xFFFFF pagine da 4 KB = 4 GB: tutta la memoria */
    gdt[0] = (struct riga_gdt){0};   /* riga 0: vuota, la vuole Intel */
    scrivi_riga(1, 0, 0xFFFFF, PRESENTE | RING(0) | CODICE_DATI | ESEGUIBILE | LEGGI_SCRIVI, FLAG_4K_32BIT); /* codice kernel */
    scrivi_riga(2, 0, 0xFFFFF, PRESENTE | RING(0) | CODICE_DATI | LEGGI_SCRIVI, FLAG_4K_32BIT);              /* dati kernel */
    scrivi_riga(3, 0, 0xFFFFF, PRESENTE | RING(3) | CODICE_DATI | ESEGUIBILE | LEGGI_SCRIVI, FLAG_4K_32BIT); /* codice programmi */
    scrivi_riga(4, 0, 0xFFFFF, PRESENTE | RING(3) | CODICE_DATI | LEGGI_SCRIVI, FLAG_4K_32BIT);              /* dati programmi */

    /* riga 5: non un'area di memoria per il codice, ma "a questo indirizzo c'e' la TSS,
     * lunga 104 byte". La pila del kernel e' nel segmento dei dati del kernel; esp0 lo
     * scrivera' lo scheduler. mappa_porte oltre la fine = nessuna porta di I/O per il ring 3. */
    tss.ss0 = SEL_KERNEL_DATI;
    tss.mappa_porte = sizeof tss;
    scrivi_riga(5, (unsigned int)&tss, sizeof tss - 1, PRESENTE | RING(0) | TIPO_TSS, FLAG_BYTE);

    gdtr.dimensione = sizeof gdt - 1;
    gdtr.indirizzo  = (unsigned int)gdt;

    __asm__ volatile(
        /* 1. "la GDT e' qui" */
        "lgdt %0\n"

        /* 2. i registri dei dati e della pila: si caricano con un semplice mov */
        "mov %1, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"

        /* 3. CS non si puo' scrivere con mov: si cambia solo saltando.
         *    Un salto "lungo" dice: vai all'etichetta 1, usando il segmento 0x08. */
        "ljmp %2, $1f\n"
        "1:\n"

        /* 4. "la TSS e' quella descritta nella riga 5": la CPU se lo ricorda nel
         *    suo registro TR e da li' in poi la legge da sola quando serve */
        "mov %3, %%ax\n"
        "ltr %%ax\n"
        :
        : "m"(gdtr), "i"(SEL_KERNEL_DATI), "i"(SEL_KERNEL_CODICE), "i"(SEL_TSS)
        : "eax", "memory");
}

/* --- per vedere cosa abbiamo fatto --- */

void gdt_mostra(void) {
    struct registro_gdt letto;
    unsigned short cs, ds, ss, tr;
    __asm__ volatile("sgdt %0" : "=m"(letto));   /* chiede alla CPU: dov'e' la GDT adesso? */
    __asm__ volatile("str %0" : "=r"(tr));       /* e quale riga usa come TSS? */
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    __asm__ volatile("mov %%ds, %0" : "=r"(ds));
    __asm__ volatile("mov %%ss, %0" : "=r"(ss));

    kprintf("GDT     : %s a %p, %u righe, CS=%02x DS=%02x SS=%02x TR=%02x\n",
            letto.indirizzo == (unsigned int)gdt ? "la nostra" : "NON la nostra",
            (void *)letto.indirizzo, (letto.dimensione + 1) / 8, cs, ds, ss, tr);
}
