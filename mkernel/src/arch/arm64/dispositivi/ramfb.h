/* Lo schermo "ramfb" di QEMU, parlato attraverso fw_cfg. */
#ifndef RAMFB_H
#define RAMFB_H

#include "kernel.h"   /* struct schermo */

/* Cerca fw_cfg e il file etc/ramfb; prende la memoria per i pixel (larghezza x altezza x 4),
 * la azzera, la dichiara a QEMU e riempie *s. 1 se c'e' lo schermo, 0 se no (niente fw_cfg,
 * niente etc/ramfb: QEMU senza -device ramfb). Il device tree deve essere gia' stato letto
 * (albero_prepara) e il magazziniere delle pagine pronto. */
int ramfb_prepara(struct schermo *s, unsigned int larghezza, unsigned int altezza);

#endif
