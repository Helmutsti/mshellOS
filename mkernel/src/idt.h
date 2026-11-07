/* La IDT: la tabella che dice alla CPU dove saltare quando succede qualcosa. */
#ifndef IDT_H
#define IDT_H

/* Come i registri si trovano sulla pila quando il gestore viene chiamato.
 * L'ordine e' quello in cui isr.s e la CPU li hanno messi, dal piu' recente. */
struct registri {
    unsigned int ds;                                     /* messo da isr_comune */
    unsigned int edi, esi, ebp, esp_inutile, ebx, edx, ecx, eax;   /* messi da pusha */
    unsigned int numero, errore;                         /* messi dall'ingresso isrN */
    unsigned int eip, cs, eflags;                        /* messi dalla CPU */
};

/* Prepara la tabella e la consegna alla CPU. */
void idt_prepara(void);

/* Un gestore di interruzione hardware: riceve i registri salvati. */
typedef void (*gestore_irq)(struct registri *r);

/* Dice quale funzione chiamare quando arriva la linea irq (0-15). */
void irq_collega(int irq, gestore_irq gestore);

/* Stampa dove sta la IDT e quante righe sono collegate. */
void idt_mostra(void);

#endif
