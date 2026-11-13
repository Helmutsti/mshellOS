/* UIKit, il nucleo: l'avviso, il messaggio che interrompe e chiede una risposta (come si
 * apre, si dispone, si disegna e risponde). Le dichiarazioni sono in uikit.h e in
 * interno/nucleo.h. */
#include "uikit/uikit.h"
#include "uikit/interno/nucleo.h"

static int avviso_larghezza(const struct avviso *v) {
    return v->tre ? AVVISO_LARGHEZZA_TRE : AVVISO_LARGHEZZA;
}

static int avviso_righe(struct avviso *v, char righe[][MAX_RIGA_NOME]) {
    return testo_a_capo(FONT_CORPO, v->messaggio, avviso_larghezza(v) - 2 * FINESTRA_PADDING,
                        righe, AVVISO_RIGHE);
}

/* L'altezza del riquadro: margine, titolo, messaggio, spazio, pulsanti, margine. */
static int avviso_altezza(struct avviso *v) {
    char righe[AVVISO_RIGHE][MAX_RIGA_NOME];
    int n = v->messaggio[0] ? avviso_righe(v, righe) : 0;
    int pl, pa;
    widget_misura(&v->conferma.base, &pl, &pa);
    return 2 * FINESTRA_PADDING + INTERLINEA_SEZIONE + (n ? SPAZIO_8 + n * INTERLINEA_CORPO : 0) +
           SPAZIO_24 + pa;
}

void avviso_sistema(struct avviso *v, int fl, int fa) {
    v->l = avviso_larghezza(v);
    v->a = avviso_altezza(v);
    v->x = (fl - v->l) / 2;
    v->y = (fa - v->a) / 2;
    v->base.x = v->x; v->base.y = v->y; v->base.l = v->l; v->base.a = v->a;
    /* da destra: la conferma, poi (se c'e') l'alternativa, poi Annulla, a SPAZIO_8 */
    struct pulsante *fila[3] = { &v->conferma, v->tre ? &v->alternativa : 0, &v->annulla };
    int ca, x = v->x + v->l - FINESTRA_PADDING;
    int dummy;
    widget_misura(&v->conferma.base, &dummy, &ca);
    int y = v->y + v->a - FINESTRA_PADDING - ca;
    for (int i = 0; i < 3; i++) {
        if (!fila[i]) continue;
        int bl, ba;
        widget_misura(&fila[i]->base, &bl, &ba);
        x -= bl;
        fila[i]->base.x = x; fila[i]->base.y = y; fila[i]->base.l = bl; fila[i]->base.a = ba;
        x -= SPAZIO_8;
    }
}

void avviso_sotto(struct avviso *v, struct pennello *p, int fl, int fa) {
    if (v->bloccante) {
        struct tondo velo = tondo(0, 0, fl, fa, RAGGIO_FINESTRA_NUOVO);
        pennello_colore(p, SUPERFICIE_VELO);
        riempi_forma(p, &velo);
    }
    struct tondo r = tondo(v->x, v->y, v->l, v->a, RAGGIO_FINESTRA_NUOVO);
    ombre_strati_fuori(p, &r, OMBRA_TAB(finestra), OMBRA_N(finestra));
}

static void avviso_disegna(struct widget *w, struct pennello *p) {
    struct avviso *v = (struct avviso *)w;
    struct tondo r = tondo(v->x, v->y, v->l, v->a, RAGGIO_FINESTRA_NUOVO);
    pennello_salva(p);
    /* Pieno, non vetro: in Figma l'avviso e' di vetro con la sfocatura dello sfondo, ma
     * dentro una finestra nessuno sfoca quello che sta sotto (lo fa solo il compositore,
     * sotto le finestre): il vetro lascerebbe leggere il testo di sotto. Come i menu di Files. */
    pennello_colore(p, SUPERFICIE_ELEVATA);
    riempi_forma(p, &r);
    ombre_strati_dentro(p, &r, OMBRA_TAB(finestra), OMBRA_N(finestra));
    int x = v->x + FINESTRA_PADDING, y = v->y + FINESTRA_PADDING;
    pennello_colore(p, TESTO_PRIMARIO);
    testo_centrato(p, FONT_SEZIONE_NUOVO, x, y, INTERLINEA_SEZIONE, v->titolo);
    y += INTERLINEA_SEZIONE;
    if (v->messaggio[0]) {
        char righe[AVVISO_RIGHE][MAX_RIGA_NOME];
        int n = avviso_righe(v, righe);
        y += SPAZIO_8;
        pennello_colore(p, TESTO_SECONDARIO);
        for (int i = 0; i < n; i++, y += INTERLINEA_CORPO)
            testo_centrato(p, FONT_CORPO, x, y, INTERLINEA_CORPO, righe[i]);
    }
    pennello_ripristina(p);
}

/* Il riquadro prende i clic che non vanno ai pulsanti: cosi' sotto non arriva niente. (Se
 * l'avviso e' bloccante, i clic fuori dal riquadro li butta via la finestra.) */
static void avviso_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    (void)w; (void)x; (void)y; (void)pulsanti;
}

static void avviso_chiudi(struct avviso *v);

void avviso_rispondi(struct avviso *v, int scelta) {
    avviso_chiudi(v);
    if (v->al_risposta) v->al_risposta(v, scelta, v->dati);
}

void avviso_tasto(struct widget *w, unsigned int tasto) {
    struct avviso *v = (struct avviso *)w;
    if (tasto == '\n' || tasto == '\r') avviso_rispondi(v, AVVISO_CONFERMA);
}

static const struct tipo_widget tipo_avviso = {
    0, 0, avviso_disegna, avviso_puntatore, avviso_tasto, 0, 0, 0, 1
};

static void avviso_al_annulla(struct pulsante *b, void *dati) { (void)b; avviso_rispondi(dati, AVVISO_ANNULLA); }

static void avviso_al_conferma(struct pulsante *b, void *dati) { (void)b; avviso_rispondi(dati, AVVISO_CONFERMA); }

static void avviso_al_alternativa(struct pulsante *b, void *dati) { (void)b; avviso_rispondi(dati, AVVISO_ALTERNATIVA); }

void avviso_prepara(struct avviso *v, const char *titolo, const char *messaggio,
                    const char *conferma, int tipo, int bloccante,
                    al_risposta_avviso al_risposta, void *dati) {
    *v = (struct avviso){ .base = { .tipo = &tipo_avviso, .nascosto = 1 }, .bloccante = bloccante,
                          .al_risposta = al_risposta, .dati = dati };
    copia_testo(v->titolo, titolo, MAX_TESTO_WIDGET);
    copia_testo(v->messaggio, messaggio ? messaggio : "", sizeof v->messaggio);
    pulsante_prepara(&v->annulla, "Annulla", NESSUNA_ICONA, avviso_al_annulla, v);
    pulsante_prepara(&v->conferma, conferma, NESSUNA_ICONA, avviso_al_conferma, v);
    pulsante_stile(&v->conferma, tipo, MISURA_M);
    widget_aggiungi(&v->base, &v->annulla.base);
    widget_aggiungi(&v->base, &v->conferma.base);
}

void avviso_annulla(struct avviso *v, const char *testo) {
    copia_testo(v->annulla.testo, testo, MAX_TESTO_WIDGET);
}

void avviso_alternativa(struct avviso *v, const char *testo, int tipo) {
    if (!v->tre) {
        pulsante_prepara(&v->alternativa, testo, NESSUNA_ICONA, avviso_al_alternativa, v);
        widget_aggiungi(&v->base, &v->alternativa.base);
        v->tre = 1;
    } else {
        copia_testo(v->alternativa.testo, testo, MAX_TESTO_WIDGET);
    }
    pulsante_stile(&v->alternativa, tipo, MISURA_M);
}

void avviso_apri(struct avviso *v) {
    struct finestra_tk *f = widget_finestra(&v->base);
    v->base.nascosto = 0;
    v->annulla.premuto = v->conferma.premuto = 0;
    avviso_sistema(v, f->base.l, f->base.a);
    if (f->base.tipo == &tipo_finestra_tk) {
        uikit_sotto(f, 0);
        uikit_fuoco(f, &v->base);
        uikit_cambiata_tutta(f);           /* il velo e l'ombra: tutta la finestra */
    }
}

static void avviso_chiudi(struct avviso *v) {
    struct finestra_tk *f = widget_finestra(&v->base);
    v->base.nascosto = 1;
    if (f->base.tipo == &tipo_finestra_tk) {
        if (f->con_fuoco == &v->base || f->con_fuoco == &v->annulla.base ||
            f->con_fuoco == &v->conferma.base) uikit_fuoco(f, 0);
        if (f->sotto == &v->annulla.base || f->sotto == &v->conferma.base) uikit_sotto(f, 0);
        uikit_cambiata_tutta(f);
    }
}
