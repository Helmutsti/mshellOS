/* Per le scene dei widget: un PIANO, cioe' un pannello di SUPERFICIE_ELEVATA (con un margine
 * di 20 pixel) da mettere sotto i widget. La finestra e' ancora quella di vetro del vecchio
 * stile (il suo rifacimento e' un altro lavoro), e su un vetro gli incassati e le ombre non si
 * leggono; sul piano si vede come sara' sulla superficie vera. Uso: piano_prepara(&piano,
 * &contenuto.base), e la scena passa &piano.base alla finestra. */
#ifndef PIANO_H
#define PIANO_H

#define PIANO_MARGINE 20

struct piano { struct widget base; };

static void piano_misura(struct widget *w, int *l, int *a) {
    widget_misura(w->figli[0], l, a);
    *l += 2 * PIANO_MARGINE;
    *a += 2 * PIANO_MARGINE;
}
static void piano_disponi(struct widget *w) {
    struct widget *f = w->figli[0];
    int l, a;
    widget_misura(f, &l, &a);
    f->x = w->x + PIANO_MARGINE; f->y = w->y + PIANO_MARGINE;
    f->l = w->l - 2 * PIANO_MARGINE; f->a = w->a - 2 * PIANO_MARGINE;
    if (!f->largo) { f->l = l; f->a = a; }
}
static void piano_disegna(struct widget *w, struct pennello *p) {
    pennello_colore(p, SUPERFICIE_ELEVATA);
    pennello_ritaglia(p, w->x, w->y, w->l, w->a);
    riempi_tondo(p, w->x, w->y, w->l, w->a, 0);
}
static const struct tipo_widget tipo_piano = { piano_misura, piano_disponi, piano_disegna, 0, 0, 0, 0, 0 };
static void piano_prepara(struct piano *pi, struct widget *contenuto) {
    *pi = (struct piano){ .base = { .tipo = &tipo_piano, .largo = 1 } };
    widget_aggiungi(&pi->base, contenuto);
}
#endif
