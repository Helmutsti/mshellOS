/* LA BARRA DI UTILIZZO E LA LEGENDA, dal file Figma (pagina "App - Attivita'", "Barra di
 * utilizzo" e "Legenda") (UIKit, incluso da uikit.h). Come la pagina "Archiviazione" del Mac:
 * quanto e' usato qualcosa (la memoria, il disco), diviso in pezzi.
 *
 * IL GRAFICO e' l'elenco dei pezzi: nome, tono, quanti byte. La barra e la legenda sono due
 * widget che leggono lo stesso grafico.
 *
 * I TONI. Il disegno e' in scala di grigi: i pezzi si distinguono per quanto sono forti, dal
 * piu' forte al piu' leggero: TESTO_PRIMARIO, TESTO_SECONDARIO, TESTO_DISABILITATO,
 * BORDO_CONTROLLO (i colori del tema: nel chiaro dallo scuro al chiaro, nello scuro al
 * contrario). Il RESTO (la parte libera) non e' un pezzo: e' il fondo della barra.
 *
 * LA BARRA: una pillola alta GRAFICO_SPESSORE (8), fondo SUPERFICIE_INCASSATA, piatta (nel
 * disegno non ha ombre). Ogni pezzo e' la barra intera ritagliata nella sua striscia: gli
 * angoli tondi restano solo ai due capi.
 *
 * LA LEGENDA: una voce per pezzo, alta ALTEZZA_S (32), a SPAZIO_4 l'una dall'altra: il pallino
 * di 8 (il resto: SUPERFICIE_INCASSATA come il fondo della barra), a SPAZIO_8 il nome in
 * FONT_CORPO e TESTO_PRIMARIO, e a destra quanto pesa, FONT_CORPO e TESTO_SECONDARIO. */
#ifndef UIKIT_GRAFICO_H
#define UIKIT_GRAFICO_H

#define MAX_PEZZI_GRAFICO 6
#define NESSUN_PEZZO      (-1)
#define GRAFICO_SPESSORE  8
#define LEGENDA_VOCE      ALTEZZA_S
#define LEGENDA_TRA       SPAZIO_4
#define LEGENDA_PALLINO   8

struct grafico {
    int quanti;
    const char *const *nomi;
    const int *toni;                       /* il tono di ogni pezzo: 0 il piu' forte, 3 il piu' leggero */
    unsigned long long byte[MAX_PEZZI_GRAFICO];
    unsigned long long totale;             /* la barra intera; 0: la somma dei pezzi */
    int resto;                             /* il pezzo che e' il fondo della barra, o NESSUN_PEZZO */
};

static inline unsigned int grafico_tono(int i) {
    switch (i) {
    case 0:  return TESTO_PRIMARIO;
    case 1:  return TESTO_SECONDARIO;
    case 2:  return TESTO_DISABILITATO;
    default: return BORDO_CONTROLLO;
    }
}

/* Un numero di byte in testo, come nel disegno: "0,9 MB", "17,6 MB", "1,7 GB", "2 GB" (una
 * cifra dopo la virgola, senza ",0"); sotto un decimo di MB, in KB ("64 KB"). Dove finisce. */
static inline char *testo_byte(char *d, unsigned long long byte) {
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

static inline unsigned long long grafico_totale(const struct grafico *g) {
    if (g->totale) return g->totale;
    unsigned long long t = 0;
    for (int i = 0; i < g->quanti; i++) t += g->byte[i];
    return t;
}

/* --- la barra --- */

struct barra_utilizzo { struct widget base; const struct grafico *grafico; };

static inline void barra_utilizzo_misura(struct widget *w, int *l, int *a) {
    (void)w;
    *l = 0;
    *a = GRAFICO_SPESSORE;
}

static inline void barra_utilizzo_disegna(struct widget *w, struct pennello *p) {
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

static inline void barra_utilizzo_prepara(struct barra_utilizzo *b, const struct grafico *g) {
    *b = (struct barra_utilizzo){ .base = { .tipo = &tipo_barra_utilizzo, .largo = 1 }, .grafico = g };
}

/* --- la legenda --- */

struct legenda { struct widget base; const struct grafico *grafico; };

static inline void legenda_misura(struct widget *w, int *l, int *a) {
    int n = ((struct legenda *)w)->grafico->quanti;
    *l = 0;
    *a = n ? n * (LEGENDA_VOCE + LEGENDA_TRA) - LEGENDA_TRA : 0;
}

static inline void legenda_disegna(struct widget *w, struct pennello *p) {
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

static inline void legenda_prepara(struct legenda *l, const struct grafico *g) {
    *l = (struct legenda){ .base = { .tipo = &tipo_legenda, .largo = 1 }, .grafico = g };
}

#endif
