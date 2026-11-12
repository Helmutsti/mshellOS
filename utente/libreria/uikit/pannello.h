/* IL PANNELLO DEL CONTENUTO, dal file Figma (la finestra di Files: "Area principale" ->
 * "Margine riquadro" -> "Pannello contenuto"). E' l'eccezione decisa per Files alla struttura
 * della finestra: il contenuto non sta sul vetro ma in un pannello chiaro, SUPERFICIE_ELEVATA,
 * raggio RAGGIO_PANNELLO, staccato dal resto da un margine di SPAZIO_8 a sinistra, a destra e
 * sotto (sopra no: la barra degli strumenti ha gia' il suo padding). Il pannello non ha padding
 * suo: lo spazio dentro lo danno i suoi figli (la tabella, la griglia, lo stato vuoto).
 *
 * I figli stanno uno sopra l'altro, tutti grandi quanto il pannello: se ne mostra uno alla
 * volta (gli altri nascosti), come le pagine. */
#ifndef PANNELLO_H
#define PANNELLO_H

#define PANNELLO_MARGINE SPAZIO_8

struct pannello_tk { struct widget base; };

static inline void pannello_misura(struct widget *w, int *l, int *a) { (void)w; *l = 0; *a = 0; }

/* Il pannello vero, dentro il margine. */
static inline void pannello_dentro(const struct widget *w, int *x, int *y, int *l, int *a) {
    *x = w->x + PANNELLO_MARGINE;
    *y = w->y;
    *l = w->l - 2 * PANNELLO_MARGINE;
    *a = w->a - PANNELLO_MARGINE;
}

static inline void pannello_disponi(struct widget *w) {
    int x, y, l, a;
    pannello_dentro(w, &x, &y, &l, &a);
    for (int i = 0; i < w->quanti_figli; i++) {
        struct widget *f = w->figli[i];
        f->x = x; f->y = y; f->l = l; f->a = a;
    }
}

static inline void pannello_disegna(struct widget *w, struct pennello *p) {
    int x, y, l, a;
    pannello_dentro(w, &x, &y, &l, &a);
    pennello_colore(p, SUPERFICIE_ELEVATA);
    riempi_tondo(p, x, y, l, a, RAGGIO_PANNELLO);
}

static const struct tipo_widget tipo_pannello = { pannello_misura, pannello_disponi, pannello_disegna, 0, 0, 0, 0, 0, 0 };

/* Elastico e largo: prende tutto lo spazio che avanza. */
static inline void pannello_prepara(struct pannello_tk *p) {
    *p = (struct pannello_tk){ .base = { .tipo = &tipo_pannello, .elastico = 1, .largo = 1 } };
}

#endif
