/* GKit, DEL SISTEMA: lo schermo, il lato del compositore (gkit.h spiega i livelli). Lo usa
 * solo chi possiede lo schermo: il compositore, e init, che lo apre per lui e glielo presta.
 * Le funzioni sono in gkit/interno/schermo.c (il DRM di Linux); nella sandbox Docker le
 * sostituisce strumenti/sandbox/schermo_sandbox.c, con gli stessi nomi. I tipi info_schermo e
 * zona_schermo sono in corekit/chiamate.h. */
#ifndef GKIT_SISTEMA_SCHERMO_H
#define GKIT_SISTEMA_SCHERMO_H

#include "corekit/corekit.h"

/* Apre lo schermo (la scheda video: /dev/dri/card0) e ne diventa il padrone. Il descrittore,
 * o -1 se manca o se e' gia' di un altro programma. */
int apri_schermo(void);

/* Mette l'immagine dello schermo del descrittore 'd' nella memoria del programma:
 * scrivere li' un numero (0x00RRGGBB) vuol dire accendere un pixel. 'info' dice com'e' fatta
 * (larghezza, altezza, byte per riga). L'indirizzo, o 0 se non riesce. */
void *mappa_schermo(int d, struct info_schermo *info);

/* Dice che nelle 'quante' zone dello schermo si e' scritto qualcosa di nuovo (le schede
 * video virtuali, come quella di QEMU, copiano solo quelle). 0, o -1 se la scheda rifiuta. */
int schermo_cambiato(int d, const struct zona_schermo *zone, int quante);

#endif
