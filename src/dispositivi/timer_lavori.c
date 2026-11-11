/* I lavoretti a ogni tick (vedi timer.h). Neutro: il tick lo da' la CPU (arch), i
 * lavoretti sono gli stessi ovunque. */
#include "timer.h"

#define MAX_LAVORI 8

static void (*lavori[MAX_LAVORI])(void);
static int quanti;

int timer_ogni_tick(void (*lavoro)(void)) {
    if (quanti == MAX_LAVORI) return 0;
    lavori[quanti++] = lavoro;
    return 1;
}

void timer_esegui_lavori(void) {
    for (int i = 0; i < quanti; i++) lavori[i]();
}
