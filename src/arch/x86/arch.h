/* LO SPORTELLO DELLA CPU (x86).
 *
 * Il kernel "neutro" (compiti, chiamate, lucchetti, avvio...) non tocca mai la CPU
 * direttamente: chiede qui, con nomi che valgono per ogni CPU. Ogni CPU ha il suo arch.h
 * con gli stessi nomi: questo e' quello di x86 (cli/sti, eax/ebx, ring 3, int, hlt).
 * E' come l'adattatore di una presa: il frullatore e' lo stesso, cambia l'adattatore.
 *
 * NON COSTA NIENTE. Sono tutte funzioni "static inline": il compilatore non le chiama,
 * ricopia il loro corpo al posto della chiamata. chiamata_argomento(r, 1) nel kernel
 * compilato e' esattamente r->ebx, come prima. (Linux fa cosi' con tutto arch/.) */
#ifndef ARCH_H
#define ARCH_H

/* UN INDIRIZZO, o quello che sta in un registro: tanti bit quanti ne ha la CPU. Su x86
 * a 32 bit e' un unsigned int; su ARM64 sara' di 64 bit. Le dimensioni e i contatori
 * (byte, pagine, tick) restano unsigned int: 4 GB bastano. */
typedef unsigned int indirizzo_t;

#include "idt.h"   /* struct registri: i registri salvati di chi e' stato interrotto */
#include "gdt.h"   /* tss_pila_kernel */

/* --- le interruzioni --- */

/* Le spegne e dice com'erano prima (da ridare a interruzioni_ripristina). Su x86 e'
 * il bit IF del registro EFLAGS: si salva EFLAGS e si spegne con cli. */
static inline unsigned int interruzioni_spegni(void) {
    unsigned int eflags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(eflags) : : "memory");
    return eflags;
}

static inline void interruzioni_ripristina(unsigned int eflags) {
    if (eflags & 0x200)                   /* il bit IF: erano accese? */
        __asm__ volatile("sti" : : : "memory");
}

/* Da qui la CPU ascolta le interruzioni. */
static inline void interruzioni_accendi(void) { __asm__ volatile("sti" : : : "memory"); }

/* --- fermarsi --- */

/* Ferma la CPU fino alla prossima interruzione (il compito inattivo). */
static inline void cpu_riposa(void) { __asm__ volatile("hlt"); }

/* Ferma la CPU per sempre: interruzioni spente, e se ne arrivasse una lo stesso
 * (le NMI non si possono spegnere) si torna a fermarsi. */
static inline __attribute__((noreturn)) void cpu_ferma(void) {
    for (;;) __asm__ volatile("cli; hlt");
}

/* --- le chiamate di sistema ---
 * Un programma chiede con "int 0x80": il numero del servizio in eax, gli argomenti in
 * ebx, ecx, edx, esi, edi; il risultato torna in eax. Li troviamo nei registri salvati. */

static inline unsigned int chiamata_numero(const struct registri *r) { return r->eax; }

/* L'argomento n (1-5). Con n scritto come numero, il compilatore tiene solo il caso giusto. */
static inline indirizzo_t chiamata_argomento(const struct registri *r, int n) {
    switch (n) {
    case 1:  return r->ebx;
    case 2:  return r->ecx;
    case 3:  return r->edx;
    case 4:  return r->esi;
    default: return r->edi;
    }
}

static inline void chiamata_risultato(struct registri *r, indirizzo_t valore) { r->eax = valore; }

/* --- i compiti --- */

/* L'interruzione software con cui un compito cede il turno (cedi_turno). */
#define INT_CEDI 0x81

/* Passa il turno: un'interruzione software, che passa da isr_comune come un tick:
 * i registri si salvano e lo scheduler sceglie un altro compito. */
static inline void cedi_turno(void) { __asm__ volatile("int %0" : : "i"(INT_CEDI) : "memory"); }

/* Chi era stato interrotto stava girando un programma (non il kernel)? Su x86: il
 * ring del segmento di codice salvato e' 3. */
static inline int era_un_programma(const struct registri *r) { return (r->cs & 3) == 3; }

/* Dove la CPU deve salvare i registri se arriva un'interruzione mentre gira un
 * programma: la cima della pila del kernel di quel compito. Su x86 la si scrive nella TSS. */
static inline void pila_kernel_imposta(indirizzo_t cima) { tss_pila_kernel(cima); }

/* La cima della pila dell'avvio: e' quella del compito inattivo (kernel_head.s). */
static inline indirizzo_t pila_avvio_cima(void) {
    extern char stack_top[];
    return (indirizzo_t)stack_top;
}

/* Prepara, sulla pila del kernel di un compito nuovo (cima_kernel), i registri "come
 * se" fosse stato interrotto un programma all'indirizzo 'inizio', con la sua pila a
 * 'pila_programma'. Restituisce dove ripartire (l'esp da dare allo scheduler).
 * In contesto.c. */
indirizzo_t programma_prepara_pila(indirizzo_t cima_kernel, indirizzo_t inizio,
                                   indirizzo_t pila_programma);

/* --- l'avvio --- */

/* I dispositivi della macchina, nel momento giusto dell'avvio (dopo la paginazione):
 * le interruzioni, il timer, tastiera e mouse, il bus. Alla fine le interruzioni sono
 * accese. In avvio/avvio.c. */
void arch_dispositivi_prepara(void);

#endif
