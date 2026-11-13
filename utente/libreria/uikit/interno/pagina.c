/* UIKit, i componenti: la pagina (il contenitore in colonna con il suo margine, che puo'
 * avere il fondo e scorrere) e contenitore_svuota. Le dichiarazioni sono in pagina.h. */
#include "uikit/uikit.h"
#include "uikit/interno/nucleo.h"

static const struct tipo_widget tipo_pagina;

/* Da' al widget la larghezza 'l' prima di misurarlo, e la passa ai figli come farebbe il
 * contenitore disponendoli: in una colonna a quelli larghi, in una riga a ognuno la sua misura
 * e agli elastici quello che avanza; in una pagina, meno i margini. */
static void pagina_larghezza(struct widget *w, int l) {
    w->l = l;
    if (w->tipo == &tipo_pagina) {
        struct pagina *g = (struct pagina *)w;
        for (int i = 0; i < w->quanti_figli; i++)
            if (w->figli[i] != &g->barra.base && !w->figli[i]->nascosto)
                pagina_larghezza(w->figli[i], l - 2 * g->margine_x);
        return;
    }
    if (w->tipo != &tipo_contenitore) return;
    struct contenitore *c = (struct contenitore *)w;
    if (!c->in_riga) {
        for (int i = 0; i < w->quanti_figli; i++) {
            struct widget *f = w->figli[i];
            if (f->nascosto) continue;
            int fl = l, fa;
            if (!f->largo) widget_misura(f, &fl, &fa);
            pagina_larghezza(f, fl);
        }
        return;
    }
    int somma = 0, visti = 0, elastici = 0;
    for (int i = 0; i < w->quanti_figli; i++) {
        struct widget *f = w->figli[i];
        if (f->nascosto) continue;
        int fl, fa;
        widget_misura(f, &fl, &fa);
        somma += fl + (visti++ ? c->spazio : 0);
        elastici += f->elastico;
    }
    int in_piu = l - somma;
    if (in_piu < 0) in_piu = 0;
    for (int i = 0; i < w->quanti_figli; i++) {
        struct widget *f = w->figli[i];
        if (f->nascosto) continue;
        int fl, fa;
        widget_misura(f, &fl, &fa);
        pagina_larghezza(f, fl + (f->elastico ? in_piu / elastici : 0));
    }
}

/* Sposta in su di 'dy' un widget e tutto quello che c'e' dentro. */
static void pagina_sposta(struct widget *w, int dy) {
    w->y -= dy;
    for (int i = 0; i < w->quanti_figli; i++) pagina_sposta(w->figli[i], dy);
}

/* Le altezze dei figli (con la larghezza gia' data) e la loro somma, con margini e spazi. */
static int pagina_totale(struct pagina *g) {
    struct widget *w = &g->base;
    int totale = g->margine_sopra + g->margine_sotto, visti = 0;
    for (int i = 0; i < w->quanti_figli; i++) {
        struct widget *f = w->figli[i];
        if (f == &g->barra.base || f->nascosto) continue;
        int fl, fa;
        widget_misura(f, &fl, &fa);
        totale += fa + (visti++ ? g->spazio : 0);
    }
    return totale;
}

static void pagina_misura(struct widget *w, int *l, int *a) {
    struct pagina *g = (struct pagina *)w;
    int largo = 0;
    for (int i = 0; i < w->quanti_figli; i++) {
        struct widget *f = w->figli[i];
        if (f == &g->barra.base || f->nascosto) continue;
        int fl, fa;
        widget_misura(f, &fl, &fa);
        if (fl > largo) largo = fl;
    }
    *l = largo + 2 * g->margine_x;
    *a = g->scorre ? 0 : pagina_totale(g);   /* se scorre, sta in quello che riceve (e' elastica) */
}

/* La barra di scorrimento di un figlio che scorre da solo (una tabella, una griglia: ha la
 * barra tra i suoi figli), o 0. Le pagine figlie non contano: scorrono dentro i loro margini. */
static struct barra_scorrimento *pagina_barra_del_figlio(struct widget *f) {
    if (f->tipo == &tipo_pagina) return 0;
    for (int i = 0; i < f->quanti_figli; i++)
        if (f->figli[i]->tipo == &tipo_barra_scorrimento) return (struct barra_scorrimento *)f->figli[i];
    return 0;
}

static void pagina_figli_larghi(struct pagina *g, int dentro) {
    struct widget *w = &g->base;
    for (int i = 0; i < w->quanti_figli; i++)
        if (w->figli[i] != &g->barra.base && !w->figli[i]->nascosto) pagina_larghezza(w->figli[i], dentro);
}

/* LA BARRA DI SCORRIMENTO NON VA SOPRA IL CONTENUTO (barra_scorrimento_riserva, uikit.h).
 *   - La barra della pagina sta nel suo margine destro. Se il margine e' piu' stretto della
 *     barra (14) e la pagina scorre, i figli si stringono a destra di quello che manca.
 *   - Un figlio che scorre da solo (tabella, griglia) riceve anche il margine destro della
 *     pagina (si allarga fino al bordo della pagina) e sa che quei pixel sono padding: la sua
 *     barra ci sta dentro, il suo contenuto no. Cosi' il contenuto resta dov'e' (stessa
 *     distanza dal bordo a sinistra e a destra) e la barra sta nel padding, non sopra. */
static void pagina_disponi(struct widget *w) {
    struct pagina *g = (struct pagina *)w;
    int dentro = w->l - 2 * g->margine_x;
    pagina_figli_larghi(g, dentro);
    int totale = pagina_totale(g), elastici = 0;
    if (g->scorre && totale > w->a && g->margine_x < SCORRIMENTO_ZONA) {
        dentro -= SCORRIMENTO_ZONA - g->margine_x;   /* la corsia della barra della pagina */
        pagina_figli_larghi(g, dentro);
        totale = pagina_totale(g);
    }
    for (int i = 0; i < w->quanti_figli; i++)
        if (w->figli[i] != &g->barra.base && !w->figli[i]->nascosto) elastici += w->figli[i]->elastico;
    int in_piu = w->a - totale;
    if (in_piu < 0) in_piu = 0;
    if (g->scorre) {
        scorrimento_misure(&g->barra.scorrimento, totale, w->a);
        barra_scorrimento_posto(&g->barra, w->x, w->y, w->l, w->a);
    }
    g->spostato = g->scorre ? g->barra.scorrimento.posizione : 0;
    int y = w->y + g->margine_sopra - g->spostato;
    for (int i = 0; i < w->quanti_figli; i++) {
        struct widget *f = w->figli[i];
        if (f == &g->barra.base || f->nascosto) continue;
        int fl, fa;
        widget_misura(f, &fl, &fa);
        if (f->elastico && elastici) fa += in_piu / elastici;
        f->x = w->x + g->margine_x;
        f->y = y;
        f->l = f->largo ? dentro : fl;
        f->a = fa;
        struct barra_scorrimento *b = f->largo ? pagina_barra_del_figlio(f) : 0;
        if (b) {                            /* il margine destro della pagina, per la sua barra */
            b->fuori = w->x + w->l - (f->x + f->l);
            f->l += b->fuori;
        }
        y += fa + g->spazio;
    }
}

/* Il fondo dell'area dei contenuti; se scorre, i figli si vedono solo qui dentro (il ritaglio
 * resta per i figli, che si disegnano dopo). Prima, se nel frattempo si e' scorso (rotella o
 * barra), si spostano i figli. */
static void pagina_disegna(struct widget *w, struct pennello *p) {
    struct pagina *g = (struct pagina *)w;
    if (g->fondo) {
        pennello_colore(p, SUPERFICIE_ELEVATA);
        riempi_tondo(p, w->x, w->y, w->l, w->a, RAGGIO_PANNELLO);
    }
    if (!g->scorre) return;
    int dy = g->barra.scorrimento.posizione - g->spostato;
    if (dy) {
        for (int i = 0; i < w->quanti_figli; i++)
            if (w->figli[i] != &g->barra.base) pagina_sposta(w->figli[i], dy);
        g->spostato += dy;
    }
    pennello_ritaglia(p, w->x, w->y, w->l, w->a);
}

static void pagina_rotella(struct widget *w, int sx, int sy) {
    (void)sx;
    struct pagina *g = (struct pagina *)w;
    if (g->scorre && barra_scorrimento_rotella(&g->barra, sy)) widget_cambiato(w);
}

static const struct tipo_widget tipo_pagina = {
    pagina_misura, pagina_disponi, pagina_disegna, 0, 0, pagina_rotella, 0, 0, 0
};

void pagina_prepara(struct pagina *g, int spazio, int margine_x, int margine_sopra,
                    int margine_sotto) {
    *g = (struct pagina){ .base = { .tipo = &tipo_pagina, .largo = 1, .elastico = 1 },
                          .margine_x = margine_x, .margine_sopra = margine_sopra,
                          .margine_sotto = margine_sotto, .spazio = spazio };
    barra_scorrimento_prepara(&g->barra);
    widget_aggiungi(&g->base, &g->barra.base);
}

void pagina_aggiungi(struct pagina *g, struct widget *figlio) {
    struct widget *w = &g->base;
    if (w->quanti_figli >= MAX_FIGLI) return;
    w->figli[w->quanti_figli - 1] = figlio;
    w->figli[w->quanti_figli++] = &g->barra.base;
    figlio->genitore = w;
}

void pagina_svuota(struct pagina *g) {
    for (int i = 0; i < g->base.quanti_figli - 1; i++)   /* l'ultimo e' la barra: resta */
        widget_esce(g->base.figli[i]);
    g->base.figli[0] = &g->barra.base;
    g->base.quanti_figli = 1;
    g->barra.scorrimento.posizione = 0;
    g->spostato = 0;
    widget_cambiato(&g->base);
}

void contenitore_svuota(struct contenitore *c) {
    for (int i = 0; i < c->base.quanti_figli; i++) widget_esce(c->base.figli[i]);
    c->base.quanti_figli = 0;
    widget_cambiato(&c->base);
}
