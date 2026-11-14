/* UIKit, il nucleo: il suggerimento (tooltip). Un widget con un testo di suggerimento
 * (widget_suggerimento) lo mostra dopo SUGGERIMENTO_ATTESA ms col puntatore fermo sopra; sparisce
 * quando il puntatore esce o si preme. Le dichiarazioni sono in uikit.h e interno/nucleo.h. */
#include "uikit/uikit.h"
#include "uikit/interno/nucleo.h"

void widget_suggerimento(struct widget *w, const char *testo, const char *scorciatoia) {
    w->suggerimento = testo;
    w->scorciatoia = scorciatoia;
}

/* Quanto e' largo il suggerimento di 'w', e dove sta (x, y), dentro la finestra. */
static void suggerimento_misure(const struct finestra_tk *f, const struct widget *w, int *x, int *y, int *l) {
    int tl = larghezza_testo(FONT_PICCOLO_NUOVO, w->suggerimento);
    *l = 2 * SUGGERIMENTO_PADDING_X + tl;
    if (w->scorciatoia && w->scorciatoia[0])
        *l += SUGGERIMENTO_GAP + larghezza_testo(FONT_PICCOLO_NUOVO, w->scorciatoia);
    if (*l > SUGGERIMENTO_MAX) *l = SUGGERIMENTO_MAX;
    *x = w->x + (w->l - *l) / 2;
    if (*x + *l > f->base.l - SPAZIO_4) *x = f->base.l - SPAZIO_4 - *l;
    if (*x < SPAZIO_4) *x = SPAZIO_4;
    *y = w->y + w->a + SUGGERIMENTO_DISTANZA;
    if (*y + SUGGERIMENTO_ALTEZZA > f->base.a - SPAZIO_4)             /* non ci sta sotto: sopra */
        *y = w->y - SUGGERIMENTO_DISTANZA - SUGGERIMENTO_ALTEZZA;
}

static void suggerimento_segna(struct finestra_tk *f) {
    int s = ombra_sporgenza(OMBRA_TAB(sovrapposto), OMBRA_N(sovrapposto));
    widget_cambiato_zona(&f->base, f->sugg_x - s, f->sugg_y - s, f->sugg_l + 2 * s,
                         SUGGERIMENTO_ALTEZZA + 2 * s);
}

static void suggerimento_scatta(struct finestra_tk *f, void *dati) {
    (void)dati;
    finestra_tk_ogni(f, 0, suggerimento_scatta, f);     /* una volta sola */
    struct widget *w = f->sotto;
    if (!w || !w->suggerimento || finestra_tk_bloccata(f)) return;
    suggerimento_misure(f, w, &f->sugg_x, &f->sugg_y, &f->sugg_l);
    f->sugg_visibile = 1;
    suggerimento_segna(f);
}

void suggerimento_ferma(struct finestra_tk *f) {
    finestra_tk_ogni(f, 0, suggerimento_scatta, f);
    if (f->sugg_visibile) {
        f->sugg_visibile = 0;
        suggerimento_segna(f);
    }
}

void suggerimento_sotto(struct finestra_tk *f, struct widget *w) {
    suggerimento_ferma(f);
    if (w && w->suggerimento) finestra_tk_ogni(f, SUGGERIMENTO_ATTESA, suggerimento_scatta, f);
}

void suggerimento_disegna_forma(struct pennello *p, int x, int y, int l, const char *testo,
                                const char *scorciatoia) {
    struct tondo t = tondo(x, y, l, SUGGERIMENTO_ALTEZZA, RAGGIO_CASELLA);
    ombre_strati_fuori(p, &t, OMBRA_TAB(sovrapposto), OMBRA_N(sovrapposto));
    pennello_salva(p);
    pennello_colore(p, SUGGERIMENTO_SFONDO);
    riempi_forma(p, &t);
    ombre_strati_dentro(p, &t, OMBRA_TAB(sovrapposto), OMBRA_N(sovrapposto));
    int tx = x + SUGGERIMENTO_PADDING_X, fine = x + l - SUGGERIMENTO_PADDING_X;
    int sl = scorciatoia && scorciatoia[0] ? larghezza_testo(FONT_PICCOLO_NUOVO, scorciatoia) : 0;
    pennello_colore(p, SUGGERIMENTO_TESTO);
    testo_tagliato(p, FONT_PICCOLO_NUOVO, tx, y, SUGGERIMENTO_ALTEZZA,
                   fine - tx - (sl ? sl + SUGGERIMENTO_GAP : 0), testo, 0);
    if (sl) {
        pennello_colore(p, SUGGERIMENTO_TESTO_SECONDARIO);
        testo_centrato(p, FONT_PICCOLO_NUOVO, fine - sl, y, SUGGERIMENTO_ALTEZZA, scorciatoia);
    }
    pennello_ripristina(p);
}

void suggerimento_disegna(struct finestra_tk *f, struct pennello *p) {
    if (!f->sugg_visibile || !f->sotto || !f->sotto->suggerimento) return;
    suggerimento_disegna_forma(p, f->sugg_x, f->sugg_y, f->sugg_l, f->sotto->suggerimento,
                               f->sotto->scorciatoia);
}
