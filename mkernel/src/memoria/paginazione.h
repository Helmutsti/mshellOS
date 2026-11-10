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

/* Costruisce le tabelle del kernel (indirizzo virtuale = fisico per la RAM e per la
 * memoria dello schermo, pagina 0 assente, codice di sola lettura) e accende la paginazione. */
void paginazione_prepara(indirizzo_t schermo, unsigned int byte_schermo);

/* Dove porta l'indirizzo virtuale 'virt', nella mappa che la CPU sta usando adesso?
 * Mette in *fis l'indirizzo fisico e in *permessi i bit. 1 se la pagina c'e', 0 se no. */
int pag_traduci(indirizzo_t virt, indirizzo_t *fis, unsigned int *permessi);

unsigned int pag_tabelle(void);   /* quante tabelle (pagine da 4 KB) usano le mappe */

/* --- le mappe dei programmi --- */

/* Da PROGRAMMI_INIZIO a PROGRAMMI_FINE (1-2 GB, in chiamate.h) gli indirizzi sono dei
 * programmi: ogni mappa ha li' le sue pagine. Il resto e' del kernel, uguale e
 * condiviso in tutte. */
#include "chiamate.h"

/* Una mappa nuova: la parte del kernel gia' dentro, quella del programma vuota.
 * Restituisce l'indirizzo della sua directory, o 0 se la memoria e' finita. */
indirizzo_t pag_nuova_mappa(void);

/* Nella mappa di un programma collega 'virt' (tra 1 e 2 GB) alla pagina fisica 'fis'.
 * 1 se fatto, 0 se la memoria per le tabelle e' finita o 'virt' e' fuori da 1-2 GB. */
int pag_mappa_programma(indirizzo_t mappa, indirizzo_t virt, indirizzo_t fis, unsigned int permessi);

/* Toglie 'virt' dalla mappa di un programma (la pagina non si restituisce: e' di chi
 * l'ha prestata). */
void pag_togli_programma(indirizzo_t mappa, indirizzo_t virt);

/* Quante pagine sono del programma nella sua mappa (non quelle PRESTATE: lo schermo e i
 * fogli condivisi, che non sono sue). */
unsigned int pag_conta_programma(indirizzo_t mappa);

/* Restituisce al magazziniere tutte le pagine del programma (non quelle PRESTATE), le
 * sue tabelle e la directory. Non si puo' fare sulla mappa che la CPU sta usando. */
void pag_cancella_mappa(indirizzo_t mappa);

/* Fa usare alla CPU questa mappa (0 = quella del kernel). */
void pag_usa_mappa(indirizzo_t mappa);

#endif
