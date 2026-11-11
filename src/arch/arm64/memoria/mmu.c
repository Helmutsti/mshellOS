/* La MMU di ARM64 (il "paginazione.c" di x86, lezione 18).
 *
 * LO STESSO MESTIERE, UN'ALTRA FORMA. Ogni indirizzo che il codice usa passa da una
 * traduzione fatta dalla CPU, che controlla anche i permessi. Su ARM64 gli indirizzi
 * virtuali sono di 48 bit (256 TB), e la traduzione e' un albero di 4 livelli di
 * tabelle, ognuna di 512 voci da 8 byte (una pagina da 4 KB):
 *
 *   indirizzo:  | L0: 9 bit | L1: 9 bit | L2: 9 bit | L3: 9 bit | dentro la pagina: 12 bit |
 *               ogni voce di L0 copre 512 GB, di L1 1 GB, di L2 2 MB, di L3 4 KB
 *
 * SCORCIATOIE: una voce di L1 o di L2 puo' essere un BLOCCO invece di puntare alla
 * tabella sotto: "tutto questo GB (o questi 2 MB) va li'". Come le pagine da 4 MB di x86.
 *
 * UNA VOCE (8 byte), i bit che usiamo:
 *   bit 1-0   11 = tabella (L0-L2) o pagina (L3);  01 = blocco (L1, L2);  00 = niente
 *   bit 4-2   il TIPO DI MEMORIA: un numero che rimanda alla tabella MAIR_EL1 (sotto)
 *   bit 7-6   AP, i permessi: 00 kernel lettura e scrittura, 10 kernel solo lettura
 *             (01 e 11 sono come questi, ma anche per i programmi: verranno)
 *   bit 9-8   SH: condivisa tra le CPU (11)
 *   bit 10    AF, "e' stata usata": se e' 0 la CPU da' errore al primo uso. Lo mettiamo a 1.
 *   bit 47-12 l'indirizzo fisico (della tabella sotto, o del blocco, o della pagina)
 *   bit 53    PXN: il kernel non puo' eseguire qui
 *   bit 54    UXN: i programmi non possono eseguire qui
 *
 * IL TIPO DI MEMORIA (la novita' rispetto a x86). Con la MMU spenta ARM tratta TUTTO come
 * un dispositivo: niente cache, niente letture "storte", ogni accesso arriva davvero.
 * Con la MMU accesa ogni voce dice cos'e':
 *   0: DISPOSITIVO (Device-nGnRnE): seriale, GIC... ogni accesso deve arrivare, subito,
 *      nell'ordine scritto, e una sola volta;
 *   1: MEMORIA NORMALE con la cache (Write-Back): la RAM. Molto piu' veloce.
 *
 * LA MAPPA DEL KERNEL, identica (virtuale = fisico), come all'inizio su x86, costruita da
 * DOVE STA LA RAM (lo dice il device tree): niente indirizzi di una macchina particolare.
 * Si copre il primo pezzo da 512 GB (la voce 0 di L0), un GB per voce di L1:
 *   - un GB senza RAM e' tutto DISPOSITIVI (un blocco da 1 GB): la seriale, il GIC, le
 *     finestre del bus PCI... Se a un indirizzo non c'e' niente, si avra' un errore solo
 *     usandolo;
 *   - un GB con della RAM scende a blocchi da 2 MB (una tabella L2): quelli dentro la RAM
 *     sono memoria normale, gli altri dispositivi;
 *   - il blocco da 2 MB dove sta il kernel scende a pagine da 4 KB (una tabella L3), per
 *     dare a ogni pezzo il suo permesso:
 *       codice     si esegue, non si scrive
 *       costanti   solo lettura, non si eseguono
 *       il resto   si scrive, non si esegue (dati, .bss, la pila)
 *   - i primi 2 MB sono sempre ASSENTI: un puntatore a 0 (NULL) fa errore, come la
 *     pagina 0 di x86. */
#include "mmu.h"
#include "albero.h"

#define VOCI 512

#define TABELLA   0x3UL
#define BLOCCO    0x1UL
#define PAGINA4K  0x3UL
#define TIPO_DISPOSITIVO (0UL << 2)
#define TIPO_NORMALE     (1UL << 2)
#define SOLO_LETTURA     (2UL << 6)
#define CONDIVISA        (3UL << 8)
#define USATA            (1UL << 10)
#define NO_ESEGUI_KERNEL (1UL << 53)
#define NO_ESEGUI_PROG   (1UL << 54)
#define NON_ESEGUIRE     (NO_ESEGUI_KERNEL | NO_ESEGUI_PROG)

#define MB2 0x200000UL
#define GB1 0x40000000UL

/* Le tabelle: pagine da 4 KB, allineate a 4 KB, nella .bss (a zero = voci vuote).
 * Le L2 si prendono da una piccola scorta: una per ogni GB che ha RAM (o il primo). */
#define MAX_L2 16
static unsigned long l0[VOCI] __attribute__((aligned(4096)));
static unsigned long l1[VOCI] __attribute__((aligned(4096)));
static unsigned long scorta_l2[MAX_L2][VOCI] __attribute__((aligned(4096)));
static unsigned long l3_kernel[VOCI] __attribute__((aligned(4096)));
static int l2_usate;

extern char kernel_start[], codice_fine[], costanti_fine[];   /* da linker.ld */

static unsigned long indirizzo(const void *p) { return (unsigned long)p; }

static unsigned long ram_inizio, ram_fine;

static int e_ram(unsigned long da, unsigned long a) { return da >= ram_inizio && a <= ram_fine; }

/* I 2 MB che cominciano a 'base' (dentro la RAM): pagina per pagina, ognuna col suo
 * permesso secondo il pezzo del kernel che contiene. */
static void prepara_l3_kernel(unsigned long base) {
    unsigned long codice = indirizzo(kernel_start), fine_codice = indirizzo(codice_fine);
    unsigned long fine_costanti = indirizzo(costanti_fine);
    for (unsigned long i = 0; i < VOCI; i++) {
        unsigned long p = base + i * 4096;
        unsigned long permessi;
        if (p >= codice && p < fine_codice)
            permessi = SOLO_LETTURA;                    /* codice: si esegue */
        else if (p >= codice && p < fine_costanti)
            permessi = SOLO_LETTURA | NON_ESEGUIRE;     /* costanti */
        else
            permessi = NON_ESEGUIRE;                    /* dati, pila, il resto */
        l3_kernel[i] = p | PAGINA4K | TIPO_NORMALE | CONDIVISA | USATA | permessi;
    }
}

/* Il GB numero g, a blocchi da 2 MB. */
static unsigned long prepara_l2(unsigned long g) {
    unsigned long *l2 = scorta_l2[l2_usate++];
    unsigned long base_kernel = indirizzo(kernel_start) & ~(MB2 - 1);
    for (unsigned long i = 0; i < VOCI; i++) {
        unsigned long p = g * GB1 + i * MB2;
        if (p == 0)
            l2[i] = 0;                                  /* i primi 2 MB: assenti */
        else if (p == base_kernel) {
            prepara_l3_kernel(p);
            l2[i] = indirizzo(l3_kernel) | TABELLA;
        } else if (e_ram(p, p + MB2))
            l2[i] = p | BLOCCO | TIPO_NORMALE | CONDIVISA | USATA | NON_ESEGUIRE;
        else
            l2[i] = p | BLOCCO | TIPO_DISPOSITIVO | USATA | NON_ESEGUIRE;
    }
    return indirizzo(l2) | TABELLA;
}

void mmu_prepara(void) {
    /* Dove sta la RAM: dal device tree. Senza, quella della macchina "virt" di QEMU
     * (1 GB da 0x40000000). Si tiene solo la parte in blocchi interi da 2 MB. */
    unsigned long dove, quanto;
    if (!albero_memoria(&dove, &quanto)) { dove = GB1; quanto = GB1; }
    ram_inizio = (dove + MB2 - 1) & ~(MB2 - 1);
    ram_fine = (dove + quanto) & ~(MB2 - 1);

    /* L0: la voce 0 copre i primi 512 GB, e punta a L1 */
    l0[0] = indirizzo(l1) | TABELLA;
    /* L1: un GB per voce */
    for (unsigned long g = 0; g < VOCI; g++) {
        unsigned long da = g * GB1, a = da + GB1;
        int ha_ram = ram_inizio < a && ram_fine > da;
        if ((g == 0 || ha_ram) && l2_usate < MAX_L2)
            l1[g] = prepara_l2(g);
        else
            l1[g] = da | BLOCCO | TIPO_DISPOSITIVO | USATA | NON_ESEGUIRE;
    }

    /* MAIR_EL1: la tabella dei tipi di memoria, un byte per tipo.
     *   tipo 0 = 0x00: dispositivo (nGnRnE)
     *   tipo 1 = 0xFF: normale, con la cache (Write-Back) in lettura e in scrittura */
    unsigned long mair = 0x00UL << 0 | 0xFFUL << 8;

    /* TCR_EL1: com'e' fatta la traduzione.
     *   T0SZ = 16    indirizzi di 64 - 16 = 48 bit: si parte da L0
     *   IRGN0, ORGN0 = 01, SH0 = 11: le tabelle stesse stanno in memoria con la cache
     *   TG0 = 00     pagine da 4 KB
     *   EPD1 = 1     la meta' "alta" degli indirizzi (TTBR1) per ora non si usa
     *   IPS          quanti bit hanno gli indirizzi fisici: quelli che ha questa CPU
     *                (ID_AA64MMFR0_EL1, bit 3-0) */
    unsigned long mmfr0;
    __asm__ volatile("mrs %0, id_aa64mmfr0_el1" : "=r"(mmfr0));
    unsigned long tcr = 16UL << 0 | 1UL << 8 | 1UL << 10 | 3UL << 12 | 0UL << 14 |
                        1UL << 23 | (mmfr0 & 0x7) << 32;

    __asm__ volatile(
        "msr mair_el1, %0\n"
        "msr tcr_el1, %1\n"
        "msr ttbr0_el1, %2\n"
        "dsb ish\n"            /* le tabelle sono scritte davvero in memoria */
        "isb\n"
        "tlbi vmalle1\n"       /* si dimenticano le traduzioni vecchie (il TLB) */
        "dsb ish\n"
        "isb\n"
        : : "r"(mair), "r"(tcr), "r"(indirizzo(l0)) : "memory");

    /* SCTLR_EL1: si accende. M (bit 0) la MMU, C (bit 2) la cache dei dati, I (bit 12)
     * quella delle istruzioni. A (bit 1) spento: con la memoria normale le letture
     * "storte" sono permesse. WXN (bit 19) acceso: cio' che si scrive non si esegue mai. */
    unsigned long sctlr;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    sctlr |= 1UL << 0 | 1UL << 2 | 1UL << 12 | 1UL << 19;
    sctlr &= ~(1UL << 1);
    __asm__ volatile("msr sctlr_el1, %0\n isb" : : "r"(sctlr) : "memory");
}

/* L'indirizzo della tabella L0 del kernel: la paginazione dei programmi la copia in ogni
 * mappa nuova (la voce 0), e la usa per la "mappa 0". */
unsigned long mmu_tabella_l0(void) { return indirizzo(l0); }
