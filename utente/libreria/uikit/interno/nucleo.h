/* UIKit, INTERNO: le funzioni e i dati del nucleo (i .c di questa cartella) che servono
 * dentro UIKit ma non alle applicazioni. Nessuna applicazione lo include.
 *
 * Lo includono:
 *   - i .c del nucleo, che se li dicono tra loro (per esempio la finestra segna le zone da
 *     ridisegnare, il ciclo degli eventi le ridisegna);
 *   - i .c dei componenti (menu.c, tabella.c, griglia.c...), che usano quelle del nucleo e
 *     se ne dicono alcune tra loro (in fondo: il testo che va a capo o si taglia con "...", il
 *     campo del nome nuovo, l'intestazione di un gruppo);
 *   - l'anteprima (strumenti/anteprima), che fa al posto del ciclo degli eventi: misura,
 *     dispone e disegna la finestra una volta.
 *
 * Divise per il file che le definisce. */
#ifndef UIKIT_NUCLEO_H
#define UIKIT_NUCLEO_H

#include "../uikit.h"

/* --- widget.c --- */

/* 'w' (con quello che contiene) sta per uscire dall'albero della finestra: se il widget sotto
 * il puntatore o quello con il fuoco e' lui o sta dentro di lui, si spengono (uikit_sotto,
 * uikit_fuoco). Da chiamare PRIMA di staccarlo; la chiamano tutti quelli che tolgono figli
 * (pagina_svuota, contenitore_svuota, percorso_metti). Nascondere un widget (widget_nascondi)
 * gli toglie invece solo il puntatore. */
void widget_esce(struct widget *w);

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

/* Il puntatore sopra un widget nuovo (o su nessuno): si spegne il vecchio, si accende il
 * nuovo. La chiamano il ciclo degli eventi, e i componenti che aprono, chiudono o nascondono
 * qualcosa sotto il puntatore (l'avviso, il menu, widget_nascondi). Un'app non la usa: UIKit
 * segue il puntatore da sola. */
void uikit_sotto(struct finestra_tk *f, struct widget *w);

/* --- suggerimento.c --- */

/* Il puntatore e' passato sopra 'w' (o su nessuno): il suggerimento che si vedeva sparisce e,
 * se 'w' ne ha uno, parte l'attesa. La chiama uikit_sotto. */
void suggerimento_sotto(struct finestra_tk *f, struct widget *w);

/* Spegne il suggerimento e l'attesa (un clic). */
void suggerimento_ferma(struct finestra_tk *f);

/* Disegna il suggerimento, se si vede: l'ultima cosa, sopra tutto (uikit_disegna_tutto). */
void suggerimento_disegna(struct finestra_tk *f, struct pennello *p);

/* L'etichetta da sola, a (x, y), larga 'l': la usano il suggerimento e l'anteprima. */
void suggerimento_disegna_forma(struct pennello *p, int x, int y, int l, const char *testo,
                                const char *scorciatoia);

/* Da' il fuoco della tastiera a 'w' (0: a nessuno), senza guardare se prende i tasti; chi lo
 * aveva lo perde. Tutti e due si ridisegnano. La usano il ciclo degli eventi (al clic) e
 * l'avviso; per le app c'e' finestra_tk_dai_fuoco (uikit.h). */
void uikit_fuoco(struct finestra_tk *f, struct widget *w);

/* Il contenuto senza nessun margine, fino ai bordi della finestra: solo per chi disegna tutto
 * da se' (le scene dell'anteprima, che includono questo file). Le applicazioni usano
 * finestra_tk_aree. */
void finestra_tk_bordo(struct finestra_tk *f);

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

/* --- elemento_file.c --- */

#define MAX_RIGA_NOME 96     /* lettere di una riga del nome, compreso lo 0 finale */

/* Quanto e' largo un pezzo di testo (da 'da' a 'a', escluso). Inline: minuscola, la chiamano
 * di continuo testo_a_capo e paragrafo_a_capo mentre cercano dove tagliare. */
static inline int larghezza_pezzo(const struct font *f, const char *s, int da, int a) {
    int sp = spaziatura_del_font(f), c = 0;   /* centesimi di pixel, come larghezza_testo_spaziato */
    for (int i = da; i < a; i++) c += glifo_di(f, (unsigned char)s[i])->avanzo * 100 + sp;
    return c >= 0 ? (c + 50) / 100 : -((-c + 50) / 100);
}

/* Spezza 'testo' in righe larghe al massimo 'largo' pixel, al massimo 'massimo' righe:
 * quante righe sono uscite. Si va a capo dopo uno spazio o un trattino (come fa Figma:
 * "foto-" / "mare.png"); una parola piu' lunga della riga si spezza dove capita. Se il
 * testo non ci sta tutto, l'ultima riga finisce con "...". Lo usano l'elemento di un file,
 * l'avviso, lo stato vuoto, la griglia e testo_tagliato. */
int testo_a_capo(const struct font *f, const char *testo, int largo,
                 char righe[][MAX_RIGA_NOME], int massimo);

/* --- paragrafo.c --- */

#define PARAGRAFO_RIGA  200                /* lettere di una riga, compreso lo 0 */

/* Come testo_a_capo, ma con righe lunghe fino a PARAGRAFO_RIGA lettere (quella e' fatta per i
 * nomi dei file, e spezza dopo 92): spezza 'testo' in righe larghe al massimo 'largo' pixel, al
 * massimo 'massimo' righe, andando a capo dopo uno spazio o un trattino; se non ci sta tutto,
 * l'ultima riga finisce con "...". Quante righe. Lo usano il paragrafo, il messaggio in linea e
 * la riga di permesso. */
int paragrafo_a_capo(const struct font *f, const char *testo, int largo, char righe[][PARAGRAFO_RIGA], int massimo);

/* --- oggetti.c --- */

/* Un testo su una riga, tagliato con "..." se e' piu' largo di 'largo' (il font non ha "…"),
 * centrato in verticale nel rettangolo alto 'a'. Allineato a destra (finisce in x + largo) se
 * 'destra'. Lo usano le righe e le voci che hanno "text-ellipsis" nel disegno (la tabella, il
 * menu, la barra laterale, le informazioni). */
void testo_tagliato(struct pennello *p, const struct font *f, int x, int y, int a, int largo,
                    const char *s, int destra);

#define RINOMINA_DENTRO   SPAZIO_8     /* nel campo, ai lati del testo (nel tile: SPAZIO_4) */
#define RINOMINA_CURSORE  2            /* la larghezza del cursore */

/* IL CAMPO DEL NOME NUOVO (lo stato Rinomina di una riga della tabella o di un tile della
 * griglia). Il campo largo 'l' e alto 'a', raggio 'raggio': incassato con il fuoco come
 * campo_disegna (ombre "focus", SUPERFICIE_INCASSATA, bordo BORDO_FOCUS), con dentro il testo e
 * il cursore in fondo. Il testo centrato se 'centrato'; se e' piu' lungo del campo si vede la
 * fine. */
void campo_rinomina_disegna(struct pennello *p, int x, int y, int l, int a, int raggio,
                            const char *testo_nome, int centrato);

/* --- menu.c --- */

/* L'intestazione di un gruppo (in un menu o nella barra laterale): il testo a
 * INTESTAZIONE_MENU_DENTRO dal bordo sinistro 'x', con la cima della riga in y. */
void intestazione_menu_disegna(struct pennello *p, int x, int y, const char *titolo);

#endif
