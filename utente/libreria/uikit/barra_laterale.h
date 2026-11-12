/* LA BARRA LATERALE, dal file Figma (la "Barra laterale" della finestra di Files e il componente
 * "Voce della barra laterale", pagina "Componenti complessi"). Una colonna larga
 * BARRA_LATERALE_LARGHEZZA (200), dentro l'area dei contenuti, senza un fondo suo (si vede il
 * vetro della finestra, il colore della barra del titolo), divisa da quello che ha a destra da
 * una linea BORDO_SEPARATORE di un pixel. Padding SPAZIO_8 intorno, SPAZIO_4 tra un pezzo e
 * l'altro. I pezzi sono di due tipi:
 *
 *   - l'INTESTAZIONE di un gruppo ("Scorciatoie", "Filtri"): la stessa del menu (menu.h);
 *   - la VOCE: alta ALTEZZA_S (32), padding SPAZIO_8 ai lati, il simbolo da 16, SPAZIO_8, il
 *     nome in "corpo". Stati: Normale (piatta, senza fondo), Sopra (fondo CONTROLLO_SFONDO),
 *     Selezionata (fondo CONTROLLO_SFONDO_SOPRA e il nome in "corpo medio"). Raggio
 *     RAGGIO_CONTROLLO_NUOVO.
 *
 * Dal bordo della finestra al primo testo: 8 + 8 = 16 in x (padding della barra + della voce) e
 * 8 + 8 = 16 in y (padding della barra + lo spazio sopra il testo dell'intestazione).
 *
 * Quali voci sono selezionate lo chiede a 'selezionata' (anche piu' d'una: in Files la cartella
 * in cui si e' e il filtro acceso); un clic su una voce chiama 'al_clic' (scatta quando si
 * lascia, sulla stessa voce su cui si e' premuto, come un pulsante). */
#ifndef BARRA_LATERALE_H
#define BARRA_LATERALE_H

#define BARRA_LATERALE_LARGHEZZA 200
#define BARRA_LATERALE_PADDING   SPAZIO_8
#define BARRA_LATERALE_GAP       SPAZIO_4
#define VOCE_LATERALE_ALTA       ALTEZZA_S
#define VOCE_LATERALE_DENTRO     SPAZIO_8
#define VOCE_LATERALE_GAP        SPAZIO_8
#define VOCE_LATERALE_SIMBOLO    ICONA_S
#define MAX_VOCI_LATERALI        16

enum { LATERALE_VOCE, LATERALE_INTESTAZIONE };

struct voce_laterale_tk {
    int tipo;                          /* LATERALE_VOCE o LATERALE_INTESTAZIONE */
    const char *testo;
    int icona;                         /* ICONA_... (le voci) */
};

struct barra_laterale_tk;
typedef void (*al_clic_laterale)(struct barra_laterale_tk *b, int voce, void *dati);
typedef int (*selezionata_laterale)(struct barra_laterale_tk *b, int voce, void *dati);

struct barra_laterale_tk {
    struct widget base;
    struct voce_laterale_tk voci[MAX_VOCI_LATERALI];
    int quante;
    int sopra;                         /* la voce sotto il puntatore, o -1 */
    int premuta;                       /* quella su cui si e' premuto, o -1 */
    selezionata_laterale selezionata;
    al_clic_laterale al_clic;
    void *dati;
};

static inline int laterale_alta(const struct voce_laterale_tk *v) {
    return v->tipo == LATERALE_INTESTAZIONE ? INTESTAZIONE_MENU_ALTA : VOCE_LATERALE_ALTA;
}

/* Dove comincia la voce i (y nella finestra). */
static inline int laterale_voce_y(const struct barra_laterale_tk *b, int i) {
    int y = b->base.y + BARRA_LATERALE_PADDING;
    for (int k = 0; k < i; k++) y += laterale_alta(&b->voci[k]) + BARRA_LATERALE_GAP;
    return y;
}

/* La voce (non un'intestazione) nel punto (x, y), o -1. */
static inline int laterale_voce_in(const struct barra_laterale_tk *b, int x, int y) {
    const struct widget *w = &b->base;
    if (x < w->x + BARRA_LATERALE_PADDING || x >= w->x + w->l - BARRA_LATERALE_PADDING) return -1;
    for (int i = 0; i < b->quante; i++) {
        int vy = laterale_voce_y(b, i);
        if (b->voci[i].tipo == LATERALE_VOCE && y >= vy && y < vy + VOCE_LATERALE_ALTA) return i;
    }
    return -1;
}

/* LA VOCE DELLA BARRA LATERALE, nel rettangolo (x, y, l, VOCE_LATERALE_ALTA). */
static inline void voce_laterale_disegna(struct pennello *p, int x, int y, int l, int icona_, const char *testo,
                                         int sopra, int selezionata) {
    if (selezionata || sopra) {
        pennello_colore(p, selezionata ? CONTROLLO_SFONDO_SOPRA : CONTROLLO_SFONDO_NUOVO);
        riempi_tondo(p, x, y, l, VOCE_LATERALE_ALTA, RAGGIO_CONTROLLO_NUOVO);
    }
    int tx = x + VOCE_LATERALE_DENTRO;
    pennello_colore(p, TESTO_PRIMARIO);
    icona(p, icona_, VOCE_LATERALE_SIMBOLO, tx, y + (VOCE_LATERALE_ALTA - VOCE_LATERALE_SIMBOLO) / 2);
    tx += VOCE_LATERALE_SIMBOLO + VOCE_LATERALE_GAP;
    testo_tagliato(p, selezionata ? FONT_CORPO_MEDIO : FONT_CORPO, tx, y, VOCE_LATERALE_ALTA,
                   x + l - VOCE_LATERALE_DENTRO - tx, testo, 0);
}

static inline void barra_laterale_misura(struct widget *w, int *l, int *a) {
    (void)w;
    *l = BARRA_LATERALE_LARGHEZZA;
    *a = 0;
}

static inline void barra_laterale_disegna(struct widget *w, struct pennello *p) {
    struct barra_laterale_tk *b = (struct barra_laterale_tk *)w;
    pennello_colore(p, BORDO_SEPARATORE);
    riempi_rettangolo(p, w->x + w->l - 1, w->y, 1, w->a);
    int x = w->x + BARRA_LATERALE_PADDING, l = w->l - 2 * BARRA_LATERALE_PADDING;
    for (int i = 0; i < b->quante; i++) {
        const struct voce_laterale_tk *v = &b->voci[i];
        int y = laterale_voce_y(b, i);
        if (v->tipo == LATERALE_INTESTAZIONE) intestazione_menu_disegna(p, x, y, v->testo);
        else voce_laterale_disegna(p, x, y, l, v->icona, v->testo, b->sopra == i,
                                   b->selezionata && b->selezionata(b, i, b->dati));
    }
}

static inline void barra_laterale_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
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

static inline void barra_laterale_movimento(struct widget *w, int x, int y) {
    struct barra_laterale_tk *b = (struct barra_laterale_tk *)w;
    int voce = laterale_voce_in(b, x, y);
    if (voce != b->sopra) { b->sopra = voce; widget_cambiato(w); }
}

static inline void barra_laterale_sopra(struct widget *w) {
    struct barra_laterale_tk *b = (struct barra_laterale_tk *)w;
    if (!w->sopra && b->sopra >= 0) { b->sopra = -1; widget_cambiato(w); }
}

static const struct tipo_widget tipo_barra_laterale = {
    barra_laterale_misura, 0, barra_laterale_disegna, barra_laterale_puntatore, 0, 0,
    barra_laterale_sopra, barra_laterale_movimento, 0
};

/* Alta quanto il suo contenitore ('largo'), larga BARRA_LATERALE_LARGHEZZA. */
static inline void barra_laterale_prepara(struct barra_laterale_tk *b, selezionata_laterale selezionata,
                                          al_clic_laterale al_clic, void *dati) {
    *b = (struct barra_laterale_tk){ .base = { .tipo = &tipo_barra_laterale, .largo = 1 }, .sopra = -1,
                                     .premuta = -1, .selezionata = selezionata, .al_clic = al_clic,
                                     .dati = dati };
}

static inline void barra_laterale_intestazione(struct barra_laterale_tk *b, const char *testo) {
    if (b->quante < MAX_VOCI_LATERALI)
        b->voci[b->quante++] = (struct voce_laterale_tk){ LATERALE_INTESTAZIONE, testo, NESSUNA_ICONA };
}

/* Aggiunge una voce: il suo numero (per 'selezionata' e 'al_clic'). */
static inline int barra_laterale_voce(struct barra_laterale_tk *b, const char *testo, int icona_) {
    if (b->quante >= MAX_VOCI_LATERALI) return -1;
    b->voci[b->quante] = (struct voce_laterale_tk){ LATERALE_VOCE, testo, icona_ };
    return b->quante++;
}

#endif
