/* Il mouse PS/2, sulla linea IRQ 12. Spostamenti e pulsanti finiscono nella fila
 * degli eventi (eventi.h). */
#ifndef MOUSE_H
#define MOUSE_H

/* Accende il mouse, collega il gestore e apre la linea sul PIC. 0 se il mouse non risponde. */
int mouse_prepara(void);

#endif
