/* L'ETICHETTA DI STATO, dal file Figma ("Etichetta di stato", pagina "App - Store") (UIKit,
 * incluso da uikit.h). Una piccola pillola per uno stato: un permesso concesso o no, una cosa
 * nuova, un aggiornamento disponibile.
 *
 * Alta 24 (SPAZIO_4 + 16 + SPAZIO_4), larga quanto il testo piu' SPAZIO_8 per lato; il testo in
 * FONT_ETICHETTA. Quattro tipi, coi colori tenui dei ruoli:
 *   - Neutra: fondo CONTROLLO_SFONDO, testo TESTO_SECONDARIO ("Non concesso");
 *   - Successo: SUCCESSO_TENUE e SUCCESSO_TESTO_TENUE ("Concesso");
 *   - Informazione: INFORMAZIONE_TENUE e INFORMAZIONE_TESTO_TENUE ("Nuovo");
 *   - Avviso: AVVISO_TENUE e AVVISO_TESTO_TENUE.
 * (Nel disegno il testo ha 0,24 pixel di spaziatura tra le lettere: il font di oggi non la sa
 * fare.) */
#ifndef UIKIT_ETICHETTA_STATO_H
#define UIKIT_ETICHETTA_STATO_H

enum { ETICHETTA_NEUTRA, ETICHETTA_SUCCESSO, ETICHETTA_INFORMAZIONE, ETICHETTA_AVVISO };

#define ETICHETTA_STATO_ALTEZZA (2 * SPAZIO_4 + INTERLINEA_ETICHETTA)

static inline int etichetta_stato_larghezza(const char *testo) {
    return 2 * SPAZIO_8 + larghezza_testo(FONT_ETICHETTA, testo);
}

/* La disegna con l'angolo in alto a sinistra in (x, y). */
static inline void etichetta_stato_disegna_in(struct pennello *p, int x, int y, int tipo, const char *testo) {
    unsigned int fondo = CONTROLLO_SFONDO_NUOVO, colore = TESTO_SECONDARIO;
    if (tipo == ETICHETTA_SUCCESSO) { fondo = SUCCESSO_TENUE; colore = SUCCESSO_TESTO_TENUE; }
    else if (tipo == ETICHETTA_INFORMAZIONE) { fondo = INFORMAZIONE_TENUE; colore = INFORMAZIONE_TESTO_TENUE; }
    else if (tipo == ETICHETTA_AVVISO) { fondo = AVVISO_TENUE; colore = AVVISO_TESTO_TENUE; }
    pennello_colore(p, fondo);
    riempi_pillola(p, x, y, etichetta_stato_larghezza(testo), ETICHETTA_STATO_ALTEZZA);
    pennello_colore(p, colore);
    testo_centrato(p, FONT_ETICHETTA, x + SPAZIO_8, y, ETICHETTA_STATO_ALTEZZA, testo);
}

struct etichetta_stato {
    struct widget base;
    char testo[MAX_TESTO_WIDGET];
    int tipo;
};

static inline void etichetta_stato_misura(struct widget *w, int *l, int *a) {
    *l = etichetta_stato_larghezza(((struct etichetta_stato *)w)->testo);
    *a = ETICHETTA_STATO_ALTEZZA;
}

static inline void etichetta_stato_disegna(struct widget *w, struct pennello *p) {
    struct etichetta_stato *e = (struct etichetta_stato *)w;
    etichetta_stato_disegna_in(p, w->x, w->y, e->tipo, e->testo);
}

static const struct tipo_widget tipo_etichetta_stato = {
    etichetta_stato_misura, 0, etichetta_stato_disegna, 0, 0, 0, 0, 0, 0
};

static inline void etichetta_stato_prepara(struct etichetta_stato *e, const char *testo, int tipo) {
    *e = (struct etichetta_stato){ .base = { .tipo = &tipo_etichetta_stato }, .tipo = tipo };
    copia_testo(e->testo, testo, MAX_TESTO_WIDGET);
}

#endif
