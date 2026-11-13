/* UIKit, il nucleo: i controlli che si premono. Il pulsante (con la pressione, che usano
 * anche i pulsanti della barra), la casella di spunta, la scelta singola e l'interruttore.
 * Le dichiarazioni sono in uikit.h e in interno/nucleo.h. */
#include "uikit/uikit.h"
#include "uikit/interno/nucleo.h"

const struct misure_pulsante misure_pulsante[] = {
    { ALTEZZA_S, PADDING_S, GAP_S, ICONA_S, FONT_PULSANTE_S_NUMERO },
    { ALTEZZA_M, PADDING_M, GAP_M, ICONA_M, FONT_PULSANTE_M_NUMERO },
    { ALTEZZA_L, PADDING_L, GAP_L, ICONA_L, FONT_PULSANTE_L_NUMERO },
    { ALTEZZA_XS, PADDING_S, GAP_S, ICONA_XS, FONT_PULSANTE_S_NUMERO },   /* XS: pensato solo icona */
};

static void pulsante_misura(struct widget *w, int *l, int *a) {
    struct pulsante *b = (struct pulsante *)w;
    const struct misure_pulsante *m = &misure_pulsante[b->dimensione];
    *a = m->altezza;
    if (!b->testo[0]) { *l = m->altezza; return; }             /* solo icona: tondo, lato = altezza */
    *l = 2 * m->margine + larghezza_testo(font_del_sistema(m->font), b->testo);
    if (b->icona != NESSUNA_ICONA) *l += m->icona + m->spazio;
}

void pulsante_disegna(struct widget *w, struct pennello *p) {
    struct pulsante *b = (struct pulsante *)w;
    const struct misure_pulsante *m = &misure_pulsante[b->dimensione];
    int premuto = b->premuto && b->dentro;
    int sopra = w->sopra, spento = w->disabilitato;
    if (spento) premuto = sopra = 0;
    struct tondo t = tondo(w->x, w->y, w->l, w->a, w->a / 2);
    const struct strato_ombra *ombre = 0;
    int quante = 0;
    int pieno = b->colore == PRIMARIO || b->colore >= PERICOLO;   /* Primario e i colorati */
    if (!premuto && !spento && pieno) {
        ombre = OMBRA_TAB(sollevato_pieno); quante = OMBRA_N(sollevato_pieno);
    } else if (!premuto && !spento && b->colore == SECONDARIO) {
        ombre = OMBRA_TAB(sollevato); quante = OMBRA_N(sollevato);
    }
    if (ombre) ombre_strati_fuori(p, &t, ombre, quante);

    /* il testo e l'icona: l'icona prende il colore dalla raccolta "Colore icona" (tema.h),
     * con il modo che il disegno da' a ogni tipo di pulsante */
    unsigned int fondo = 0, testo = TESTO_PRIMARIO;
    int modo_icona = COLORE_ICONA_PRIMARIO;
    if (spento) {
        if (b->colore != GHOST) fondo = CONTROLLO_SFONDO_DISABILITATO;
        testo = TESTO_DISABILITATO;
        modo_icona = COLORE_ICONA_ATTENUATO;
    } else if (b->colore == PRIMARIO) {
        fondo = premuto ? AZIONE_SFONDO_PREMUTO : sopra ? AZIONE_SFONDO_SOPRA : AZIONE_SFONDO;
        testo = AZIONE_TESTO;
        modo_icona = COLORE_ICONA_INVERSO;
    } else if (b->colore == PERICOLO) {
        fondo = premuto ? PERICOLO_SFONDO_PREMUTO : sopra ? PERICOLO_SFONDO_SOPRA : PERICOLO_SFONDO;
        testo = PERICOLO_TESTO;
        modo_icona = COLORE_ICONA_PERICOLO;
    } else if (b->colore == SUCCESSO) {
        fondo = premuto ? SUCCESSO_SFONDO_PREMUTO : sopra ? SUCCESSO_SFONDO_SOPRA : SUCCESSO_SFONDO;
        testo = SUCCESSO_TESTO;
        modo_icona = COLORE_ICONA_SUCCESSO;
    } else if (b->colore == AVVISO) {
        fondo = premuto ? AVVISO_SFONDO_PREMUTO : sopra ? AVVISO_SFONDO_SOPRA : AVVISO_SFONDO;
        testo = AVVISO_TESTO;
        modo_icona = COLORE_ICONA_AVVISO;
    } else if (b->colore == INFORMAZIONE) {
        fondo = premuto ? INFORMAZIONE_SFONDO_PREMUTO : sopra ? INFORMAZIONE_SFONDO_SOPRA : INFORMAZIONE_SFONDO;
        testo = INFORMAZIONE_TESTO;
        modo_icona = COLORE_ICONA_INFORMAZIONE;
    } else if (b->colore == SECONDARIO) {
        fondo = premuto ? CONTROLLO_SFONDO_PREMUTO : sopra ? CONTROLLO_SFONDO_SOPRA : CONTROLLO_SFONDO_NUOVO;
    } else {                           /* Ghost: nessun fondo a riposo */
        fondo = premuto ? CONTROLLO_SFONDO_SOPRA : sopra ? CONTROLLO_SFONDO_NUOVO : 0;
    }
    if (fondo) {
        pennello_colore(p, fondo);
        riempi_forma(p, &t);
    }
    if (ombre) ombre_strati_dentro(p, &t, ombre, quante);

    /* icona e testo, centrati insieme */
    const struct font *font = font_del_sistema(m->font);
    int largo = b->testo[0] ? larghezza_testo(font, b->testo) : 0;
    if (b->icona != NESSUNA_ICONA) largo += m->icona + (b->testo[0] ? m->spazio : 0);
    int x = w->x + (w->l - largo) / 2;
    if (b->icona != NESSUNA_ICONA) {
        pennello_colore(p, COLORE_ICONA(modo_icona));
        icona(p, b->icona, m->icona, x, w->y + (w->a - m->icona) / 2);
        x += m->icona + m->spazio;
    }
    pennello_colore(p, testo);
    if (b->testo[0]) testo_centrato(p, font, x, w->y, w->a, b->testo);
}

int pressione(struct widget *w, int *premuto, int *dentro, int x, int y,
              unsigned int pulsanti) {
    int giu = pulsanti & PULSANTE_SINISTRO, sopra = widget_contiene(w, x, y);
    if (giu && !*premuto) {
        *premuto = 1;
        *dentro = sopra;
        widget_cambiato(w);
    } else if (giu && sopra != *dentro) {
        *dentro = sopra;
        widget_cambiato(w);
    } else if (!giu && *premuto) {
        *premuto = 0;
        widget_cambiato(w);
        return sopra;
    }
    return 0;
}

static void pulsante_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    struct pulsante *b = (struct pulsante *)w;
    if (pressione(w, &b->premuto, &b->dentro, x, y, pulsanti) && b->al_clic) b->al_clic(b, b->dati);
}

static const struct tipo_widget tipo_pulsante = {
    pulsante_misura, 0, pulsante_disegna, pulsante_puntatore, 0, 0, 0, 0, 1
};

void pulsante_prepara(struct pulsante *b, const char *testo, int icona,
                      al_clic_pulsante al_clic, void *dati) {
    *b = (struct pulsante){ .base = { .tipo = &tipo_pulsante }, .icona = icona,
                            .colore = SECONDARIO, .dimensione = MISURA_M, .al_clic = al_clic, .dati = dati };
    copia_testo(b->testo, testo, MAX_TESTO_WIDGET);
}

void pulsante_stile(struct pulsante *b, int colore, int dimensione) {
    b->colore = colore;
    b->dimensione = dimensione;
    widget_cambiato(&b->base);
}

static void casella_misura(struct widget *w, int *l, int *a) {
    struct casella *c = (struct casella *)w;
    *l = CASELLA_LATO + CASELLA_GAP + larghezza_testo(FONT_CORPO, c->testo);
    *a = CASELLA_LATO > FONT_CORPO->altezza ? CASELLA_LATO : FONT_CORPO->altezza;
}

/* La casella e la scelta CONTENGONO una risposta, quindi sono INCASSATE: fondo
 * SUPERFICIE_INCASSATA, ombra interna (e una sottile esterna), mai bordo. Accese cambiano
 * solo il colore del fondo (AZIONE_SFONDO, come un'azione) e mettono il segno sopra; il
 * livello non cambia. Disabilitate sono piatte: fondo CONTROLLO_SFONDO_DISABILITATO, niente
 * ombra, segno e testo TESTO_DISABILITATO. Il tondo della scelta accesa e' un punto di 8 px
 * disegnato con riempi_cerchio (non piu' un'immagine precalcolata). */
static void casella_disegna(struct widget *w, struct pennello *p) {
    struct casella *c = (struct casella *)w;
    int spento = w->disabilitato, acceso = c->valore != SPENTO;
    int x = w->x, y = w->y + (w->a - CASELLA_LATO) / 2;
    struct tondo t = tondo(x, y, CASELLA_LATO, CASELLA_LATO, c->tonda ? CASELLA_LATO / 2 : RAGGIO_CASELLA);
    const struct strato_ombra *ombre = OMBRA_TAB(incassato);
    int quante = OMBRA_N(incassato);

    if (!spento) ombre_strati_fuori(p, &t, ombre, quante);
    unsigned int fondo = SUPERFICIE_INCASSATA;
    if (spento) fondo = CONTROLLO_SFONDO_DISABILITATO;
    else if (acceso && !c->tonda) fondo = w->sopra ? AZIONE_SFONDO_SOPRA : AZIONE_SFONDO;
    pennello_colore(p, fondo);
    riempi_forma(p, &t);
    if (!spento) ombre_strati_dentro(p, &t, ombre, quante);

    unsigned int segno = spento ? TESTO_DISABILITATO : AZIONE_SFONDO;
    if (acceso && c->tonda) {
        pennello_colore(p, segno);
        riempi_cerchio(p, x + CASELLA_LATO / 2, y + CASELLA_LATO / 2, SCELTA_PUNTO / 2);
    } else if (acceso) {
        pennello_colore(p, COLORE_ICONA(spento ? COLORE_ICONA_ATTENUATO : COLORE_ICONA_INVERSO));
        int lato = CASELLA_LATO - 4;                     /* il segno, 2 px di margine */
        icona(p, c->valore == ACCESO ? ICONA_CHECK : ICONA_MINUS, lato, x + 2, y + 2);
    }
    pennello_colore(p, spento ? TESTO_DISABILITATO : TESTO_PRIMARIO);
    testo_centrato(p, FONT_CORPO, x + CASELLA_LATO + CASELLA_GAP, w->y, w->a, c->testo);
}

static void casella_puntatore(struct widget *w, int x, int y, unsigned int pulsanti);

/* Spegne le altre scelte dello stesso gruppo, in tutto l'albero. */
static void spegni_gruppo(struct widget *w, struct casella *tranne) {
    for (int i = 0; i < w->quanti_figli; i++) {
        struct widget *f = w->figli[i];
        if (f->tipo->puntatore == casella_puntatore) {
            struct casella *c = (struct casella *)f;
            if (c != tranne && c->tonda && c->gruppo == tranne->gruppo && c->valore) {
                c->valore = SPENTO;
                widget_cambiato(f);
            }
        }
        spegni_gruppo(f, tranne);
    }
}

static void casella_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    struct casella *c = (struct casella *)w;
    if (pulsanti & PULSANTE_SINISTRO) { c->premuto = 1; return; }
    if (!c->premuto) return;
    c->premuto = 0;
    if (!widget_contiene(w, x, y)) return;   /* lasciato fuori: ci ha ripensato */
    if (c->tonda) {
        if (c->valore == ACCESO) return;     /* una scelta accesa resta accesa */
        c->valore = ACCESO;
        struct widget *radice = w;
        while (radice->genitore) radice = radice->genitore;
        spegni_gruppo(radice, c);
    } else {
        c->valore = c->valore == ACCESO ? SPENTO : ACCESO;   /* "misto" diventa acceso */
    }
    widget_cambiato(w);
    if (c->al_cambio) c->al_cambio(c, c->dati);
}

static const struct tipo_widget tipo_casella = {
    casella_misura, 0, casella_disegna, casella_puntatore, 0, 0, 0, 0, 1
};

void casella_prepara(struct casella *c, const char *testo, int valore,
                     al_cambio_casella al_cambio, void *dati) {
    *c = (struct casella){ .base = { .tipo = &tipo_casella }, .valore = valore,
                           .al_cambio = al_cambio, .dati = dati };
    copia_testo(c->testo, testo, MAX_TESTO_WIDGET);
}

void scelta_prepara(struct casella *c, const char *testo, void *gruppo, int accesa,
                    al_cambio_casella al_cambio, void *dati) {
    casella_prepara(c, testo, accesa ? ACCESO : SPENTO, al_cambio, dati);
    c->tonda = 1;
    c->gruppo = gruppo;
}

static void interruttore_misura(struct widget *w, int *l, int *a) {
    (void)w;
    *l = INTERRUTTORE_LARGO;
    *a = INTERRUTTORE_ALTO;
}

static void interruttore_disegna(struct widget *w, struct pennello *p) {
    struct interruttore *i = (struct interruttore *)w;
    int spento = w->disabilitato;
    int x = w->x, y = w->y + (w->a - INTERRUTTORE_ALTO) / 2;
    struct tondo binario = tondo(x, y, INTERRUTTORE_LARGO, INTERRUTTORE_ALTO, INTERRUTTORE_ALTO / 2);
    if (!spento) ombre_strati_fuori(p, &binario, OMBRA_TAB(incassato), OMBRA_N(incassato));
    pennello_colore(p, spento ? CONTROLLO_SFONDO_DISABILITATO : i->acceso ? AZIONE_SFONDO : SUPERFICIE_INCASSATA);
    riempi_forma(p, &binario);
    if (!spento) ombre_strati_dentro(p, &binario, OMBRA_TAB(incassato), OMBRA_N(incassato));

    int px = i->acceso ? x + INTERRUTTORE_LARGO - INTERRUTTORE_MARGINE - INTERRUTTORE_POMELLO
                       : x + INTERRUTTORE_MARGINE;
    struct tondo pomello = tondo(px, y + INTERRUTTORE_MARGINE, INTERRUTTORE_POMELLO, INTERRUTTORE_POMELLO,
                                 INTERRUTTORE_POMELLO / 2);
    if (!spento) ombre_strati_fuori(p, &pomello, OMBRA_TAB(sollevato), OMBRA_N(sollevato));
    pennello_colore(p, spento ? TESTO_DISABILITATO : i->acceso ? AZIONE_TESTO : SUPERFICIE_ELEVATA);
    riempi_forma(p, &pomello);
    if (!spento) ombre_strati_dentro(p, &pomello, OMBRA_TAB(sollevato), OMBRA_N(sollevato));
}

static void interruttore_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    struct interruttore *i = (struct interruttore *)w;
    if (pulsanti & PULSANTE_SINISTRO) { i->premuto = 1; return; }
    if (!i->premuto) return;
    i->premuto = 0;
    if (!widget_contiene(w, x, y)) return;   /* lasciato fuori: ci ha ripensato */
    i->acceso = !i->acceso;
    widget_cambiato(w);
    if (i->al_cambio) i->al_cambio(i, i->dati);
}

static const struct tipo_widget tipo_interruttore = {
    interruttore_misura, 0, interruttore_disegna, interruttore_puntatore, 0, 0, 0, 0, 1
};

void interruttore_prepara(struct interruttore *i, int acceso, al_cambio_interruttore al_cambio,
                          void *dati) {
    *i = (struct interruttore){ .base = { .tipo = &tipo_interruttore }, .acceso = acceso,
                                .al_cambio = al_cambio, .dati = dati };
}
