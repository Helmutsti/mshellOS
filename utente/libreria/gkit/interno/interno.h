/* GKit, INTERNO: quello che i .c degli strumenti di disegno si dicono tra loro (gkit.h spiega
 * i livelli). Nessun programma lo include.
 *
 * Sono i passi di ogni disegno, pixel per pixel: tagliare al ritaglio del pennello, coprire un
 * pixel con il colore del pennello, stendere una striscia di pixel tutti coperti. Sono
 * 'static inline' per la stessa ragione dei conti di sistema/pixel.h: si chiamano per ogni
 * pixel, e una chiamata vera costerebbe piu' del conto. */
#ifndef GKIT_INTERNO_H
#define GKIT_INTERNO_H

#include "gkit/gkit_disegno.h"
#include "gkit/sistema/pixel.h"

/* Come copri (qui sotto), ma su un pixel gia' trovato e gia' dentro il ritaglio: lo usano
 * i disegni che hanno gia' tagliato i loro giri al ritaglio, per non controllarlo di nuovo
 * a ogni pixel. */
static inline void copri_pixel(const struct pennello *p, unsigned int *d, int copertura) {
    if (copertura <= 0) return;
    if (copertura > 256) copertura = 256;
    unsigned int a, premolt = colore_coperto(p, copertura, &a);
    if (!a) return;
    *d = sopra_a(premolt, *d);
}

/* Il colore del pennello, coperto per 'copertura' (0-256), sul pixel (x, y). TUTTO il
 * disegno passa da qui (o da copri_pixel, che e' lo stesso conto): ritaglio, opacita',
 * trasparenza. */
static inline void copri(struct pennello *p, int x, int y, int copertura) {
    if (copertura <= 0 || x < p->s.rx || x >= p->s.rx2 || y < p->s.ry || y >= p->s.ry2) return;
    copri_pixel(p, &p->tela->pixel[y * p->tela->larghezza + x], copertura);
}

/* UNA STRISCIA di 'n' pixel tutti coperti per intero: e' copri con copertura 256 su ogni
 * pixel, ma il colore si calcola una volta sola ('premolt' e 'a' vengono da
 * colore_coperto(p, 256, ...)). E' la parte piu' grande di quasi ogni disegno: l'interno
 * di un rettangolo, di un vetro, di un pulsante. Due scorciatoie, con lo stesso
 * risultato di sopra_a: un colore opaco copre e basta, e il conto di sopra_a si fa su
 * due byte alla volta (sopra_a_vero, in sistema/pixel.h). */
static inline void stendi(unsigned int *d, int n, unsigned int premolt, unsigned int a) {
    if (!a || n <= 0) return;
    unsigned int alfa = premolt >> 24;
    if (alfa == 0) return;                       /* sopra_a restituirebbe 'sotto' */
    if (alfa == 255) {
        for (int i = 0; i < n; i++) d[i] = premolt;
        return;
    }
    /* due byte alla volta, se il colore e' un premoltiplicato vero; se no (un'opacita' del
     * pennello fuori misura), sopra_a */
    if (!premoltiplicato_vero(premolt)) {
        for (int i = 0; i < n; i++) d[i] = sopra_a(premolt, d[i]);
        return;
    }
    for (int i = 0; i < n; i++) d[i] = sopra_a_vero(premolt, d[i]);
}

/* LA ZONA DA DISEGNARE: il rettangolo (x, y, l, a) tagliato al ritaglio del pennello (e
 * alla tela, per sicurezza). In (*x0, *y0) - (*x1, *y1) i pixel da guardare, estremi
 * x1 e y1 esclusi. 0 se non resta niente. Ogni disegno guarda solo questi pixel: quando
 * si ridisegna un pezzetto di finestra (il ritaglio stretto attorno a quello che e'
 * cambiato), una forma grande costa solo per il pezzetto. */
static inline int zona_da_disegnare(const struct pennello *p, int x, int y, int l, int a,
                                    int *x0, int *y0, int *x1, int *y1) {
    int xa = x, ya = y, xb = x + l, yb = y + a;
    if (xa < p->s.rx) xa = p->s.rx;
    if (ya < p->s.ry) ya = p->s.ry;
    if (xb > p->s.rx2) xb = p->s.rx2;
    if (yb > p->s.ry2) yb = p->s.ry2;
    if (xa < 0) xa = 0;
    if (ya < 0) ya = 0;
    if (xb > p->tela->larghezza) xb = p->tela->larghezza;
    if (yb > p->tela->altezza) yb = p->tela->altezza;
    *x0 = xa; *y0 = ya; *x1 = xb; *y1 = yb;
    return xa < xb && ya < yb;
}

/* Un pennello che non lascia segni: colore del tutto trasparente, o opacita' 0. (Per
 * copri l'opacita' 'a' viene 0 qualunque sia la copertura.) */
static inline int pennello_invisibile(const struct pennello *p) {
    return (p->s.colore >> 24) == 0 || p->s.opacita == 0;
}

#endif
