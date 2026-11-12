/* IL MESSAGGIO IN LINEA, dal file Figma ("Messaggio in linea", pagina "App - Store") (UIKit,
 * incluso da uikit.h). Un messaggio DENTRO la finestra, non una finestra a parte: un avviso, un
 * errore o un'informazione, con un titolo e una spiegazione. Per esempio "<app> e' stata tolta",
 * "Dati svuotati", "Manca l'icona per il tema scuro", "Non si puo' installare".
 *
 * Un riquadro con il fondo tenue del suo ruolo (AVVISO_TENUE, PERICOLO_TENUE,
 * INFORMAZIONE_TENUE), raggio RAGGIO_PANNELLO (10), SPAZIO_12 di margine interno. Dentro, in riga
 * a SPAZIO_12: il simbolo di 20 (triangolo, cerchio con la X, cerchio con la i) e i testi, a
 * SPAZIO_4 l'uno dall'altro: il titolo in FONT_CORPO_MEDIO, la spiegazione in FONT_CORPO (va a
 * capo), tutti e due alti 20 e nel colore *_TESTO_TENUE del ruolo (anche il simbolo).
 *
 * E' largo quanto il suo contenitore (la pagina gli da' la larghezza prima di misurarlo, vedi
 * pagina.h); alto quanto serve alla spiegazione. Testi nella code page 437. */
#ifndef UIKIT_MESSAGGIO_H
#define UIKIT_MESSAGGIO_H

enum { MESSAGGIO_AVVISO, MESSAGGIO_ERRORE, MESSAGGIO_INFORMAZIONE };

#define MESSAGGIO_SIMBOLO 20
#define MESSAGGIO_RIGHE   4
#define MESSAGGIO_TESTO   400

struct messaggio {
    struct widget base;
    int tipo;
    char titolo[MAX_TESTO_WIDGET * 2];
    char spiegazione[MESSAGGIO_TESTO];
};

static inline int messaggio_righe(const struct messaggio *m, int largo, char righe[][PARAGRAFO_RIGA]) {
    if (!m->spiegazione[0]) return 0;
    int testo_largo = largo - 2 * SPAZIO_12 - MESSAGGIO_SIMBOLO - SPAZIO_12;
    if (largo <= 0 || testo_largo < 40) testo_largo = 1 << 20;
    return paragrafo_a_capo(FONT_CORPO, m->spiegazione, testo_largo, righe, MESSAGGIO_RIGHE);
}

static inline void messaggio_misura(struct widget *w, int *l, int *a) {
    struct messaggio *m = (struct messaggio *)w;
    char righe[MESSAGGIO_RIGHE][PARAGRAFO_RIGA];
    int n = messaggio_righe(m, w->l, righe);
    *l = 0;
    *a = 2 * SPAZIO_12 + INTERLINEA_CORPO + (n ? SPAZIO_4 + n * INTERLINEA_CORPO : 0);
}

static inline void messaggio_disegna(struct widget *w, struct pennello *p) {
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

static inline void messaggio_prepara(struct messaggio *m, int tipo, const char *titolo, const char *spiegazione) {
    *m = (struct messaggio){ .base = { .tipo = &tipo_messaggio, .largo = 1 }, .tipo = tipo };
    copia_testo(m->titolo, titolo ? titolo : "", (int)sizeof m->titolo);
    copia_testo(m->spiegazione, spiegazione ? spiegazione : "", MESSAGGIO_TESTO);
}

#endif
