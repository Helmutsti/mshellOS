/* La libreria dei programmi.
 *
 * Sotto c'e' il kernel Linux, con la sua libreria C (musl). Un programma potrebbe usarli
 * direttamente, ma i nostri programmi parlano con il sistema solo attraverso queste
 * funzioni: sono le stesse di quando sotto c'era mkernel, il kernel didattico, e per un
 * programma non e' cambiato niente. Dentro, ognuna fa le chiamate di sistema di Linux che
 * servono (l'elenco completo, con il "come", e' in cima a libreria.c).
 *
 * Le funzioni stanno in libreria.c (programmi, file, tempo, processi) e comunicazione.c
 * (socket, fogli, aspetta): build.sh le compila una volta e le lega dentro ogni programma,
 * insieme alla libreria C (tutto in un file solo: "statico", senza librerie da caricare
 * all'avvio). Lo schermo e' della grafica: le sue funzioni sono in gkit/gkit.h.
 *
 * Qui si dichiarano solo le nostre funzioni e i nostri tipi (chiamate.h): gli header di
 * Linux e della libreria C li vede solo la libreria, cosi' i loro nomi non si mescolano
 * con quelli dei programmi. */
#ifndef LIBRERIA_H
#define LIBRERIA_H

#include "chiamate.h"

/* UNA PAROLA: quello che sta in un registro (un numero, un indirizzo): 64 bit, su
 * tutte e due le CPU. */
typedef unsigned long parola;

/* --- i programmi --- */

/* Scrive sul registro del sistema: la console del kernel (la seriale). */
int scrivi(const char *testo, int byte);

/* Non gira per quel tempo. */
void dormi(unsigned int millisecondi);

/* Il programma finisce. */
void esci(void) __attribute__((noreturn));

/* Fa partire il programma 'percorso' del disco, con lo stesso uid. Il suo numero, o -1. */
int avvia(const char *percorso);

/* Come avvia, ma il programma nuovo ha quell'uid. Solo per il sistema. */
int avvia_come(const char *percorso, unsigned int uid);

/* Come avvia_come, con un ARGOMENTO per il programma nuovo (un testo, per esempio la
 * cartella da aprire): lui lo trova in main, come secondo argomento,
 *     int main(int quanti, char **argomenti)   ->   argomenti[1] (se quanti > 1)
 * Un argomento vuoto (o 0) e' come nessun argomento. */
int avvia_come_con(const char *percorso, unsigned int uid, const char *argomento);

/* Fa partire un programma che trova gia' aperti i descrittori dell'elenco, ai posti
 * 0, 1, 2... Il suo numero, o -1. */
int avvia_con(const char *percorso, const int *descrittori, int quanti);

/* Un descrittore del programma 'numero': aspetta lo vede pronto quando e' finito. */
int processo(int numero);

/* --- i socket: parlare con gli altri programmi (vedi chiamate.h) --- */

int ascolta(const char *nome);
int collega(const char *nome);
int accetta(int d);

/* Due descrittori collegati tra loro, in d[0] e d[1]. 0, o -1. */
int coppia(int d[2]);

/* Manda un messaggio: i byte mandati, o -1 se l'altro ha chiuso. */
int manda(int d, const void *dati, unsigned int byte);

/* Come manda, ma senza mai aspettare: -1 anche se la casella dell'altro e' piena. */
int manda_subito(int d, const void *dati, unsigned int byte);

/* Come manda, con allegato il foglio del descrittore 'foglio'. */
int manda_foglio(int d, const void *dati, unsigned int byte, int foglio);

/* Riceve il prossimo messaggio (dorme se non ce n'e'): i byte ricevuti, 0 se l'altro
 * ha chiuso, -1 se errore. Un foglio allegato, se c'e', si butta. */
int ricevi(int d, void *dove, unsigned int byte);

/* Come ricevi; in *foglio il descrittore del foglio allegato, o -1 se non c'era. */
int ricevi_foglio(int d, void *dove, unsigned int byte, int *foglio);

int chiudi(int d);

/* Dorme finche' almeno uno dei descrittori dell'elenco ha qualcosa da ricevere, o
 * finche' non passano 'ms' millisecondi (-1: senza limite). Segna 'pronto' in ognuno;
 * quanti sono pronti (0: tempo scaduto), o -1. */
int aspetta(struct da_guardare *elenco, int quanti, int ms);

/* L'uid del programma all'altro capo del collegamento 'd', o -1. Lo sa il kernel: non
 * si puo' fingere. */
int chi(int d);

/* Il percorso del programma all'altro capo del collegamento 'd', in 'dove'
 * (MAX_PERCORSO_PROGRAMMA byte). 0, o -1. */
int programma_di(int d, char *dove);

/* Chiude il programma all'altro capo del collegamento 'd'. 0 se fatto, o -1. */
int termina(int d);

/* --- i fogli: memoria condivisa con gli altri programmi (vedi chiamate.h) --- */

/* Un foglio nuovo di 'byte' byte, pieno di zeri: il descrittore, o -1. */
int foglio(unsigned int byte);

/* Mette il foglio nella memoria del programma: dove sta, o 0. In *byte (se non e' 0)
 * quanto e' grande. */
void *mappa(int d, unsigned int *byte);

/* Lo leva dalla memoria. */
int togli(void *indirizzo);

/* --- il tempo --- */

/* I millisecondi passati dall'avvio. */
unsigned int tempo(void);

/* La data e l'ora, dall'orologio del PC. */
int data_ora(struct data_ora *d);

/* --- i file e le cartelle (vedi chiamate.h) --- */

/* Apre un file o una cartella: il descrittore, o un ERRORE_... (negativo). modo:
 * APRI_LEGGERE, APRI_SCRIVERE, APRI_CREARE, APRI_SVUOTARE, sommati. */
int apri(const char *percorso, unsigned int modo);

/* Legge fino a 'byte' byte dalla posizione del file: quanti ne ha letti, 0 alla fine.
 * Su una cartella: la prossima voce (struct voce_cartella), 0 quando sono finite. */
int leggi(int d, void *dove, unsigned int byte);

/* Scrive 'byte' byte alla posizione del file (allungandolo se serve): quanti ne ha scritti. */
int scrivi_in(int d, const void *dati, unsigned int byte);

/* La prossima lettura o scrittura da 'byte' (per una cartella: dalla voce numero 'byte').
 * La posizione nuova, o un errore. */
long long posiziona(int d, unsigned long long byte);

int info(const char *percorso, struct info_file *i);
int crea_cartella(const char *percorso);
int cancella(const char *percorso);

/* Le cartelle piene. Si scende al massimo di MAX_PROFONDITA livelli e i collegamenti simbolici
 * non si seguono mai (si contano, si copiano e si tolgono come collegamenti).
 *  - conta_dentro: quanti file e cartelle ci sono dentro 'percorso', a tutti i livelli;
 *  - cancella_tutto: toglie la cartella con tutto quello che ha dentro. Se qualcosa non si
 *    riesce a togliere (un permesso, un disco in sola lettura) va avanti con il resto e
 *    restituisce il primo errore: quello che non si e' tolto resta dov'e';
 *  - copia_cartella: copia la cartella e tutto quello che ha dentro in 'a' (che non deve
 *    esistere). Se non riesce, la copia fatta a meta' si toglie. */
#define MAX_PROFONDITA 64
int conta_dentro(const char *percorso, unsigned long long *quanti);
int cancella_tutto(const char *percorso);
int copia_cartella(const char *da, const char *a);

/* Rinomina o sposta. Un file gia' presente in 'a' si sostituisce. */
int rinomina(const char *da, const char *a);

/* Cambia proprietario e permessi (il proprietario lo cambia solo il sistema). */
int cambia_permessi(const char *percorso, unsigned int proprietario, unsigned int permessi);

/* Quanto e' grande, e quanto e' libera, la partizione in cui sta 'percorso'. 0, o un
 * errore. */
int stato_disco(const char *percorso, struct info_disco *d);

/* --- i processi e la memoria (vedi chiamate.h) --- */

/* I processi che ci sono (al massimo 'max'): quanti sono, o -1. */
int elenco_processi(struct info_processo *v, int max);

/* Termina il processo 'numero' (vedi chiamate.h per chi si puo'). 0, o -1. */
int termina_processo(int numero);

/* Com'e' usata la RAM. */
int stato_memoria(struct info_memoria *m);

/* --- la memoria dinamica: quella della libreria C (musl) --- */

void *malloc(unsigned long byte);
void free(void *p);

/* --- funzioni comode --- */

static inline int lunghezza(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static inline void scrivi_testo(const char *s) { scrivi(s, lunghezza(s)); }

#endif
