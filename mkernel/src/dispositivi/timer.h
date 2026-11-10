/* Il timer: chiama il kernel un numero fisso di volte al secondo (un tick).
 * L'interfaccia e' la stessa su ogni CPU; su x86 il timer e' il PIT, sulla linea IRQ 0
 * (arch/x86/dispositivi/timer.c). */
#ifndef TIMER_H
#define TIMER_H

/* Imposta il timer a 'hz' tick al secondo e accende la sua interruzione. */
void timer_prepara(unsigned int hz);

/* Quanti tick sono passati dall'avvio del timer. */
unsigned int timer_tick(void);

#endif
