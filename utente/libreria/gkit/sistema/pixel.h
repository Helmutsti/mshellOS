/* GKit, DEL SISTEMA: i conti che si fanno PER OGNI PIXEL (gkit.h spiega i livelli).
 *
 * Mettere un colore premoltiplicato sopra un altro, e misurare quanto un pixel sta dentro una
 * forma arrotondata. Li usano gli strumenti di disegno di GKit (gkit/interno/) per ogni pixel
 * che toccano, e il compositore, che compone le finestre sullo schermo e disegna da se' le
 * ombre e gli angoli arrotondati delle finestre. Un'applicazione non li usa: disegna con le
 * funzioni di gkit_disegno.h, che li chiamano per lei.
 *
 * Sono funzioni 'static inline', cioe' il loro codice si copia dentro chi le chiama: si
 * chiamano milioni di volte per ogni immagine, e una chiamata vera (salvare i registri,
 * saltare, tornare) costerebbe piu' del conto stesso. Per questo stanno in un header e non
 * in un .c, come le macro di un sistema vero (pixman, la libreria dei pixel di Linux, fa lo
 * stesso con i suoi conti per pixel).
 *
 * Tutto con numeri interi, in 256esimi di pixel e in 255esimi di opacita' (vedi
 * gkit_disegno.h, "i bordi lisci"). */
#ifndef GKIT_SISTEMA_PIXEL_H
#define GKIT_SISTEMA_PIXEL_H

#include "gkit/gkit_disegno.h"

/* --- mettere un colore su un pixel --- */

/* x / 255, arrotondato, senza dividere. Per x fino a 255 * 255 + 255. */
static inline unsigned int diviso_255(unsigned int x) { return (x + 128 + ((x + 128) >> 8)) >> 8; }

/* Un pixel premoltiplicato 'sopra' messo su 'sotto': sopra + sotto * (1 - opacita' di sopra),
 * byte per byte. Restituisce il pixel che ne viene. */
static inline unsigned int sopra_a(unsigned int sopra, unsigned int sotto) {
    unsigned int resto = 255 - (sopra >> 24);
    if (resto == 255) return sotto;
    if (resto == 0) return sopra;
    /* sotto vuoto: ogni byte e' quello di sopra piu' diviso_255(0), cioe' piu' 0 */
    if (!sotto) return sopra;
    unsigned int r = 0;
    for (int b = 0; b < 32; b += 8)
        r |= (((sopra >> b) & 0xFF) + diviso_255(((sotto >> b) & 0xFF) * resto)) << b;
    return r;
}

/* 1 se 'p' e' un premoltiplicato VERO: nessun colore supera la sua opacita' (come quelli di
 * colore_coperto); 0 se no. */
static inline int premoltiplicato_vero(unsigned int p) {
    unsigned int alfa = p >> 24;
    return ((p >> 16) & 0xFF) <= alfa && ((p >> 8) & 0xFF) <= alfa && (p & 0xFF) <= alfa;
}

/* sopra_a per un 'sopra' premoltiplicato vero, DUE BYTE ALLA VOLTA; restituisce lo stesso
 * pixel di sopra_a. sopra_a fa, per ogni byte, sopra + diviso_255(sotto * resto). Si possono
 * mettere due byte di 'sotto' in un numero, a 16 bit di distanza (rosso e blu, poi alfa e
 * verde): ognuno per 'resto' fa al massimo 255 * 255 + 128 = 65153, che sta nei suoi 16 bit,
 * e anche il "+ (x >> 8)" di diviso_255 ci sta (65407): i due conti non si toccano e vengono
 * uguali a quelli fatti uno per uno. Poi si somma 'sopra': se ogni suo byte non supera la sua
 * opacita', byte + diviso_255(sotto * resto) <= opacita' + resto = 255, quindi non ci sono
 * riporti da un byte all'altro e la somma e' uguale all'OR di sopra_a. Anche le scorciatoie
 * di sopra_a vengono uguali: opacita' piena (resto 0) da' 'sopra', 'sotto' vuoto da' 'sopra',
 * opacita' 0 (e allora 'sopra' e' 0) da' 'sotto'. Con un 'sopra' che non e' un
 * premoltiplicato vero il risultato e' sbagliato: si controlla prima con premoltiplicato_vero. */
static inline unsigned int sopra_a_vero(unsigned int sopra, unsigned int sotto) {
    unsigned int resto = 255 - (sopra >> 24);
    unsigned int rb = (sotto & 0x00FF00FFu) * resto + 0x00800080u;
    rb = ((rb + ((rb >> 8) & 0x00FF00FFu)) >> 8) & 0x00FF00FFu;
    unsigned int ag = ((sotto >> 8) & 0x00FF00FFu) * resto + 0x00800080u;
    ag = (ag + ((ag >> 8) & 0x00FF00FFu)) & 0xFF00FF00u;
    return sopra + rb + ag;
}

/* Il colore del pennello, coperto per 'copertura' (0-256), gia' premoltiplicato: il pixel da
 * mettere sopra. In 'a' la sua opacita' (0-255): se e' 0 non c'e' niente da mettere. */
static inline unsigned int colore_coperto(const struct pennello *p, int copertura, unsigned int *a) {
    unsigned int c = p->s.colore;
    *a = ((c >> 24) * copertura >> 8) * (p->s.opacita + 1) >> 8;   /* 0-255 */
    return *a << 24 | diviso_255(((c >> 16) & 0xFF) * *a) << 16 |
           diviso_255(((c >> 8) & 0xFF) * *a) << 8 | diviso_255((c & 0xFF) * *a);
}

/* UNA RIGA DI 'n' PIXEL premoltiplicati 's' messa sopra 'd': per ogni pixel e' sopra_a, con
 * le scorciatoie di stendi (gkit/interno/interno.h): opaco copre e basta, trasparente niente,
 * il resto due byte alla volta se il pixel e' un premoltiplicato vero. Lo usa il compositore
 * per le immagini delle finestre. */
static inline void sopra_a_riga(unsigned int *d, const unsigned int *s, int n) {
    for (int i = 0; i < n; i++) {
        unsigned int p = s[i], alfa = p >> 24;
        if (alfa == 255) d[i] = p;
        else if (alfa) d[i] = premoltiplicato_vero(p) ? sopra_a_vero(p, d[i]) : sopra_a(p, d[i]);
    }
}

/* --- quanto un pixel sta dentro una forma arrotondata --- */

/* La radice quadrata intera di 'n', per difetto (cifra binaria per cifra binaria: solo
 * spostamenti e somme). */
static inline unsigned int radice(unsigned long long n) {
    unsigned long long r = 0, bit = 1ULL << 62;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= r + bit) { n -= r + bit; r = (r >> 1) + bit; }
        else r >>= 1;
        bit >>= 2;
    }
    return (unsigned int)r;
}

/* LA DISTANZA del centro del pixel (px, py) dal bordo della forma 't', in 256esimi di pixel:
 * negativa dentro, positiva fuori. Un rettangolo arrotondato e' un rettangolo piu' piccolo
 * "gonfiato" di r in tutte le direzioni: la distanza dal bordo e' la distanza dal rettangolo
 * piccolo, meno r. Un raggio piu' grande di meta' del lato piu' corto conta come quella meta'. */
static inline int distanza(const struct tondo *t, int px, int py) {
    int cx = px * 256 + 128, cy = py * 256 + 128;
    int sinistra = cx < (t->x * 2 + t->l) * 128, sopra = cy < (t->y * 2 + t->a) * 128;
    int r = t->r[sopra ? (sinistra ? 0 : 1) : (sinistra ? 3 : 2)];
    int massimo = (t->l < t->a ? t->l : t->a) / 2;
    if (r > massimo) r = massimo;
    int x1 = (t->x + r) * 256, x2 = (t->x + t->l - r) * 256;
    int y1 = (t->y + r) * 256, y2 = (t->y + t->a - r) * 256;
    int dx = cx < x1 ? x1 - cx : cx > x2 ? cx - x2 : 0;
    int dy = cy < y1 ? y1 - cy : cy > y2 ? cy - y2 : 0;
    if (dx && dy) return (int)radice((unsigned long long)dx * dx + (unsigned long long)dy * dy) - r * 256;
    if (dx || dy) return dx + dy - r * 256;
    /* dentro il rettangolo piccolo: quanto manca al suo lato piu' vicino, piu' r */
    int vicino = cx - x1;
    if (x2 - cx < vicino) vicino = x2 - cx;
    if (cy - y1 < vicino) vicino = cy - y1;
    if (y2 - cy < vicino) vicino = y2 - cy;
    return -vicino - r * 256;
}

/* Quanto e' coperto un pixel a 'distanza' dal bordo (0-256): tutto a mezzo pixel dentro,
 * niente a mezzo pixel fuori, in mezzo a sfumare. */
static inline int coperto(int distanza) {
    int c = 128 - distanza;
    return c < 0 ? 0 : c > 256 ? 256 : c;
}

/* LA SFUMATURA di un'ombra. 'd' e' la distanza con segno (in 256esimi di pixel, come
 * 'distanza') dal bordo della forma che fa l'ombra; 's' e' la meta' della larghezza della
 * sfumatura, in 256esimi di pixel (almeno 256). Restituisce 0-256: 256 tutto dentro la
 * forma (a 's' o piu' dal bordo), 0 tutto fuori, e in mezzo una curva a S (smoothstep,
 * 3v^2 - 2v^3) che inizia e finisce PIANO: una rampa dritta farebbe vedere due "spigoli"
 * dove l'ombra comincia e dove finisce; con la curva a S l'occhio non trova il bordo,
 * che e' quello che fa sembrare una sfocatura vera (una gaussiana, in pratica). Sul
 * bordo esatto vale 128, cioe' meta'. */
static inline int sfuma(int d, int s) {
    int v = (s - d) * 128 / s;                       /* 0-256 lungo la sfumatura */
    if (v <= 0) return 0;
    if (v > 256) v = 256;
    return v * v * (768 - 2 * v) >> 16;
}

#endif
