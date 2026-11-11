/* LO SPORTELLO DELLA CPU (x86-64, il PC).
 *
 * Il kernel "neutro" (compiti, chiamate, lucchetti, avvio...) non tocca mai la CPU
 * direttamente: chiede qui, con nomi che valgono per ogni CPU. Ogni CPU ha il suo arch.h
 * con gli stessi nomi: questo e' quello del PC a 64 bit (cli/sti, rax/rdi, ring 3, int,
 * hlt). E' come l'adattatore di una presa: il frullatore e' lo stesso, cambia l'adattatore.
 *
 * NON COSTA NIENTE. Sono tutte funzioni "static inline": il compilatore non le chiama,
 * ricopia il loro corpo al posto della chiamata. chiamata_argomento(r, 1) nel kernel
 * compilato e' esattamente r->rdi. (Linux fa cosi' con tutto arch/.)
 *
 * IL PC A 64 BIT. Il PC parte dal firmware UEFI, che fa partire il nostro caricatore
 * (caricatore/), che carica il kernel e gli salta dentro gia' a 64 bit (vedi
 * comune/consegna.h). Una volta il PC partiva a 32 bit da GRUB: un kernel a 32 bit vede
 * al massimo 4 GB, e con piu' di 1 GB di RAM la zona dei programmi non ci stava piu'.
 *
 * DA IMPLEMENTARE:
 *   - le interruzioni con l'APIC (al posto del vecchio PIC 8259) e il suo timer, al posto
 *     del PIT; le tabelle ACPI (la consegna dice gia' dove sono) per sapere com'e' fatta
 *     la macchina;
 *   - piu' CPU (multicore): oggi lavora solo la prima;
 *   - syscall/sysret, la porta veloce per le chiamate di sistema (oggi: int 0x80). */
#ifndef ARCH_H
#define ARCH_H

/* UN INDIRIZZO, o quello che sta in un registro: su x86-64, 64 bit. Le dimensioni e i
 * contatori (byte, pagine, tick) restano unsigned int. */
typedef unsigned long indirizzo_t;

#include "idt.h"   /* struct registri: i registri salvati di chi e' stato interrotto */
#include "gdt.h"   /* tss_pila_kernel */

/* --- le interruzioni --- */

/* Le spegne e dice com'erano prima (da ridare a interruzioni_ripristina). E' il bit IF
 * (bit 9) del registro RFLAGS: si salva RFLAGS e si spegne con cli. */
static inline unsigned int interruzioni_spegni(void) {
    unsigned long rflags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(rflags) : : "memory");
    return (unsigned int)rflags;
}

static inline void interruzioni_ripristina(unsigned int rflags) {
    if (rflags & 0x200)                   /* il bit IF: erano accese? */
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
 * Un programma chiede con "int 0x80": il numero del servizio in rax, gli argomenti in
 * rdi, rsi, rdx, r10, r8 (gli stessi registri di Linux a 64 bit); il risultato torna in
 * rax. Li troviamo nei registri salvati. */

static inline unsigned int chiamata_numero(const struct registri *r) { return (unsigned int)r->rax; }

/* L'argomento n (1-5). Con n scritto come numero, il compilatore tiene solo il caso giusto. */
static inline indirizzo_t chiamata_argomento(const struct registri *r, int n) {
    switch (n) {
    case 1:  return r->rdi;
    case 2:  return r->rsi;
    case 3:  return r->rdx;
    case 4:  return r->r10;
    default: return r->r8;
    }
}

static inline void chiamata_risultato(struct registri *r, indirizzo_t valore) { r->rax = valore; }

/* --- i compiti --- */

/* L'interruzione software con cui un compito cede il turno (cedi_turno). */
#define INT_CEDI 0x81

/* Passa il turno: un'interruzione software, che passa da isr_comune come un tick:
 * i registri si salvano e lo scheduler sceglie un altro compito. */
static inline void cedi_turno(void) { __asm__ volatile("int %0" : : "i"(INT_CEDI) : "memory"); }

/* Chi era stato interrotto stava girando un programma (non il kernel)? Il ring del
 * segmento di codice salvato e' 3. */
static inline int era_un_programma(const struct registri *r) { return (r->cs & 3) == 3; }

/* Dove la CPU deve salvare i registri se arriva un'interruzione mentre gira un
 * programma: la cima della pila del kernel di quel compito. La si scrive nella TSS. */
static inline void pila_kernel_imposta(indirizzo_t cima) { tss_pila_kernel(cima); }

/* La cima della pila dell'avvio: e' quella del compito inattivo (kernel_head.s). */
static inline indirizzo_t pila_avvio_cima(void) {
    extern char pila_cima[];
    return (indirizzo_t)pila_cima;
}

/* Prepara, sulla pila del kernel di un compito nuovo (cima_kernel), i registri "come
 * se" fosse stato interrotto un programma all'indirizzo 'inizio', con la sua pila a
 * 'pila_programma'. Restituisce dove ripartire (l'rsp da dare allo scheduler).
 * In contesto.c. */
indirizzo_t programma_prepara_pila(indirizzo_t cima_kernel, indirizzo_t inizio,
                                   indirizzo_t pila_programma);

/* --- l'avvio --- */

/* I dispositivi della macchina, nel momento giusto dell'avvio (dopo la paginazione):
 * le interruzioni, il timer, tastiera e mouse, il bus. Alla fine le interruzioni sono
 * accese. In avvio/avvio.c. */
void arch_dispositivi_prepara(void);

#endif
