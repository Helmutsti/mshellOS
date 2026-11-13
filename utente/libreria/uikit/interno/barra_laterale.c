/* UIKit, i componenti: la barra laterale e le sue voci. Le dichiarazioni sono in
 * barra_laterale.h. */
#include "uikit/uikit.h"
#include "uikit/interno/nucleo.h"

static int laterale_alta(const struct voce_laterale_tk *v) {
    return v->tipo == LATERALE_INTESTAZIONE ? INTESTAZIONE_MENU_ALTA : VOCE_LATERALE_ALTA;
}

/* Dove comincia la voce i (y nella finestra). */
static int laterale_voce_y(const struct barra_laterale_tk *b, int i) {
    int y = b->base.y;
    for (int k = 0; k < i; k++) y += laterale_alta(&b->voci[k]) + BARRA_LATERALE_GAP;
    return y;
}

/* La voce (non un'intestazione) nel punto (x, y), o -1. */
static int laterale_voce_in(const struct barra_laterale_tk *b, int x, int y) {
    const struct widget *w = &b->base;
    if (x < w->x || x >= w->x + w->l) return -1;
    for (int i = 0; i < b->quante; i++) {
        int vy = laterale_voce_y(b, i);
        if (b->voci[i].tipo == LATERALE_VOCE && y >= vy && y < vy + VOCE_LATERALE_ALTA) return i;
    }
    return -1;
}

/* LA VOCE DELLA BARRA LATERALE, nel rettangolo (x, y, l, VOCE_LATERALE_ALTA). */
static void voce_laterale_disegna(struct pennello *p, int x, int y, int l, int icona_, const char *testo,
                                  int sopra, int selezionata) {
    if (selezionata || sopra) {
        pennello_colore(p, selezionata ? CONTROLLO_SELEZIONE_VETRO : CONTROLLO_SOPRA_VETRO);
        riempi_tondo(p, x, y, l, VOCE_LATERALE_ALTA, RAGGIO_CONTROLLO_NUOVO);
    }
    int tx = x + VOCE_LATERALE_DENTRO;
    pennello_colore(p, COLORE_ICONA(COLORE_ICONA_PRIMARIO));
    icona(p, icona_, VOCE_LATERALE_SIMBOLO, tx, y + (VOCE_LATERALE_ALTA - VOCE_LATERALE_SIMBOLO) / 2);
    tx += VOCE_LATERALE_SIMBOLO + VOCE_LATERALE_GAP;
    pennello_colore(p, TESTO_PRIMARIO);
    testo_tagliato(p, selezionata ? FONT_CORPO_MEDIO : FONT_CORPO, tx, y, VOCE_LATERALE_ALTA,
                   x + l - VOCE_LATERALE_DENTRO - tx, testo, 0);
}

static void barra_laterale_misura(struct widget *w, int *l, int *a) {
    (void)w;
    *l = BARRA_LATERALE_LARGHEZZA;
    *a = 0;
}

static void barra_laterale_disegna(struct widget *w, struct pennello *p) {
    struct barra_laterale_tk *b = (struct barra_laterale_tk *)w;
    int x = w->x, l = w->l;
    for (int i = 0; i < b->quante; i++) {
        const struct voce_laterale_tk *v = &b->voci[i];
        int y = laterale_voce_y(b, i);
        if (v->tipo == LATERALE_INTESTAZIONE) intestazione_menu_disegna(p, x, y, v->testo);
        else voce_laterale_disegna(p, x, y, l, v->icona, v->testo, b->sopra == i,
                                   b->selezionata && b->selezionata(b, i, b->dati));
    }
}

static void barra_laterale_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    struct barra_laterale_tk *b = (struct barra_laterale_tk *)w;
    int voce = laterale_voce_in(b, x, y);
    if (pulsanti & PULSANTE_SINISTRO) {
        if (b->premuta < 0) b->premuta = voce;
        return;
    }
    int premuta = b->premuta;
    b->premuta = -1;
    if (pulsanti || premuta < 0 || voce != premuta) return;
    widget_cambiato(w);
    if (b->al_clic) b->al_clic(b, voce, b->dati);
}

/* La voce i va ridisegnata: solo lei. */
static void laterale_voce_cambiata(struct barra_laterale_tk *b, int i) {
    if (i < 0 || i >= b->quante) return;
    widget_cambiato_zona(&b->base, b->base.x, laterale_voce_y(b, i), b->base.l, laterale_alta(&b->voci[i]));
}

static void barra_laterale_movimento(struct widget *w, int x, int y) {
    struct barra_laterale_tk *b = (struct barra_laterale_tk *)w;
    int voce = laterale_voce_in(b, x, y);
    if (voce == b->sopra) return;
    laterale_voce_cambiata(b, b->sopra);
    b->sopra = voce;
    laterale_voce_cambiata(b, voce);
}

static void barra_laterale_sopra(struct widget *w) {
    struct barra_laterale_tk *b = (struct barra_laterale_tk *)w;
    if (!w->sopra && b->sopra >= 0) { laterale_voce_cambiata(b, b->sopra); b->sopra = -1; }
}

static const struct tipo_widget tipo_barra_laterale = {
    barra_laterale_misura, 0, barra_laterale_disegna, barra_laterale_puntatore, 0, 0,
    barra_laterale_sopra, barra_laterale_movimento, 0
};

void barra_laterale_prepara(struct barra_laterale_tk *b, selezionata_laterale selezionata,
                            al_clic_laterale al_clic, void *dati) {
    *b = (struct barra_laterale_tk){ .base = { .tipo = &tipo_barra_laterale, .largo = 1 }, .sopra = -1,
                                     .premuta = -1, .selezionata = selezionata, .al_clic = al_clic,
                                     .dati = dati };
}

void barra_laterale_intestazione(struct barra_laterale_tk *b, const char *testo) {
    if (b->quante < MAX_VOCI_LATERALI)
        b->voci[b->quante++] = (struct voce_laterale_tk){ LATERALE_INTESTAZIONE, testo, NESSUNA_ICONA };
}

int barra_laterale_voce(struct barra_laterale_tk *b, const char *testo, int icona_) {
    if (b->quante >= MAX_VOCI_LATERALI) return -1;
    b->voci[b->quante] = (struct voce_laterale_tk){ LATERALE_VOCE, testo, icona_ };
    return b->quante++;
}
