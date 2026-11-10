/* Il GIC, il centralino delle interruzioni di ARM: il PIC di x86.
 *
 * Ne esistono due versioni, e la macchina "virt" puo' avere l'una o l'altra:
 *   - la VERSIONE 2 quando QEMU emula la CPU;
 *   - la VERSIONE 3 quando usa la virtualizzazione del Mac (hvf): e' quella dei computer
 *     ARM moderni.
 * Il kernel guarda quale ha davanti con due domande sicure:
 *   1. alla CPU: ID_AA64PFR0_EL1 (bit 27-24) vale 1 se ha i registri del GIC versione 3;
 *   2. se dice di no, al distributore: GICD_TYPER (+0x004) c'e' in tutte e due le
 *      versioni; nella 3 i bit 23-19 dicono quanti bit ha il numero di una linea, nella 2
 *      sono riservati e valgono 0. Serve con la virtualizzazione del Mac: li' il GIC 3 lo
 *      simula macOS, e la CPU risponde di no alla prima domanda.
 * (Il registro d'identita' del GIC, PIDR2, sta in posti diversi nelle due versioni:
 * leggere quello sbagliato vuol dire toccare memoria che non c'e', e la CPU si ferma.)
 *
 * LE LINEE. 0-15 sono "software" (una CPU che chiama un'altra), 16-31 sono di ogni CPU
 * (il timer generico e' la 27), 32 in poi sono i dispositivi (seriale, disco...).
 * Il numero 1023 da "chi suona?" vuol dire "nessuno": un falso allarme.
 *
 * VERSIONE 2: due pezzi di memoria (MMIO).
 *   IL DISTRIBUTORE (GICD, 0x08000000): tutte le linee, quali sono accese.
 *     +0x000 CTLR       1 = acceso
 *     +0x100 ISENABLER  un bit per linea: scrivere 1 la accende
 *     +0x400 IPRIORITYR un byte per linea: la priorita' (0 la piu' alta)
 *   L'INTERFACCIA DELLA CPU (GICC, 0x08010000):
 *     +0x000 CTLR 1 = accesa;  +0x004 PMR la soglia;
 *     +0x00C IAR  leggerlo = "chi suona?";  +0x010 EOIR scriverci il numero = "fatto"
 *
 * VERSIONE 3: tre pezzi.
 *   IL DISTRIBUTORE (0x08000000): come prima, ma solo per le linee dei dispositivi (32+).
 *     Va acceso con ARE ("instradamento per affinita'": si dice a quale CPU, per numero).
 *   IL REDISTRIBUTORE di ogni CPU (GICR, 0x080A0000 per la CPU 0): le linee 0-31, quelle
 *     della CPU. All'inizio "dorme" (WAKER): lo si sveglia. Le sue linee si configurano
 *     nella sua seconda pagina (+0x10000), con gli stessi registri del distributore.
 *   L'INTERFACCIA DELLA CPU: non e' piu' memoria, sono REGISTRI DELLA CPU (ICC_...):
 *     ICC_SRE_EL1 "usa i registri", ICC_PMR_EL1 la soglia, ICC_IGRPEN1_EL1 accesa,
 *     ICC_IAR1_EL1 "chi suona?", ICC_EOIR1_EL1 "fatto". Piu' veloce: niente memoria.
 *   I GRUPPI: ogni linea sta nel gruppo 0 (arriva come FIQ) o 1 (arriva come IRQ). Si
 *     mettono tutte nel gruppo 1 (IGROUPR). */
#include "gic.h"
#include "seriale.h"

#define GICD 0x08000000UL
#define GICC 0x08010000UL   /* versione 2 */
#define GICR 0x080A0000UL   /* versione 3: il redistributore della CPU 0 */
#define GICR_SGI (GICR + 0x10000)   /* ... la sua pagina delle linee 0-31 */

#define GICD_CTLR       0x000
#define GICD_TYPER      0x004
#define GICD_IGROUPR    0x080
#define GICD_ISENABLER  0x100
#define GICD_IPRIORITYR 0x400
#define GICD_IROUTER    0x6000
#define GICC_CTLR       0x000
#define GICC_PMR        0x004
#define GICC_IAR        0x00C
#define GICC_EOIR       0x010
#define GICR_WAKER      0x014

#define NESSUNO 1023

static gestore_linea gestori[GIC_MAX_LINEE];
static int versione;

static inline volatile unsigned int *reg32(unsigned long base, unsigned long scostamento) {
    return (volatile unsigned int *)(base + scostamento);
}

/* --- versione 3: i registri della CPU --- */
static inline void icc_scrivi_sre(unsigned long v) { __asm__ volatile("msr icc_sre_el1, %0; isb" : : "r"(v)); }
static inline unsigned long icc_leggi_sre(void) { unsigned long v; __asm__ volatile("mrs %0, icc_sre_el1" : "=r"(v)); return v; }
static inline void icc_scrivi_pmr(unsigned long v) { __asm__ volatile("msr icc_pmr_el1, %0" : : "r"(v)); }
static inline void icc_scrivi_igrpen1(unsigned long v) { __asm__ volatile("msr icc_igrpen1_el1, %0; isb" : : "r"(v)); }
static inline unsigned long icc_leggi_iar1(void) { unsigned long v; __asm__ volatile("mrs %0, icc_iar1_el1" : "=r"(v)); return v; }
static inline void icc_scrivi_eoir1(unsigned long v) { __asm__ volatile("msr icc_eoir1_el1, %0" : : "r"(v)); }

int gic_versione(void) { return versione; }

void gic_prepara(void) {
    unsigned long pfr0;
    __asm__ volatile("mrs %0, id_aa64pfr0_el1" : "=r"(pfr0));
    unsigned int bit_linee = (*reg32(GICD, GICD_TYPER) >> 19) & 0x1F;
    versione = ((pfr0 >> 24) & 0xF) || bit_linee ? 3 : 2;

    if (versione == 2) {
        *reg32(GICD, GICD_CTLR) = 1;      /* il distributore: acceso */
        *reg32(GICC, GICC_PMR) = 0xFF;    /* la soglia piu' bassa: passa tutto */
        *reg32(GICC, GICC_CTLR) = 1;      /* l'interfaccia della CPU: accesa */
        return;
    }

    /* versione 3. Il distributore: gruppo 1 acceso, con ARE (bit 4) */
    *reg32(GICD, GICD_CTLR) = (1u << 4) | (1u << 1);
    /* il redistributore della CPU 0: lo si sveglia (ProcessorSleep, bit 1, a 0) e si
     * aspetta che dica di essere sveglio (ChildrenAsleep, bit 2, a 0) */
    *reg32(GICR, GICR_WAKER) &= ~(1u << 1);
    while (*reg32(GICR, GICR_WAKER) & (1u << 2))
        ;
    /* l'interfaccia della CPU, con i registri: si usano quelli (SRE), soglia, gruppo 1 */
    icc_scrivi_sre(icc_leggi_sre() | 1);
    icc_scrivi_pmr(0xFF);
    icc_scrivi_igrpen1(1);
}

/* La priorita': un byte per linea, scritto a 4 byte per volta (la parola che lo
 * contiene): con la MMU spenta gli accessi al GIC vanno fatti a parole intere. */
static void metti_priorita(unsigned long base, unsigned int linea) {
    volatile unsigned int *p = reg32(base, GICD_IPRIORITYR + (linea & ~3u));
    unsigned int posto = (linea & 3) * 8;
    *p = (*p & ~(0xFFu << posto)) | (0x80u << posto);
}

void gic_collega(unsigned int linea, gestore_linea gestore) {
    if (linea >= GIC_MAX_LINEE) return;
    gestori[linea] = gestore;
    unsigned int bit = 1u << (linea % 32);

    if (versione == 2) {
        metti_priorita(GICD, linea);
        *reg32(GICD, GICD_ISENABLER + (linea / 32) * 4) = bit;
        return;
    }
    /* versione 3: le linee della CPU (0-31) nel suo redistributore, le altre nel
     * distributore, con la CPU a cui mandarle (la 0) */
    unsigned long base = linea < 32 ? GICR_SGI : GICD;
    *reg32(base, GICD_IGROUPR + (linea / 32) * 4) |= bit;   /* gruppo 1: arriva come IRQ */
    metti_priorita(base, linea);
    if (linea >= 32) {
        volatile unsigned int *router = reg32(GICD, GICD_IROUTER + 8UL * linea);
        router[0] = 0;   /* alla CPU con affinita' 0.0.0.0 */
        router[1] = 0;
    }
    *reg32(base, GICD_ISENABLER + (linea / 32) * 4) = bit;
}

void gic_interruzione(void) {
    unsigned int iar = versione == 2 ? *reg32(GICC, GICC_IAR) : (unsigned int)icc_leggi_iar1();
    unsigned int linea = iar & 0x3FF;
    if (linea == NESSUNO) return;                  /* falso allarme: niente da dire */
    if (linea < GIC_MAX_LINEE && gestori[linea]) gestori[linea]();
    else {
        seriale_testo("[gic] interruzione senza gestore sulla linea ");
        seriale_esadecimale(linea);
        seriale_scrivi('\n');
    }
    if (versione == 2) *reg32(GICC, GICC_EOIR) = iar;   /* fatto */
    else icc_scrivi_eoir1(iar);
}
