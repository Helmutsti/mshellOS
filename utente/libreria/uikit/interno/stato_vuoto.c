/* UIKit, il nucleo: lo stato vuoto (un'area senza contenuto, o in errore). Le dichiarazioni
 * sono in uikit.h. */
#include "uikit/uikit.h"
#include "uikit/interno/nucleo.h"

static int stato_vuoto_righe(struct stato_vuoto *s, int largo, char righe[][MAX_RIGA_NOME]) {
    if (!s->spiegazione[0]) return 0;
    return testo_a_capo(FONT_CORPO, s->spiegazione, largo, righe, STATO_VUOTO_RIGHE);
}

static void stato_vuoto_misura(struct widget *w, int *l, int *a) {
    struct stato_vuoto *s = (struct stato_vuoto *)w;
    char righe[STATO_VUOTO_RIGHE][MAX_RIGA_NOME];
    int n = stato_vuoto_righe(s, AVVISO_LARGHEZZA, righe);
    int t = larghezza_testo(FONT_SEZIONE_NUOVO, s->titolo);
    *l = t > AVVISO_LARGHEZZA ? t : AVVISO_LARGHEZZA;
    *a = INTERLINEA_SEZIONE +
         (s->icona != NESSUNA_ICONA ? ICONA_OGGETTO + SPAZIO_8 : 0) + (n ? SPAZIO_8 + n * INTERLINEA_CORPO : 0);
}

static void stato_vuoto_disegna(struct widget *w, struct pennello *p) {
    struct stato_vuoto *s = (struct stato_vuoto *)w;
    char righe[STATO_VUOTO_RIGHE][MAX_RIGA_NOME];
    int n = stato_vuoto_righe(s, w->l, righe);
    int alto = INTERLINEA_SEZIONE + (s->icona != NESSUNA_ICONA ? ICONA_OGGETTO + SPAZIO_8 : 0) +
               (n ? SPAZIO_8 + n * INTERLINEA_CORPO : 0);
    int y = w->y + (w->a - alto) / 2, cx = w->x + w->l / 2;
    if (s->icona != NESSUNA_ICONA) {
        pennello_colore(p, COLORE_ICONA(s->colore_icona));
        icona(p, s->icona, ICONA_OGGETTO, cx - ICONA_OGGETTO / 2, y);
        y += ICONA_OGGETTO + SPAZIO_8;
    }
    pennello_colore(p, TESTO_PRIMARIO);
    testo_centrato(p, FONT_SEZIONE_NUOVO, cx - larghezza_testo(FONT_SEZIONE_NUOVO, s->titolo) / 2, y,
                   INTERLINEA_SEZIONE, s->titolo);
    y += INTERLINEA_SEZIONE + SPAZIO_8;
    pennello_colore(p, TESTO_SECONDARIO);
    for (int i = 0; i < n; i++, y += INTERLINEA_CORPO)
        testo_centrato(p, FONT_CORPO, cx - larghezza_testo(FONT_CORPO, righe[i]) / 2, y, INTERLINEA_CORPO, righe[i]);
}

static const struct tipo_widget tipo_stato_vuoto = { stato_vuoto_misura, 0, stato_vuoto_disegna, 0, 0, 0, 0, 0, 0 };

void stato_vuoto_prepara(struct stato_vuoto *s, int icona_, const char *titolo,
                         const char *spiegazione) {
    *s = (struct stato_vuoto){ .base = { .tipo = &tipo_stato_vuoto, .elastico = 1, .largo = 1 },
                               .icona = icona_, .colore_icona = COLORE_ICONA_SECONDARIO };
    copia_testo(s->titolo, titolo, MAX_TESTO_WIDGET);
    copia_testo(s->spiegazione, spiegazione ? spiegazione : "", sizeof s->spiegazione);
}

void stato_vuoto_colore_icona(struct stato_vuoto *s, int modo) { s->colore_icona = modo; }

void stato_vuoto_cambia(struct stato_vuoto *s, int icona_, const char *titolo,
                        const char *spiegazione) {
    s->icona = icona_;
    copia_testo(s->titolo, titolo, MAX_TESTO_WIDGET);
    copia_testo(s->spiegazione, spiegazione ? spiegazione : "", sizeof s->spiegazione);
    widget_cambiato(&s->base);
}
