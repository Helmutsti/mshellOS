/* Il primo avvio di un programma, su x86-64.
 *
 * Un compito nuovo non e' mai stato interrotto: sulla sua pila del kernel si preparano a
 * mano i registri "come se" lo fosse stato mentre girava in ring 3 all'inizio del
 * programma. Al primo turno, isr_comune li ripesca e con iretq si scende in ring 3. */
#include "kernel.h"
#include "arch.h"

#define RFLAGS_INTERRUZIONI_ACCESE 0x202   /* bit IF acceso (+ il bit 1, sempre a 1) */

/* A 64 bit la CPU mette sempre, oltre a RFLAGS, CS, RIP, anche la pila di chi e' stato
 * interrotto (RSP e SS). iretq li riprende tutti e cinque; e' lui a vedere che il ring
 * di CS (3) e' diverso da quello attuale (0), e a scendere in ring 3. Tutti indirizzi
 * VIRTUALI della mappa del programma.
 *
 *   SS  = dati dei programmi (riga 4 della GDT, ring 3)
 *   RSP = cima della pila del programma, meno 8: li' c'e' l'indirizzo di ritorno 0 (la
 *         pagina della pila e' a zero). E' come se _start fosse stata chiamata con call:
 *         il C conta di trovare la pila cosi' (multipla di 16, meno 8)
 *   RFLAGS, CS = codice dei programmi (riga 3, ring 3), RIP = inizio
 *   errore, numero, i 15 registri a 0          <- qui punta l'rsp che si restituisce */
indirizzo_t programma_prepara_pila(indirizzo_t cima_kernel, indirizzo_t inizio,
                                   indirizzo_t pila_programma) {
    struct registri *r = (struct registri *)(cima_kernel - sizeof(struct registri));
    memset(r, 0, sizeof *r);
    r->rip = inizio;
    r->cs = SEL_UTENTE_CODICE;
    r->rflags = RFLAGS_INTERRUZIONI_ACCESE;   /* il timer deve poterlo fermare */
    r->rsp = pila_programma - 8;
    r->ss = SEL_UTENTE_DATI;
    return (indirizzo_t)r;
}
