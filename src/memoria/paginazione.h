/* La paginazione: la CPU traduce e controlla ogni indirizzo usando le tabelle delle pagine. */
#ifndef PAGINAZIONE_H
#define PAGINAZIONE_H

#include "kernel.h"   /* indirizzo_t */

/* I permessi di una pagina (i bit bassi di ogni riga delle tabelle). */
#define PAG_PRESENTE  0x1   /* la pagina c'e' */
#define PAG_SCRITTURA 0x2   /* si puo' scrivere (senza: sola lettura) */
#define PAG_UTENTE    0x4   /* la puo' usare anche il ring 3 (senza: solo kernel) */
/* Un bit che la CPU ignora e lascia al kernel (i bit 9-11 sono liberi): la pagina e'
 * PRESTATA al programma, non e' del magazziniere (per esempio la memoria dello schermo).
 * Quando la mappa si cancella, non va restituita. */
#define PAG_PRESTATA  0x200

/* La paginazione del kernel: le sue tabelle (la meta' alta degli indirizzi: la mappa
 * diretta della RAM, i dispositivi, il kernel con il codice di sola lettura; la pianta e'
 * in pagine.h) le ha gia' fatte l'avvio della CPU (arch: mmu.c su ARM, paginazione.c sul
 * PC). Qui non resta niente da fare: c'e' per simmetria con l'avvio delle altre parti. */
void paginazione_prepara(indirizzo_t schermo, unsigned int byte_schermo);

/* Dove porta l'indirizzo virtuale 'virt', nella mappa che la CPU sta usando adesso?
 * Mette in *fis l'indirizzo fisico e in *permessi i bit. 1 se la pagina c'e', 0 se no.
 * Risponde solo per la zona dei programmi (sotto): per il resto, 0. */
int pag_traduci(indirizzo_t virt, indirizzo_t *fis, unsigned int *permessi);

unsigned int pag_tabelle(void);   /* quante tabelle (pagine da 4 KB) usano le mappe */

/* --- le mappe dei programmi --- */

/* Da PROGRAMMI_INIZIO a PROGRAMMI_FINE (da 4 MB a 2 GB, in chiamate.h), nella meta' bassa,
 * gli indirizzi sono dei programmi: ogni mappa ha li' le sue pagine. La meta' alta e' del
 * kernel, uguale e condivisa in tutte. */
#include "chiamate.h"

/* Una mappa nuova: la meta' del kernel gia' dentro, quella del programma vuota.
 * Restituisce l'indirizzo FISICO della sua radice (la tabella piu' in alto: quello che si
 * da' alla CPU), o 0 se la memoria e' finita. E' il "numero" della mappa per le funzioni
 * qui sotto. */
indirizzo_t pag_nuova_mappa(void);

/* Nella mappa di un programma collega 'virt' (nella zona dei programmi) alla pagina
 * fisica 'fis'. 1 se fatto, 0 se la memoria per le tabelle e' finita o 'virt' e' fuori. */
int pag_mappa_programma(indirizzo_t mappa, indirizzo_t virt, indirizzo_t fis, unsigned int permessi);

/* Toglie 'virt' dalla mappa di un programma (la pagina non si restituisce: e' di chi
 * l'ha prestata). */
void pag_togli_programma(indirizzo_t mappa, indirizzo_t virt);

/* Quante pagine sono del programma nella sua mappa (non quelle PRESTATE: lo schermo e i
 * fogli condivisi, che non sono sue). */
unsigned int pag_conta_programma(indirizzo_t mappa);

/* Restituisce al magazziniere tutte le pagine del programma (non quelle PRESTATE), le
 * sue tabelle e la radice. Non si puo' fare sulla mappa che la CPU sta usando. */
void pag_cancella_mappa(indirizzo_t mappa);

/* Fa usare alla CPU questa mappa (0 = quella del kernel, con la meta' bassa vuota). */
void pag_usa_mappa(indirizzo_t mappa);

/* Rende raggiungibile al kernel la memoria di un dispositivo (i suoi registri, MMIO, che
 * stanno all'indirizzo fisico 'fisico'): la mette nella ZONA_DISPOSITIVI (pagine.h),
 * lettura e scrittura, SENZA CACHE (ogni accesso deve arrivare davvero al dispositivo).
 * Restituisce l'indirizzo VIRTUALE da usare, o 0 se non si e' potuto. Chiedere di nuovo
 * un pezzo gia' mappato da' lo stesso indirizzo. Si chiama dopo pagine_prepara (le
 * tabelle sono pagine del magazziniere); la zona e' condivisa da tutte le mappe, quindi
 * va bene anche dopo che sono nati i programmi. */
indirizzo_t pag_mappa_dispositivo(indirizzo_t fisico, unsigned int byte);

#endif
