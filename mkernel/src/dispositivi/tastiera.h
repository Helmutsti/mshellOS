/* La tastiera PS/2, sulla linea IRQ 1. I tasti premuti finiscono nella fila degli
 * eventi (eventi.h). */
#ifndef TASTIERA_H
#define TASTIERA_H

/* Collega il gestore e apre la linea sul PIC. */
void tastiera_prepara(void);

#endif
