/* La libreria di disegno: il secondo strato della grafica, come Cairo su Linux o Core
 * Graphics sul Mac. Disegna su una TELA (un pezzo di memoria con dei pixel, per
 * esempio l'immagine di una finestra) e non sa niente di finestre o di compositore.
 *
 * L'IDEA DEL PENNELLO. Invece di passare colore, spessore e ritaglio a ogni disegno, si
 * prepara un pennello e si dice "da adesso rosso", "da adesso spessore 2": i disegni
 * dopo li usano. Il pennello si puo' SALVARE e RIPRISTINARE, come un segnalibro: si
 * salva, si cambia quello che serve per un pezzo di disegno, si ripristina.
 *
 *     struct tela t = { pixel, larghezza, altezza };
 *     struct pennello p;
 *     pennello_prepara(&p, &t);
 *     pennello_colore(&p, 0x3A6EA5);
 *     riempi_rettangolo(&p, 0, 0, 100, 20);
 *     pennello_colore(&p, 0xFFFFFF);
 *     testo(&p, 4, 2, "Ciao");
 *
 * IL RITAGLIO. Il pennello disegna solo dentro un rettangolo, all'inizio tutta la tela.
 * pennello_ritaglia lo restringe: quello che esce non si disegna. Servira' al toolkit,
 * perche' un pulsante non sbordi fuori dal suo spazio.
 *
 * PER ORA L'ESSENZIALE: rettangoli, linee dritte, testo con il font Spleen (a bitmap,
 * una sola misura). I colori sono 0xRRGGBB, sempre pieni. Piu' avanti, come Cairo: i
 * tracciati con le curve, i bordi lisci (antialiasing) e la trasparenza. I nomi e il
 * pennello sono gia' pensati per aggiungerli senza cambiare quello che c'e'.
 *
 * Coordinate in pixel, (0, 0) in alto a sinistra, y verso il basso. */
#ifndef DISEGNO_H
#define DISEGNO_H

#include "font.h"

struct tela {
    unsigned int *pixel;       /* 0xRRGGBB, riga dopo riga, senza spazi tra le righe */
    int larghezza, altezza;
};

/* Lo stato che si salva e si ripristina. */
struct stato_pennello {
    unsigned int colore;
    int spessore;
    int rx, ry, rx2, ry2;      /* il ritaglio: si disegna solo con rx <= x < rx2, ry <= y < ry2 */
};

#define MAX_SALVATI 8

struct pennello {
    struct tela *tela;
    struct stato_pennello s;
    struct stato_pennello salvati[MAX_SALVATI];
    int quanti_salvati;
};

/* --- il pennello --- */

/* Nero, spessore 1, ritaglio su tutta la tela. */
static inline void pennello_prepara(struct pennello *p, struct tela *t) {
    p->tela = t;
    p->s = (struct stato_pennello){ .colore = 0x000000, .spessore = 1,
                                    .rx = 0, .ry = 0, .rx2 = t->larghezza, .ry2 = t->altezza };
    p->quanti_salvati = 0;
}

static inline void pennello_colore(struct pennello *p, unsigned int rgb) { p->s.colore = rgb & 0xFFFFFF; }

/* Lo spessore delle linee e dei contorni, in pixel. */
static inline void pennello_spessore(struct pennello *p, int spessore) {
    p->s.spessore = spessore < 1 ? 1 : spessore;
}

/* Restringe il ritaglio a questo rettangolo (dentro quello che c'era gia': si puo'
 * solo stringere, per allargare si ripristina). */
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

/* --- le forme --- */

/* Un punto, se sta nel ritaglio. Tutto passa da qui: quando ci sara' la trasparenza,
 * sara' qui che il colore nuovo si mescola con quello sotto. */
static inline void disegna_punto(struct pennello *p, int x, int y) {
    if (x < p->s.rx || x >= p->s.rx2 || y < p->s.ry || y >= p->s.ry2) return;
    p->tela->pixel[y * p->tela->larghezza + x] = p->s.colore;
}

static inline void riempi_rettangolo(struct pennello *p, int x, int y, int l, int a) {
    int x1 = x < p->s.rx ? p->s.rx : x, y1 = y < p->s.ry ? p->s.ry : y;
    int x2 = x + l > p->s.rx2 ? p->s.rx2 : x + l, y2 = y + a > p->s.ry2 ? p->s.ry2 : y + a;
    for (int r = y1; r < y2; r++)   /* gia' ritagliato: si scrive diretto, riga per riga */
        for (int k = x1; k < x2; k++)
            p->tela->pixel[r * p->tela->larghezza + k] = p->s.colore;
}

/* Il contorno di un rettangolo, spesso quanto il pennello, verso l'interno. */
static inline void contorna_rettangolo(struct pennello *p, int x, int y, int l, int a) {
    int s = p->s.spessore;
    if (2 * s >= l || 2 * s >= a) { riempi_rettangolo(p, x, y, l, a); return; }
    riempi_rettangolo(p, x, y, l, s);                  /* sopra */
    riempi_rettangolo(p, x, y + a - s, l, s);          /* sotto */
    riempi_rettangolo(p, x, y + s, s, a - 2 * s);      /* sinistra */
    riempi_rettangolo(p, x + l - s, y + s, s, a - 2 * s); /* destra */
}

/* Una linea dritta da (x1, y1) a (x2, y2), estremi compresi, in qualunque direzione.
 * E' l'algoritmo di Bresenham (1962): si avanza di un pixel alla volta lungo la
 * direzione piu' lunga, e si tiene il conto di quanto ci si sta allontanando dalla
 * linea vera nell'altra; quando l'errore supera mezzo pixel, si fa un passo anche li'.
 * Solo somme di interi, niente divisioni. Con spessore > 1, in ogni punto si mette un
 * quadratino di quel lato. Senza antialiasing le oblique vengono a scalini. */
static inline void linea(struct pennello *p, int x1, int y1, int x2, int y2) {
    int dx = x2 > x1 ? x2 - x1 : x1 - x2, passo_x = x1 < x2 ? 1 : -1;
    int dy = y2 > y1 ? y2 - y1 : y1 - y2, passo_y = y1 < y2 ? 1 : -1;
    int errore = dx - dy;
    int s = p->s.spessore, meta = (s - 1) / 2;
    for (;;) {
        if (s == 1) disegna_punto(p, x1, y1);
        else riempi_rettangolo(p, x1 - meta, y1 - meta, s, s);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * errore;
        if (e2 > -dy) { errore -= dy; x1 += passo_x; }
        if (e2 < dx)  { errore += dx; y1 += passo_y; }
    }
}

/* --- il testo --- */

/* Quanto e' largo un testo, in pixel: serve per centrarlo o allinearlo a destra.
 * Con un font a bitmap a larghezza fissa e' facile: tutte le lettere sono larghe uguali. */
static inline int larghezza_testo(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n * FONT_LARGHEZZA;
}

static inline int altezza_testo(void) { return FONT_ALTEZZA; }

/* Scrive 's' con l'angolo in alto a sinistra della prima lettera in (x, y). Si
 * colorano solo i pixel delle lettere: sotto resta quello che c'era. I caratteri sono
 * quelli della code page 437, come i tasti. */
static inline void testo(struct pennello *p, int x, int y, const char *s) {
    for (; *s; s++, x += FONT_LARGHEZZA) {
        const unsigned char *lettera = font_8x16[(unsigned char)*s];
        for (int r = 0; r < FONT_ALTEZZA; r++)
            for (int k = 0; k < FONT_LARGHEZZA; k++)
                if (lettera[r] & (0x80 >> k)) disegna_punto(p, x + k, y + r);
    }
}

#endif
