/* GKit: le forme arrotondate con i bordi lisci, le ombre, cerchi, pillole e archi (le
 * dichiarazioni, con cosa fa ogni funzione, sono in gkit_disegno.h; i conti per pixel,
 * distanza, coperto e sfuma, in sistema/pixel.h). */
#include "interno.h"

struct tondo tondo(int x, int y, int l, int a, int raggio) {
    struct tondo t = { x, y, l, a, { raggio, raggio, raggio, raggio } };
    return t;
}

/* PERCHE' SI PUO' SALTARE L'INTERNO. Su una riga, 'distanza' e' fatta di due meta': nella
 * meta' sinistra usa un raggio solo e, andando verso il centro, non cresce mai (ci si
 * allontana dal bordo sinistro, e la radice intera non torna indietro); nella meta' destra
 * lo stesso, a specchio. Quindi i pixel di una riga che stanno "abbastanza dentro"
 * (distanza al massimo un certo valore) sono SEMPRE una striscia sola, senza buchi: la
 * fine della meta' sinistra piu' l'inizio della destra. Basta cercarne i due capi, dai
 * due lati della riga, e tutto quello che sta in mezzo e' dentro senza calcolarlo.
 * (La prova, pixel per pixel contro il modo lento, e' in un programma di prova fuori dal
 * sistema.) */

void riempi_forma(struct pennello *p, const struct tondo *t) {
    int x0, y0, x1, y1;
    if (pennello_invisibile(p) || !zona_da_disegnare(p, t->x, t->y, t->l, t->a, &x0, &y0, &x1, &y1))
        return;
    unsigned int alfa, pieno = colore_coperto(p, 256, &alfa);
    for (int y = y0; y < y1; y++) {
        unsigned int *riga = p->tela->pixel + y * p->tela->larghezza;
        /* da sinistra fino al primo pixel coperto tutto, poi da destra lo stesso */
        int a = x0, b = x1;
        for (; a < b; a++) {
            int c = coperto(distanza(t, a, y));
            if (c == 256) break;
            copri_pixel(p, &riga[a], c);
        }
        for (; b > a; b--) {
            int c = coperto(distanza(t, b - 1, y));
            if (c == 256) break;
            copri_pixel(p, &riga[b - 1], c);
        }
        stendi(riga + a, b - a, pieno, alfa);   /* in mezzo, tutto coperto */
    }
}

void riempi_tondo(struct pennello *p, int x, int y, int l, int a, int raggio) {
    struct tondo t = tondo(x, y, l, a, raggio);
    riempi_forma(p, &t);
}

void contorna_forma(struct pennello *p, const struct tondo *t, int spessore) {
    int x0, y0, x1, y1;
    /* con spessore 0 o meno la forma ristretta copre almeno quanto la forma: niente */
    if (spessore <= 0 || pennello_invisibile(p) ||
        !zona_da_disegnare(p, t->x, t->y, t->l, t->a, &x0, &y0, &x1, &y1)) return;
    for (int y = y0; y < y1; y++) {
        unsigned int *riga = p->tela->pixel + y * p->tela->larghezza;
        /* dove anche la forma ristretta copre tutto, la differenza e' 0: si salta (vedi
         * "perche' si puo' saltare l'interno", sopra riempi_forma) */
        int a = x0, b = x1;
        for (; a < b; a++) {
            int d = distanza(t, a, y);
            if (coperto(d + spessore) == 256) break;
            copri_pixel(p, &riga[a], coperto(d) - coperto(d + spessore));
        }
        for (; b > a; b--) {
            int d = distanza(t, b - 1, y);
            if (coperto(d + spessore) == 256) break;
            copri_pixel(p, &riga[b - 1], coperto(d) - coperto(d + spessore));
        }
    }
}

void contorna_tondo(struct pennello *p, int x, int y, int l, int a, int raggio, int spessore) {
    struct tondo t = tondo(x, y, l, a, raggio);
    contorna_forma(p, &t, spessore);
}

/* --- le ombre --- */

/* COME SI FA L'OMBRA. Si allarga (o si stringe) la forma di 'diffusione' sottraendo
 * diffusione * 256 dalla sua distanza con segno: gonfiare un rettangolo arrotondato di d
 * pixel da' un rettangolo arrotondato col raggio piu' grande di d, e la sua distanza dal bordo
 * e' esattamente quella di prima meno d. Poi si sfuma con 'sfuma'. Niente buffer, niente
 * sfocatura vera a due passate: ogni pixel dell'ombra si calcola da solo dalla distanza,
 * quindi non costa memoria. Il prezzo e' che l'ombra di una forma qualunque e' sempre "la
 * forma, ammorbidita": per un rettangolo arrotondato e' quasi indistinguibile da una
 * gaussiana vera. */
void ombra_forma_diffusa(struct pennello *p, const struct tondo *t, int dx, int dy, int sfocatura,
                         int diffusione) {
    struct tondo o = *t;
    o.x += dx;
    o.y += dy;
    int s = sfocatura * 256;
    if (s < 256) s = 256;
    int fuori_dal_bordo = sfocatura + (diffusione > 0 ? diffusione : 0);
    if (diffusione && sfocatura < 1) fuori_dal_bordo++;      /* la sfumatura di 1 px minimo */
    /* non si calcola quello che il ritaglio butterebbe via */
    int x0, y0, x1, y1;
    if (pennello_invisibile(p) ||
        !zona_da_disegnare(p, o.x - fuori_dal_bordo, o.y - fuori_dal_bordo,
                           o.l + 2 * fuori_dal_bordo, o.a + 2 * fuori_dal_bordo, &x0, &y0, &x1, &y1))
        return;
    for (int y = y0; y < y1; y++) {
        unsigned int *riga = p->tela->pixel + y * p->tela->larghezza;
        /* sotto la forma (coperta tutta) l'ombra non si vede: si salta, come in
         * riempi_forma, cercando i due capi dell'interno dai due lati della riga */
        int a = x0, b = x1;
        for (; a < b; a++) {
            int fuori = 256 - coperto(distanza(t, a, y));
            if (!fuori) break;
            int dolce = sfuma(distanza(&o, a, y) - diffusione * 256, s);
            if (dolce) copri_pixel(p, &riga[a], dolce * fuori >> 8);
        }
        for (; b > a; b--) {
            int fuori = 256 - coperto(distanza(t, b - 1, y));
            if (!fuori) break;
            int dolce = sfuma(distanza(&o, b - 1, y) - diffusione * 256, s);
            if (dolce) copri_pixel(p, &riga[b - 1], dolce * fuori >> 8);
        }
    }
}

void ombra_forma(struct pennello *p, const struct tondo *t, int dx, int dy, int sfocatura) {
    ombra_forma_diffusa(p, t, dx, dy, sfocatura, 0);
}

/* Ombra = 256 - sfuma(distanza dal buco): e' il contrario dell'ombra esterna (li' si
 * colorava dove la sfumatura vale, qui dove NON vale), e si moltiplica per quanto e'
 * coperto il pixel dalla forma vera: cosi' il bordo liscio (antialiasing) della forma
 * non viene sporcato e il ritaglio del pennello vale come sempre. */
void ombra_interna_diffusa(struct pennello *p, const struct tondo *t, int dx, int dy, int sfocatura,
                           int diffusione) {
    struct tondo buco = *t;
    buco.x += dx;
    buco.y += dy;
    int s = sfocatura * 256;
    if (s < 256) s = 256;
    int x0, y0, x1, y1;
    if (pennello_invisibile(p) || !zona_da_disegnare(p, t->x, t->y, t->l, t->a, &x0, &y0, &x1, &y1))
        return;
    for (int y = y0; y < y1; y++) {
        unsigned int *riga = p->tela->pixel + y * p->tela->larghezza;
        /* dove il buco copre tutto (sfuma = 256) non c'e' ombra: e' una striscia sola
         * nel mezzo della riga (il buco e' un tondo anche lui), e si salta */
        int a = x0, b = x1;
        for (; a < b; a++) {
            int ombra = 256 - sfuma(distanza(&buco, a, y) + diffusione * 256, s);
            if (!ombra) break;
            int dentro = coperto(distanza(t, a, y));
            if (dentro) copri_pixel(p, &riga[a], ombra * dentro >> 8);
        }
        for (; b > a; b--) {
            int ombra = 256 - sfuma(distanza(&buco, b - 1, y) + diffusione * 256, s);
            if (!ombra) break;
            int dentro = coperto(distanza(t, b - 1, y));
            if (dentro) copri_pixel(p, &riga[b - 1], ombra * dentro >> 8);
        }
    }
}

void ombra_interna(struct pennello *p, const struct tondo *t, int dx, int dy, int sfocatura) {
    ombra_interna_diffusa(p, t, dx, dy, sfocatura, 0);
}

void ombre_strati_fuori(struct pennello *p, const struct tondo *t, const struct strato_ombra *strati,
                        int n) {
    pennello_salva(p);
    for (int i = n - 1; i >= 0; i--) {
        if (strati[i].tipo != OMBRA_ESTERNA) continue;
        pennello_colore(p, strati[i].colore);
        ombra_forma_diffusa(p, t, strati[i].dx, strati[i].dy, strati[i].sfocatura,
                            strati[i].diffusione);
    }
    pennello_ripristina(p);
}

void ombre_strati_dentro(struct pennello *p, const struct tondo *t, const struct strato_ombra *strati,
                         int n) {
    pennello_salva(p);
    for (int i = n - 1; i >= 0; i--) {
        if (strati[i].tipo != OMBRA_INTERNA) continue;
        pennello_colore(p, strati[i].colore);
        ombra_interna_diffusa(p, t, strati[i].dx, strati[i].dy, strati[i].sfocatura,
                              strati[i].diffusione);
    }
    pennello_ripristina(p);
}

void ombre_strati(struct pennello *p, const struct tondo *t, const struct strato_ombra *strati, int n) {
    ombre_strati_fuori(p, t, strati, n);
    ombre_strati_dentro(p, t, strati, n);
}

/* --- cerchi e pillole --- */

void riempi_pillola(struct pennello *p, int x, int y, int l, int a) {
    riempi_tondo(p, x, y, l, a, (l < a ? l : a) / 2);
}

void riempi_cerchio(struct pennello *p, int cx, int cy, int raggio) {
    riempi_tondo(p, cx - raggio, cy - raggio, 2 * raggio, 2 * raggio, raggio);
}

/* --- archi e anelli --- */

/* Il seno di un angolo intero in gradi, moltiplicato per 4096 (punto fisso 4.12).
 * Basta la tabella del primo quarto (0-90 gradi): gli altri tre sono lo stesso
 * disegno specchiato. 91 numeri, nessuna libreria matematica. */
static const short tabella_seno[91] = {
    0, 71, 143, 214, 286, 357, 428, 499, 570, 641, 711, 782, 852,
    921, 991, 1060, 1129, 1198, 1266, 1334, 1401, 1468, 1534, 1600, 1666, 1731,
    1796, 1860, 1923, 1986, 2048, 2110, 2171, 2231, 2290, 2349, 2408, 2465, 2522,
    2578, 2633, 2687, 2741, 2793, 2845, 2896, 2946, 2996, 3044, 3091, 3138, 3183,
    3228, 3271, 3314, 3355, 3396, 3435, 3474, 3511, 3547, 3582, 3617, 3650, 3681,
    3712, 3742, 3770, 3798, 3824, 3849, 3873, 3896, 3917, 3937, 3956, 3974, 3991,
    4006, 4021, 4034, 4046, 4056, 4065, 4074, 4080, 4086, 4090, 4094, 4095, 4096,
};

static int seno_gradi(int g) {
    g %= 360;
    if (g < 0) g += 360;
    int quarto = g / 90, r = g % 90;
    int v = (quarto & 1) ? tabella_seno[90 - r] : tabella_seno[r];
    return quarto >= 2 ? -v : v;
}

/* COME, senza coseni e senza arctan. Per ogni pixel si calcolano due distanze con
 * segno (in 256esimi di pixel, come per le altre forme) e si prende la peggiore:
 *   - dalla CORONA: la distanza del pixel dal centro (con 'radice'), confrontata con i
 *     due raggi: serve il massimo tra "troppo lontano" e "troppo vicino";
 *   - dal SETTORE: l'arco e' una fetta di torta, cioe' l'incrocio (se la fetta e' al
 *     massimo mezzo giro) o l'unione (se e' piu' di mezzo giro) di due mezzi piani, uno
 *     per ogni lato dritto della fetta. La distanza del pixel da una retta per il
 *     centro con direzione u e' il prodotto vettoriale u x P (P = pixel meno centro),
 *     perche' u e' lungo 1. Le direzioni u vengono dalla tabella del seno.
 * Il bordo sfuma con 'coperto' e quindi non ha scalini, qualunque sia l'angolo. I tappi
 * tondi sono una mezza pillola a ogni estremita': un cerchio di diametro pari allo spessore,
 * centrato a meta' corona. */
void riempi_arco_fine(struct pennello *p, int cx, int cy, int re, int ri, int inizio, int fine,
                      int tappi_tondi) {
    if (re <= ri || re <= 0) return;
    if (ri < 0) ri = 0;
    int giro = fine - inizio;
    int intero = giro >= 360;
    if (!intero) {
        giro %= 360;
        if (giro < 0) giro += 360;
        if (giro == 0) return;
    }
    /* direzioni (seno, -coseno) delle due estremita', in 1/4096; y verso il basso */
    int u0x = seno_gradi(inizio), u0y = -seno_gradi(inizio + 90);
    int u1x = seno_gradi(fine), u1y = -seno_gradi(fine + 90);
    int meta = (re - ri) / 2, medio = (re + ri) / 2;
    int c0x = (int)(((long long)medio * u0x) >> 12), c0y = (int)(((long long)medio * u0y) >> 12);
    int c1x = (int)(((long long)medio * u1x) >> 12), c1y = (int)(((long long)medio * u1y) >> 12);
    int fuori = (re + 255) / 256 + 1;
    int y0 = (cy >> 8) - fuori, y1 = (cy >> 8) + fuori + 1;
    int x0 = (cx >> 8) - fuori, x1 = (cx >> 8) + fuori + 1;
    if (y0 < p->s.ry) y0 = p->s.ry;
    if (y1 > p->s.ry2) y1 = p->s.ry2;
    if (x0 < p->s.rx) x0 = p->s.rx;
    if (x1 > p->s.rx2) x1 = p->s.rx2;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            int px = x * 256 + 128 - cx, py = y * 256 + 128 - cy;
            int dist = (int)radice((unsigned long long)((long long)px * px + (long long)py * py));
            int d = dist - re;
            if (ri - dist > d) d = ri - dist;                    /* fuori dalla corona */
            if (!intero) {
                int sa = (int)(((long long)u0x * py - (long long)u0y * px) >> 12);   /* >0: dopo l'inizio */
                int sb = -(int)(((long long)u1x * py - (long long)u1y * px) >> 12);  /* >0: prima della fine */
                int ds = giro <= 180 ? -(sa < sb ? sa : sb) : -(sa > sb ? sa : sb);
                if (ds > d) d = ds;
                if (tappi_tondi && d > -128) {
                    int ax = px - c0x, ay = py - c0y, bx = px - c1x, by = py - c1y;
                    if (ax < 0) ax = -ax;
                    if (ay < 0) ay = -ay;
                    if (bx < 0) bx = -bx;
                    if (by < 0) by = -by;
                    if (ax < meta + 256 && ay < meta + 256) {
                        int dc = (int)radice((unsigned long long)ax * ax + (unsigned long long)ay * ay) - meta;
                        if (dc < d) d = dc;
                    }
                    if (bx < meta + 256 && by < meta + 256) {
                        int dc = (int)radice((unsigned long long)bx * bx + (unsigned long long)by * by) - meta;
                        if (dc < d) d = dc;
                    }
                }
            }
            copri(p, x, y, coperto(d));
        }
}

void riempi_arco(struct pennello *p, int cx, int cy, int raggio_esterno, int raggio_interno,
                 int angolo_inizio, int angolo_fine) {
    riempi_arco_fine(p, cx * 256, cy * 256, raggio_esterno * 256, raggio_interno * 256,
                     angolo_inizio, angolo_fine, 1);
}

void riempi_arco_dritto(struct pennello *p, int cx, int cy, int raggio_esterno, int raggio_interno,
                        int angolo_inizio, int angolo_fine) {
    riempi_arco_fine(p, cx * 256, cy * 256, raggio_esterno * 256, raggio_interno * 256,
                     angolo_inizio, angolo_fine, 0);
}

/* --- la luce sul bordo --- */

void luce_sopra(struct pennello *p, const struct tondo *t) {
    struct tondo giu = *t;
    giu.y += 1;
    int x0, y0, x1, y1;
    if (pennello_invisibile(p) || !zona_da_disegnare(p, t->x, t->y, t->l, t->a, &x0, &y0, &x1, &y1))
        return;
    for (int y = y0; y < y1; y++) {
        unsigned int *riga = p->tela->pixel + y * p->tela->larghezza;
        /* dove tutte e due le forme coprono tutto la differenza e' 0: due strisce
         * senza buchi (vedi sopra riempi_forma) si incrociano in una striscia sola, e
         * si salta */
        int a = x0, b = x1;
        for (; a < b; a++) {
            int c = coperto(distanza(t, a, y)), g = coperto(distanza(&giu, a, y));
            if (c == 256 && g == 256) break;
            copri_pixel(p, &riga[a], c - g);
        }
        for (; b > a; b--) {
            int c = coperto(distanza(t, b - 1, y)), g = coperto(distanza(&giu, b - 1, y));
            if (c == 256 && g == 256) break;
            copri_pixel(p, &riga[b - 1], c - g);
        }
    }
}
