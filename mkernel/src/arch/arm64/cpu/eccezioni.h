/* Le eccezioni su ARM64: la tabella dei vettori (vettori.s) e il C che le gestisce. */
#ifndef ECCEZIONI_H
#define ECCEZIONI_H

/* I registri di chi e' stato interrotto, come li salva vettori.s sulla pila
 * (272 byte: sp resta multiplo di 16). */
struct registri {
    unsigned long x[31];     /* x0-x30 (x30 e' il "link register": dove torna una funzione) */
    unsigned long elr;       /* dove riprendere (l'eip di x86) */
    unsigned long spsr;      /* lo stato di prima: livello, interruzioni spente... (l'eflags) */
    unsigned long sp_el0;    /* la pila dei programmi (EL0) */
};

/* Carica la tabella dei vettori nella CPU (VBAR_EL1). */
void vettori_prepara(void);

/* PER LE PROVE: il prossimo errore di memoria del kernel non lo ferma. L'eccezione si
 * annota l'indirizzo e la classe, salta l'istruzione sbagliata e si va avanti.
 * prova_esito dice com'e' andata: 1 se l'errore c'e' stato (e lo descrive), 0 se no. */
void prova_errore_atteso(void);
int prova_esito(unsigned long *indirizzo, unsigned long *classe);

#endif
