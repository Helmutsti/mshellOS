/* UIKit, i componenti piccoli: l'icona di un'app (oggi il segnaposto). Si disegna come widget,
 * o dentro un altro componente (la riga dell'app). Le dichiarazioni sono in icona_app.h e in
 * interno/componenti_piccoli.h. */
#include "uikit/uikit.h"
#include "uikit/interno/componenti_piccoli.h"

void icona_app_disegna_in(struct pennello *p, int x, int y, int lato) {
    pennello_colore(p, CONTROLLO_SFONDO_SOPRA);
    riempi_tondo(p, x, y, lato, lato, (lato * 22 + 48) / 96);
    int simbolo = lato * 20 / 32;
    pennello_colore(p, COLORE_ICONA(COLORE_ICONA_SECONDARIO));
    icona(p, ICONA_PACKAGE, simbolo, x + (lato - simbolo) / 2, y + (lato - simbolo) / 2);
}

static void icona_app_misura(struct widget *w, int *l, int *a) {
    *l = *a = ((struct icona_app *)w)->lato;
}

static void icona_app_disegna(struct widget *w, struct pennello *p) {
    icona_app_disegna_in(p, w->x, w->y, ((struct icona_app *)w)->lato);
}

static const struct tipo_widget tipo_icona_app = { icona_app_misura, 0, icona_app_disegna, 0, 0, 0, 0, 0, 0 };

void icona_app_prepara(struct icona_app *i, int lato) {
    *i = (struct icona_app){ .base = { .tipo = &tipo_icona_app }, .lato = lato };
}
