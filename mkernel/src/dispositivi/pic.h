/* Il PIC: il centralino che passa alla CPU le interruzioni dei dispositivi (IRQ 0-15). */
#ifndef PIC_H
#define PIC_H

#define IRQ_BASE 32   /* IRQ 0 arriva sulla riga 32 della IDT, IRQ 15 sulla 47 */

#define IRQ_TIMER    0
#define IRQ_TASTIERA 1
#define IRQ_MOUSE    12

/* Riprogramma i due PIC per usare le righe 32-47 e chiude tutte le linee. */
void pic_prepara(void);

/* Lascia passare la linea irq. */
void pic_apri(int irq);

/* Il "ricevuto" (EOI): va mandato alla fine di ogni interruzione hardware. */
void pic_ricevuto(int irq);

#endif
