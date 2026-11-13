/* L'ELEMENTO DI UN FILE nella vista a icone: l'icona grande e sotto il nome, come nel
 * disegno Figma ("Elemento file"). Lo usano Files (le cartelle e i file di una cartella)
 * e la scrivania (le sue icone in alto a sinistra: oggi la sola "Shell", provvisoria):
 * stesso disegno, stesse misure.
 *
 *     +--------------+   largo ELEMENTO_LARGHEZZA (84), angoli di 10
 *     |   [icona]    |   l'icona (56) in alto, a 8 dal bordo
 *     |  nome del    |   il nome sotto, centrato, al massimo su due righe; se non ci sta
 *     |  file lun... |   la seconda finisce con "..."
 *     +--------------+
 *
 * Gli stati: normale (niente fondo), sotto il puntatore (ELEMENTO_SOPRA, appena velato),
 * scelto (ELEMENTO_SCELTO, vetro chiaro). L'icona e' un'immagine a colori dei materiali del
 * sistema (gli oggetti, oggetti.h): i file sono "contenuto", non icone dell'interfaccia. */
#ifndef ELEMENTO_FILE_H
#define ELEMENTO_FILE_H

#include "stile.h"

#define ELEMENTO_NORMALE 0
#define ELEMENTO_STATO_SOPRA  1
#define ELEMENTO_STATO_SCELTO 2

/* Quante righe prende il nome sotto l'icona (1 o 2). */
int elemento_righe(const char *nome);

/* L'altezza dell'elemento con quel nome: margine, icona, spazio, le righe, margine. */
int elemento_altezza(const char *nome);

/* Il fondo dell'elemento (sotto il puntatore o scelto), alto 'alto'. */
void elemento_fondo(struct pennello *p, int x, int y, int alto, int stato);

/* L'icona dell'elemento con l'angolo in (x, y). */
void elemento_icona(struct pennello *p, int x, int y, const struct immagine *icona);

/* Dove comincia il nome (la sua prima riga). */
int elemento_nome_y(int y);

/* Il nome sotto l'icona, centrato, su una o due righe. */
void elemento_nome(struct pennello *p, int x, int y, const char *nome);

/* Tutto l'elemento: fondo, icona e nome. 'alto' e' l'altezza del suo posto (in una riga
 * di elementi, quella del piu' alto): il fondo la prende tutta. Con un oggetto (oggetti.h)
 * al posto dell'immagine: elemento_disegna_oggetto, in oggetti.h. */
void elemento_disegna(struct pennello *p, int x, int y, int alto,
                      const struct immagine *icona, const char *nome, int stato);

#endif
