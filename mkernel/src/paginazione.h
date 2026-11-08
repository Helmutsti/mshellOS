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

/* Collega la pagina virtuale 'virt' alla pagina fisica 'fis' con questi permessi,
 * nelle tabelle attuali. */
void pag_mappa(unsigned int virt, unsigned int fis, unsigned int permessi);

/* Dove porta l'indirizzo virtuale 'virt'? Mette in *fis l'indirizzo fisico e in
 * *permessi i bit. Restituisce 1 se la pagina c'e', 0 se no. */
int pag_traduci(unsigned int virt, unsigned int *fis, unsigned int *permessi);

unsigned int pag_tabelle(void);   /* quante tabelle (pagine da 4 KB) usano le mappe */

#endif
