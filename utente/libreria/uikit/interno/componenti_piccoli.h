/* UIKit, INTERNO: le funzioni dei componenti piccoli che servono ad altri componenti, ma non
 * alle applicazioni. Nessuna applicazione lo include.
 *
 * Sono i componenti che si disegnano anche DENTRO un altro, senza essere un widget a parte:
 * l'etichetta di stato in fondo alla riga di permesso, l'icona dell'app all'inizio della riga
 * dell'app. Un'applicazione usa invece i widget (etichetta_stato_prepara, icona_app_prepara).
 *
 * Divise per il file che le definisce. */
#ifndef UIKIT_COMPONENTI_PICCOLI_H
#define UIKIT_COMPONENTI_PICCOLI_H

#include "../uikit.h"

/* --- etichetta_stato.c --- */

/* Quanto e' larga l'etichetta con questo testo: il testo in FONT_ETICHETTA e SPAZIO_8 per
 * lato. L'altezza e' sempre ETICHETTA_STATO_ALTEZZA. */
int etichetta_stato_larghezza(const char *testo);

/* Disegna l'etichetta del tipo 'tipo' (ETICHETTA_...) con l'angolo in alto a sinistra in
 * (x, y). */
void etichetta_stato_disegna_in(struct pennello *p, int x, int y, int tipo, const char *testo);

/* --- icona_app.c --- */

/* Disegna l'icona dell'app (oggi il segnaposto), un quadrato di lato 'lato' con l'angolo in
 * alto a sinistra in (x, y). */
void icona_app_disegna_in(struct pennello *p, int x, int y, int lato);

#endif
