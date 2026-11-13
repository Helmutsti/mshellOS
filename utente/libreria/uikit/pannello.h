/* IL PANNELLO DEL CONTENUTO, dal file Figma ("Pannello contenuto" delle finestre: Files,
 * Terminale). Il contenuto principale di una finestra non sta sul vetro ma in un pannello
 * chiaro incassato: SUPERFICIE_ELEVATA, raggio RAGGIO_PANNELLO. Non ha margine suo: sta nel
 * corpo della finestra a aree (finestra_tk_aree), che gli lascia CORPO_PADDING (8) intorno e
 * CORPO_GAP (8) verso le altre aree.
 *
 * E' il SOLO livello con padding (la regola della spaziatura): 'padding' su tutti i lati
 * (SPAZIO_8 se i figli hanno righe con un fondo e il loro padding, come la tabella; SPAZIO_16 se
 * no, come il Terminale). I figli stanno dentro il padding, senza padding loro. Un figlio che
 * scorre da solo (ha la sua barra di scorrimento: la tabella, la griglia, il terminale) riceve
 * anche il padding destro, come nella pagina (pagina_disponi): la sua barra sta li', non sopra
 * il contenuto.
 *
 * I figli stanno uno sopra l'altro, tutti grandi quanto il pannello: se ne mostra uno alla
 * volta (gli altri nascosti), come le pagine. */
#ifndef PANNELLO_H
#define PANNELLO_H

#include "interno/nucleo.h"   /* le funzioni interne del nucleo che usa (finche' e' tutto nell'header) */

struct pannello_tk {
    struct widget base;
    int padding;                       /* su tutti i lati: SPAZIO_8 (con righe) o SPAZIO_16 */
};

static inline void pannello_misura(struct widget *w, int *l, int *a) { (void)w; *l = 0; *a = 0; }

static inline void pannello_disponi(struct widget *w) {
    int d = ((struct pannello_tk *)w)->padding;
    for (int i = 0; i < w->quanti_figli; i++) {
        struct widget *f = w->figli[i];
        f->x = w->x + d; f->y = w->y + d; f->l = w->l - 2 * d; f->a = w->a - 2 * d;
        for (int k = 0; k < f->quanti_figli; k++)   /* la sua barra, nel padding destro */
            if (f->figli[k]->tipo == &tipo_barra_scorrimento) {
                ((struct barra_scorrimento *)f->figli[k])->fuori = d;
                f->l += d;
            }
    }
}

static inline void pannello_disegna(struct widget *w, struct pennello *p) {
    pennello_colore(p, SUPERFICIE_ELEVATA);
    riempi_tondo(p, w->x, w->y, w->l, w->a, RAGGIO_PANNELLO);
}

static const struct tipo_widget tipo_pannello = { pannello_misura, pannello_disponi, pannello_disegna, 0, 0, 0, 0, 0, 0 };

/* Elastico e largo: prende tutto lo spazio che avanza. */
static inline void pannello_prepara(struct pannello_tk *p, int padding) {
    *p = (struct pannello_tk){ .base = { .tipo = &tipo_pannello, .elastico = 1, .largo = 1 }, .padding = padding };
}

#endif
