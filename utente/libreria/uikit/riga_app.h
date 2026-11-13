/* LA RIGA DELL'APP, dal file Figma ("Riga dell'app", pagina "App - Store") (UIKit, incluso da
 * uikit.h). Una riga dell'elenco delle app installate: l'icona dell'app (32), il nome e sotto
 * "autore . versione", e a destra la freccia che apre il dettaglio. Un clic (quando si lascia,
 * ancora sopra) chiama al_clic. Il codice e' in interno/riga_app.c.
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

/* Chiamata quando si fa clic sulla riga 'r'; 'dati' e' quello dato a riga_app_prepara. */
typedef void (*al_clic_riga_app)(struct riga_app *r, void *dati);

struct riga_app {
    struct widget base;
    char nome[MAX_TESTO_WIDGET];
    char dettagli[MAX_TESTO_WIDGET * 2];
    al_clic_riga_app al_clic;
    void *dati;
    int premuto, dentro;                   /* come i pulsanti */
};

/* Prepara la riga con il nome e i dettagli (0: niente; tagliati alla misura dei loro posti
 * nella struttura). Al clic chiama al_clic(r, dati) (al_clic 0: niente). */
void riga_app_prepara(struct riga_app *r, const char *nome, const char *dettagli,
                      al_clic_riga_app al_clic, void *dati);

#endif
