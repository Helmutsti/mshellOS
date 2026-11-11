/* Il timer: chiama il kernel un numero fisso di volte al secondo (un tick).
 * L'interfaccia e' la stessa su ogni CPU; su x86 il timer e' il PIT, sulla linea IRQ 0
 * (arch/x86_64/dispositivi/timer.c). */
#ifndef TIMER_H
#define TIMER_H

/* QUANTE VOLTE AL SECONDO. 250: un tick ogni 4 millisecondi. Con 100 (un tick ogni
 * 10 ms) il mouse si leggeva 100 volte al secondo e i tempi avevano 10 ms di grana:
 * troppo grossa per uno schermo a 60 Hz, dove un fotogramma dura 16,7 ms. Ogni tick
 * costa un po' di lavoro alla CPU: 250 e' un compromesso (Linux usa 100, 250 o 1000).
 * Deve dividere 1000 (100, 200, 250, 500, 1000). */
#define TIMER_HZ 250
#define MS_PER_TICK (1000 / TIMER_HZ)

/* Da millisecondi a tick (arrotondato in su) e da tick a millisecondi. */
static inline unsigned int ms_in_tick(unsigned int ms) { return (ms + MS_PER_TICK - 1) / MS_PER_TICK; }
static inline unsigned int tick_in_ms(unsigned int tick) { return tick * MS_PER_TICK; }

/* Imposta il timer a 'hz' tick al secondo e accende la sua interruzione. */
void timer_prepara(unsigned int hz);

/* Quanti tick sono passati dall'avvio del timer. */
unsigned int timer_tick(void);

/* UN LAVORETTO A OGNI TICK: la funzione verra' chiamata a ogni tick, dentro
 * l'interruzione del timer (quindi corta, e senza dormire). Serve ai driver che
 * controllano da soli se il dispositivo ha qualcosa per loro ("polling"), come quelli
 * virtio. 0 se non c'e' piu' posto. */
int timer_ogni_tick(void (*lavoro)(void));

/* La chiama il driver del timer (arch) a ogni tick: esegue i lavoretti. */
void timer_esegui_lavori(void);

#endif
