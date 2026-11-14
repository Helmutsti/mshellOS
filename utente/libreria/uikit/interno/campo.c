/* UIKit, il nucleo: il campo di testo e la ricerca (lo stesso widget, a pillola). Le
 * dichiarazioni sono in uikit.h e in interno/nucleo.h; qui le misure del disegno, che servono
 * solo al campo. */
#include "uikit/uikit.h"
#include "uikit/interno/nucleo.h"

/* Le misure del disegno, una riga per misura: altezza, margine ai lati (dentro), lato
 * dell'icona, spazio dopo di lei, raggio degli angoli (RAGGIO_CAMPO_S/M/L: il campo e' un
 * rettangolo arrotondato, NON una pillola; la ricerca ha il raggio = meta' altezza) e font.
 * Il font e' Plex Regular (non Medium come i pulsanti: si scrive, non si preme) della stessa
 * dimensione del testo dei pulsanti: 13, 14, 16. Larghezza: 280 il campo (di solito si
 * allarga), 180 la ricerca. Il font per numero, come in misure_pulsante. */
struct misure_campo { int altezza, margine, icona, spazio, raggio, font; };
static const struct misure_campo misure_campo[] = {
    { ALTEZZA_S, PADDING_S, ICONA_S, GAP_S, RAGGIO_CAMPO_S, MATERIALE_plex_13 },
    { ALTEZZA_M, PADDING_M, ICONA_M, GAP_M, RAGGIO_CAMPO_M, MATERIALE_plex_14 },
    { ALTEZZA_L, PADDING_L, ICONA_L, GAP_L, RAGGIO_CAMPO_L, MATERIALE_plex_16 },
};
#define CAMPO_LARGHEZZA_NORMALE 280

static void campo_misura(struct widget *w, int *l, int *a) {
    struct campo *c = (struct campo *)w;
    *l = c->larghezza ? c->larghezza : c->piccolo ? RICERCA_LARGHEZZA_NUOVA : CAMPO_LARGHEZZA_NORMALE;   /* il campo normale di solito si allarga */
    *a = misure_campo[c->misura].altezza;
}

void campo_disegna(struct widget *w, struct pennello *p) {
    struct campo *c = (struct campo *)w;
    const struct misure_campo *m = &misure_campo[c->misura];
    const struct font *font = font_del_sistema(m->font);
    int spento = w->disabilitato;
    struct tondo t = tondo(w->x, w->y, w->l, w->a, c->piccolo ? w->a / 2 : m->raggio);
    if (!spento) {
        const struct strato_ombra *ombre = w->fuoco ? OMBRA_TAB(focus) : OMBRA_TAB(incassato);
        int quante = w->fuoco ? OMBRA_N(focus) : OMBRA_N(incassato);
        ombre_strati_fuori(p, &t, ombre, quante);
        pennello_colore(p, SUPERFICIE_INCASSATA);
        riempi_forma(p, &t);
        ombre_strati_dentro(p, &t, ombre, quante);
        if (c->errore || w->fuoco) {       /* errore o fuoco: bordo di un pixel e mezzo */
            pennello_colore(p, BORDO_FOCUS);
            contorna_forma(p, &t, BORDO_FOCUS_SPESSORE);
        }
    } else {
        pennello_colore(p, CONTROLLO_SFONDO_DISABILITATO);
        riempi_forma(p, &t);
    }

    pennello_salva(p);
    pennello_ritaglia(p, w->x + 1, w->y + 1, w->l - 2, w->a - 2);
    unsigned int colore_testo = spento ? TESTO_DISABILITATO : c->errore ? TESTO_SECONDARIO : TESTO_PRIMARIO;
    int x = w->x + m->margine;
    int destra = w->x + w->l - m->margine;
    if (c->errore && !spento) {            /* il triangolo di avviso, a destra */
        pennello_colore(p, COLORE_ICONA(COLORE_ICONA_SECONDARIO));
        icona(p, ICONA_TRIANGLE_ALERT, m->icona, destra - m->icona, w->y + (w->a - m->icona) / 2);
        destra -= m->icona + m->spazio;
    }
    if (c->icona != NESSUNA_ICONA) {   /* come nel disegno: secondaria, primaria con il fuoco */
        pennello_colore(p, COLORE_ICONA(spento ? COLORE_ICONA_ATTENUATO
                                        : w->fuoco ? COLORE_ICONA_PRIMARIO : COLORE_ICONA_SECONDARIO));
        icona(p, c->icona, m->icona, x, w->y + (w->a - m->icona) / 2);
        x += m->icona + m->spazio;
    }
    pennello_colore(p, colore_testo);
    if (c->testo[0] || w->fuoco) {
        /* LO SCORRIMENTO. Il testo si vede in una striscia, dal suo inizio fino al margine
         * destro (lo stesso di sinistra). Se e' piu' lungo, si fa scorrere
         * a sinistra quanto basta perche' il cursore resti dentro, come in ogni campo di
         * testo. Il cursore e' sempre in fondo (la tastiera non manda ancora le frecce),
         * quindi basta guardare la fine del testo: scrivendo il testo va a sinistra,
         * cancellando torna a destra. Senza fuoco si vede l'inizio, come sul Mac. */
        int fine = larghezza_testo(font, c->testo) + CAMPO_CURSORE_SPAZIO + 2;   /* il cursore compreso */
        int scorrimento = w->fuoco && x + fine > destra ? x + fine - destra : 0;
        pennello_ritaglia(p, x - 2, w->y, destra - (x - 2), w->a);  /* 2: le lettere che sbordano */
        x = testo_centrato(p, font, x - scorrimento, w->y, w->a, c->testo);
        if (w->fuoco) {                    /* il cursore: una barretta alta quanto la riga */
            int alto = font->altezza;
            struct tondo cursore = tondo(x + CAMPO_CURSORE_SPAZIO, w->y + (w->a - alto) / 2, 2, alto, 0);
            riempi_forma(p, &cursore);
        }
    } else {
        pennello_colore(p, spento ? TESTO_DISABILITATO : TESTO_SECONDARIO);
        testo_centrato(p, font, x, w->y, w->a, c->suggerimento);
    }
    pennello_ripristina(p);
}

/* Il clic gli da' il fuoco (lo fa UIKit): qui non serve altro. */
static void campo_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    (void)w; (void)x; (void)y; (void)pulsanti;
}

/* I tasti: le lettere si aggiungono in fondo, "cancella" toglie l'ultima. */
static void campo_tasto(struct widget *w, unsigned int tasto) {
    struct campo *c = (struct campo *)w;
    int n = lunghezza(c->testo);
    if (tasto == '\b') {
        if (!n) return;
        c->testo[n - 1] = 0;
    } else if (tasto >= ' ' && tasto != 0x7F) {
        if (n >= MAX_TESTO_WIDGET - 1) return;
        c->testo[n] = (char)tasto;
        c->testo[n + 1] = 0;
    } else {
        return;                            /* Invio, Tab...: non sono per lui */
    }
    widget_cambiato(w);
    if (c->al_cambio) c->al_cambio(c, c->dati);
}

static const struct tipo_widget tipo_campo = {
    campo_misura, 0, campo_disegna, campo_puntatore, campo_tasto, 0, 0, 0, 1
};

void campo_prepara(struct campo *c, const char *testo, const char *suggerimento,
                   int icona, al_cambio_campo al_cambio, void *dati) {
    *c = (struct campo){ .base = { .tipo = &tipo_campo, .largo = 1, .forma_puntatore = FORMA_TESTO }, .icona = icona, .misura = MISURA_M,
                         .al_cambio = al_cambio, .dati = dati };
    copia_testo(c->testo, testo, MAX_TESTO_WIDGET);
    copia_testo(c->suggerimento, suggerimento, MAX_TESTO_WIDGET);
}

void campo_stile(struct campo *c, int misura) {
    c->misura = misura;
    widget_cambiato(&c->base);
}

void campo_errore(struct campo *c, int errore) {
    c->errore = errore;
    widget_cambiato(&c->base);
}

void ricerca_prepara(struct campo *c, const char *suggerimento,
                     al_cambio_campo al_cambio, void *dati) {
    campo_prepara(c, "", suggerimento, ICONA_SEARCH, al_cambio, dati);
    c->piccolo = 1;
    c->misura = MISURA_S;
    c->base.largo = 0;
}
