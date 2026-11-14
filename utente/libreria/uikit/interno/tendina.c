/* UIKit, i componenti: il menu a tendina (il campo con la freccia e l'elenco sotto di lui).
 * Le dichiarazioni sono in tendina.h; l'elenco e' il menu di menu.c. */
#include "uikit/uikit.h"
#include "uikit/interno/nucleo.h"

/* Le misure del disegno (stesse del campo): altezza, margine ai lati, lato della freccia,
 * spazio tra testo e freccia, raggio, font del pulsante e larghezza di base. */
struct misure_tendina { int altezza, margine, freccia, spazio, raggio, font, larghezza; };
static const struct misure_tendina misure_tendina[] = {
    { ALTEZZA_S, PADDING_S, ICONA_S, GAP_S, RAGGIO_CAMPO_S, FONT_PULSANTE_S_NUMERO, 200 },
    { ALTEZZA_M, PADDING_M, ICONA_M, GAP_M, RAGGIO_CAMPO_M, FONT_PULSANTE_M_NUMERO, 240 },
    { ALTEZZA_L, PADDING_L, ICONA_L, GAP_L, RAGGIO_CAMPO_L, FONT_PULSANTE_L_NUMERO, 280 },
};

static const struct voce_menu_tk *tendina_voce_scelta(const struct tendina_tk *t) {
    for (int i = 0; i < t->quante; i++)
        if (t->voci[i].azione == t->valore) return &t->voci[i];
    return 0;
}

static void tendina_misura(struct widget *w, int *l, int *a) {
    struct tendina_tk *t = (struct tendina_tk *)w;
    *l = misure_tendina[t->misura].larghezza;
    *a = misure_tendina[t->misura].altezza;
}

static void tendina_disegna(struct widget *w, struct pennello *p) {
    struct tendina_tk *t = (struct tendina_tk *)w;
    const struct misure_tendina *m = &misure_tendina[t->misura];
    const struct font *font = font_del_sistema(m->font);
    int spento = w->disabilitato, aperto = menu_aperto(&t->menu);
    int attivo = aperto || w->fuoco;
    struct tondo r = tondo(w->x, w->y, w->l, w->a, m->raggio);
    if (!spento) {
        const struct strato_ombra *ombre = attivo ? OMBRA_TAB(focus) : OMBRA_TAB(incassato);
        int quante = attivo ? OMBRA_N(focus) : OMBRA_N(incassato);
        ombre_strati_fuori(p, &r, ombre, quante);
        pennello_colore(p, SUPERFICIE_INCASSATA);
        riempi_forma(p, &r);
        ombre_strati_dentro(p, &r, ombre, quante);
        if (attivo) {
            pennello_colore(p, BORDO_FOCUS);
            contorna_forma(p, &r, BORDO_FOCUS_SPESSORE);
        }
    } else {
        pennello_colore(p, CONTROLLO_SFONDO_DISABILITATO);
        riempi_forma(p, &r);
    }

    const struct voce_menu_tk *scelta = tendina_voce_scelta(t);
    /* la freccia: secondaria col segnaposto, primaria con una scelta o con l'elenco aperto */
    pennello_colore(p, COLORE_ICONA(spento ? COLORE_ICONA_ATTENUATO
                                    : scelta || aperto ? COLORE_ICONA_PRIMARIO : COLORE_ICONA_SECONDARIO));
    int fx = w->x + w->l - m->margine - m->freccia;
    icona(p, aperto ? ICONA_CHEVRON_UP : ICONA_CHEVRON_DOWN, m->freccia, fx, w->y + (w->a - m->freccia) / 2);

    pennello_colore(p, spento ? TESTO_DISABILITATO : scelta ? TESTO_PRIMARIO : TESTO_SECONDARIO);
    testo_tagliato(p, font, w->x + m->margine, w->y, w->a, fx - m->spazio - (w->x + m->margine),
                   scelta ? scelta->testo : t->segnaposto, 0);
}

void tendina_apri(struct tendina_tk *t) {
    struct widget *w = &t->base;
    if (w->disabilitato || !t->quante || menu_aperto(&t->menu)) return;
    t->menu.larghezza = w->l;
    menu_apri(&t->menu, t->voci, t->quante, w->x, w->y + w->a + TENDINA_STACCO);
    widget_cambiato(w);
}

static void tendina_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    struct tendina_tk *t = (struct tendina_tk *)w;
    if (pulsanti & PULSANTE_SINISTRO) { t->premuto = 1; return; }
    if (!t->premuto) return;
    t->premuto = 0;
    if (widget_contiene(w, x, y)) tendina_apri(t);
}

/* Con il fuoco, Invio e spazio aprono l'elenco, con la scelta attuale accesa: da li' in poi
 * i tasti li ascolta il menu. */
static void tendina_tasto(struct widget *w, unsigned int tasto) {
    struct tendina_tk *t = (struct tendina_tk *)w;
    if (tasto != '\n' && tasto != '\r' && tasto != ' ') return;
    tendina_apri(t);
    for (int i = 0; i < t->quante; i++)
        if (t->voci[i].azione == t->valore) { menu_seleziona(&t->menu, i); break; }
}

static const struct tipo_widget tipo_tendina = {
    tendina_misura, 0, tendina_disegna, tendina_puntatore, tendina_tasto, 0, 0, 0, 1
};

/* Il menu e' chiuso (scelta, Esc, clic fuori): il campo torna come prima. */
static void tendina_chiuso(struct menu_tk *m, void *dati) {
    (void)m;
    widget_cambiato(&((struct tendina_tk *)dati)->base);
}

static void tendina_scelto(struct menu_tk *m, int azione, void *dati) {
    (void)m;
    struct tendina_tk *t = dati;
    t->valore = azione;
    widget_cambiato(&t->base);
    if (t->al_cambio) t->al_cambio(t, azione, t->dati);
}

static int tendina_spuntata(int azione, void *dati) {
    return ((struct tendina_tk *)dati)->valore == azione;
}

void tendina_prepara(struct tendina_tk *t, const struct voce_menu_tk *voci, int quante, int valore,
                     const char *segnaposto, al_cambio_tendina al_cambio, void *dati) {
    *t = (struct tendina_tk){ .base = { .tipo = &tipo_tendina }, .misura = MISURA_M, .voci = voci,
                              .quante = quante, .valore = valore, .segnaposto = segnaposto,
                              .al_cambio = al_cambio, .dati = dati };
    menu_prepara(&t->menu, MISURA_M, 0, 0, tendina_spuntata, tendina_scelto, t);
    t->menu.alla_chiusura = tendina_chiuso;
    t->menu.dati_chiusura = t;
}

void tendina_stile(struct tendina_tk *t, int misura) {
    t->misura = misura;
    t->menu.misura = misura == MISURA_S ? MISURA_S : MISURA_M;   /* L: voci da 40, come M */
    widget_cambiato(&t->base);
}

void tendina_scegli(struct tendina_tk *t, int valore) {
    t->valore = valore;
    widget_cambiato(&t->base);
}

int tendina_valore(const struct tendina_tk *t) { return t->valore; }

void tendina_aggiungi_menu(struct finestra_tk *f, struct tendina_tk *t) {
    menu_aggiungi(f, &t->menu);
}

int tendina_aperta(const struct tendina_tk *t) { return menu_aperto(&t->menu); }
