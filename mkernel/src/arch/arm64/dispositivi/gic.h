/* Il GIC: il centralino delle interruzioni di ARM. */
#ifndef GIC_H
#define GIC_H

#define GIC_MAX_LINEE 128   /* le linee che gestiamo (0-15 software, 16-31 della CPU, 32+ dispositivi) */

typedef void (*gestore_linea)(void);

/* Accende il centralino (distributore e interfaccia della CPU). */
void gic_prepara(void);

/* Quale versione ha trovato gic_prepara: 2 o 3. */
int gic_versione(void);

/* Collega una funzione alla linea e la accende. */
void gic_collega(unsigned int linea, gestore_linea gestore);

/* Un'interruzione e' arrivata: chiede al centralino chi e', chiama il suo gestore e dice
 * "fatto". La chiama eccezioni.c. */
void gic_interruzione(void);

#endif
