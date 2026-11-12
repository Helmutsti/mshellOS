/* LA RIGA DELL'APP, dal file Figma ("Riga dell'app", pagina "App - Store") (UIKit, incluso da
 * uikit.h). Una riga dell'elenco delle app installate: l'icona dell'app (32), il nome e sotto
 * "autore . versione", e a destra la freccia che apre il dettaglio. Un clic (quando si lascia,
 * ancora sopra) chiama al_clic.
 *
 * Alta 52: SPAZIO_8 di margine interno per lato, perche' sotto il puntatore ha un fondo
 * (CONTROLLO_SFONDO, raggio RAGGIO_CONTROLLO_NUOVO; premuta CONTROLLO_SFONDO_SOPRA, come i
 * ghost): chi la contiene toglie 8 dal suo margine (la regola della spaziatura, punto 3). Dentro,
 * in riga a SPAZIO_12: l'icona, i testi (il nome in FONT_CORPO_MEDIO alto 20, TESTO_PRIMARIO; i
 * dettagli in FONT_PICCOLO_NUOVO alti 16, TESTO_SECONDARIO), la freccia di 16 (TESTO_SECONDARIO).
 * Larga quanto il suo contenitore. Testi nella code page 437. */
#ifndef UIKIT_RIGA_APP_H
#define UIKIT_RIGA_APP_H

#define RIGA_APP_ICONA   32
#define RIGA_APP_ALTEZZA (2 * SPAZIO_8 + RIGA_APP_ICONA + 4)   /* 52: i due testi (20 + 16) */

struct riga_app;
typedef void (*al_clic_riga_app)(struct riga_app *r, void *dati);

struct riga_app {
    struct widget base;
    char nome[MAX_TESTO_WIDGET];
    char dettagli[MAX_TESTO_WIDGET * 2];
    al_clic_riga_app al_clic;
    void *dati;
    int premuto, dentro;                   /* come i pulsanti (vedi pressione) */
};

static inline void riga_app_misura(struct widget *w, int *l, int *a) {
    (void)w;
    *l = 0;
    *a = RIGA_APP_ALTEZZA;
}

static inline void riga_app_disegna(struct widget *w, struct pennello *p) {
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

static inline void riga_app_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    struct riga_app *r = (struct riga_app *)w;
    if (pressione(w, &r->premuto, &r->dentro, x, y, pulsanti) && r->al_clic) r->al_clic(r, r->dati);
}

static const struct tipo_widget tipo_riga_app = {
    riga_app_misura, 0, riga_app_disegna, riga_app_puntatore, 0, 0, 0, 0, 0
};

static inline void riga_app_prepara(struct riga_app *r, const char *nome, const char *dettagli,
                                    al_clic_riga_app al_clic, void *dati) {
    *r = (struct riga_app){ .base = { .tipo = &tipo_riga_app, .largo = 1 }, .al_clic = al_clic, .dati = dati };
    copia_testo(r->nome, nome, MAX_TESTO_WIDGET);
    copia_testo(r->dettagli, dettagli ? dettagli : "", (int)sizeof r->dettagli);
}

#endif
