/* GKit: il testo e le icone (le dichiarazioni, con cosa fa ogni funzione, sono in
 * gkit_disegno.h). Tutti e due sono tabelle di alfa (quanto e' coperto ogni pixel) colorate
 * con il colore del pennello. */
#include "interno.h"

const struct glifo *glifo_di(const struct font *f, unsigned char c) {
    unsigned char i = f->indice[c];
    if (!i && c != ' ') i = f->indice['?'];   /* un carattere che il font non ha */
    return &f->glifi[i];
}

int larghezza_testo(const struct font *f, const char *s) {
    int l = 0;
    for (; *s; s++) l += glifo_di(f, (unsigned char)*s)->avanzo;
    return l;
}

int testo(struct pennello *p, const struct font *f, int x, int y, const char *s) {
    int base = y + f->ascesa;
    int invisibile = pennello_invisibile(p);
    for (; *s; s++) {
        const struct glifo *g = glifo_di(f, (unsigned char)*s);
        const unsigned char *alfa = f->alfa + g->inizio;
        int gx = x + g->x, gy = base - g->y;
        int x0, y0, x1, y1;
        /* solo la parte della lettera dentro il ritaglio (una lettera fuori: niente) */
        if (!invisibile && zona_da_disegnare(p, gx, gy, g->l, g->a, &x0, &y0, &x1, &y1))
            for (int r = y0; r < y1; r++) {
                unsigned int *d = p->tela->pixel + r * p->tela->larghezza + x0;
                const unsigned char *v = alfa + (r - gy) * g->l + (x0 - gx);
                for (int k = 0; k < x1 - x0; k++)
                    if (v[k]) copri_pixel(p, &d[k], v[k] + (v[k] >> 7));
            }
        x += g->avanzo;
    }
    return x;
}

void disegna_icona(struct pennello *p, const struct icona *misure, int quante, int lato, int x, int y) {
    const struct icona *scelta = &misure[0];
    for (int i = 1; i < quante; i++)
        if (misure[i].lato <= lato) scelta = &misure[i];
    int l = scelta->lato;
    x += (lato - l) / 2;
    y += (lato - l) / 2;
    int x0, y0, x1, y1;
    if (pennello_invisibile(p) || !zona_da_disegnare(p, x, y, l, l, &x0, &y0, &x1, &y1)) return;
    for (int r = y0; r < y1; r++) {
        unsigned int *d = p->tela->pixel + r * p->tela->larghezza + x0;
        const unsigned char *v = scelta->alfa + (r - y) * l + (x0 - x);
        for (int k = 0; k < x1 - x0; k++)
            if (v[k]) copri_pixel(p, &d[k], v[k] + (v[k] >> 7));
    }
}
