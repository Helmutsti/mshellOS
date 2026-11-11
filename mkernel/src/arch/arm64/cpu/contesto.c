/* Il primo avvio di un programma, su ARM64.
 *
 * Un compito nuovo non e' mai stato interrotto: in cima alla sua pila del kernel si
 * preparano a mano i registri "come se" lo fosse stato mentre girava a EL0, all'inizio
 * del programma. Al primo turno, vettori.s li ripesca e con eret scende a EL0:
 *   ELR    = l'inizio del programma (da dove riprendere)
 *   SPSR   = 0: livello EL0 con la pila SP_EL0 (EL0t), e tutti i rubinetti DAIF aperti
 *            (il timer deve poterlo fermare)
 *   SP_EL0 = la cima della pila del programma
 *   x0-x30 = 0 (x30, dove "tornare", a 0: se il programma tornasse da _start invece di
 *            chiamare esci, salterebbe a 0 e il kernel lo chiuderebbe) */
#include "kernel.h"
#include "arch.h"

indirizzo_t programma_prepara_pila(indirizzo_t cima_kernel, indirizzo_t inizio,
                                   indirizzo_t pila_programma) {
    struct registri *r = (struct registri *)(cima_kernel - sizeof(struct registri));
    memset(r, 0, sizeof *r);
    r->elr = inizio;
    r->spsr = 0;
    r->sp_el0 = pila_programma;
    return (indirizzo_t)r;
}
