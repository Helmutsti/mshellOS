/* Il timer (PIT): chiama il kernel un numero fisso di volte al secondo, sulla linea IRQ 0. */
#ifndef TIMER_H
#define TIMER_H

/* Imposta il timer a 'hz' tick al secondo e apre la sua linea sul PIC. */
void timer_prepara(unsigned int hz);

/* Quanti tick sono passati dall'avvio del timer. */
unsigned int timer_tick(void);

#endif
