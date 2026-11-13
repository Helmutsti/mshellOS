/* UIKit, i componenti piccoli: la riga di permesso (simbolo, nome, spiegazione e, a destra,
 * l'etichetta di stato). Le dichiarazioni e il disegno sono in riga_permesso.h; l'etichetta la
 * disegna etichetta_stato.c (interno/componenti_piccoli.h). */
#include "uikit/uikit.h"
#include "uikit/interno/componenti_piccoli.h"

/* Quanto spazio resta ai testi nella riga larga 'largo': tolti il simbolo e, se c'e',
 * l'etichetta. */
static int riga_permesso_testi(const struct riga_permesso *r, int largo) {
    int l = largo - RIGA_PERMESSO_SIMBOLO - SPAZIO_12;
    if (r->stato[0]) l -= SPAZIO_12 + etichetta_stato_larghezza(r->stato);
    return l;
}

/* Divide la spiegazione nelle righe che stanno nella riga larga 'largo'. Se la larghezza non si
 * sa ancora (o e' troppo poca) non va a capo. Restituisce quante righe. */
static int riga_permesso_righe(const struct riga_permesso *r, int largo, char righe[][PARAGRAFO_RIGA]) {
    if (!r->spiegazione[0]) return 0;
    int l = riga_permesso_testi(r, largo);
    if (largo <= 0 || l < 40) l = 1 << 20;
    return paragrafo_a_capo(FONT_PICCOLO_NUOVO, r->spiegazione, l, righe, RIGA_PERMESSO_RIGHE);
}

static void riga_permesso_misura(struct widget *w, int *l, int *a) {
    struct riga_permesso *r = (struct riga_permesso *)w;
    char righe[RIGA_PERMESSO_RIGHE][PARAGRAFO_RIGA];
    *l = 0;
    *a = INTERLINEA_CORPO + riga_permesso_righe(r, w->l, righe) * INTERLINEA_PICCOLO;
}

static void riga_permesso_disegna(struct widget *w, struct pennello *p) {
    struct riga_permesso *r = (struct riga_permesso *)w;
    char righe[RIGA_PERMESSO_RIGHE][PARAGRAFO_RIGA];
    int n = riga_permesso_righe(r, w->l, righe);
    pennello_colore(p, COLORE_ICONA(COLORE_ICONA_SECONDARIO));   /* come la "Riga di permesso" del disegno */
    icona(p, r->simbolo, RIGA_PERMESSO_SIMBOLO, w->x, w->y + (w->a - RIGA_PERMESSO_SIMBOLO) / 2);
    int x = w->x + RIGA_PERMESSO_SIMBOLO + SPAZIO_12, y = w->y;
    pennello_salva(p);
    pennello_ritaglia(p, x, w->y, riga_permesso_testi(r, w->l), w->a);
    pennello_colore(p, TESTO_PRIMARIO);
    testo_centrato(p, FONT_CORPO_MEDIO, x, y, INTERLINEA_CORPO, r->nome);
    y += INTERLINEA_CORPO;
    pennello_colore(p, TESTO_SECONDARIO);
    for (int i = 0; i < n; i++, y += INTERLINEA_PICCOLO)
        testo_centrato(p, FONT_PICCOLO_NUOVO, x, y, INTERLINEA_PICCOLO, righe[i]);
    pennello_ripristina(p);
    if (r->stato[0])
        etichetta_stato_disegna_in(p, w->x + w->l - etichetta_stato_larghezza(r->stato),
                                   w->y + (w->a - ETICHETTA_STATO_ALTEZZA) / 2, r->tipo_stato, r->stato);
}

static const struct tipo_widget tipo_riga_permesso = {
    riga_permesso_misura, 0, riga_permesso_disegna, 0, 0, 0, 0, 0, 0
};

void riga_permesso_prepara(struct riga_permesso *r, int simbolo, const char *nome,
                           const char *spiegazione, const char *stato, int tipo_stato) {
    *r = (struct riga_permesso){ .base = { .tipo = &tipo_riga_permesso, .largo = 1 }, .simbolo = simbolo,
                                 .tipo_stato = tipo_stato };
    copia_testo(r->nome, nome, MAX_TESTO_WIDGET);
    copia_testo(r->spiegazione, spiegazione ? spiegazione : "", (int)sizeof r->spiegazione);
    copia_testo(r->stato, stato ? stato : "", MAX_TESTO_WIDGET);
}
