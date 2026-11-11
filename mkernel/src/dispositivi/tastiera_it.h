/* La tastiera italiana, neutra (vedi tastiera_it.c). */
#ifndef TASTIERA_IT_H
#define TASTIERA_IT_H

#define TASTO_ALTGR 100   /* KEY_RIGHTALT di Linux */

/* Un tasto e' stato premuto (premuto = 1) o rilasciato (0). 'codice' e' il codice di
 * Linux (KEY_...), che per i tasti principali e' lo scancode PS/2. Se ne esce un
 * carattere, va nella fila degli eventi. */
void tastiera_tasto(unsigned int codice, int premuto);

#endif
