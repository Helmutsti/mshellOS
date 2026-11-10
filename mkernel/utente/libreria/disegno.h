/* La libreria di disegno: il secondo strato della grafica, come Cairo su Linux o Core
 * Graphics sul Mac. Disegna su una TELA (un pezzo di memoria con dei pixel, per
 * esempio l'immagine di una finestra) e non sa niente di finestre o di compositore.
 *
 * L'IDEA DEL PENNELLO. Invece di passare colore, opacita' e ritaglio a ogni disegno, si
 * prepara un pennello e si dice "da adesso questo colore": i disegni dopo lo usano. Il
 * pennello si puo' SALVARE e RIPRISTINARE, come un segnalibro.
 *
 *     struct tela t = { pixel, larghezza, altezza };
 *     struct pennello p;
 *     pennello_prepara(&p, &t);
 *     pulisci(&p);                                   tutta trasparente
 *     pennello_colore(&p, 0x24FFFFFF);               bianco al 14%
 *     riempi_tondo(&p, 10, 10, 120, 36, 8);          angoli di raggio 8
 *     pennello_colore(&p, OPACO(0xFFFFFF));
 *     testo(&p, &inter_14, 22, 19, "Ciao");
 *
 * I COLORI sono 0xAARRGGBB: AA e' quanto e' opaco (0 invisibile, FF pieno). I pixel della
 * tela invece sono PREMOLTIPLICATI: ogni colore e' gia' moltiplicato per la sua opacita'
 * (un bianco al 50% e' 0x80808080). E' il formato di Wayland e delle schede video: per
 * mettere un pixel sopra un altro basta "sopra + sotto * (1 - opacita' di sopra)".
 *
 * I BORDI LISCI (antialiasing). Per ogni pixel si calcola QUANTO e' coperto dalla forma,
 * da 0 a 256, e lo si colora solo per quella parte. Per le forme arrotondate si usa la
 * DISTANZA del centro del pixel dal bordo: dentro di mezzo pixel o piu', coperto tutto;
 * fuori di mezzo pixel o piu', niente; in mezzo, a sfumare. Tutto con numeri interi, in
 * 256esimi di pixel: i programmi non possono usare i numeri con la virgola (il kernel
 * non salva quella parte del processore quando passa da un programma all'altro).
 *
 * IL TESTO E LE ICONE vengono da grafica_dati.h: tabelle di copertura gia' calcolate
 * (font Inter, icone Lucide). Qui si colorano e si mettono sulla tela.
 *
 * Coordinate in pixel, (0, 0) in alto a sinistra, y verso il basso. */
#ifndef DISEGNO_H
#define DISEGNO_H

#include "grafica_dati.h"

#define OPACO(rgb) (0xFF000000u | (unsigned int)(rgb))
/* Un colore con opacita' da 0 a 255: VELATO(0xFFFFFF, 36) e' il bianco al 14%. */
#define VELATO(rgb, alfa) ((unsigned int)(alfa) << 24 | (unsigned int)(rgb))

struct tela {
    unsigned int *pixel;       /* 0xAARRGGBB premoltiplicati, riga dopo riga */
    int larghezza, altezza;
};

/* Lo stato che si salva e si ripristina. */
struct stato_pennello {
    unsigned int colore;
    int opacita;               /* 0-255: moltiplica tutto quello che si disegna */
    int spessore;              /* per linee e contorni dritti */
    int rx, ry, rx2, ry2;      /* il ritaglio: si disegna solo con rx <= x < rx2, ry <= y < ry2 */
};

#define MAX_SALVATI 16

struct pennello {
    struct tela *tela;
    struct stato_pennello s;
    struct stato_pennello salvati[MAX_SALVATI];
    int quanti_salvati;
};

/* --- il pennello --- */

/* Nero pieno, opacita' piena, spessore 1, ritaglio su tutta la tela. */
static inline void pennello_prepara(struct pennello *p, struct tela *t) {
    p->tela = t;
    p->s = (struct stato_pennello){ .colore = OPACO(0), .opacita = 255, .spessore = 1,
                                    .rx = 0, .ry = 0, .rx2 = t->larghezza, .ry2 = t->altezza };
    p->quanti_salvati = 0;
}

static inline void pennello_colore(struct pennello *p, unsigned int argb) { p->s.colore = argb; }

/* Da adesso tutto e' anche piu' trasparente: si moltiplica per 'opacita' (0-255). Serve
 * per i widget disabilitati (al 40%), che restano visibili ma "spenti". */
static inline void pennello_opacita(struct pennello *p, int opacita) {
    p->s.opacita = p->s.opacita * opacita / 255;
}

static inline void pennello_spessore(struct pennello *p, int spessore) {
    p->s.spessore = spessore < 1 ? 1 : spessore;
}

/* Restringe il ritaglio a questo rettangolo (dentro quello che c'era gia'). */
static inline void pennello_ritaglia(struct pennello *p, int x, int y, int l, int a) {
    if (x > p->s.rx) p->s.rx = x;
    if (y > p->s.ry) p->s.ry = y;
    if (x + l < p->s.rx2) p->s.rx2 = x + l;
    if (y + a < p->s.ry2) p->s.ry2 = y + a;
}

static inline void pennello_salva(struct pennello *p) {
    if (p->quanti_salvati < MAX_SALVATI) p->salvati[p->quanti_salvati++] = p->s;
}

static inline void pennello_ripristina(struct pennello *p) {
    if (p->quanti_salvati > 0) p->s = p->salvati[--p->quanti_salvati];
}

/* --- mettere un colore su un pixel --- */

/* x / 255, arrotondato, senza dividere */
static inline unsigned int diviso_255(unsigned int x) { return (x + 128 + ((x + 128) >> 8)) >> 8; }

/* Un pixel premoltiplicato 'sopra' messo su 'sotto'. */
static inline unsigned int sopra_a(unsigned int sopra, unsigned int sotto) {
    unsigned int resto = 255 - (sopra >> 24);
    if (resto == 255) return sotto;
    if (resto == 0) return sopra;
    unsigned int r = 0;
    for (int b = 0; b < 32; b += 8)
        r |= (((sopra >> b) & 0xFF) + diviso_255(((sotto >> b) & 0xFF) * resto)) << b;
    return r;
}

/* Il colore del pennello, coperto per 'copertura' (0-256), sul pixel (x, y). TUTTO il
 * disegno passa da qui: ritaglio, opacita', trasparenza. */
static inline void copri(struct pennello *p, int x, int y, int copertura) {
    if (copertura <= 0 || x < p->s.rx || x >= p->s.rx2 || y < p->s.ry || y >= p->s.ry2) return;
    if (copertura > 256) copertura = 256;
    unsigned int c = p->s.colore;
    unsigned int a = ((c >> 24) * copertura >> 8) * (p->s.opacita + 1) >> 8;   /* 0-255 */
    if (!a) return;
    unsigned int premolt = a << 24 | diviso_255(((c >> 16) & 0xFF) * a) << 16 |
                           diviso_255(((c >> 8) & 0xFF) * a) << 8 | diviso_255((c & 0xFF) * a);
    unsigned int *d = &p->tela->pixel[y * p->tela->larghezza + x];
    *d = sopra_a(premolt, *d);
}

/* Tutta la zona del ritaglio trasparente: si comincia sempre cosi', perche' le forme
 * semitrasparenti si sommano a quello che c'e' gia' sotto. */
static inline void pulisci(struct pennello *p) {
    for (int y = p->s.ry; y < p->s.ry2; y++)
        for (int x = p->s.rx; x < p->s.rx2; x++)
            p->tela->pixel[y * p->tela->larghezza + x] = 0;
}

/* --- le forme dritte --- */

static inline void riempi_rettangolo(struct pennello *p, int x, int y, int l, int a) {
    for (int r = y; r < y + a; r++)
        for (int k = x; k < x + l; k++) copri(p, k, r, 256);
}

/* Il contorno di un rettangolo, spesso quanto il pennello, verso l'interno. */
static inline void contorna_rettangolo(struct pennello *p, int x, int y, int l, int a) {
    int s = p->s.spessore;
    if (2 * s >= l || 2 * s >= a) { riempi_rettangolo(p, x, y, l, a); return; }
    riempi_rettangolo(p, x, y, l, s);
    riempi_rettangolo(p, x, y + a - s, l, s);
    riempi_rettangolo(p, x, y + s, s, a - 2 * s);
    riempi_rettangolo(p, x + l - s, y + s, s, a - 2 * s);
}

/* Una linea dritta da (x1, y1) a (x2, y2), estremi compresi (Bresenham, 1962): si
 * avanza di un pixel alla volta lungo la direzione piu' lunga, tenendo il conto di
 * quanto ci si allontana dalla linea vera nell'altra. Senza bordi lisci. */
static inline void linea(struct pennello *p, int x1, int y1, int x2, int y2) {
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

/* --- le forme arrotondate, con i bordi lisci --- */

/* La radice quadrata intera (cifra binaria per cifra binaria: solo spostamenti e somme). */
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

/* Un rettangolo arrotondato, con un raggio per angolo (in alto a sinistra, in alto a
 * destra, in basso a destra, in basso a sinistra). */
struct tondo {
    int x, y, l, a;
    int r[4];
};

static inline struct tondo tondo(int x, int y, int l, int a, int raggio) {
    struct tondo t = { x, y, l, a, { raggio, raggio, raggio, raggio } };
    return t;
}

/* LA DISTANZA del centro del pixel (px, py) dal bordo della forma, in 256esimi di pixel:
 * negativa dentro, positiva fuori. Un rettangolo arrotondato e' un rettangolo piu'
 * piccolo "gonfiato" di r in tutte le direzioni: la distanza dal bordo e' la distanza
 * dal rettangolo piccolo, meno r. */
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

/* Quanto e' coperto un pixel a 'distanza' dal bordo (0-256): tutto a mezzo pixel
 * dentro, niente a mezzo pixel fuori, in mezzo a sfumare. */
static inline int coperto(int distanza) {
    int c = 128 - distanza;
    return c < 0 ? 0 : c > 256 ? 256 : c;
}

static inline void riempi_forma(struct pennello *p, const struct tondo *t) {
    for (int y = t->y; y < t->y + t->a; y++)
        for (int x = t->x; x < t->x + t->l; x++) copri(p, x, y, coperto(distanza(t, x, y)));
}

static inline void riempi_tondo(struct pennello *p, int x, int y, int l, int a, int raggio) {
    struct tondo t = tondo(x, y, l, a, raggio);
    riempi_forma(p, &t);
}

/* Il bordo interno della forma, spesso 'spessore' 256esimi di pixel (256 = 1 pixel,
 * 384 = 1,5): quello che e' dentro la forma ma non dentro la forma ristretta. */
static inline void contorna_forma(struct pennello *p, const struct tondo *t, int spessore) {
    for (int y = t->y; y < t->y + t->a; y++)
        for (int x = t->x; x < t->x + t->l; x++) {
            int d = distanza(t, x, y);
            copri(p, x, y, coperto(d) - coperto(d + spessore));
        }
}

static inline void contorna_tondo(struct pennello *p, int x, int y, int l, int a, int raggio,
                                  int spessore) {
    struct tondo t = tondo(x, y, l, a, raggio);
    contorna_forma(p, &t, spessore);
}

/* Un'OMBRA morbida sotto la forma, spostata di (dx, dy) e sfocata di 'sfocatura' pixel:
 * la copertura sfuma dolcemente (non a scalino) per tutta la larghezza della
 * sfocatura. Si vede solo FUORI dalla forma: dentro c'e' la forma stessa. */
static inline void ombra_forma(struct pennello *p, const struct tondo *t, int dx, int dy,
                               int sfocatura) {
    struct tondo o = *t;
    o.x += dx;
    o.y += dy;
    int s = sfocatura * 256;
    if (s < 256) s = 256;
    for (int y = o.y - sfocatura; y < o.y + o.a + sfocatura; y++)
        for (int x = o.x - sfocatura; x < o.x + o.l + sfocatura; x++) {
            int fuori = 256 - coperto(distanza(t, x, y));
            if (!fuori) continue;
            int v = (s - distanza(&o, x, y)) * 128 / s;      /* 0-256 lungo la sfumatura */
            if (v <= 0) continue;
            if (v > 256) v = 256;
            int dolce = v * v * (768 - 2 * v) >> 16;         /* smoothstep: inizia e finisce piano */
            copri(p, x, y, dolce * fuori >> 8);
        }
}

/* Una riga di luce sul bordo alto, all'interno: quello che e' dentro la forma ma non
 * dentro la forma abbassata di un pixel. Fa sembrare il vetro "in rilievo". */
static inline void luce_sopra(struct pennello *p, const struct tondo *t) {
    struct tondo giu = *t;
    giu.y += 1;
    for (int y = t->y; y < t->y + t->a; y++)
        for (int x = t->x; x < t->x + t->l; x++)
            copri(p, x, y, coperto(distanza(t, x, y)) - coperto(distanza(&giu, x, y)));
}

/* --- il testo --- */

static inline const struct glifo *glifo_di(const struct font *f, unsigned char c) {
    unsigned char i = f->indice[c];
    if (!i && c != ' ') i = f->indice['?'];   /* un carattere che il font non ha */
    return &f->glifi[i];
}

/* Quanto e' largo un testo, in pixel: serve per centrarlo o allinearlo a destra. */
static inline int larghezza_testo(const struct font *f, const char *s) {
    int l = 0;
    for (; *s; s++) l += glifo_di(f, (unsigned char)*s)->avanzo;
    return l;
}

/* Scrive 's' con la cima della riga in (x, y), nel colore del pennello. Restituisce
 * dove finisce (la x dopo l'ultima lettera). */
static inline int testo(struct pennello *p, const struct font *f, int x, int y, const char *s) {
    int base = y + f->ascesa;
    for (; *s; s++) {
        const struct glifo *g = glifo_di(f, (unsigned char)*s);
        const unsigned char *alfa = f->alfa + g->inizio;
        int gx = x + g->x, gy = base - g->y;
        for (int r = 0; r < g->a; r++)
            for (int k = 0; k < g->l; k++) {
                int v = alfa[r * g->l + k];
                if (v) copri(p, gx + k, gy + r, v + (v >> 7));
            }
        x += g->avanzo;
    }
    return x;
}

/* --- le icone e le immagini --- */

/* L'icona 'quale' (ICONA_...) di lato 'lato' pixel, nel colore del pennello. Se quella
 * misura non c'e', si usa la piu' vicina. */
static inline void icona(struct pennello *p, int quale, int lato, int x, int y) {
    const struct icona *scelta = &icone[quale][0];
    for (int i = 1; i < ICONA_MISURE; i++)
        if (icone[quale][i].lato <= lato) scelta = &icone[quale][i];
    int l = scelta->lato;
    x += (lato - l) / 2;
    y += (lato - l) / 2;
    for (int r = 0; r < l; r++)
        for (int k = 0; k < l; k++) {
            int v = scelta->alfa[r * l + k];
            if (v) copri(p, x + k, y + r, v + (v >> 7));
        }
}

/* Un'immagine a colori (gia' premoltiplicata) con l'angolo in (x, y). */
static inline void disegna_immagine(struct pennello *p, const struct immagine *im, int x, int y) {
    unsigned int colore = p->s.colore;
    for (int r = 0; r < im->a; r++)
        for (int k = 0; k < im->l; k++) {
            unsigned int px = im->pixel[r * im->l + k];
            unsigned int a = px >> 24;
            if (!a) continue;
            /* si torna al colore non premoltiplicato, e lo si copre per la sua opacita' */
            unsigned int rr = ((px >> 16) & 0xFF) * 255 / a, gg = ((px >> 8) & 0xFF) * 255 / a,
                         bb = (px & 0xFF) * 255 / a;
            p->s.colore = OPACO(rr << 16 | gg << 8 | bb);
            copri(p, x + k, y + r, a + (a >> 7));
        }
    p->s.colore = colore;
}

#endif
