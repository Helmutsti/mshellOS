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
 * tabella sotto: "tutto questo GB (o questi 2 MB) va li'". Come le pagine da 2 MB di x86.
 *
 * DUE RADICI. ARM ha due registri per la radice dell'albero, uno per meta':
 *   TTBR0_EL1  la META' BASSA (indirizzi che cominciano con 0x0000): i programmi. Cambia
 *              a ogni cambio di programma (paginazione.c);
 *   TTBR1_EL1  la META' ALTA (indirizzi che cominciano con 0xFFFF): il kernel. Non cambia
 *              mai: e' la stessa per tutti, senza bisogno di copiarla in ogni mappa.
 * La pianta della meta' alta e' quella di pagine.h, uguale al PC:
 *   voce 256 di L0 (MAPPA_DIRETTA)     tutta la RAM, a blocchi da 1 GB o 2 MB
 *   voce 384 (ZONA_DISPOSITIVI)        i dispositivi, aggiunti da pag_mappa_dispositivo
 *   voce 511 (KERNEL_IMMAGINE)         il kernel, a pagine da 4 KB
 *
 * UNA VOCE (8 byte), i bit che usiamo:
 *   bit 1-0   11 = tabella (L0-L2) o pagina (L3);  01 = blocco (L1, L2);  00 = niente
 *   bit 4-2   il TIPO DI MEMORIA: un numero che rimanda alla tabella MAIR_EL1 (sotto)
 *   bit 7-6   AP, i permessi: 00 kernel lettura e scrittura, 10 kernel solo lettura
 *             (01 e 11 sono come questi, ma anche per i programmi: paginazione.c)
 *   bit 9-8   SH: condivisa tra le CPU (11)
 *   bit 10    AF, "e' stata usata": se e' 0 la CPU da' errore al primo uso. Lo mettiamo a 1.
 *   bit 47-12 l'indirizzo FISICO (della tabella sotto, o del blocco, o della pagina)
 *   bit 53    PXN: il kernel non puo' eseguire qui
 *   bit 54    UXN: i programmi non possono eseguire qui
 * Nelle voci di TABELLA della meta' alta (L0) ci sono anche due bit che valgono per tutto
 * quello che sta sotto: APTable = 01 (bit 61: i programmi non possono usare niente qui
 * sotto, qualunque cosa dicano le voci piu' in basso) e UXNTable (bit 60: e non possono
 * eseguirci). E' la CPU stessa a tenere i programmi fuori dalla meta' del kernel.
 *
 * IL TIPO DI MEMORIA (la novita' rispetto a x86). Con la MMU spenta ARM tratta TUTTO come
 * un dispositivo: niente cache, niente letture "storte", ogni accesso arriva davvero.
 * Con la MMU accesa ogni voce dice cos'e':
 *   0: DISPOSITIVO (Device-nGnRnE): seriale, GIC... ogni accesso deve arrivare, subito,
 *      nell'ordine scritto, e una sola volta;
 *   1: MEMORIA NORMALE con la cache (Write-Back): la RAM. Molto piu' veloce.
 *
 * DUE TEMPI.
 *   1. mmu_avvio, chiamata da kernel_head.s con la MMU SPENTA, mentre il kernel gira
 *      ancora al suo indirizzo fisico: fa la voce del kernel in alto (codice si esegue e
 *      non si scrive, costanti solo lettura, il resto si scrive e non si esegue) e una
 *      mappa PROVVISORIA della meta' bassa uguale a quella fisica (virtuale = fisico, i
 *      primi 512 GB a blocchi da 1 GB): il GB dove sta il kernel e' memoria normale di
 *      sola lettura (ci si esegue l'istruzione dopo l'accensione), gli altri dispositivi
 *      (la seriale, e il device tree se sta altrove). Poi kernel_head.s accende la MMU e
 *      salta in alto.
 *      ATTENZIONE: mmu_avvio gira PRIMA del salto in alto: puo' usare solo indirizzi
 *      calcolati dalla posizione dell'istruzione (il compilatore li fa cosi', con adrp).
 *      Niente variabili inizializzate con un puntatore, niente tabelle di puntatori: li'
 *      ci sarebbero gli indirizzi in alto, che con la MMU spenta non esistono. Il
 *      vantaggio: adesso &qualcosa e' proprio l'indirizzo fisico da scrivere nelle voci.
 *   2. mmu_prepara, in alto, dopo aver letto il device tree: la mappa diretta della RAM e
 *      la voce della zona dei dispositivi. La mappa provvisoria la toglie poi avvio.c
 *      (pag_usa_mappa(0)), quando la seriale e il device tree hanno il loro posto in alto.
 *
 * DA IMPLEMENTARE: su una macchina vera le tabelle scritte con la MMU (e la cache) spenta
 * vanno pulite dalla cache prima di accenderla, e i blocchi che cambiano forma (un
 * blocco che diventa tabella) vanno tolti e rimessi ("break-before-make"); in QEMU
 * emulato non serve. Linux lo fa in arch/arm64/kernel/head.S. */
#include "kernel.h"
#include "pagine.h"
#include "mmu.h"
#include "albero.h"

#define VOCI 512
#define INDICE_L0(v) (((v) >> 39) & 0x1FF)
#define INDICE_L1(v) (((v) >> 30) & 0x1FF)

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
#define SOLO_KERNEL      (1UL << 61 | 1UL << 60)   /* APTable = 01 e UXNTable, per le voci di L0 */

#define MB2 0x200000UL
#define GB1 0x40000000UL

/* Le tabelle: pagine da 4 KB, allineate a 4 KB, nella .bss (a zero = voci vuote). */
#define ALLINEATA __attribute__((aligned(4096)))
#define MAX_L3_KERNEL 4    /* il kernel: fino a 8 MB (linker.ld lo controlla) */
#define MAX_L2 16          /* la mappa diretta: un L2 per ogni GB che ha RAM solo in parte */
#define MAX_L3_CODICE 2    /* la mappa diretta: i 2 MB dove sta il codice del kernel */
static unsigned long l0_alto[VOCI] ALLINEATA;              /* TTBR1: la meta' alta */
static unsigned long l1_kernel[VOCI] ALLINEATA;
static unsigned long l2_kernel[VOCI] ALLINEATA;
static unsigned long l3_kernel[MAX_L3_KERNEL][VOCI] ALLINEATA;
static unsigned long l0_avvio[VOCI] ALLINEATA;             /* TTBR0 provvisoria */
static unsigned long l1_avvio[VOCI] ALLINEATA;
static unsigned long l1_diretta[VOCI] ALLINEATA;
static unsigned long scorta_l2[MAX_L2][VOCI] ALLINEATA;
static unsigned long l3_codice[MAX_L3_CODICE][VOCI] ALLINEATA;
static unsigned long l1_dispositivi[VOCI] ALLINEATA;
static int l2_usate, l3_usate;

extern char kernel_start[], codice_fine[], costanti_fine[], kernel_end[];   /* da linker.ld */

/* --- 1. con la MMU spenta --- */

void mmu_avvio(void) {
    /* Gli indirizzi del kernel, calcolati dal PC: con la MMU spenta sono quelli fisici. */
    unsigned long fisico = (unsigned long)kernel_start;
    unsigned long codice = (unsigned long)codice_fine - fisico;      /* byte di codice */
    unsigned long costanti = (unsigned long)costanti_fine - fisico;  /* ... e costanti */
    unsigned long byte = (unsigned long)kernel_end - fisico;
    kernel_fisico = fisico;

    /* Il kernel in alto: L0 voce 511, L1 voce 510, poi un L3 ogni 2 MB. */
    l0_alto[INDICE_L0(KERNEL_IMMAGINE)] = (unsigned long)l1_kernel | TABELLA | SOLO_KERNEL;
    l1_kernel[INDICE_L1(KERNEL_IMMAGINE)] = (unsigned long)l2_kernel | TABELLA;
    for (unsigned long i = 0; i * 4096 < byte; i++) {
        unsigned long *l3 = l3_kernel[i / VOCI];
        if (i % VOCI == 0) l2_kernel[i / VOCI] = (unsigned long)l3 | TABELLA;
        unsigned long dentro = i * 4096, permessi;
        if (dentro < codice)
            permessi = SOLO_LETTURA | NO_ESEGUI_PROG;   /* codice: il kernel lo esegue */
        else if (dentro < costanti)
            permessi = SOLO_LETTURA | NON_ESEGUIRE;     /* costanti */
        else
            permessi = NON_ESEGUIRE;                    /* dati, .bss, la pila */
        l3[i % VOCI] = (fisico + dentro) | PAGINA4K | TIPO_NORMALE | CONDIVISA | USATA | permessi;
    }

    /* La mappa provvisoria della meta' bassa: virtuale = fisico, 1 GB per voce di L1. */
    l0_avvio[0] = (unsigned long)l1_avvio | TABELLA;
    for (unsigned long g = 0; g < VOCI; g++)
        l1_avvio[g] = g * GB1 | BLOCCO | USATA |
                      (g == fisico / GB1 ? TIPO_NORMALE | CONDIVISA | SOLO_LETTURA | NO_ESEGUI_PROG
                                         : TIPO_DISPOSITIVO | NON_ESEGUIRE);

    /* MAIR_EL1: la tabella dei tipi di memoria, un byte per tipo.
     *   tipo 0 = 0x00: dispositivo (nGnRnE)
     *   tipo 1 = 0xFF: normale, con la cache (Write-Back) in lettura e in scrittura */
    unsigned long mair = 0x00UL << 0 | 0xFFUL << 8;

    /* TCR_EL1: com'e' fatta la traduzione, per le due meta'.
     *   T0SZ = 16, T1SZ = 16   tutte e due a 48 bit: si parte da L0
     *   IRGN, ORGN = 01, SH = 11 (per le due): le tabelle stanno in memoria con la cache
     *   TG0 = 00, TG1 = 10     pagine da 4 KB (i due campi hanno codici diversi!)
     *   IPS                    quanti bit hanno gli indirizzi fisici: quelli che ha questa
     *                          CPU (ID_AA64MMFR0_EL1, bit 3-0) */
    unsigned long mmfr0;
    __asm__ volatile("mrs %0, id_aa64mmfr0_el1" : "=r"(mmfr0));
    unsigned long tcr = 16UL << 0 | 1UL << 8 | 1UL << 10 | 3UL << 12 | 0UL << 14 |
                        16UL << 16 | 1UL << 24 | 1UL << 26 | 3UL << 28 | 2UL << 30 |
                        (mmfr0 & 0x7) << 32;

    __asm__ volatile(
        "msr mair_el1, %0\n"
        "msr tcr_el1, %1\n"
        "msr ttbr0_el1, %2\n"
        "msr ttbr1_el1, %3\n"
        "dsb ish\n"            /* le tabelle sono scritte davvero in memoria */
        "isb\n"
        "tlbi vmalle1\n"       /* si dimenticano le traduzioni vecchie (il TLB) */
        "dsb ish\n"
        "isb\n"
        : : "r"(mair), "r"(tcr), "r"((unsigned long)l0_avvio), "r"((unsigned long)l0_alto)
        : "memory");
    /* la MMU la accende kernel_head.s, appena tornati */
}

/* --- 2. in alto --- */

static unsigned long fis(const void *tabella) { return fisico_da_virt(tabella); }

void mmu_prepara(void) {
    /* Dove sta la RAM: dal device tree. Senza, quella della macchina "virt" di QEMU
     * (1 GB da 0x40000000). Si tiene solo la parte in blocchi interi da 2 MB. */
    unsigned long dove, quanto;
    if (!albero_memoria(&dove, &quanto)) { dove = GB1; quanto = GB1; }
    unsigned long ram_inizio = (dove + MB2 - 1) & ~(MB2 - 1);
    unsigned long ram_fine = (dove + quanto) & ~(MB2 - 1);
    /* Il codice del kernel, in fisico: anche nella mappa diretta e' di sola lettura. */
    unsigned long codice_da = kernel_fisico;
    unsigned long codice_a = kernel_fisico + ((unsigned long)costanti_fine - (unsigned long)kernel_start);

    /* La mappa diretta: un GB per voce di L1 (la RAM fino a 512 GB). Un GB tutto di RAM
     * e' un blocco; un GB con RAM solo in parte scende a blocchi da 2 MB; i 2 MB con il
     * codice del kernel scendono a pagine da 4 KB. Niente si esegue da qui. */
    for (unsigned long g = 0; g < VOCI; g++) {
        unsigned long da = g * GB1, a = da + GB1;
        if (ram_fine <= da || ram_inizio >= a) continue;           /* niente RAM qui */
        int codice_qui = codice_da < a && codice_a > da;
        if (ram_inizio <= da && ram_fine >= a && !codice_qui) {
            l1_diretta[g] = da | BLOCCO | TIPO_NORMALE | CONDIVISA | USATA | NON_ESEGUIRE;
            continue;
        }
        if (l2_usate == MAX_L2) panico("mmu: la RAM e' in troppi pezzi");
        unsigned long *l2 = scorta_l2[l2_usate++];
        for (unsigned long i = 0; i < VOCI; i++) {
            unsigned long p = da + i * MB2;
            if (p < ram_inizio || p + MB2 > ram_fine) continue;    /* non e' RAM: assente */
            if (p < codice_a && p + MB2 > codice_da) {
                if (l3_usate == MAX_L3_CODICE) panico("mmu: il codice del kernel e' troppo grande");
                unsigned long *l3 = l3_codice[l3_usate++];
                for (unsigned long k = 0; k < VOCI; k++) {
                    unsigned long q = p + k * 4096;
                    l3[k] = q | PAGINA4K | TIPO_NORMALE | CONDIVISA | USATA | NON_ESEGUIRE |
                            (q >= codice_da && q < codice_a ? SOLO_LETTURA : 0);
                }
                l2[i] = fis(l3) | TABELLA;
            } else {
                l2[i] = p | BLOCCO | TIPO_NORMALE | CONDIVISA | USATA | NON_ESEGUIRE;
            }
        }
        l1_diretta[g] = fis(l2) | TABELLA;
    }

    /* Le due voci nuove della meta' alta. Erano vuote: non c'e' niente da far
     * dimenticare alla CPU, basta che le scritture siano arrivate. */
    l0_alto[INDICE_L0(MAPPA_DIRETTA)] = fis(l1_diretta) | TABELLA | SOLO_KERNEL;
    l0_alto[INDICE_L0(ZONA_DISPOSITIVI)] = fis(l1_dispositivi) | TABELLA | SOLO_KERNEL;
    __asm__ volatile("dsb ishst\n isb" : : : "memory");
}

unsigned long *mmu_l1_dispositivi(void) { return l1_dispositivi; }
