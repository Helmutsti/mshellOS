/* LO SPORTELLO DELLA CPU (ARM64).
 *
 * Gli stessi nomi di arch/x86_64/arch.h: il kernel neutro chiede qui, e non sa se sotto c'e'
 * x86 o ARM. Questo e' l'adattatore per ARM64 (DAIF, x0-x8, EL0, svc, wfi).
 * Sono tutte funzioni "static inline": non costano niente (vedi la versione x86). */
#ifndef ARCH_H
#define ARCH_H

/* UN INDIRIZZO, o quello che sta in un registro: su ARM64, 64 bit. */
typedef unsigned long indirizzo_t;

#include "eccezioni.h"   /* struct registri: i registri salvati di chi e' stato interrotto */

/* --- le interruzioni ---
 * DAIF sono quattro "rubinetti": D debug (bit 9), A SError (8), I IRQ (7), F FIQ (6).
 * Spegnere le interruzioni vuol dire chiudere I. */

static inline unsigned int interruzioni_spegni(void) {
    unsigned long daif;
    __asm__ volatile("mrs %0, daif; msr daifset, #2" : "=r"(daif) : : "memory");
    return (unsigned int)daif;
}

static inline void interruzioni_ripristina(unsigned int daif) {
    if (!(daif & (1u << 7)))              /* il rubinetto I era aperto? */
        __asm__ volatile("msr daifclr, #2" : : : "memory");
}

static inline void interruzioni_accendi(void) { __asm__ volatile("msr daifclr, #2" : : : "memory"); }

/* --- fermarsi --- */

static inline void cpu_riposa(void) { __asm__ volatile("wfi"); }   /* l'hlt di ARM */

static inline __attribute__((noreturn)) void cpu_ferma(void) {
    for (;;) __asm__ volatile("msr daifset, #0xF; wfi");
}

/* --- le chiamate di sistema ---
 * Un programma chiede con "svc #0": il numero del servizio in x8, gli argomenti in x0-x4;
 * il risultato torna in x0 (come Linux su ARM64). */

static inline unsigned int chiamata_numero(const struct registri *r) { return (unsigned int)r->x[8]; }

static inline indirizzo_t chiamata_argomento(const struct registri *r, int n) { return r->x[n - 1]; }

static inline void chiamata_risultato(struct registri *r, indirizzo_t valore) { r->x[0] = valore; }

/* --- i compiti --- */

/* Il numero della svc con cui il kernel stesso cede il turno (cedi_turno). Arriva come
 * un'eccezione sincrona "dal kernel": eccezioni.c la riconosce e chiama lo scheduler. */
#define SVC_CEDI 0x81

static inline void cedi_turno(void) { __asm__ volatile("svc #0x81" : : : "memory"); }

/* Chi era stato interrotto stava girando un programma? I bit 3-0 di SPSR (il livello di
 * prima): 0 vuol dire EL0. */
static inline int era_un_programma(const struct registri *r) { return (r->spsr & 0xF) == 0; }

/* Dove la CPU salva i registri se arriva un'interruzione mentre gira un programma: su
 * ARM e' la pila del kernel SP_EL1, e quando si torna al programma (eret) SP_EL1 e'
 * proprio la cima della pila del kernel di quel compito (i suoi registri stavano li' in
 * cima, e sono appena stati tolti). Non c'e' niente da scrivere: niente TSS. */
static inline void pila_kernel_imposta(indirizzo_t cima) { (void)cima; }

/* La cima della pila dell'avvio: e' quella del compito inattivo (kernel_head.s). */
static inline indirizzo_t pila_avvio_cima(void) {
    extern char pila_cima[];
    return (indirizzo_t)pila_cima;
}

/* I registri "come se" fosse stato interrotto un programma all'inizio. In contesto.c. */
indirizzo_t programma_prepara_pila(indirizzo_t cima_kernel, indirizzo_t inizio,
                                   indirizzo_t pila_programma);

/* --- l'avvio --- */

/* Il centralino (GIC) e il timer: alla fine le interruzioni sono accese. In avvio.c. */
void arch_dispositivi_prepara(void);

#endif
