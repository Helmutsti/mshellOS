/* La GDT (Global Descriptor Table).
 *
 * La forma la decide Intel: ogni riga e' di 8 byte, con i campi spezzettati
 * in posizioni fisse (per ragioni storiche: i 286 avevano righe piu' corte e i
 * 386 hanno aggiunto i pezzi mancanti in fondo).
 *
 * A 64 BIT I SEGMENTI NON DIVIDONO PIU' NIENTE: la CPU ignora dove cominciano e quanto
 * sono lunghi (ogni segmento e' tutta la memoria). Servono solo a dire:
 *   - a che livello di privilegio sta girando il codice (ring 0 kernel, ring 3 programma);
 *   - che il codice e' a 64 bit (il bit L delle righe di codice);
 *   - dov'e' la TSS (sotto).
 * Il firmware UEFI ci lascia la sua GDT, in memoria che per noi e' libera: si fa subito
 * la nostra. */
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
    unsigned long  indirizzo;
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
 *   G = 1 (0x80): la lunghezza si conta in pagine da 4 KB
 *   L = 1 (0x20): codice a 64 bit (per i dati non conta) */
#define FLAG_64BIT 0xA0
#define FLAG_DATI  0x80
#define FLAG_BYTE  0x00   /* la lunghezza e' in byte (serve per la TSS) */

/* Il "tipo" di una riga di sistema (bit 4 a zero): 9 = TSS a 64 bit, libera */
#define TIPO_TSS 0x09

/* LA TSS (Task State Segment).
 *
 * Quando arriva un'interruzione mentre gira codice in ring 3, la CPU deve passare
 * al ring 0, e non puo' salvare i registri sulla pila del programma (non e' affidabile
 * e il programma la vede). Prende allora una pila del kernel: la trova QUI, nel campo
 * rsp0 (la cima).
 *
 * A 64 bit la TSS e' piu' corta di quella a 32 (104 byte comunque): niente piu' copie
 * dei registri, solo le pile. rsp0-rsp2 per i ring 0-2, ist1-ist7 per pile speciali
 * di certe eccezioni (non le usiamo). Lo scheduler aggiorna rsp0 a ogni cambio di
 * turno: deve indicare la pila del kernel di chi sta per girare. */
struct tss {
    unsigned int  riservato0;
    unsigned long rsp0, rsp1, rsp2;      /* la pila per il ring 0: l'unica che usiamo */
    unsigned long riservato1;
    unsigned long ist[7];
    unsigned long riservato2;
    unsigned short riservato3;
    unsigned short mappa_porte;          /* dove inizia il permesso delle porte di I/O */
} __attribute__((packed));

static struct tss tss;

/* 7 righe: la TSS a 64 bit ne occupa due (l'indirizzo, di 8 byte, non sta in una). */
#define RIGHE 7
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

void tss_pila_kernel(unsigned long cima) { tss.rsp0 = cima; }

void gdt_prepara(void) {
    gdt[0] = (struct riga_gdt){0};   /* riga 0: vuota, la vuole Intel */
    scrivi_riga(1, 0, 0xFFFFF, PRESENTE | RING(0) | CODICE_DATI | ESEGUIBILE | LEGGI_SCRIVI, FLAG_64BIT); /* codice kernel */
    scrivi_riga(2, 0, 0xFFFFF, PRESENTE | RING(0) | CODICE_DATI | LEGGI_SCRIVI, FLAG_DATI);               /* dati kernel */
    scrivi_riga(3, 0, 0xFFFFF, PRESENTE | RING(3) | CODICE_DATI | ESEGUIBILE | LEGGI_SCRIVI, FLAG_64BIT); /* codice programmi */
    scrivi_riga(4, 0, 0xFFFFF, PRESENTE | RING(3) | CODICE_DATI | LEGGI_SCRIVI, FLAG_DATI);               /* dati programmi */

    /* righe 5 e 6: non un'area di memoria per il codice, ma "a questo indirizzo c'e' la
     * TSS, lunga 104 byte". La riga 5 ha i primi 32 bit dell'indirizzo, come una riga
     * normale; la 6 i 32 bit alti. mappa_porte oltre la fine = nessuna porta di I/O per
     * il ring 3. rsp0 lo scrivera' lo scheduler. */
    unsigned long t = (unsigned long)&tss;
    tss.mappa_porte = sizeof tss;
    scrivi_riga(5, (unsigned int)t, sizeof tss - 1, PRESENTE | RING(0) | TIPO_TSS, FLAG_BYTE);
    *(unsigned long *)&gdt[6] = t >> 32;

    gdtr.dimensione = sizeof gdt - 1;
    gdtr.indirizzo  = (unsigned long)gdt;

    __asm__ volatile(
        /* 1. "la GDT e' qui" */
        "lgdt %0\n"

        /* 2. i registri dei dati e della pila: si caricano con un semplice mov */
        "mov %1, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        "xor %%eax, %%eax\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"

        /* 3. CS non si puo' scrivere con mov: si cambia solo "tornando" da una chiamata
         *    lunga. Sulla pila: il segmento nuovo e dove riprendere (l'etichetta 1);
         *    lretq li toglie tutti e due e salta. */
        "lea 1f(%%rip), %%rax\n"
        "pushq %2\n"
        "push %%rax\n"
        "lretq\n"
        "1:\n"

        /* 4. "la TSS e' quella descritta nella riga 5": la CPU se lo ricorda nel
         *    suo registro TR e da li' in poi la legge da sola quando serve */
        "mov %3, %%ax\n"
        "ltr %%ax\n"
        :
        : "m"(gdtr), "i"(SEL_KERNEL_DATI), "i"(SEL_KERNEL_CODICE), "i"(SEL_TSS)
        : "rax", "memory");
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
            letto.indirizzo == (unsigned long)gdt ? "la nostra" : "NON la nostra",
            (void *)letto.indirizzo, (letto.dimensione + 1) / 8, cs, ds, ss, tr);
}
