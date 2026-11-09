/* La libreria dei programmi in ring 3.
 *
 * Un programma non puo' chiamare le funzioni del kernel: stanno in pagine senza il
 * bit utente, e saltarci farebbe page fault. Tutto quello che gli serve deve stare
 * nella zona dei programmi: queste funzioni sono "static inline", quindi il
 * compilatore le copia dentro il programma stesso.
 *
 * Ognuna bussa alla porta (int 0x80): numero del servizio in EAX, argomenti in EBX
 * ed ECX, risposta in EAX. */
#ifndef LIBRERIA_H
#define LIBRERIA_H

#include "chiamate.h"

static inline int scrivi(const char *testo, int byte) {
    int risultato;
    __asm__ volatile("int %1"
                     : "=a"(risultato)
                     : "i"(INT_SISTEMA), "a"(SIS_SCRIVI), "b"(testo), "c"(byte)
                     : "memory");
    return risultato;
}

static inline void dormi(unsigned int millisecondi) {
    int risultato;
    __asm__ volatile("int %1"
                     : "=a"(risultato)
                     : "i"(INT_SISTEMA), "a"(SIS_DORMI), "b"(millisecondi)
                     : "memory");
}

static inline void esci(void) {
    __asm__ volatile("int %0" : : "i"(INT_SISTEMA), "a"(SIS_ESCI) : "memory");
    for (;;) ;   /* non si arriva mai qui: il kernel non torna */
}

/* Il kernel non c'e': anche contare i caratteri di una stringa va fatto da soli. */
static inline int lunghezza(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static inline void scrivi_testo(const char *s) { scrivi(s, lunghezza(s)); }

#endif
