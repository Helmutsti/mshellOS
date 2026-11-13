/* LA PAGINA: un contenitore in colonna con il suo margine, che puo' avere il fondo dell'area dei
 * contenuti e puo' scorrere (UIKit, incluso da uikit.h). E' il "Contenuto" e il "Pannello
 * contenuto" dei disegni del file Figma.
 *
 * LA REGOLA DELLA SPAZIATURA. Lo spazio interno sta in UN SOLO livello: il contenitore che
 * delimita un'area visibile. La pagina e' quel livello: ha un margine (ai lati, sopra, sotto) e
 * uno spazio tra un figlio e l'altro; i figli non hanno margini verso di lei. Se i figli hanno un
 * fondo visibile con un loro margine interno (le righe sotto il puntatore), la pagina toglie lo
 * stesso valore dal suo margine su quel lato: la somma resta quella del design.
 *
 * IL FONDO ('fondo' a 1): la pagina e' il PANNELLO DEL CONTENUTO della finestra, chiaro e
 * incassato (SUPERFICIE_ELEVATA, raggio RAGGIO_PANNELLO, come pannello.h), e il suo margine e'
 * il padding del pannello. Si usa nel corpo della finestra a aree (finestra_tk_aree), che le
 * lascia 8 intorno. Se scorre, scorre tutto il pannello: il contenuto si taglia sul suo bordo.
 *
 * LA LARGHEZZA DEI FIGLI. La pagina da' a ogni figlio tutta la sua larghezza (meno i margini)
 * PRIMA di misurarlo: cosi' un paragrafo o una riga che va a capo sanno quanto sono alti
 * (pagina_larghezza, che scende anche dentro righe e colonne). Lo spazio in altezza che avanza va
 * ai figli 'elastico'.
 *
 * LO SCORRIMENTO ('scorre' a 1): se i figli sono piu' alti della pagina, si vede un pezzo e la
 * rotella lo fa scorrere, con la barra di scorrimento di UIKit sul bordo destro; i figli si
 * vedono solo dentro la pagina.
 *
 *     pagina_prepara(&elenco, SPAZIO_16, SPAZIO_24, SPAZIO_24, SPAZIO_24);
 *     elenco.fondo = 1; elenco.scorre = 1;
 *     pagina_aggiungi(&elenco, &sezione.base);
 *
 * I figli si aggiungono con pagina_aggiungi (non widget_aggiungi): la barra di scorrimento resta
 * l'ultima, sopra a tutti. pagina_svuota toglie tutti i figli, per rifare la pagina. Il codice
 * e' in interno/pagina.c. */
#ifndef UIKIT_PAGINA_H
#define UIKIT_PAGINA_H

struct pagina {
    struct widget base;
    int margine_x, margine_sopra, margine_sotto;
    int spazio;                            /* tra un figlio e l'altro */
    int fondo;                             /* 1: il fondo chiaro dell'area dei contenuti */
    int scorre;                            /* 1: se non ci sta, scorre */
    int spostato;                          /* lo scorrimento con cui sono messi ora i figli */
    struct barra_scorrimento barra;        /* sempre l'ultimo figlio */
};

/* Prepara la pagina, vuota, larga ed elastica: 'spazio' tra un figlio e l'altro, il margine ai
 * lati, sopra e sotto. Senza fondo e senza scorrere ('fondo' e 'scorre' a 0: li si accende
 * dopo). */
void pagina_prepara(struct pagina *g, int spazio, int margine_x, int margine_sopra,
                    int margine_sotto);

/* Aggiunge un figlio, prima della barra di scorrimento (che resta sopra a tutti). Oltre
 * MAX_FIGLI non aggiunge niente. */
void pagina_aggiungi(struct pagina *g, struct widget *figlio);

/* Toglie tutti i figli (resta la barra) e torna in cima. */
void pagina_svuota(struct pagina *g);

/* Toglie tutti i figli di una riga o di una colonna, per rifarla. */
void contenitore_svuota(struct contenitore *c);

#endif
