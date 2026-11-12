/* LA PAGINA: un contenitore in colonna con il suo margine, che puo' avere il fondo dell'area dei
 * contenuti e puo' scorrere (UIKit, incluso da uikit.h). E' il "Contenuto" e il "Pannello
 * contenuto" dei disegni del file Figma.
 *
 * LA REGOLA DELLA SPAZIATURA. Lo spazio interno sta in UN SOLO livello: il contenitore che
 * delimita un'area visibile. La pagina e' quel livello: ha un margine (ai lati, sopra, sotto) e
 * uno spazio tra un figlio e l'altro; i figli non hanno margini verso di lei. Se i figli hanno un
 * fondo visibile con un loro margine interno (le righe sotto il puntatore), la pagina toglie lo
 * stesso valore dal suo margine su quel lato: la somma resta quella del design.
 *
 * IL FONDO ('fondo' a 1): l'AREA DEI CONTENUTI della finestra, chiara (SUPERFICIE_ELEVATA), sotto
 * la barra degli strumenti; se arriva al fondo della finestra, i suoi angoli in basso seguono
 * quelli della finestra (RAGGIO_FINESTRA_NUOVO). Si usa con finestra_tk_bordo: la pagina prende
 * tutta la finestra sotto le barre.
 *
 * LA LARGHEZZA DEI FIGLI. La pagina da' a ogni figlio tutta la sua larghezza (meno i margini)
 * PRIMA di misurarlo: cosi' un paragrafo o una riga che va a capo sanno quanto sono alti
 * (pagina_larghezza, che scende anche dentro righe e colonne). Lo spazio in altezza che avanza va
 * ai figli 'elastico'.
 *
 * LO SCORRIMENTO ('scorre' a 1): se i figli sono piu' alti della pagina, si vede un pezzo e la
 * rotella lo fa scorrere, con la barra di scorrimento di UIKit sul bordo destro; i figli si
 * vedono solo dentro la pagina.
 *
 *     pagina_prepara(&elenco, SPAZIO_16, SPAZIO_24, SPAZIO_24, SPAZIO_24);
 *     elenco.fondo = 1; elenco.scorre = 1;
 *     pagina_aggiungi(&elenco, &sezione.base);
 *
 * I figli si aggiungono con pagina_aggiungi (non widget_aggiungi): la barra di scorrimento resta
 * l'ultima, sopra a tutti. pagina_svuota toglie tutti i figli, per rifare la pagina. */
#ifndef UIKIT_PAGINA_H
#define UIKIT_PAGINA_H

struct pagina {
    struct widget base;
    int margine_x, margine_sopra, margine_sotto;
    int spazio;                            /* tra un figlio e l'altro */
    int fondo;                             /* 1: il fondo chiaro dell'area dei contenuti */
    int scorre;                            /* 1: se non ci sta, scorre */
    int spostato;                          /* lo scorrimento con cui sono messi ora i figli */
    struct barra_scorrimento barra;        /* sempre l'ultimo figlio */
};

static const struct tipo_widget tipo_pagina;

/* Da' al widget la larghezza 'l' prima di misurarlo, e la passa ai figli come farebbe il
 * contenitore disponendoli: in una colonna a quelli larghi, in una riga a ognuno la sua misura
 * e agli elastici quello che avanza; in una pagina, meno i margini. */
static inline void pagina_larghezza(struct widget *w, int l) {
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
static inline void pagina_sposta(struct widget *w, int dy) {
    w->y -= dy;
    for (int i = 0; i < w->quanti_figli; i++) pagina_sposta(w->figli[i], dy);
}

/* Le altezze dei figli (con la larghezza gia' data) e la loro somma, con margini e spazi. */
static inline int pagina_totale(struct pagina *g) {
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

static inline void pagina_misura(struct widget *w, int *l, int *a) {
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

static inline void pagina_disponi(struct widget *w) {
    struct pagina *g = (struct pagina *)w;
    int dentro = w->l - 2 * g->margine_x;
    for (int i = 0; i < w->quanti_figli; i++)
        if (w->figli[i] != &g->barra.base && !w->figli[i]->nascosto) pagina_larghezza(w->figli[i], dentro);
    int totale = pagina_totale(g), elastici = 0;
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
        y += fa + g->spazio;
    }
}

/* Il fondo dell'area dei contenuti; se scorre, i figli si vedono solo qui dentro (il ritaglio
 * resta per i figli, che si disegnano dopo). Prima, se nel frattempo si e' scorso (rotella o
 * barra), si spostano i figli. */
static inline void pagina_disegna(struct widget *w, struct pennello *p) {
    struct pagina *g = (struct pagina *)w;
    if (g->fondo) {
        struct finestra_tk *f = widget_finestra(w);
        int r = w->y + w->a >= f->base.a ? RAGGIO_FINESTRA_NUOVO : 0;
        struct tondo t = { w->x, w->y, w->l, w->a, { 0, 0, r, r } };
        pennello_colore(p, SUPERFICIE_ELEVATA);
        riempi_forma(p, &t);
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

static inline void pagina_rotella(struct widget *w, int sx, int sy) {
    (void)sx;
    struct pagina *g = (struct pagina *)w;
    if (g->scorre && barra_scorrimento_rotella(&g->barra, sy)) widget_cambiato(w);
}

static const struct tipo_widget tipo_pagina = {
    pagina_misura, pagina_disponi, pagina_disegna, 0, 0, pagina_rotella, 0, 0, 0
};

static inline void pagina_prepara(struct pagina *g, int spazio, int margine_x, int margine_sopra,
                                  int margine_sotto) {
    *g = (struct pagina){ .base = { .tipo = &tipo_pagina, .largo = 1, .elastico = 1 },
                          .margine_x = margine_x, .margine_sopra = margine_sopra,
                          .margine_sotto = margine_sotto, .spazio = spazio };
    barra_scorrimento_prepara(&g->barra);
    widget_aggiungi(&g->base, &g->barra.base);
}

/* Aggiunge un figlio, prima della barra di scorrimento (che resta sopra a tutti). */
static inline void pagina_aggiungi(struct pagina *g, struct widget *figlio) {
    struct widget *w = &g->base;
    if (w->quanti_figli >= MAX_FIGLI) return;
    w->figli[w->quanti_figli - 1] = figlio;
    w->figli[w->quanti_figli++] = &g->barra.base;
    figlio->genitore = w;
}

/* Toglie tutti i figli (resta la barra) e torna in cima. */
static inline void pagina_svuota(struct pagina *g) {
    g->base.figli[0] = &g->barra.base;
    g->base.quanti_figli = 1;
    g->barra.scorrimento.posizione = 0;
    g->spostato = 0;
    widget_cambiato(&g->base);
}

/* Toglie tutti i figli di una riga o di una colonna, per rifarla. */
static inline void contenitore_svuota(struct contenitore *c) {
    c->base.quanti_figli = 0;
    widget_cambiato(&c->base);
}

#endif
