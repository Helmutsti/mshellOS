/* UIKit, i componenti: il pannello chiaro incassato del contenuto. Le dichiarazioni sono in
 * pannello.h. */
#include "uikit/uikit.h"
#include "uikit/interno/nucleo.h"

static void pannello_misura(struct widget *w, int *l, int *a) { (void)w; *l = 0; *a = 0; }

static void pannello_disponi(struct widget *w) {
    int d = ((struct pannello_tk *)w)->padding;
    for (int i = 0; i < w->quanti_figli; i++) {
        struct widget *f = w->figli[i];
        f->x = w->x + d; f->y = w->y + d; f->l = w->l - 2 * d; f->a = w->a - 2 * d;
        for (int k = 0; k < f->quanti_figli; k++)   /* la sua barra, nel padding destro */
            if (f->figli[k]->tipo == &tipo_barra_scorrimento) {
                ((struct barra_scorrimento *)f->figli[k])->fuori = d;
                f->l += d;
            }
    }
}

static void pannello_disegna(struct widget *w, struct pennello *p) {
    pennello_colore(p, SUPERFICIE_ELEVATA);
    riempi_tondo(p, w->x, w->y, w->l, w->a, RAGGIO_PANNELLO);
}

static const struct tipo_widget tipo_pannello = { pannello_misura, pannello_disponi, pannello_disegna, 0, 0, 0, 0, 0, 0 };

void pannello_prepara(struct pannello_tk *p, int padding) {
    *p = (struct pannello_tk){ .base = { .tipo = &tipo_pannello, .elastico = 1, .largo = 1 }, .padding = padding };
}
