/* UIKit, i componenti piccoli: la riga dell'app (icona, nome, dettagli e la freccia; un clic
 * apre il dettaglio). Le dichiarazioni e il disegno sono in riga_app.h; l'icona la disegna
 * icona_app.c (interno/componenti_piccoli.h), il clic lo conta la pressione dei pulsanti
 * (interno/nucleo.h). */
#include "uikit/uikit.h"
#include "uikit/interno/nucleo.h"
#include "uikit/interno/componenti_piccoli.h"

static void riga_app_misura(struct widget *w, int *l, int *a) {
    (void)w;
    *l = 0;
    *a = RIGA_APP_ALTEZZA;
}

static void riga_app_disegna(struct widget *w, struct pennello *p) {
    struct riga_app *r = (struct riga_app *)w;
    int premuto = r->premuto && r->dentro;
    if (premuto || w->sopra) {
        pennello_colore(p, premuto ? CONTROLLO_SFONDO_SOPRA : CONTROLLO_SFONDO_NUOVO);
        riempi_tondo(p, w->x, w->y, w->l, w->a, RAGGIO_CONTROLLO_NUOVO);
    }
    int x = w->x + SPAZIO_8;
    icona_app_disegna_in(p, x, w->y + (w->a - RIGA_APP_ICONA) / 2, RIGA_APP_ICONA);
    x += RIGA_APP_ICONA + SPAZIO_12;
    int freccia = w->x + w->l - SPAZIO_8 - ICONA_S;
    int y = w->y + (w->a - INTERLINEA_CORPO - INTERLINEA_PICCOLO) / 2;
    pennello_salva(p);
    pennello_ritaglia(p, x, w->y, freccia - SPAZIO_12 - x, w->a);
    pennello_colore(p, TESTO_PRIMARIO);
    testo_centrato(p, FONT_CORPO_MEDIO, x, y, INTERLINEA_CORPO, r->nome);
    pennello_colore(p, TESTO_SECONDARIO);
    testo_centrato(p, FONT_PICCOLO_NUOVO, x, y + INTERLINEA_CORPO, INTERLINEA_PICCOLO, r->dettagli);
    pennello_ripristina(p);
    pennello_colore(p, COLORE_ICONA(COLORE_ICONA_ATTENUATO));   /* come nel disegno: la freccia e' attenuata */
    icona(p, ICONA_CHEVRON_RIGHT, ICONA_S, freccia, w->y + (w->a - ICONA_S) / 2);
}

static void riga_app_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    struct riga_app *r = (struct riga_app *)w;
    if (pressione(w, &r->premuto, &r->dentro, x, y, pulsanti) && r->al_clic) r->al_clic(r, r->dati);
}

static const struct tipo_widget tipo_riga_app = {
    riga_app_misura, 0, riga_app_disegna, riga_app_puntatore, 0, 0, 0, 0, 0
};

void riga_app_prepara(struct riga_app *r, const char *nome, const char *dettagli,
                      al_clic_riga_app al_clic, void *dati) {
    *r = (struct riga_app){ .base = { .tipo = &tipo_riga_app, .largo = 1 }, .al_clic = al_clic, .dati = dati };
    copia_testo(r->nome, nome, MAX_TESTO_WIDGET);
    copia_testo(r->dettagli, dettagli ? dettagli : "", (int)sizeof r->dettagli);
}
