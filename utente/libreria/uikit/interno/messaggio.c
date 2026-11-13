/* UIKit, i componenti piccoli: il messaggio in linea (un avviso, un errore o un'informazione
 * dentro la finestra). Le dichiarazioni e il disegno sono in messaggio.h. */
#include "uikit/uikit.h"
#include "uikit/interno/nucleo.h"

/* Divide la spiegazione nelle righe che stanno nel messaggio largo 'largo' (tolti i margini e
 * il simbolo). Se la larghezza non si sa ancora (o e' troppo poca) non va a capo. Restituisce
 * quante righe. */
static int messaggio_righe(const struct messaggio *m, int largo, char righe[][PARAGRAFO_RIGA]) {
    if (!m->spiegazione[0]) return 0;
    int testo_largo = largo - 2 * SPAZIO_12 - MESSAGGIO_SIMBOLO - SPAZIO_12;
    if (largo <= 0 || testo_largo < 40) testo_largo = 1 << 20;
    return paragrafo_a_capo(FONT_CORPO, m->spiegazione, testo_largo, righe, MESSAGGIO_RIGHE);
}

static void messaggio_misura(struct widget *w, int *l, int *a) {
    struct messaggio *m = (struct messaggio *)w;
    char righe[MESSAGGIO_RIGHE][PARAGRAFO_RIGA];
    int n = messaggio_righe(m, w->l, righe);
    *l = 0;
    *a = 2 * SPAZIO_12 + INTERLINEA_CORPO + (n ? SPAZIO_4 + n * INTERLINEA_CORPO : 0);
}

static void messaggio_disegna(struct widget *w, struct pennello *p) {
    struct messaggio *m = (struct messaggio *)w;
    unsigned int fondo = AVVISO_TENUE, colore = AVVISO_TESTO_TENUE;
    int simbolo = ICONA_TRIANGLE_ALERT;
    if (m->tipo == MESSAGGIO_ERRORE) { fondo = PERICOLO_TENUE; colore = PERICOLO_TESTO_TENUE; simbolo = ICONA_CIRCLE_X; }
    else if (m->tipo == MESSAGGIO_INFORMAZIONE) { fondo = INFORMAZIONE_TENUE; colore = INFORMAZIONE_TESTO_TENUE; simbolo = ICONA_INFO; }
    pennello_colore(p, fondo);
    riempi_tondo(p, w->x, w->y, w->l, w->a, RAGGIO_PANNELLO);
    pennello_colore(p, colore);
    int x = w->x + SPAZIO_12, y = w->y + SPAZIO_12;
    icona(p, simbolo, MESSAGGIO_SIMBOLO, x, y);
    x += MESSAGGIO_SIMBOLO + SPAZIO_12;
    testo_centrato(p, FONT_CORPO_MEDIO, x, y, INTERLINEA_CORPO, m->titolo);
    char righe[MESSAGGIO_RIGHE][PARAGRAFO_RIGA];
    int n = messaggio_righe(m, w->l, righe);
    y += INTERLINEA_CORPO + SPAZIO_4;
    for (int i = 0; i < n; i++, y += INTERLINEA_CORPO) testo_centrato(p, FONT_CORPO, x, y, INTERLINEA_CORPO, righe[i]);
}

static const struct tipo_widget tipo_messaggio = { messaggio_misura, 0, messaggio_disegna, 0, 0, 0, 0, 0, 0 };

void messaggio_prepara(struct messaggio *m, int tipo, const char *titolo, const char *spiegazione) {
    *m = (struct messaggio){ .base = { .tipo = &tipo_messaggio, .largo = 1 }, .tipo = tipo };
    copia_testo(m->titolo, titolo ? titolo : "", (int)sizeof m->titolo);
    copia_testo(m->spiegazione, spiegazione ? spiegazione : "", MESSAGGIO_TESTO);
}
