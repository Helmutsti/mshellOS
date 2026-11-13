/* UIKit, i componenti piccoli: la barra di utilizzo e la legenda (quanto e' usato qualcosa,
 * diviso in pezzi), e il numero di byte in testo che usano tutte e due. Le dichiarazioni e il
 * disegno sono in grafico.h. */
#include "uikit/uikit.h"

/* Il colore del tono 'i' (0 il piu' forte, 3 il piu' leggero): i colori del tema in uso. */
static unsigned int grafico_tono(int i) {
    switch (i) {
    case 0:  return TESTO_PRIMARIO;
    case 1:  return TESTO_SECONDARIO;
    case 2:  return TESTO_DISABILITATO;
    default: return BORDO_CONTROLLO;
    }
}

char *testo_byte(char *d, unsigned long long byte) {
    static const char *const unita[] = { " MB", " GB", " TB" };
    unsigned long long kb = byte / 1024;
    if (kb < 103) {                        /* meno di 0,1 MB */
        char c[24]; int k = 0;
        do { c[k++] = (char)('0' + kb % 10); kb /= 10; } while (kb);
        while (k) *d++ = c[--k];
        *d++ = ' '; *d++ = 'K'; *d++ = 'B'; *d = 0;
        return d;
    }
    unsigned long long decimi = kb * 10 / 1024;   /* in decimi di MB */
    int u = 0;
    while (decimi >= 10240 && u < 2) { decimi = decimi / 1024; u++; }   /* oltre 1024: l'unita' dopo */
    unsigned long long intero = decimi / 10;
    char c[24]; int k = 0;
    do { c[k++] = (char)('0' + intero % 10); intero /= 10; } while (intero);
    while (k) *d++ = c[--k];
    if (decimi % 10) { *d++ = ','; *d++ = (char)('0' + decimi % 10); }
    for (const char *s = unita[u]; *s; s++) *d++ = *s;
    *d = 0;
    return d;
}

/* Quanto vale la barra intera: il totale, se c'e', se no la somma dei pezzi. */
static unsigned long long grafico_totale(const struct grafico *g) {
    if (g->totale) return g->totale;
    unsigned long long t = 0;
    for (int i = 0; i < g->quanti; i++) t += g->byte[i];
    return t;
}

int grafico_uguale(const struct grafico *a, const struct grafico *b, int largo) {
    if (a->quanti != b->quanti) return 0;
    unsigned long long ta = grafico_totale(a), tb = grafico_totale(b), sa = 0, sb = 0;
    if ((ta == 0) != (tb == 0)) return 0;
    for (int i = 0; i < a->quanti; i++) {
        char qa[24], qb[24];
        testo_byte(qa, a->byte[i]);
        testo_byte(qb, b->byte[i]);
        for (int k = 0; qa[k] || qb[k]; k++)
            if (qa[k] != qb[k]) return 0;
        if (i == a->resto) continue;
        sa += a->byte[i];
        sb += b->byte[i];
        if (ta && sa * (unsigned int)largo / ta != sb * (unsigned int)largo / tb) return 0;
    }
    return 1;
}

/* --- la barra --- */

static void barra_utilizzo_misura(struct widget *w, int *l, int *a) {
    (void)w;
    *l = 0;
    *a = GRAFICO_SPESSORE;
}

static void barra_utilizzo_disegna(struct widget *w, struct pennello *p) {
    const struct grafico *g = ((struct barra_utilizzo *)w)->grafico;
    struct tondo t = tondo(w->x, w->y, w->l, w->a, w->a / 2);
    pennello_colore(p, SUPERFICIE_INCASSATA);
    riempi_forma(p, &t);
    unsigned long long totale = grafico_totale(g);
    if (!totale) return;
    int x = w->x;
    unsigned long long somma = 0;
    for (int i = 0; i < g->quanti; i++) {
        if (i == g->resto) continue;
        somma += g->byte[i];
        int fine = w->x + (int)(somma * (unsigned int)w->l / totale);
        if (fine <= x) continue;
        pennello_salva(p);
        pennello_ritaglia(p, x, w->y, fine - x, w->a);
        pennello_colore(p, grafico_tono(g->toni[i]));
        riempi_forma(p, &t);
        pennello_ripristina(p);
        x = fine;
    }
}

static const struct tipo_widget tipo_barra_utilizzo = {
    barra_utilizzo_misura, 0, barra_utilizzo_disegna, 0, 0, 0, 0, 0, 0
};

void barra_utilizzo_prepara(struct barra_utilizzo *b, const struct grafico *g) {
    *b = (struct barra_utilizzo){ .base = { .tipo = &tipo_barra_utilizzo, .largo = 1 }, .grafico = g };
}

/* --- la legenda --- */

static void legenda_misura(struct widget *w, int *l, int *a) {
    int n = ((struct legenda *)w)->grafico->quanti;
    *l = 0;
    *a = n ? n * (LEGENDA_VOCE + LEGENDA_TRA) - LEGENDA_TRA : 0;
}

static void legenda_disegna(struct widget *w, struct pennello *p) {
    const struct grafico *g = ((struct legenda *)w)->grafico;
    for (int i = 0; i < g->quanti; i++) {
        int y = w->y + i * (LEGENDA_VOCE + LEGENDA_TRA);
        pennello_colore(p, i == g->resto ? SUPERFICIE_INCASSATA : grafico_tono(g->toni[i]));
        riempi_cerchio(p, w->x + LEGENDA_PALLINO / 2, y + LEGENDA_VOCE / 2, LEGENDA_PALLINO / 2);
        char quanto[24];
        testo_byte(quanto, g->byte[i]);
        pennello_colore(p, TESTO_PRIMARIO);
        testo_centrato(p, FONT_CORPO, w->x + LEGENDA_PALLINO + SPAZIO_8, y, LEGENDA_VOCE, g->nomi[i]);
        pennello_colore(p, TESTO_SECONDARIO);
        testo_centrato(p, FONT_CORPO, w->x + w->l - larghezza_testo(FONT_CORPO, quanto), y, LEGENDA_VOCE, quanto);
    }
}

static const struct tipo_widget tipo_legenda = { legenda_misura, 0, legenda_disegna, 0, 0, 0, 0, 0, 0 };

void legenda_prepara(struct legenda *l, const struct grafico *g) {
    *l = (struct legenda){ .base = { .tipo = &tipo_legenda, .largo = 1 }, .grafico = g };
}
