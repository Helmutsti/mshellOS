/* UIKit, i componenti: la griglia di icone e i suoi tile. Le dichiarazioni sono in
 * griglia.h. */
#include "uikit/uikit.h"
#include "uikit/interno/nucleo.h"

/* Quanti tile in una riga: nello spazio meno il padding e meno la corsia della barra di
 * scorrimento (barra_scorrimento_riserva: il padding di 8 a destra, se scorre, diventa di 14). */
static int griglia_per_riga(const struct griglia_tk *g) {
    int n = (g->base.l - g->riserva - 2 * GRIGLIA_PADDING + GRIGLIA_TRA) / (TILE_LARGHEZZA + GRIGLIA_TRA);
    return n < 1 ? 1 : n;
}

/* L'angolo del tile i (nella finestra, gia' scorso). */
static void griglia_tile(const struct griglia_tk *g, int i, int *x, int *y) {
    int n = griglia_per_riga(g);
    *x = g->base.x + GRIGLIA_PADDING + i % n * (TILE_LARGHEZZA + GRIGLIA_TRA);
    *y = g->base.y + GRIGLIA_PADDING + i / n * (TILE_ALTEZZA + GRIGLIA_TRA) - g->barra.scorrimento.posizione;
}

int griglia_in(const struct griglia_tk *g, int x, int y) {
    if (!widget_contiene(&g->base, x, y)) return -1;
    for (int i = 0; i < g->quanti; i++) {
        int tx, ty;
        griglia_tile(g, i, &tx, &ty);
        if (x >= tx && x < tx + TILE_LARGHEZZA && y >= ty && y < ty + TILE_ALTEZZA) return i;
    }
    return -1;
}

static int griglia_totale(const struct griglia_tk *g) {
    int righe = (g->quanti + griglia_per_riga(g) - 1) / griglia_per_riga(g);
    return 2 * GRIGLIA_PADDING + (righe ? righe * (TILE_ALTEZZA + GRIGLIA_TRA) - GRIGLIA_TRA : 0);
}

/* Prima senza la corsia della barra; se cosi' non ci sta tutto, la barra c'e': si rifa' con la
 * corsia (che puo' togliere un tile per riga, e allungare la griglia). */
static void griglia_impagina(struct griglia_tk *g) {
    struct widget *w = &g->base;
    int posizione = g->barra.scorrimento.posizione;   /* la prima misura non deve spostarla */
    barra_scorrimento_posto(&g->barra, w->x, w->y, w->l, w->a);
    g->riserva = g->barra.fuori;
    scorrimento_misure(&g->barra.scorrimento, griglia_totale(g), w->a);
    g->riserva = barra_scorrimento_riserva(&g->barra, GRIGLIA_PADDING);
    g->barra.scorrimento.posizione = posizione;
    scorrimento_misure(&g->barra.scorrimento, griglia_totale(g), w->a);
}

static void griglia_misura(struct widget *w, int *l, int *a) { (void)w; *l = 0; *a = 0; }

static void griglia_disponi(struct widget *w) { griglia_impagina((struct griglia_tk *)w); }

/* IL TILE, con l'angolo in (x, y). 'stato': 0 normale, 1 sopra, 2 selezionato. */
static void tile_disegna(struct pennello *p, int x, int y, const struct elemento_griglia_tk *e, int stato,
                         const char *rinomina) {
    int rx = x + (TILE_LARGHEZZA - TILE_RIQUADRO) / 2, ry = y;
    if (stato) {
        pennello_colore(p, stato == 2 ? CONTROLLO_SFONDO_SOPRA : CONTROLLO_SFONDO_NUOVO);
        riempi_tondo(p, rx, ry, TILE_RIQUADRO, TILE_RIQUADRO, RAGGIO_CONTROLLO_NUOVO);
    }
    oggetto(p, e->oggetto, OGGETTO_GRANDE, rx + (TILE_RIQUADRO - OGGETTO_LATO_GRANDE) / 2,
            ry + (TILE_RIQUADRO - OGGETTO_LATO_GRANDE) / 2);
    int ny = ry + TILE_RIQUADRO + TILE_DENTRO;
    if (rinomina) {
        campo_rinomina_disegna(p, x, ny, TILE_LARGHEZZA, TILE_CAMPO_ALTO, RAGGIO_CASELLA, rinomina, 1);
        return;
    }
    char righe[TILE_RIGHE][MAX_RIGA_NOME];
    int n = testo_a_capo(FONT_CORPO, e->nome, TILE_LARGHEZZA - 2 * TILE_ETICHETTA_DENTRO, righe, TILE_RIGHE);
    if (stato == 2 && n) {
        pennello_colore(p, AZIONE_SFONDO);
        riempi_tondo(p, x, ny, TILE_LARGHEZZA, n * INTERLINEA_CORPO, RAGGIO_CASELLA);
    }
    pennello_colore(p, stato == 2 ? AZIONE_TESTO : TESTO_PRIMARIO);
    for (int i = 0; i < n; i++)
        testo_centrato(p, FONT_CORPO, x + (TILE_LARGHEZZA - larghezza_testo(FONT_CORPO, righe[i])) / 2,
                       ny + i * INTERLINEA_CORPO, INTERLINEA_CORPO, righe[i]);
}

static void griglia_disegna(struct widget *w, struct pennello *p) {
    struct griglia_tk *g = (struct griglia_tk *)w;
    pennello_salva(p);
    pennello_ritaglia(p, w->x, w->y, w->l, w->a);
    for (int i = 0; i < g->quanti; i++) {
        int x, y;
        griglia_tile(g, i, &x, &y);
        if (y + TILE_ALTEZZA + SPAZIO_8 <= w->y || y - SPAZIO_8 >= w->y + w->a) continue;
        struct elemento_griglia_tk e = { .oggetto = OGGETTO_FILE };
        if (g->elemento) g->elemento(g, i, &e, g->dati);
        int stato = i == g->scelto || i == g->rinomina ? 2 : i == g->sopra ? 1 : 0;
        tile_disegna(p, x, y, &e, stato, i == g->rinomina ? g->testo_rinomina : 0);
    }
    pennello_ripristina(p);
}

static void griglia_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    struct griglia_tk *g = (struct griglia_tk *)w;
    unsigned int giu = pulsanti & (PULSANTE_SINISTRO | PULSANTE_DESTRO);
    int appena = giu && !(g->pulsanti_prima & (PULSANTE_SINISTRO | PULSANTE_DESTRO));
    g->pulsanti_prima = pulsanti;
    if (!appena) return;
    int i = griglia_in(g, x, y);
    unsigned int adesso = tempo();
    int come = pulsanti & PULSANTE_DESTRO ? CLIC_DESTRO
             : i >= 0 && i == g->clic_su && (int)(adesso - g->clic_prima) < DOPPIO_CLIC ? CLIC_DOPPIO : CLIC_SEMPLICE;
    g->clic_su = come == CLIC_SEMPLICE ? i : -1;
    g->clic_prima = adesso;
    if (g->al_clic) g->al_clic(g, i, come, x, y, g->dati);
}

static void griglia_tasto(struct widget *w, unsigned int tasto) {
    struct griglia_tk *g = (struct griglia_tk *)w;
    if (g->al_tasto) g->al_tasto(g, tasto, g->dati);
}

static void griglia_rotella(struct widget *w, int sx, int sy) {
    (void)sx;
    barra_scorrimento_rotella(&((struct griglia_tk *)w)->barra, sy);
}

/* Il tile i va ridisegnato (solo lui, con un po' d'aria intorno: il nome puo' andare a capo). */
static void griglia_tile_cambiato(struct griglia_tk *g, int i) {
    if (i < 0 || i >= g->quanti) return;
    int x, y;
    griglia_tile(g, i, &x, &y);
    widget_cambiato_zona(&g->base, x - SPAZIO_8, y - SPAZIO_8, TILE_LARGHEZZA + 2 * SPAZIO_8,
                         TILE_ALTEZZA + 2 * SPAZIO_8);
}

static void griglia_movimento(struct widget *w, int x, int y) {
    struct griglia_tk *g = (struct griglia_tk *)w;
    int i = griglia_in(g, x, y);
    if (i == g->sopra) return;
    griglia_tile_cambiato(g, g->sopra);
    g->sopra = i;
    griglia_tile_cambiato(g, i);
}

static void griglia_sopra(struct widget *w) {
    struct griglia_tk *g = (struct griglia_tk *)w;
    if (!w->sopra && g->sopra >= 0) { griglia_tile_cambiato(g, g->sopra); g->sopra = -1; }
}

static const struct tipo_widget tipo_griglia = {
    griglia_misura, griglia_disponi, griglia_disegna, griglia_puntatore, griglia_tasto, griglia_rotella,
    griglia_sopra, griglia_movimento, 0
};

void griglia_prepara(struct griglia_tk *g, dati_elemento_griglia elemento, al_clic_griglia al_clic,
                     void *dati) {
    *g = (struct griglia_tk){ .base = { .tipo = &tipo_griglia, .elastico = 1, .largo = 1 }, .scelto = -1,
                              .sopra = -1, .rinomina = -1, .elemento = elemento, .al_clic = al_clic,
                              .dati = dati, .clic_su = -1 };
    barra_scorrimento_prepara(&g->barra);
    widget_aggiungi(&g->base, &g->barra.base);
}

void griglia_aggiorna(struct griglia_tk *g, int quanti) {
    g->quanti = quanti;
    if (g->scelto >= quanti) g->scelto = -1;
    g->sopra = -1;
    griglia_impagina(g);
    widget_cambiato(&g->base);
}

void griglia_scegli(struct griglia_tk *g, int i) {
    if (g->scelto == i) return;
    g->scelto = i;
    widget_cambiato(&g->base);
}

void griglia_rinomina(struct griglia_tk *g, int i, const char *testo) {
    g->rinomina = i;
    g->testo_rinomina = testo;
    widget_cambiato(&g->base);
}
