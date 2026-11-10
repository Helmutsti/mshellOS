/* Il primo avvio di un programma, su x86.
 *
 * Un compito nuovo non e' mai stato interrotto: sulla sua pila del kernel si preparano a
 * mano i registri "come se" lo fosse stato mentre girava in ring 3 all'inizio del
 * programma. Al primo turno, isr_comune li ripesca e con iret si scende in ring 3. */
#include "kernel.h"
#include "arch.h"

#define EFLAGS_INTERRUZIONI_ACCESE 0x202   /* bit IF acceso (+ il bit 1, sempre a 1) */

/* Quando viene interrotto un programma in ring 3, la CPU, oltre a EFLAGS, CS, EIP, mette
 * anche la pila del programma (ESP e SS). E' iret a vedere che il ring di CS (3) e'
 * diverso da quello attuale (0): allora riprende anche ESP e SS e scende in ring 3.
 * Tutti indirizzi VIRTUALI della mappa del programma.
 *
 *   SS  = dati dei programmi (riga 4 della GDT, ring 3)
 *   ESP = cima della pila del programma (sotto l'indirizzo di ritorno 0)
 *   EFLAGS, CS = codice dei programmi (riga 3, ring 3), EIP = inizio
 *   errore, numero, registri (pusha)
 *   DS  = dati dei programmi            <- qui punta l'esp che si restituisce */
indirizzo_t programma_prepara_pila(indirizzo_t cima_kernel, indirizzo_t inizio,
                                   indirizzo_t pila_programma) {
    unsigned int *cima = (unsigned int *)cima_kernel;
    cima[-1] = SEL_UTENTE_DATI;
    cima[-2] = pila_programma - 4;

    struct registri *r = (struct registri *)((unsigned char *)&cima[-2] - sizeof(struct registri));
    memset(r, 0, sizeof *r);
    r->ds = SEL_UTENTE_DATI;
    r->eip = inizio;
    r->cs = SEL_UTENTE_CODICE;
    r->eflags = EFLAGS_INTERRUZIONI_ACCESE;   /* il timer deve poterlo fermare */
    return (indirizzo_t)r;
}
