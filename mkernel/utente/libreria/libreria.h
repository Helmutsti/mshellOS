/* La libreria dei programmi in ring 3.
 *
 * Un programma non puo' chiamare le funzioni del kernel: nella sua mappa il kernel
 * c'e', ma in pagine senza il bit utente, e saltarci farebbe page fault. Tutto quello
 * che gli serve deve stare nel programma stesso: queste funzioni sono "static inline",
 * quindi il compilatore le copia dentro ogni programma.
 *
 * Le chiamate di sistema bussano alla porta (int 0x80): numero del servizio in EAX,
 * argomenti in EBX ed ECX, risposta in EAX. */
#ifndef LIBRERIA_H
#define LIBRERIA_H

#include "chiamate.h"

/* --- le chiamate di sistema --- */

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

/* Dorme finche' non arriva un evento (un tasto, il mouse) e lo mette in 'e'.
 * 0 se fatto, -1 se un altro programma sta gia' aspettando gli eventi. */
static inline int aspetta_evento(struct evento *e) {
    int risultato;
    __asm__ volatile("int %1"
                     : "=a"(risultato)
                     : "i"(INT_SISTEMA), "a"(SIS_EVENTO), "b"(e)
                     : "memory");
    return risultato;
}

/* Fa partire il programma 'percorso' del disco. Il suo numero di compito, o -1. */
static inline int avvia(const char *percorso) {
    int risultato;
    __asm__ volatile("int %1"
                     : "=a"(risultato)
                     : "i"(INT_SISTEMA), "a"(SIS_AVVIA), "b"(percorso)
                     : "memory");
    return risultato;
}

/* Chiede lo schermo al kernel. Se va bene (0) la sua memoria e' a SCHERMO_PROGRAMMA e
 * 'info' dice com'e' fatto; -1 se e' gia' di un altro programma. */
static inline int prendi_schermo(struct info_schermo *info) {
    int risultato;
    __asm__ volatile("int %1"
                     : "=a"(risultato)
                     : "i"(INT_SISTEMA), "a"(SIS_SCHERMO), "b"(info)
                     : "memory");
    return risultato;
}

/* --- funzioni comode, fatte con le chiamate qui sopra --- */

/* Il kernel non c'e': anche contare i caratteri di una stringa va fatto da soli. */
static inline int lunghezza(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static inline void scrivi_testo(const char *s) { scrivi(s, lunghezza(s)); }

#endif
