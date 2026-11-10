/* Lo schermo come cosa aperta (vedi chiamate.h, apri_schermo e mappa_schermo). */
#ifndef SCHERMO_H
#define SCHERMO_H

struct aperto;

/* Lo schermo, preso alla console. 0 se e' gia' aperto (lo tiene qualcun altro). */
struct aperto *schermo_apri(void);

/* E' lo schermo? */
int e_lo_schermo(struct aperto *a);

#endif
