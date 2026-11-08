/* La tastiera PS/2, sulla linea IRQ 1. */
#ifndef TASTIERA_H
#define TASTIERA_H

/* Collega il gestore e apre la linea sul PIC. */
void tastiera_prepara(void);

/* Il prossimo carattere scritto, oppure 0 se non ce ne sono. Non aspetta. */
char tastiera_leggi(void);

/* Il prossimo carattere scritto: se non ce ne sono, il compito dorme finche' non arriva. */
char tastiera_aspetta(void);

#endif
