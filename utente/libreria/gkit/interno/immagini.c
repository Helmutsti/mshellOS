/* GKit: disegnare e rimpicciolire le immagini a colori (le dichiarazioni, con cosa fa ogni
 * funzione, sono in gkit_disegno.h). Leggerle da un file e' in immagine_file.c (le PNG) e
 * materiali_file.c (le .image). */
#include "interno.h"

/* Ogni pixel dell'immagine e' premoltiplicato: si torna al colore non premoltiplicato, lo si
 * mette nel pennello e lo si copre per la sua opacita' (per 'm', la parte del pixel dentro la
 * forma, nelle due funzioni con il bordo arrotondato). Cosi' passa da copri_pixel come tutto
 * il resto: ritaglio e opacita' del pennello valgono anche qui. */

void disegna_immagine(struct pennello *p, const struct immagine *im, int x, int y) {
    int x0, y0, x1, y1;
    if (p->s.opacita == 0 || !zona_da_disegnare(p, x, y, im->l, im->a, &x0, &y0, &x1, &y1)) return;
    unsigned int colore = p->s.colore;
    for (int r = y0; r < y1; r++) {
        unsigned int *d = p->tela->pixel + r * p->tela->larghezza + x0;
        const unsigned int *sorgente = im->pixel + (r - y) * im->l + (x0 - x);
        for (int k = 0; k < x1 - x0; k++) {
            unsigned int px = sorgente[k];
            unsigned int a = px >> 24;
            if (!a) continue;
            unsigned int rr = ((px >> 16) & 0xFF) * 255 / a, gg = ((px >> 8) & 0xFF) * 255 / a,
                         bb = (px & 0xFF) * 255 / a;
            p->s.colore = OPACO(rr << 16 | gg << 8 | bb);
            copri_pixel(p, &d[k], a + (a >> 7));
        }
    }
    p->s.colore = colore;
}

void disegna_immagine_tonda(struct pennello *p, const struct immagine *im, int x, int y, int lato) {
    int ix = x + (lato - im->l) / 2, iy = y + (lato - im->a) / 2;
    int d = im->l < im->a ? im->l : im->a;
    struct tondo t = tondo(ix + (im->l - d) / 2, iy + (im->a - d) / 2, d, d, d / 2);
    int x0, y0, x1, y1;
    if (p->s.opacita == 0 || !zona_da_disegnare(p, ix, iy, im->l, im->a, &x0, &y0, &x1, &y1)) return;
    unsigned int colore = p->s.colore;
    for (int r = y0; r < y1; r++) {
        unsigned int *d = p->tela->pixel + r * p->tela->larghezza + x0;
        const unsigned int *sorgente = im->pixel + (r - iy) * im->l + (x0 - ix);
        for (int k = 0; k < x1 - x0; k++) {
            unsigned int px = sorgente[k];
            unsigned int a = px >> 24;
            if (!a) continue;
            int m = coperto(distanza(&t, x0 + k, r));
            if (!m) continue;
            unsigned int rr = ((px >> 16) & 0xFF) * 255 / a, gg = ((px >> 8) & 0xFF) * 255 / a,
                         bb = (px & 0xFF) * 255 / a;
            p->s.colore = OPACO(rr << 16 | gg << 8 | bb);
            copri_pixel(p, &d[k], (int)((a + (a >> 7)) * m >> 8));
        }
    }
    p->s.colore = colore;
}

void disegna_immagine_arrotondata(struct pennello *p, const struct immagine *im, int x, int y,
                                  int raggio) {
    struct tondo t = tondo(x, y, im->l, im->a, raggio);
    int x0, y0, x1, y1;
    if (p->s.opacita == 0 || !zona_da_disegnare(p, x, y, im->l, im->a, &x0, &y0, &x1, &y1)) return;
    unsigned int colore = p->s.colore;
    for (int r = y0; r < y1; r++) {
        unsigned int *d = p->tela->pixel + r * p->tela->larghezza + x0;
        const unsigned int *sorgente = im->pixel + (r - y) * im->l + (x0 - x);
        for (int k = 0; k < x1 - x0; k++) {
            unsigned int px = sorgente[k];
            unsigned int a = px >> 24;
            if (!a) continue;
            int m = coperto(distanza(&t, x0 + k, r));
            if (!m) continue;
            unsigned int rr = ((px >> 16) & 0xFF) * 255 / a, gg = ((px >> 8) & 0xFF) * 255 / a,
                         bb = (px & 0xFF) * 255 / a;
            p->s.colore = OPACO(rr << 16 | gg << 8 | bb);
            copri_pixel(p, &d[k], (int)((a + (a >> 7)) * m >> 8));
        }
    }
    p->s.colore = colore;
}

/* --- rimpicciolire un'immagine --- */

/* Ogni pixel nuovo e' la media dei pixel di 'da' che copre, pesati per quanto ne copre (i
 * bordi a frazioni di pixel contano a meta', a un terzo...), in 256esimi di pixel. */
struct immagine immagine_rimpicciolita(const struct immagine *da, unsigned int *pixel, int l, int a) {
    struct immagine im = { (unsigned short)l, (unsigned short)a, pixel };
    if (da->l < 1 || da->a < 1) {
        for (int i = 0; i < l * a; i++) pixel[i] = 0;
        return im;
    }
    /* la finestra di 'da' che si usa, in 256esimi di pixel: tutta la larghezza o tutta l'altezza */
    long long fl, fa;
    if ((long long)l * da->a >= (long long)a * da->l) {   /* il rettangolo e' piu' largo: si taglia sopra e sotto */
        fl = (long long)da->l * 256;
        fa = (long long)a * da->l * 256 / l;
    } else {                                                /* piu' alto: si taglia a destra e a sinistra */
        fa = (long long)da->a * 256;
        fl = (long long)l * da->a * 256 / a;
    }
    long long ox = ((long long)da->l * 256 - fl) / 2, oy = ((long long)da->a * 256 - fa) / 2;
    for (int y = 0; y < a; y++) {
        long long sy0 = oy + y * fa / a, sy1 = oy + (y + 1) * fa / a;
        for (int x = 0; x < l; x++) {
            long long sx0 = ox + x * fl / l, sx1 = ox + (x + 1) * fl / l;
            unsigned long long somma[4] = { 0, 0, 0, 0 }, peso = 0;
            for (long long j = sy0 >> 8; j <= (sy1 - 1) >> 8 && j < da->a; j++) {
                long long py = (sy1 < (j + 1) * 256 ? sy1 : (j + 1) * 256) - (sy0 > j * 256 ? sy0 : j * 256);
                for (long long i = sx0 >> 8; i <= (sx1 - 1) >> 8 && i < da->l; i++) {
                    long long px = (sx1 < (i + 1) * 256 ? sx1 : (i + 1) * 256) - (sx0 > i * 256 ? sx0 : i * 256);
                    unsigned long long w = (unsigned long long)(px * py);
                    unsigned int c = da->pixel[j * da->l + i];
                    for (int k = 0; k < 4; k++) somma[k] += ((c >> (k * 8)) & 0xFF) * w;
                    peso += w;
                }
            }
            unsigned int c = 0;
            if (peso)
                for (int k = 0; k < 4; k++)
                    c |= (unsigned int)((somma[k] + peso / 2) / peso) << (k * 8);
            pixel[y * l + x] = c;
        }
    }
    return im;
}
