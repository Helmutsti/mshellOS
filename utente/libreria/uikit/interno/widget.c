/* UIKit, il nucleo: il widget in generale (l'albero, la misura, il disegno, chi sta sotto il
 * puntatore), i contenitori riga e colonna, gli spazi e l'etichetta. Le dichiarazioni, con
 * cosa fa ogni funzione, sono in uikit.h (quelle pubbliche) e in interno/nucleo.h (quelle che
 * servono solo dentro UIKit). */
#include "uikit/uikit.h"
#include "uikit/interno/nucleo.h"

void widget_aggiungi(struct widget *genitore, struct widget *figlio) {
    if (genitore->quanti_figli >= MAX_FIGLI) return;
    genitore->figli[genitore->quanti_figli++] = figlio;
    figlio->genitore = genitore;
}

void widget_misura(struct widget *w, int *l, int *a) {
    *l = *a = 0;
    if (w->tipo->misura) w->tipo->misura(w, l, a);
}

void widget_disponi(struct widget *w) {
    if (w->tipo->disponi) w->tipo->disponi(w);
    for (int i = 0; i < w->quanti_figli; i++) widget_disponi(w->figli[i]);
}

void widget_disegna(struct widget *w, struct pennello *p) {
    if (w->nascosto) return;
    /* tutto quello che il widget (e i suoi figli) disegna sta nel suo spazio piu' lo sbordo:
     * se quello non tocca il ritaglio (la zona che si sta ridisegnando), non c'e' niente da fare */
    if (w->x + w->l + WIDGET_SBORDO <= p->s.rx || w->x - WIDGET_SBORDO >= p->s.rx2 ||
        w->y + w->a + WIDGET_SBORDO <= p->s.ry || w->y - WIDGET_SBORDO >= p->s.ry2) return;
    pennello_salva(p);
    pennello_ritaglia(p, w->x - WIDGET_SBORDO, w->y - WIDGET_SBORDO,
                     w->l + 2 * WIDGET_SBORDO, w->a + 2 * WIDGET_SBORDO);   /* anelli e ombre sbordano */
    if (w->disabilitato && !w->tipo->disegna_disabilitato) pennello_opacita(p, OPACITA_DISABILITATO);
    if (w->tipo->disegna) w->tipo->disegna(w, p);
    for (int i = 0; i < w->quanti_figli; i++) widget_disegna(w->figli[i], p);
    pennello_ripristina(p);
}

int widget_contiene(const struct widget *w, int x, int y) {
    return x >= w->x && x < w->x + w->l && y >= w->y && y < w->y + w->a;
}

struct widget *widget_sotto(struct widget *w, int x, int y) {
    if (w->disabilitato || w->nascosto || !widget_contiene(w, x, y)) return 0;
    for (int i = w->quanti_figli - 1; i >= 0; i--) {
        struct widget *trovato = widget_sotto(w->figli[i], x, y);
        if (trovato) return trovato;
    }
    return w->tipo->puntatore ? w : 0;
}

struct widget *widget_in(struct widget *w, int x, int y) {
    if (w->disabilitato || w->nascosto || !widget_contiene(w, x, y)) return 0;
    for (int i = w->quanti_figli - 1; i >= 0; i--) {
        struct widget *trovato = widget_in(w->figli[i], x, y);
        if (trovato) return trovato;
    }
    return w;
}

int widget_visibile(const struct widget *w) {
    for (; w; w = w->genitore)
        if (w->nascosto) return 0;
    return 1;
}

void widget_disabilita(struct widget *w, int disabilitato) {
    w->disabilitato = disabilitato;
    widget_cambiato(w);
}

void widget_nascondi(struct widget *w, int nascosto) {
    w->nascosto = nascosto;
    if (!w->genitore || widget_visibile(w->genitore)) widget_segna(w);
}

int ombra_sporgenza(const struct strato_ombra *strati, int n) {
    int massimo = 0;
    for (int i = 0; i < n; i++) {
        if (strati[i].tipo != OMBRA_ESTERNA) continue;
        int dx = strati[i].dx < 0 ? -strati[i].dx : strati[i].dx, dy = strati[i].dy < 0 ? -strati[i].dy : strati[i].dy;
        int s = (dx > dy ? dx : dy) + strati[i].sfocatura + (strati[i].diffusione > 0 ? strati[i].diffusione : 0) + 2;
        if (s > massimo) massimo = s;
    }
    return massimo;
}

void copia_testo(char *dove, const char *da, int massimo) {
    int i = 0;
    for (; i < massimo - 1 && da[i]; i++) dove[i] = da[i];
    dove[i] = 0;
}

int testo_centrato(struct pennello *p, const struct font *f, int x, int y, int a,
                   const char *s) {
    return testo(p, f, x, y + (a - f->altezza) / 2, s);
}

/* Si somma lungo la direzione del contenitore, si prende il piu' grande nell'altra. */
static void contenitore_misura(struct widget *w, int *l, int *a) {
    struct contenitore *c = (struct contenitore *)w;
    int visti = 0;
    for (int i = 0; i < w->quanti_figli; i++) {
        if (w->figli[i]->nascosto) continue;
        int fl, fa;
        widget_misura(w->figli[i], &fl, &fa);
        int lungo = c->in_riga ? fl : fa, largo = c->in_riga ? fa : fl;
        int *somma = c->in_riga ? l : a, *massimo = c->in_riga ? a : l;
        *somma += lungo + (visti++ ? c->spazio : 0);
        if (largo > *massimo) *massimo = largo;
    }
}

/* Ognuno la sua misura; lo spazio in piu' diviso tra gli elastici. Nell'altra
 * direzione: tutto lo spazio a chi e' "largo", agli altri la loro misura (in una riga,
 * centrati in altezza). */
static void contenitore_disponi(struct widget *w) {
    struct contenitore *c = (struct contenitore *)w;
    int l = 0, a = 0, elastici = 0;
    contenitore_misura(w, &l, &a);
    for (int i = 0; i < w->quanti_figli; i++)
        if (!w->figli[i]->nascosto) elastici += w->figli[i]->elastico;
    int in_piu = c->in_riga ? w->l - l : w->a - a;
    if (in_piu < 0) in_piu = 0;
    int pos = c->in_riga ? w->x : w->y;
    for (int i = 0; i < w->quanti_figli; i++) {
        struct widget *f = w->figli[i];
        if (f->nascosto) continue;
        int fl, fa;
        widget_misura(f, &fl, &fa);
        int lungo = (c->in_riga ? fl : fa) + (f->elastico ? in_piu / elastici : 0);
        if (c->in_riga) {
            f->x = pos; f->l = lungo;
            f->a = f->largo ? w->a : fa;
            f->y = w->y + (w->a - f->a) / 2;
        } else {
            f->y = pos; f->a = lungo;
            f->l = f->largo ? w->l : fl;
            f->x = w->x;
        }
        pos += lungo + c->spazio;
    }
}

const struct tipo_widget tipo_contenitore = {
    contenitore_misura, contenitore_disponi, 0, 0, 0, 0, 0, 0, 0
};

void riga_prepara(struct contenitore *c, int spazio) {
    *c = (struct contenitore){ .base = { .tipo = &tipo_contenitore, .largo = 1 },
                               .in_riga = 1, .spazio = spazio };
}

void colonna_prepara(struct contenitore *c, int spazio) {
    *c = (struct contenitore){ .base = { .tipo = &tipo_contenitore, .largo = 1 },
                               .in_riga = 0, .spazio = spazio };
}

const struct tipo_widget tipo_spazio = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };

void spazio_prepara(struct spazio *s) {
    *s = (struct spazio){ .base = { .tipo = &tipo_spazio, .elastico = 1 } };
}

static void spazio_fisso_misura(struct widget *w, int *l, int *a) {
    *l = *a = ((struct spazio_fisso *)w)->misura;
}

static const struct tipo_widget tipo_spazio_fisso = { spazio_fisso_misura, 0, 0, 0, 0, 0, 0, 0, 0 };

void spazio_fisso_prepara(struct spazio_fisso *s, int misura) {
    *s = (struct spazio_fisso){ .base = { .tipo = &tipo_spazio_fisso }, .misura = misura };
}

static void etichetta_misura(struct widget *w, int *l, int *a) {
    struct etichetta *e = (struct etichetta *)w;
    *l = larghezza_testo(e->font, e->testo);
    *a = e->font->altezza;
}

static void etichetta_disegna(struct widget *w, struct pennello *p) {
    struct etichetta *e = (struct etichetta *)w;
    pennello_colore(p, TESTO_PRIMARIO);   /* chiesto qui, non copiato: segue il tema */
    testo_centrato(p, e->font, w->x, w->y, w->a, e->testo);
}

static const struct tipo_widget tipo_etichetta = { etichetta_misura, 0, etichetta_disegna, 0, 0, 0, 0, 0, 0 };

void etichetta_prepara(struct etichetta *e, const char *testo, const struct font *font) {
    *e = (struct etichetta){ .base = { .tipo = &tipo_etichetta }, .font = font };
    copia_testo(e->testo, testo, MAX_TESTO_WIDGET);
}

void etichetta_testo(struct etichetta *e, const char *testo) {
    copia_testo(e->testo, testo, MAX_TESTO_WIDGET);
    widget_cambiato(&e->base);
}
