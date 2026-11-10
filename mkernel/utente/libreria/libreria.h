/* La libreria dei programmi in ring 3.
 *
 * Un programma non puo' chiamare le funzioni del kernel: nella sua mappa il kernel
 * c'e', ma in pagine senza il bit utente, e saltarci farebbe page fault. Tutto quello
 * che gli serve deve stare nel programma stesso: queste funzioni sono "static inline",
 * quindi il compilatore le copia dentro ogni programma.
 *
 * Le chiamate di sistema bussano alla porta (int 0x80): numero del servizio in EAX,
 * argomenti in EBX, ECX ed EDX, risposta in EAX. */
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

/* Un descrittore da cui ricevere gli eventi di tastiera e mouse (struct evento), o -1
 * se li ha gia' un altro programma. */
static inline int apri_eventi(void) {
    int risultato;
    __asm__ volatile("int %1" : "=a"(risultato) : "i"(INT_SISTEMA), "a"(SIS_APRI_EVENTI) : "memory");
    return risultato;
}

/* --- i socket: parlare con gli altri programmi (vedi chiamate.h) --- */

static inline int chiamata1(int servizio, unsigned int a) {
    int risultato;
    __asm__ volatile("int %1" : "=a"(risultato) : "i"(INT_SISTEMA), "a"(servizio), "b"(a) : "memory");
    return risultato;
}

static inline int chiamata3(int servizio, unsigned int a, unsigned int b, unsigned int c) {
    int risultato;
    __asm__ volatile("int %1"
                     : "=a"(risultato)
                     : "i"(INT_SISTEMA), "a"(servizio), "b"(a), "c"(b), "d"(c)
                     : "memory");
    return risultato;
}

/* Si mette in ascolto con 'nome': il descrittore su cui arrivano i collegamenti, o -1. */
static inline int ascolta(const char *nome) { return chiamata1(SIS_ASCOLTA, (unsigned int)nome); }

/* Si collega a chi ascolta su 'nome': il descrittore per parlargli, o -1. */
static inline int collega(const char *nome) { return chiamata1(SIS_COLLEGA, (unsigned int)nome); }

/* Il prossimo collegamento arrivato su 'd' (dorme se non ce n'e'): un descrittore nuovo. */
static inline int accetta(int d) { return chiamata1(SIS_ACCETTA, d); }

static inline int chiamata4(int servizio, unsigned int a, unsigned int b, unsigned int c,
                            unsigned int d) {
    int risultato;
    __asm__ volatile("int %1"
                     : "=a"(risultato)
                     : "i"(INT_SISTEMA), "a"(servizio), "b"(a), "c"(b), "d"(c), "S"(d)
                     : "memory");
    return risultato;
}

/* Manda un messaggio: i byte mandati, o -1 se l'altro ha chiuso. */
static inline int manda(int d, const void *dati, unsigned int byte) {
    return chiamata4(SIS_MANDA, d, (unsigned int)dati, byte, -1);
}

/* Come manda, con allegato il foglio del descrittore 'foglio'. */
static inline int manda_foglio(int d, const void *dati, unsigned int byte, int foglio) {
    return chiamata4(SIS_MANDA, d, (unsigned int)dati, byte, foglio);
}

/* Riceve il prossimo messaggio (dorme se non ce n'e'): i byte ricevuti, 0 se l'altro
 * ha chiuso, -1 se errore. Un foglio allegato, se c'e', si lascia. */
static inline int ricevi(int d, void *dove, unsigned int byte) {
    return chiamata4(SIS_RICEVI, d, (unsigned int)dove, byte, 0);
}

/* Come ricevi; in *foglio il descrittore del foglio allegato, o -1 se non c'era. */
static inline int ricevi_foglio(int d, void *dove, unsigned int byte, int *foglio) {
    return chiamata4(SIS_RICEVI, d, (unsigned int)dove, byte, (unsigned int)foglio);
}

/* --- i fogli: memoria condivisa con gli altri programmi (vedi chiamate.h) --- */

/* Un foglio nuovo di 'byte' byte: il descrittore, o -1. */
static inline int foglio(unsigned int byte) { return chiamata1(SIS_FOGLIO, byte); }

/* Mette il foglio nella memoria del programma: dove sta, o 0. */
static inline void *mappa(int d) {
    int r = chiamata1(SIS_MAPPA, d);
    return r == -1 ? 0 : (void *)r;
}

/* Lo leva dalla memoria. */
static inline int togli(void *indirizzo) { return chiamata1(SIS_TOGLI, (unsigned int)indirizzo); }

static inline int chiudi(int d) { return chiamata1(SIS_CHIUDI, d); }

/* Dorme finche' almeno uno dei descrittori dell'elenco ha qualcosa da ricevere.
 * Segna 'pronto' in ognuno; quanti sono pronti, o -1. */
static inline int aspetta(struct da_guardare *elenco, int quanti) {
    return chiamata3(SIS_ASPETTA, (unsigned int)elenco, quanti, 0);
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
