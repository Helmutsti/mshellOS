/* I materiali del sistema, di UIKit (vedi LIBRERIE.md): il font del sistema (IBM Plex Sans, e
 * Inter finche' non sparisce), le icone Lucide, le icone di file e cartelle, le immagini dei
 * controlli e lo sfondo. E' COSA si disegna: il design di mshell. La scrivania e tutte le
 * app li prendono da qui, cosi' una cartella e' uguale ovunque.
 *
 * I dati stanno in materiali_dati.h, generato da strumenti/grafica/genera.py; i tipi (struct
 * font, struct icona, struct immagine) e il codice che li disegna sono di GKit
 * (gkit_disegno.h): testo() riceve un font, disegna_icona() un'icona, disegna_immagine()
 * un'immagine. Qui c'e' solo il modo di chiamare un'icona del sistema per numero. */
#ifndef MATERIALI_H
#define MATERIALI_H

#include "gkit/gkit_disegno.h"
#include "materiali_dati.h"

/* L'icona del sistema 'quale' (ICONA_...) di lato 'lato' pixel, nel colore del pennello.
 * Se quella misura non c'e', si usa la piu' vicina. */
static inline void icona(struct pennello *p, int quale, int lato, int x, int y) {
    disegna_icona(p, icone[quale], ICONA_MISURE, lato, x, y);
}

#endif
