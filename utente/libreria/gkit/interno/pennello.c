/* GKit: il pennello e le forme dritte (le dichiarazioni, con cosa fa ogni funzione, sono in
 * gkit_disegno.h). */
#include "interno.h"

/* --- il pennello --- */

void pennello_prepara(struct pennello *p, struct tela *t) {
    p->tela = t;
    p->s = (struct stato_pennello){ .colore = OPACO(0), .opacita = 255, .spessore = 1,
                                    .rx = 0, .ry = 0, .rx2 = t->larghezza, .ry2 = t->altezza };
    p->quanti_salvati = 0;
}

void pennello_colore(struct pennello *p, unsigned int argb) { p->s.colore = argb; }

void pennello_opacita(struct pennello *p, int opacita) {
    p->s.opacita = p->s.opacita * opacita / 255;
}

void pennello_spessore(struct pennello *p, int spessore) {
    p->s.spessore = spessore < 1 ? 1 : spessore;
}

void pennello_ritaglia(struct pennello *p, int x, int y, int l, int a) {
    if (x > p->s.rx) p->s.rx = x;
    if (y > p->s.ry) p->s.ry = y;
    if (x + l < p->s.rx2) p->s.rx2 = x + l;
    if (y + a < p->s.ry2) p->s.ry2 = y + a;
}

void pennello_salva(struct pennello *p) {
    if (p->quanti_salvati < MAX_SALVATI) p->salvati[p->quanti_salvati++] = p->s;
}

void pennello_ripristina(struct pennello *p) {
    if (p->quanti_salvati > 0) p->s = p->salvati[--p->quanti_salvati];
}

void pulisci(struct pennello *p) {
    int x0, y0, x1, y1;
    if (!zona_da_disegnare(p, p->s.rx, p->s.ry, p->s.rx2 - p->s.rx, p->s.ry2 - p->s.ry,
                           &x0, &y0, &x1, &y1)) return;
    for (int y = y0; y < y1; y++) {
        unsigned int *riga = p->tela->pixel + y * p->tela->larghezza;
        for (int x = x0; x < x1; x++) riga[x] = 0;
    }
}

/* --- le forme dritte --- */

void riempi_rettangolo(struct pennello *p, int x, int y, int l, int a) {
    int x0, y0, x1, y1;
    if (pennello_invisibile(p) || !zona_da_disegnare(p, x, y, l, a, &x0, &y0, &x1, &y1)) return;
    unsigned int alfa, pieno = colore_coperto(p, 256, &alfa);
    for (int r = y0; r < y1; r++)
        stendi(p->tela->pixel + r * p->tela->larghezza + x0, x1 - x0, pieno, alfa);
}

void contorna_rettangolo(struct pennello *p, int x, int y, int l, int a) {
    int s = p->s.spessore;
    if (2 * s >= l || 2 * s >= a) { riempi_rettangolo(p, x, y, l, a); return; }
    riempi_rettangolo(p, x, y, l, s);
    riempi_rettangolo(p, x, y + a - s, l, s);
    riempi_rettangolo(p, x, y + s, s, a - 2 * s);
    riempi_rettangolo(p, x + l - s, y + s, s, a - 2 * s);
}

/* Bresenham (1962): si avanza di un pixel alla volta lungo la direzione piu' lunga, tenendo
 * il conto di quanto ci si allontana dalla linea vera nell'altra. */
void linea(struct pennello *p, int x1, int y1, int x2, int y2) {
    int dx = x2 > x1 ? x2 - x1 : x1 - x2, passo_x = x1 < x2 ? 1 : -1;
    int dy = y2 > y1 ? y2 - y1 : y1 - y2, passo_y = y1 < y2 ? 1 : -1;
    int errore = dx - dy;
    int s = p->s.spessore, meta = (s - 1) / 2;
    for (;;) {
        riempi_rettangolo(p, x1 - meta, y1 - meta, s, s);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * errore;
        if (e2 > -dy) { errore -= dy; x1 += passo_x; }
        if (e2 < dx)  { errore += dx; y1 += passo_y; }
    }
}
