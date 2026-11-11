/* La libreria dei programmi (ring 3 su x86, EL0 su ARM).
 *
 * Un programma non puo' chiamare le funzioni del kernel: nella sua mappa il kernel
 * c'e', ma in pagine riservate al kernel, e saltarci darebbe errore. Tutto quello che
 * gli serve deve stare nel programma stesso: queste funzioni sono "static inline",
 * quindi il compilatore le copia dentro ogni programma.
 *
 * LE CHIAMATE DI SISTEMA bussano alla porta del kernel, in un modo diverso per CPU (ma
 * passano tutte da chiamata5, qui sotto):
 *   x86-64: int 0x80. Numero del servizio in RAX, argomenti in RDI, RSI, RDX, R10, R8,
 *           risposta in RAX (gli stessi registri di Linux).
 *   ARM64: svc #0. Numero del servizio in x8, argomenti in x0-x4, risposta in x0 (come
 *          Linux). */
#ifndef LIBRERIA_H
#define LIBRERIA_H

#include "chiamate.h"

/* --- le chiamate di sistema --- */

/* UNA PAROLA: quello che sta in un registro (un numero, un indirizzo): 64 bit, su
 * tutte e due le CPU. */
typedef unsigned long parola;

static inline long chiamata5(int servizio, parola a, parola b, parola c, parola d, parola e) {
#ifdef __aarch64__
    register parola x8 __asm__("x8") = (parola)servizio;
    register parola x0 __asm__("x0") = a;
    register parola x1 __asm__("x1") = b;
    register parola x2 __asm__("x2") = c;
    register parola x3 __asm__("x3") = d;
    register parola x4 __asm__("x4") = e;
    __asm__ volatile("svc #0"
                     : "+r"(x0)
                     : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4)
                     : "memory");
    return (long)x0;
#else
    /* r10 e r8 non hanno una lettera loro nei vincoli dell'asm: si legano per nome */
    register parola r10 __asm__("r10") = d;
    register parola r8 __asm__("r8") = e;
    long risultato;
    __asm__ volatile("int %1"
                     : "=a"(risultato)
                     : "i"(INT_SISTEMA), "a"((parola)servizio), "D"(a), "S"(b), "d"(c),
                       "r"(r10), "r"(r8)
                     : "memory");
    return risultato;
#endif
}
#define chiamata4(s, a, b, c, d) chiamata5(s, a, b, c, d, 0)
#define chiamata3(s, a, b, c)    chiamata5(s, a, b, c, 0, 0)
#define chiamata1(s, a)          chiamata5(s, a, 0, 0, 0, 0)
#define chiamata0(s)             chiamata5(s, 0, 0, 0, 0, 0)

static inline int scrivi(const char *testo, int byte) {
    return (int)chiamata3(SIS_SCRIVI, (parola)testo, (parola)byte, 0);
}

static inline void dormi(unsigned int millisecondi) { chiamata1(SIS_DORMI, millisecondi); }

static inline void esci(void) {
    chiamata0(SIS_ESCI);
    for (;;) ;   /* non si arriva mai qui: il kernel non torna */
}

/* Fa partire il programma 'percorso' del disco. Il suo numero di compito, o -1. */
static inline int avvia(const char *percorso) { return (int)chiamata1(SIS_AVVIA, (parola)percorso); }

/* Lo schermo: un descrittore, o -1 se e' gia' aperto (vedi chiamate.h). */
static inline int apri_schermo(void) { return (int)chiamata0(SIS_APRI_SCHERMO); }

/* Un descrittore da cui ricevere gli eventi di tastiera e mouse (struct evento), o -1
 * se li ha gia' un altro programma. */
static inline int apri_eventi(void) { return (int)chiamata0(SIS_APRI_EVENTI); }

/* --- i socket: parlare con gli altri programmi (vedi chiamate.h) --- */

/* Si mette in ascolto con 'nome': il descrittore su cui arrivano i collegamenti, o -1. */
static inline int ascolta(const char *nome) { return chiamata1(SIS_ASCOLTA, (parola)nome); }

/* Si collega a chi ascolta su 'nome': il descrittore per parlargli, o -1. */
static inline int collega(const char *nome) { return chiamata1(SIS_COLLEGA, (parola)nome); }

/* Il prossimo collegamento arrivato su 'd' (dorme se non ce n'e'): un descrittore nuovo. */
static inline int accetta(int d) { return chiamata1(SIS_ACCETTA, d); }


/* Manda un messaggio: i byte mandati, o -1 se l'altro ha chiuso. */
static inline int manda(int d, const void *dati, unsigned int byte) {
    return chiamata4(SIS_MANDA, d, (parola)dati, byte, -1);
}

/* Come manda, ma senza mai aspettare: -1 anche se la casella dell'altro e' piena. */
static inline int manda_subito(int d, const void *dati, unsigned int byte) {
    return chiamata5(SIS_MANDA, d, (parola)dati, byte, -1, MANDA_SUBITO);
}

/* Come manda, con allegato il foglio del descrittore 'foglio'. */
static inline int manda_foglio(int d, const void *dati, unsigned int byte, int foglio) {
    return chiamata4(SIS_MANDA, d, (parola)dati, byte, foglio);
}

/* Riceve il prossimo messaggio (dorme se non ce n'e'): i byte ricevuti, 0 se l'altro
 * ha chiuso, -1 se errore. Un foglio allegato, se c'e', si lascia. */
static inline int ricevi(int d, void *dove, unsigned int byte) {
    return chiamata4(SIS_RICEVI, d, (parola)dove, byte, 0);
}

/* Come ricevi; in *foglio il descrittore del foglio allegato, o -1 se non c'era. */
static inline int ricevi_foglio(int d, void *dove, unsigned int byte, int *foglio) {
    return chiamata4(SIS_RICEVI, d, (parola)dove, byte, (parola)foglio);
}

/* --- i fogli: memoria condivisa con gli altri programmi (vedi chiamate.h) --- */

/* Un foglio nuovo di 'byte' byte: il descrittore, o -1. */
static inline int foglio(unsigned int byte) { return chiamata1(SIS_FOGLIO, byte); }

/* Mette il foglio nella memoria del programma: dove sta, o 0. In *byte (se non e' 0)
 * quanto e' grande. */
static inline void *mappa(int d, unsigned int *byte) {
    long r = chiamata3(SIS_MAPPA, d, (parola)byte, 0);
    return r == -1 ? 0 : (void *)r;
}

/* Lo leva dalla memoria. */
static inline int togli(void *indirizzo) { return chiamata1(SIS_TOGLI, (parola)indirizzo); }

static inline int chiudi(int d) { return chiamata1(SIS_CHIUDI, d); }

/* Dorme finche' almeno uno dei descrittori dell'elenco ha qualcosa da ricevere, o
 * finche' non passano 'ms' millisecondi (-1: senza limite). Segna 'pronto' in ognuno;
 * quanti sono pronti (0: tempo scaduto), o -1. */
static inline int aspetta(struct da_guardare *elenco, int quanti, int ms) {
    return chiamata3(SIS_ASPETTA, (parola)elenco, quanti, ms);
}

/* I millisecondi passati dall'avvio. */
static inline unsigned int tempo(void) { return (unsigned int)chiamata0(SIS_TEMPO); }

/* Chiude il programma all'altro capo del collegamento 'd'. 0 se fatto, o -1. */
static inline int termina(int d) { return chiamata1(SIS_TERMINA, d); }

/* --- i file e le cartelle (vedi chiamate.h) --- */

/* Apre un file o una cartella: il descrittore, o -1. modo: APRI_LEGGERE, APRI_SCRIVERE,
 * APRI_CREARE, APRI_SVUOTARE, sommati. */
static inline int apri(const char *percorso, unsigned int modo) {
    return chiamata3(SIS_APRI, (parola)percorso, modo, 0);
}

/* Legge fino a 'byte' byte dalla posizione del file: quanti ne ha letti, 0 alla fine.
 * Su una cartella: la prossima voce (struct voce_cartella), 0 quando sono finite. */
static inline int leggi(int d, void *dove, unsigned int byte) { return ricevi(d, dove, byte); }

/* Scrive 'byte' byte alla posizione del file (allungandolo se serve): quanti ne ha scritti. */
static inline int scrivi_in(int d, const void *dati, unsigned int byte) { return manda(d, dati, byte); }

static inline int posiziona(int d, unsigned int byte) { return chiamata3(SIS_POSIZIONA, d, byte, 0); }

static inline int info(const char *percorso, struct info_file *i) {
    return chiamata3(SIS_INFO, (parola)percorso, (parola)i, 0);
}

static inline int crea_cartella(const char *percorso) { return chiamata1(SIS_CREA_CARTELLA, (parola)percorso); }
static inline int cancella(const char *percorso) { return chiamata1(SIS_CANCELLA, (parola)percorso); }

/* Cambia proprietario e permessi (vedi chiamate.h: il proprietario lo cambia solo il sistema). */
static inline int cambia_permessi(const char *percorso, unsigned int proprietario, unsigned int permessi) {
    return chiamata3(SIS_PERMESSI, (parola)percorso, proprietario, permessi);
}

/* Fa partire un programma con un altro uid: solo il sistema. Il numero del compito, o -1. */
static inline int avvia_come(const char *percorso, unsigned int uid) {
    return chiamata3(SIS_AVVIA_COME, (parola)percorso, uid, 0);
}

/* Mette lo schermo del descrittore 'd' nella memoria del programma, a SCHERMO_PROGRAMMA:
 * scrivere li' un numero vuol dire accendere un pixel. 'info' dice com'e' fatto. 0, o -1. */
static inline int mappa_schermo(int d, struct info_schermo *info) {
    return chiamata3(SIS_MAPPA_SCHERMO, d, (parola)info, 0);
}

/* Fa partire un programma che trova gia' aperti i descrittori dell'elenco, ai posti
 * 0, 1, 2... Il numero del compito, o -1. */
static inline int avvia_con(const char *percorso, const int *descrittori, int quanti) {
    return chiamata3(SIS_AVVIA_CON, (parola)percorso, (parola)descrittori, quanti);
}

/* Un descrittore del programma 'numero': ricevi aspetta che finisca. -1 se non c'e'. */
static inline int processo(int numero) { return chiamata1(SIS_PROCESSO, numero); }

/* I processi che ci sono (al massimo 'max'): quanti sono, o -1. */
static inline int elenco_processi(struct info_processo *v, int max) {
    return chiamata3(SIS_ELENCO_PROCESSI, (parola)v, max, 0);
}

/* Termina il processo 'numero' (vedi chiamate.h per chi si puo'). 0, o -1. */
static inline int termina_processo(int numero) { return chiamata1(SIS_TERMINA_PROCESSO, numero); }

/* Com'e' usata la RAM. */
static inline int stato_memoria(struct info_memoria *m) { return chiamata1(SIS_MEMORIA, (parola)m); }

/* La data e l'ora, dall'orologio del PC. */
static inline int data_ora(struct data_ora *d) { return chiamata1(SIS_DATA_ORA, (parola)d); }

/* Il percorso del programma all'altro capo del collegamento 'd', in 'dove' (MAX_PERCORSO
 * byte). 0, o -1. */
static inline int programma_di(int d, char *dove) {
    return chiamata3(SIS_PROGRAMMA_DI, d, (parola)dove, 0);
}

/* L'uid del programma all'altro capo del collegamento 'd', o -1. */
static inline int chi(int d) { return chiamata1(SIS_CHI, d); }

/* Rinomina o sposta. Un file gia' presente in 'a' si sostituisce. */
static inline int rinomina(const char *da, const char *a) {
    return chiamata3(SIS_RINOMINA, (parola)da, (parola)a, 0);
}

/* --- la memoria dinamica --- */

/* Sposta il confine della zona dinamica (vedi chiamate.h); rottura(0) dice dov'e'.
 * Di solito non serve: c'e' malloc. */
static inline parola rottura(parola indirizzo) {
    return (parola)chiamata1(SIS_ROTTURA, indirizzo);
}

/* Un pezzo di almeno 'byte' byte, allineato a 8, o 0 se la memoria e' finita. Il
 * contenuto e' pieno di zeri solo la prima volta che quella memoria viene usata.
 * (memoria.c) */
void *malloc(unsigned int byte);
/* Restituisce un pezzo avuto da malloc. free(0) non fa niente. */
void free(void *p);

/* --- funzioni comode, fatte con le chiamate qui sopra --- */

/* Il kernel non c'e': anche contare i caratteri di una stringa va fatto da soli. */
static inline int lunghezza(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static inline void scrivi_testo(const char *s) { scrivi(s, lunghezza(s)); }

#endif
