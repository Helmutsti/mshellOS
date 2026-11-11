/* Il GIC: il centralino delle interruzioni di ARM. */
#ifndef GIC_H
#define GIC_H

#define GIC_MAX_LINEE 128
#define GIC_LINEA_TIMER 27   /* il timer generico (virtuale) */   /* le linee che gestiamo (0-15 software, 16-31 della CPU, 32+ dispositivi) */

typedef void (*gestore_linea)(void);

/* Accende il centralino (distributore e interfaccia della CPU). */
void gic_prepara(void);

/* Quale versione ha trovato gic_prepara: 2 o 3. */
int gic_versione(void);
unsigned long gic_distributore(void);   /* dove sta il distributore */

/* Collega una funzione alla linea e la accende. */
void gic_collega(unsigned int linea, gestore_linea gestore);

/* Un'interruzione e' arrivata: chiede al centralino chi e', chiama il suo gestore e dice
 * "fatto". La chiama eccezioni.c. Restituisce la linea (1023: nessuna). */
unsigned int gic_interruzione(void);

#endif
