/* UIKit, INTERNO: le funzioni e i dati del nucleo (i .c di questa cartella) che servono
 * dentro UIKit ma non alle applicazioni. Nessuna applicazione lo include.
 *
 * Lo includono:
 *   - i .c del nucleo, che se li dicono tra loro (per esempio la finestra segna le zone da
 *     ridisegnare, il ciclo degli eventi le ridisegna);
 *   - i componenti che sono ancora scritti tutti nei loro header (menu.h, tabella.h,
 *     pagina.h...: i gruppi 3 e 4 del riordino, vedi uikit.h). Le loro funzioni si compilano
 *     dentro il programma che li usa, quindi anche il programma vede queste dichiarazioni:
 *     non per questo diventano pubbliche. Quando i componenti avranno i loro .c, lo
 *     includeranno solo quelli;
 *   - l'anteprima (strumenti/anteprima), che fa al posto del ciclo degli eventi: misura,
 *     dispone e disegna la finestra una volta.
 *
 * Divise per il file che le definisce. */
#ifndef UIKIT_NUCLEO_H
#define UIKIT_NUCLEO_H

#include "../uikit.h"

/* --- widget.c --- */

/* Il widget piu' interno sotto (x, y) che vuole sentire il puntatore, o 0. I widget
 * disabilitati (e quello che c'e' dentro) non si trovano. */
struct widget *widget_sotto(struct widget *w, int x, int y);

/* Il widget piu' interno sotto (x, y), anche se non sente il puntatore, o 0. */
struct widget *widget_in(struct widget *w, int x, int y);

/* Di quanto un'ombra a strati esce dalla sua forma, al massimo (per sapere cosa ridisegnare
 * quando la forma compare o sparisce). */
int ombra_sporgenza(const struct strato_ombra *strati, int n);

/* I tipi della riga e della colonna (riga_prepara, colonna_prepara) e dello spazio elastico:
 * chi li riconosce in un albero, o li usa per un contenitore suo. Uno per tutto il
 * programma, quindi si possono confrontare gli indirizzi (w->tipo == &tipo_contenitore). */
extern const struct tipo_widget tipo_contenitore;
extern const struct tipo_widget tipo_spazio;

/* --- pulsanti.c --- */

/* Le misure del disegno: altezza, margine ai lati, spazio icona-testo, lato dell'icona,
 * font (il suo numero: font_del_sistema, materiali.h). Il raggio non c'e': i pulsanti sono
 * pillole (raggio = meta' altezza). */
struct misure_pulsante { int altezza, margine, spazio, icona, font; };

extern const struct misure_pulsante misure_pulsante[];

/* LA PROFONDITA' del pulsante. Si preme, quindi e' SOLLEVATO: il Primario ha l'ombra
 * "sollevato-pieno", il Secondario "sollevato", e le ombre ci sono sia a riposo sia sotto
 * il puntatore. Premuto "scende" e disabilitato "si spegne": tutti e due PIATTI, senza
 * ombra. Ghost e' sempre piatto. L'ordine e' quello di gkit_disegno.h: ombre esterne, fondo,
 * ombre interne (nel tema scuro c'e' un filo di luce sul bordo alto), poi icona e testo. */
void pulsante_disegna(struct widget *w, struct pennello *p);

/* Come i pulsanti veri: scatta quando si LASCIA, e solo se il puntatore e' ancora
 * sopra. Chi preme e poi cambia idea esce dal pulsante prima di lasciare. Tiene il conto
 * in 'premuto' e 'dentro' (del widget che lo usa) e dice 1 quando il clic e' fatto. Lo
 * usano tutti i pulsanti: questo e quelli della barra degli strumenti. */
int pressione(struct widget *w, int *premuto, int *dentro, int x, int y,
              unsigned int pulsanti);

/* --- campo.c --- */

/* Un campo CONTIENE quello che si scrive, quindi e' INCASSATO: fondo SUPERFICIE_INCASSATA,
 * ombra interna e una sottile esterna, mai bordo a riposo. Il fuoco NON cambia livello: il
 * campo resta incassato e aggiunge il bordo BORDO_FOCUS di 1,5 px (dentro) e l'anello
 * "focus" (la tabella ombra_focus_*: stessa ombra interna, ma la esterna e' un anello di
 * 3 px). L'errore non ha rosso (scala di grigi): ha lo stesso bordo del fuoco, piu' il
 * triangolo di avviso a destra e il testo secondario. Disabilitato e' piatto. */
void campo_disegna(struct widget *w, struct pennello *p);

/* --- avviso.c --- */

/* Dispone il riquadro al centro della finestra (larga fl, alta fa) e i due pulsanti in
 * fondo a destra. */
void avviso_sistema(struct avviso *v, int fl, int fa);

/* Sotto il riquadro: il velo (se bloccante) su tutta la finestra (larga fl, alta fa), con i
 * suoi angoli, e l'ombra del riquadro. Lo chiama la finestra, senza ritagli. */
void avviso_sotto(struct avviso *v, struct pennello *p, int fl, int fa);

/* Chiude l'avviso e chiama la sua 'al_risposta' con la 'scelta' (AVVISO_CONFERMA,
 * AVVISO_ANNULLA, AVVISO_ALTERNATIVA). */
void avviso_rispondi(struct avviso *v, int scelta);

/* Invio arriva come carattere; Esc no (non e' un carattere): lo da' all'avviso
 * uikit_tasto_fisico, come tasto fisico. */
void avviso_tasto(struct widget *w, unsigned int tasto);

/* --- finestra_tk.c --- */

/* Segna da ridisegnare tutto lo spazio del widget (con lo sbordo), anche se e' nascosto:
 * serve quando compare o sparisce (widget_nascondi). Prima di uikit_esegui non fa niente. */
void widget_segna(struct widget *w);

/* L'altezza della testata: TITOLO_ALTEZZA (40), o ALTEZZA_BARRA_MINI (28) nella mini finestra. */
int testata_altezza(const struct finestra_tk *f);

/* L'avviso della finestra (finestra_tk_avviso), se e' aperto; se no 0. */
struct avviso *finestra_tk_avviso_aperto(const struct finestra_tk *f);

/* 1 se la finestra ha aperto un avviso bloccante: sotto non si usa niente, nemmeno i
 * comandi della testata. Se no 0. */
int finestra_tk_bloccata(const struct finestra_tk *f);

/* La finestra in cui sta un widget: la radice del suo albero. */
struct finestra_tk *widget_finestra(struct widget *w);

/* Tutta la finestra (l'impaginazione e' cambiata, o il tema). */
void uikit_cambiata_tutta(struct finestra_tk *f);

/* La testata (il titolo, i comandi riduci e chiudi), con il suo divisore. */
void uikit_cambiata_testata(struct finestra_tk *f);

/* Il puntatore e' sopra il comando? La mini finestra non si riduce: ha solo chiudi. */
int su_comando(const struct finestra_tk *f, int chiudi, int x, int y);

/* Il tipo della finestra: la cima di un albero e' una finestra solo se ha questo tipo (un
 * widget preparato ma non ancora messo in una finestra no). */
extern const struct tipo_widget tipo_finestra_tk;

/* La chiamata che scade per prima, o 0 se non ce ne sono. */
struct chiamata_tk *uikit_prima_chiamata(struct finestra_tk *f);

/* Fa le chiamate scadute. Ognuna prima si rimette in fila (la prossima volta tra 'ms'),
 * poi si chiama: cosi' puo' cambiare il suo ritmo, o smettere. */
void uikit_chiamate_scadute(struct finestra_tk *f);

/* Disegna tutta la finestra: l'albero e, sopra a tutto, l'avviso aperto (velo, ombra,
 * riquadro). La usa anche l'anteprima (strumenti/anteprima). */
void uikit_disegna_tutto(struct finestra_tk *f, struct pennello *p);

/* --- barra_strumenti.c --- */

/* Lo spazio vuoto di una barra degli strumenti (non una delle sue cose), ovunque sia nella
 * finestra: da li' si trascina la finestra. */
int barra_strumenti_vuota(struct finestra_tk *f, int x, int y);

/* --- scorrimento.c --- */

/* Il tipo della barra di scorrimento: chi contiene una barra la riconosce tra i figli. */
extern const struct tipo_widget tipo_barra_scorrimento;

#endif
