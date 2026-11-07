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

#define RIGHE 5
static struct riga_gdt gdt[RIGHE];
static struct registro_gdt gdtr;

static void scrivi_riga(int i, unsigned int base, unsigned int limite, unsigned char accesso) {
    gdt[i].limite_basso = limite & 0xFFFF;
    gdt[i].base_bassa   = base & 0xFFFF;
    gdt[i].base_media   = (base >> 16) & 0xFF;
    gdt[i].accesso      = accesso;
    gdt[i].limite_flag  = ((limite >> 16) & 0x0F) | FLAG_4K_32BIT;
    gdt[i].base_alta    = (base >> 24) & 0xFF;
}

void gdt_prepara(void) {
    /* limite 0xFFFFF pagine da 4 KB = 4 GB: tutta la memoria */
    gdt[0] = (struct riga_gdt){0};   /* riga 0: vuota, la vuole Intel */
    scrivi_riga(1, 0, 0xFFFFF, PRESENTE | RING(0) | CODICE_DATI | ESEGUIBILE | LEGGI_SCRIVI); /* codice kernel */
    scrivi_riga(2, 0, 0xFFFFF, PRESENTE | RING(0) | CODICE_DATI | LEGGI_SCRIVI);              /* dati kernel */
    scrivi_riga(3, 0, 0xFFFFF, PRESENTE | RING(3) | CODICE_DATI | ESEGUIBILE | LEGGI_SCRIVI); /* codice programmi */
    scrivi_riga(4, 0, 0xFFFFF, PRESENTE | RING(3) | CODICE_DATI | LEGGI_SCRIVI);              /* dati programmi */

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
        :
        : "m"(gdtr), "i"(SEL_KERNEL_DATI), "i"(SEL_KERNEL_CODICE)
        : "eax", "memory");
}

/* --- per vedere cosa abbiamo fatto --- */

static const char *descrivi(unsigned char accesso) {
    if (!(accesso & PRESENTE)) return "vuota";
    int ring = (accesso >> 5) & 3;
    if (accesso & ESEGUIBILE) return ring ? "codice programmi" : "codice kernel";
    return ring ? "dati programmi" : "dati kernel";
}

void gdt_mostra(int dettagli) {
    struct registro_gdt letto;
    unsigned short cs, ds, ss;
    __asm__ volatile("sgdt %0" : "=m"(letto));   /* chiede alla CPU: dov'e' la GDT adesso? */
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    __asm__ volatile("mov %%ds, %0" : "=r"(ds));
    __asm__ volatile("mov %%ss, %0" : "=r"(ss));

    if (!dettagli) {
        kprintf("GDT     : %s a %p, %u righe, CS=%02x DS=%02x SS=%02x\n",
                letto.indirizzo == (unsigned int)gdt ? "la nostra" : "NON la nostra",
                (void *)letto.indirizzo, (letto.dimensione + 1) / 8, cs, ds, ss);
        return;
    }

    kprintf("La CPU usa la GDT a %p, %u righe  (la nostra e' a %p)\n",
            (void *)letto.indirizzo, (letto.dimensione + 1) / 8, (void *)gdt);
    for (int i = 0; i < RIGHE; i++) {
        unsigned int base   = gdt[i].base_bassa | (gdt[i].base_media << 16) | (gdt[i].base_alta << 24);
        unsigned int limite = gdt[i].limite_basso | ((gdt[i].limite_flag & 0x0F) << 16);
        unsigned int fine   = gdt[i].accesso ? base + limite * 4096 + 4095 : 0;
        kprintf("  riga %d  sel %02x  %08x-%08x  ring %d  %s\n",
                i, i * 8 | ((gdt[i].accesso >> 5) & 3), base, fine,
                (gdt[i].accesso >> 5) & 3, descrivi(gdt[i].accesso));
    }
    kprintf("Registri: CS=%02x DS=%02x SS=%02x\n", cs, ds, ss);
}
