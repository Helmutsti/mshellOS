/* UIKit, i componenti: le schede. Le dichiarazioni sono in schede.h. */
#include "uikit/uikit.h"
#include "uikit/interno/nucleo.h"

/* Dove sta la scheda i: x e larghezza. */
static void scheda_posto(const struct schede *s, int i, int *x, int *l) {
    int largo = (s->base.l - (s->quante - 1) * SCHEDE_TRA) / (s->quante ? s->quante : 1);
    *x = s->base.x + i * (largo + SCHEDE_TRA);
    *l = i == s->quante - 1 ? s->base.x + s->base.l - *x : largo;
}

static int scheda_in(const struct schede *s, int x, int y) {
    if (!widget_contiene(&s->base, x, y) || y >= s->base.y + ALTEZZA_S) return -1;
    for (int i = 0; i < s->quante; i++) {
        int sx, sl;
        scheda_posto(s, i, &sx, &sl);
        if (x >= sx && x < sx + sl) return i;
    }
    return -1;
}

static void schede_misura(struct widget *w, int *l, int *a) {
    struct schede *s = (struct schede *)w;
    int largo = 0;
    for (int i = 0; i < s->quante; i++) {
        int t = 2 * PADDING_S + larghezza_testo(FONT_PULSANTE_S, s->nomi[i]);
        if (t > largo) largo = t;
    }
    *l = s->quante * largo + (s->quante ? s->quante - 1 : 0) * SCHEDE_TRA;
    *a = ALTEZZA_S + SCHEDE_LINEA;
}

static void schede_disegna(struct widget *w, struct pennello *p) {
    struct schede *s = (struct schede *)w;
    int alto = ALTEZZA_S - SCHEDE_INDICATORE;
    for (int i = 0; i < s->quante; i++) {
        int x, l;
        scheda_posto(s, i, &x, &l);
        int scelta = i == s->scelta;
        if (!scelta && i == s->sopra) {   /* la "Scheda" del disegno, stato Sopra: un fondo leggero */
            pennello_colore(p, CONTROLLO_SFONDO_NUOVO);
            riempi_tondo(p, x, w->y, l, ALTEZZA_S, RAGGIO_CONTROLLO_NUOVO);
        }
        if (scelta) {
            pennello_colore(p, AZIONE_SFONDO);
            riempi_pillola(p, x + PADDING_S, w->y + alto, l - 2 * PADDING_S, SCHEDE_INDICATORE);
        }
        pennello_colore(p, scelta || i == s->sopra ? TESTO_PRIMARIO : TESTO_SECONDARIO);
        pennello_salva(p);
        pennello_ritaglia(p, x + PADDING_S, w->y, l - 2 * PADDING_S, alto);
        int lt = larghezza_testo(FONT_PULSANTE_S, s->nomi[i]);
        testo_centrato(p, FONT_PULSANTE_S, x + (l - lt) / 2, w->y, alto, s->nomi[i]);
        pennello_ripristina(p);
    }
}

void schede_scegli(struct schede *s, int i) {
    if (i < 0 || i >= s->quante || i == s->scelta) return;
    s->scelta = i;
    widget_cambiato(&s->base);
}

static void schede_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    struct schede *s = (struct schede *)w;
    int i = scheda_in(s, x, y);
    if (pulsanti & PULSANTE_SINISTRO) {
        if (s->premuta < 0) s->premuta = i;
        return;
    }
    int premuta = s->premuta;
    s->premuta = -1;
    if (premuta < 0 || i != premuta || i == s->scelta) return;
    schede_scegli(s, i);
    if (s->al_cambio) s->al_cambio(s, i, s->dati);
}

static void schede_movimento(struct widget *w, int x, int y) {
    struct schede *s = (struct schede *)w;
    int i = scheda_in(s, x, y);
    if (i == s->sopra) return;
    s->sopra = i;
    widget_cambiato(w);
}

static void schede_sopra_cambiato(struct widget *w) {
    struct schede *s = (struct schede *)w;
    if (!w->sopra && s->sopra >= 0) { s->sopra = -1; widget_cambiato(w); }
}

static const struct tipo_widget tipo_schede = {
    schede_misura, 0, schede_disegna, schede_puntatore, 0, 0, schede_sopra_cambiato, schede_movimento, 0
};

void schede_prepara(struct schede *s, al_cambio_schede al_cambio, void *dati) {
    *s = (struct schede){ .base = { .tipo = &tipo_schede, .elastico = 1 }, .sopra = -1,
                          .premuta = -1, .al_cambio = al_cambio, .dati = dati };
}

void schede_aggiungi(struct schede *s, const char *nome) {
    if (s->quante >= MAX_SCHEDE) return;
    copia_testo(s->nomi[s->quante++], nome, MAX_TESTO_WIDGET);
}
