/* UIKit, i componenti piccoli: l'etichetta di stato (la pillola "Concesso", "Nuovo"...). Si
 * disegna come widget, o dentro un altro componente (la riga di permesso). Le dichiarazioni
 * sono in etichetta_stato.h e in interno/componenti_piccoli.h. */
#include "uikit/uikit.h"
#include "uikit/interno/componenti_piccoli.h"

int etichetta_stato_larghezza(const char *testo) {
    return 2 * SPAZIO_8 + larghezza_testo(FONT_ETICHETTA, testo);
}

void etichetta_stato_disegna_in(struct pennello *p, int x, int y, int tipo, const char *testo) {
    unsigned int fondo = CONTROLLO_SFONDO_NUOVO, colore = TESTO_SECONDARIO;
    if (tipo == ETICHETTA_SUCCESSO) { fondo = SUCCESSO_TENUE; colore = SUCCESSO_TESTO_TENUE; }
    else if (tipo == ETICHETTA_INFORMAZIONE) { fondo = INFORMAZIONE_TENUE; colore = INFORMAZIONE_TESTO_TENUE; }
    else if (tipo == ETICHETTA_AVVISO) { fondo = AVVISO_TENUE; colore = AVVISO_TESTO_TENUE; }
    pennello_colore(p, fondo);
    riempi_pillola(p, x, y, etichetta_stato_larghezza(testo), ETICHETTA_STATO_ALTEZZA);
    pennello_colore(p, colore);
    testo_centrato(p, FONT_ETICHETTA, x + SPAZIO_8, y, ETICHETTA_STATO_ALTEZZA, testo);
}

static void etichetta_stato_misura(struct widget *w, int *l, int *a) {
    *l = etichetta_stato_larghezza(((struct etichetta_stato *)w)->testo);
    *a = ETICHETTA_STATO_ALTEZZA;
}

static void etichetta_stato_disegna(struct widget *w, struct pennello *p) {
    struct etichetta_stato *e = (struct etichetta_stato *)w;
    etichetta_stato_disegna_in(p, w->x, w->y, e->tipo, e->testo);
}

static const struct tipo_widget tipo_etichetta_stato = {
    etichetta_stato_misura, 0, etichetta_stato_disegna, 0, 0, 0, 0, 0, 0
};

void etichetta_stato_prepara(struct etichetta_stato *e, const char *testo, int tipo) {
    *e = (struct etichetta_stato){ .base = { .tipo = &tipo_etichetta_stato }, .tipo = tipo };
    copia_testo(e->testo, testo, MAX_TESTO_WIDGET);
}
