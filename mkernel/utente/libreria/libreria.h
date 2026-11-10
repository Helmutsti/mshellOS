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

/* Aspetta un tasto e lo restituisce. -1 se la tastiera e' gia' di un altro programma. */
static inline int leggi(void) {
    int risultato;
    __asm__ volatile("int %1" : "=a"(risultato) : "i"(INT_SISTEMA), "a"(SIS_LEGGI) : "memory");
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

/* Pulisce lo schermo e porta il cursore in alto, come il comando clear di Linux:
 * non e' una chiamata di sistema, ma un codice dei terminali (ESC [ H ESC [ 2 J)
 * scritto come testo, che la console del kernel riconosce ed esegue. */
static inline void pulisci_schermo(void) { scrivi_testo("\x1b[H\x1b[2J"); }

/* Due stringhe uguali? */
static inline int uguali(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

/* LEGGE UNA RIGA dalla tastiera: la nostra piccola "readline".
 * Il kernel consegna i tasti uno alla volta e non li mostra: e' questa funzione a
 * ristamparli mentre si scrive, a gestire il tasto per cancellare, e a consegnare la
 * riga quando si preme Invio. Mette in 'riga' al massimo max-1 lettere piu' lo 0 finale
 * e restituisce quante sono, o -1 se la tastiera e' di un altro programma. */
static inline int leggi_riga(char *riga, int max) {
    int n = 0;
    for (;;) {
        int c = leggi();
        if (c < 0) return -1;
        if (c == '\n') {                       /* Invio: la riga e' finita */
            scrivi("\n", 1);
            riga[n] = 0;
            return n;
        }
        if (c == '\b') {                       /* cancella: solo quello che si e' scritto */
            if (n > 0) { n--; scrivi("\b", 1); }
            continue;
        }
        if (c == '\t' || n == max - 1) continue;   /* tab no; riga piena: si ignora */
        riga[n++] = (char)c;
        char eco = (char)c;
        scrivi(&eco, 1);                       /* si mostra quello che si e' scritto */
    }
}

#endif
