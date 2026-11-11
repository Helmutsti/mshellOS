/* La IDT: la tabella che dice alla CPU dove saltare quando succede qualcosa. */
#ifndef IDT_H
#define IDT_H

/* Come i registri si trovano sulla pila quando il gestore viene chiamato.
 * L'ordine e' quello in cui isr.s e la CPU li hanno messi, dal piu' recente.
 * 22 numeri da 8 byte (176 byte): la pila resta multipla di 16, come vuole il C. */
struct registri {
    unsigned long r15, r14, r13, r12, r11, r10, r9, r8;     /* messi da isr_comune */
    unsigned long rbp, rdi, rsi, rdx, rcx, rbx, rax;         /* messi da isr_comune */
    unsigned long numero, errore;                           /* messi dall'ingresso isrN */
    unsigned long rip, cs, rflags, rsp, ss;                 /* messi dalla CPU */
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
