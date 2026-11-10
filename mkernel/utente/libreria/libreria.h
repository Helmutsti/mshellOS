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

/* Lo schermo: un descrittore, o -1 se e' gia' aperto (vedi chiamate.h). */
static inline int apri_schermo(void) {
    int risultato;
    __asm__ volatile("int %1" : "=a"(risultato) : "i"(INT_SISTEMA), "a"(SIS_APRI_SCHERMO) : "memory");
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

static inline int chiamata5(int servizio, unsigned int a, unsigned int b, unsigned int c,
                            unsigned int d, unsigned int e) {
    int risultato;
    __asm__ volatile("int %1"
                     : "=a"(risultato)
                     : "i"(INT_SISTEMA), "a"(servizio), "b"(a), "c"(b), "d"(c), "S"(d), "D"(e)
                     : "memory");
    return risultato;
}
#define chiamata4(s, a, b, c, d) chiamata5(s, a, b, c, d, 0)

/* Manda un messaggio: i byte mandati, o -1 se l'altro ha chiuso. */
static inline int manda(int d, const void *dati, unsigned int byte) {
    return chiamata4(SIS_MANDA, d, (unsigned int)dati, byte, -1);
}

/* Come manda, ma senza mai aspettare: -1 anche se la casella dell'altro e' piena. */
static inline int manda_subito(int d, const void *dati, unsigned int byte) {
    return chiamata5(SIS_MANDA, d, (unsigned int)dati, byte, -1, MANDA_SUBITO);
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

/* Mette il foglio nella memoria del programma: dove sta, o 0. In *byte (se non e' 0)
 * quanto e' grande. */
static inline void *mappa(int d, unsigned int *byte) {
    int r = chiamata3(SIS_MAPPA, d, (unsigned int)byte, 0);
    return r == -1 ? 0 : (void *)r;
}

/* Lo leva dalla memoria. */
static inline int togli(void *indirizzo) { return chiamata1(SIS_TOGLI, (unsigned int)indirizzo); }

static inline int chiudi(int d) { return chiamata1(SIS_CHIUDI, d); }

/* Dorme finche' almeno uno dei descrittori dell'elenco ha qualcosa da ricevere, o
 * finche' non passano 'ms' millisecondi (-1: senza limite). Segna 'pronto' in ognuno;
 * quanti sono pronti (0: tempo scaduto), o -1. */
static inline int aspetta(struct da_guardare *elenco, int quanti, int ms) {
    return chiamata3(SIS_ASPETTA, (unsigned int)elenco, quanti, ms);
}

/* I millisecondi passati dall'avvio. */
static inline unsigned int tempo(void) {
    int risultato;
    __asm__ volatile("int %1" : "=a"(risultato) : "i"(INT_SISTEMA), "a"(SIS_TEMPO) : "memory");
    return risultato;
}

/* Chiude il programma all'altro capo del collegamento 'd'. 0 se fatto, o -1. */
static inline int termina(int d) { return chiamata1(SIS_TERMINA, d); }

/* --- i file e le cartelle (vedi chiamate.h) --- */

/* Apre un file o una cartella: il descrittore, o -1. modo: APRI_LEGGERE, APRI_SCRIVERE,
 * APRI_CREARE, APRI_SVUOTARE, sommati. */
static inline int apri(const char *percorso, unsigned int modo) {
    return chiamata3(SIS_APRI, (unsigned int)percorso, modo, 0);
}

/* Legge fino a 'byte' byte dalla posizione del file: quanti ne ha letti, 0 alla fine.
 * Su una cartella: la prossima voce (struct voce_cartella), 0 quando sono finite. */
static inline int leggi(int d, void *dove, unsigned int byte) { return ricevi(d, dove, byte); }

/* Scrive 'byte' byte alla posizione del file (allungandolo se serve): quanti ne ha scritti. */
static inline int scrivi_in(int d, const void *dati, unsigned int byte) { return manda(d, dati, byte); }

static inline int posiziona(int d, unsigned int byte) { return chiamata3(SIS_POSIZIONA, d, byte, 0); }

static inline int info(const char *percorso, struct info_file *i) {
    return chiamata3(SIS_INFO, (unsigned int)percorso, (unsigned int)i, 0);
}

static inline int crea_cartella(const char *percorso) { return chiamata1(SIS_CREA_CARTELLA, (unsigned int)percorso); }
static inline int cancella(const char *percorso) { return chiamata1(SIS_CANCELLA, (unsigned int)percorso); }

/* Cambia proprietario e permessi (vedi chiamate.h: il proprietario lo cambia solo il sistema). */
static inline int cambia_permessi(const char *percorso, unsigned int proprietario, unsigned int permessi) {
    return chiamata3(SIS_PERMESSI, (unsigned int)percorso, proprietario, permessi);
}

/* Fa partire un programma con un altro uid: solo il sistema. Il numero del compito, o -1. */
static inline int avvia_come(const char *percorso, unsigned int uid) {
    return chiamata3(SIS_AVVIA_COME, (unsigned int)percorso, uid, 0);
}

/* Mette lo schermo del descrittore 'd' nella memoria del programma, a SCHERMO_PROGRAMMA:
 * scrivere li' un numero vuol dire accendere un pixel. 'info' dice com'e' fatto. 0, o -1. */
static inline int mappa_schermo(int d, struct info_schermo *info) {
    return chiamata3(SIS_MAPPA_SCHERMO, d, (unsigned int)info, 0);
}

/* Fa partire un programma che trova gia' aperti i descrittori dell'elenco, ai posti
 * 0, 1, 2... Il numero del compito, o -1. */
static inline int avvia_con(const char *percorso, const int *descrittori, int quanti) {
    return chiamata3(SIS_AVVIA_CON, (unsigned int)percorso, (unsigned int)descrittori, quanti);
}

/* Un descrittore del programma 'numero': ricevi aspetta che finisca. -1 se non c'e'. */
static inline int processo(int numero) { return chiamata1(SIS_PROCESSO, numero); }

/* I processi che ci sono (al massimo 'max'): quanti sono, o -1. */
static inline int elenco_processi(struct info_processo *v, int max) {
    return chiamata3(SIS_ELENCO_PROCESSI, (unsigned int)v, max, 0);
}

/* Termina il processo 'numero' (vedi chiamate.h per chi si puo'). 0, o -1. */
static inline int termina_processo(int numero) { return chiamata1(SIS_TERMINA_PROCESSO, numero); }

/* Com'e' usata la RAM. */
static inline int stato_memoria(struct info_memoria *m) { return chiamata1(SIS_MEMORIA, (unsigned int)m); }

/* La data e l'ora, dall'orologio del PC. */
static inline int data_ora(struct data_ora *d) { return chiamata1(SIS_DATA_ORA, (unsigned int)d); }

/* Il percorso del programma all'altro capo del collegamento 'd', in 'dove' (MAX_PERCORSO
 * byte). 0, o -1. */
static inline int programma_di(int d, char *dove) {
    return chiamata3(SIS_PROGRAMMA_DI, d, (unsigned int)dove, 0);
}

/* L'uid del programma all'altro capo del collegamento 'd', o -1. */
static inline int chi(int d) { return chiamata1(SIS_CHI, d); }

/* Rinomina o sposta. Un file gia' presente in 'a' si sostituisce. */
static inline int rinomina(const char *da, const char *a) {
    return chiamata3(SIS_RINOMINA, (unsigned int)da, (unsigned int)a, 0);
}

/* --- la memoria dinamica --- */

/* Sposta il confine della zona dinamica (vedi chiamate.h); rottura(0) dice dov'e'.
 * Di solito non serve: c'e' malloc. */
static inline unsigned int rottura(unsigned int indirizzo) {
    return (unsigned int)chiamata1(SIS_ROTTURA, indirizzo);
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
