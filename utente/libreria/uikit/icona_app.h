/* L'ICONA DI UN'APP, dal file Figma ("Icona dell'app": il quadrato arrotondato del pacchetto)
 * (UIKit, incluso da uikit.h). Nel disegno e' l'icona vera dell'app (il suo SVG, o la PNG
 * piccola). Oggi UIKit non sa ancora disegnare gli SVG ne' leggere le PNG dal disco (STATO.md,
 * "Spostare tutte le risorse su disco"): al suo posto c'e' un SEGNAPOSTO con la stessa forma, un
 * quadrato arrotondato CONTROLLO_SFONDO_SOPRA con il simbolo "pacchetto" in TESTO_SECONDARIO.
 *
 * Le misure sono quelle del disegno, in proporzione al lato: raggio 22/96 (7,5 a 32, 14,7 a 64),
 * simbolo 20/32 del lato (le icone del sistema hanno solo alcune misure: la piu' vicina). */
#ifndef UIKIT_ICONA_APP_H
#define UIKIT_ICONA_APP_H

static inline void icona_app_disegna_in(struct pennello *p, int x, int y, int lato) {
    pennello_colore(p, CONTROLLO_SFONDO_SOPRA);
    riempi_tondo(p, x, y, lato, lato, (lato * 22 + 48) / 96);
    int simbolo = lato * 20 / 32;
    pennello_colore(p, COLORE_ICONA(COLORE_ICONA_SECONDARIO));
    icona(p, ICONA_PACKAGE, simbolo, x + (lato - simbolo) / 2, y + (lato - simbolo) / 2);
}

struct icona_app { struct widget base; int lato; };

static inline void icona_app_misura(struct widget *w, int *l, int *a) {
    *l = *a = ((struct icona_app *)w)->lato;
}

static inline void icona_app_disegna(struct widget *w, struct pennello *p) {
    icona_app_disegna_in(p, w->x, w->y, ((struct icona_app *)w)->lato);
}

static const struct tipo_widget tipo_icona_app = { icona_app_misura, 0, icona_app_disegna, 0, 0, 0, 0, 0, 0 };

static inline void icona_app_prepara(struct icona_app *i, int lato) {
    *i = (struct icona_app){ .base = { .tipo = &tipo_icona_app }, .lato = lato };
}

#endif
