/* La paginazione: la CPU traduce e controlla ogni indirizzo usando le tabelle delle pagine. */
#ifndef PAGINAZIONE_H
#define PAGINAZIONE_H

/* I permessi di una pagina (i bit bassi di ogni riga delle tabelle). */
#define PAG_PRESENTE  0x1   /* la pagina c'e' */
#define PAG_SCRITTURA 0x2   /* si puo' scrivere (senza: sola lettura) */
#define PAG_UTENTE    0x4   /* la puo' usare anche il ring 3 (senza: solo kernel) */

/* Costruisce le tabelle del kernel (indirizzo virtuale = fisico, pagina 0 assente,
 * codice di sola lettura) e accende la paginazione. */
void paginazione_prepara(void);

/* Dove porta l'indirizzo virtuale 'virt', nella mappa che la CPU sta usando adesso?
 * Mette in *fis l'indirizzo fisico e in *permessi i bit. 1 se la pagina c'e', 0 se no. */
int pag_traduci(unsigned int virt, unsigned int *fis, unsigned int *permessi);

unsigned int pag_tabelle(void);   /* quante tabelle (pagine da 4 KB) usano le mappe */

/* --- le mappe dei programmi --- */

/* Da PROGRAMMI_INIZIO (1 GB, in chiamate.h) in su gli indirizzi sono dei programmi:
 * ogni mappa ha li' le sue pagine. Sotto c'e' il kernel, uguale e condiviso in tutte. */
#include "chiamate.h"

/* Una mappa nuova: la parte del kernel gia' dentro, quella del programma vuota.
 * Restituisce l'indirizzo della sua directory, o 0 se la memoria e' finita. */
unsigned int pag_nuova_mappa(void);

/* Nella mappa di un programma collega 'virt' (dal 1 GB in su) alla pagina fisica 'fis'.
 * 1 se fatto, 0 se la memoria per le tabelle e' finita o 'virt' e' sotto il 1 GB. */
int pag_mappa_programma(unsigned int mappa, unsigned int virt, unsigned int fis, unsigned int permessi);

/* Restituisce al magazziniere tutte le pagine del programma, le sue tabelle e la
 * directory. Non si puo' fare sulla mappa che la CPU sta usando. */
void pag_cancella_mappa(unsigned int mappa);

/* Fa usare alla CPU questa mappa (0 = quella del kernel). */
void pag_usa_mappa(unsigned int mappa);

#endif
