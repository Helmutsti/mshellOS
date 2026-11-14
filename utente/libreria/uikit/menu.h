/* IL MENU, dal file Figma (pagina "Componenti complessi", sezione "Menu"): il menu del tasto
 * destro e i menu delle applicazioni. Livello SOVRAPPOSTO: fondo SUPERFICIE_ELEVATA, raggio
 * RAGGIO_PANNELLO, ombra "sovrapposto". E' fatto SOLO di tre pezzi, ognuno un componente del
 * disegno:
 *
 *   - la VOCE DI MENU, S (32) o M (40), rientrata di SPAZIO_8 dai lati del menu, con SPAZIO_8 di
 *     padding dentro: [simbolo 18] [testo "corpo"] ... [scorciatoia "piccolo" secondaria | freccia
 *     del sottomenu 16 | spunta 16]. Piatta; sotto il puntatore il fondo CONTROLLO_SFONDO (raggio
 *     RAGGIO_CONTROLLO_NUOVO); Disabilitata: testo e scorciatoia TESTO_DISABILITATO, non reagisce.
 *     Se una voce di un gruppo ha il simbolo ce l'hanno tutte (lo decide chi scrive le voci);
 *   - il SEPARATORE: una linea BORDO_SEPARATORE da bordo a bordo, SPAZIO_4 sopra e sotto (9);
 *   - l'INTESTAZIONE: un titolo di gruppo, "etichetta" in TESTO_SECONDARIO, padding 8 sopra, 4
 *     sotto, 8 ai lati (alta 28). Non si clicca. La usa anche la barra laterale.
 *
 * LO SPAZIO, come vuole la regola della spaziatura: dal bordo del menu al primo testo ci sono 16
 * pixel, in x (8 di rientro della voce + 8 dentro) e in y (il padding del menu + lo spazio
 * sopra il testo nella voce: 10 + 6 con le voci S, 6 + 10 con le M; con un'intestazione in cima,
 * 8 + 8). Il padding verticale del menu si calcola cosi' (MENU_PADDING_Y), quello di sotto e'
 * lo stesso.
 *
 * IL SOTTOMENU: una voce con 'sottomenu' ha la freccia a destra; sotto il puntatore (o al clic)
 * si apre accanto, a destra (a sinistra se a destra non ci sta), con la prima voce alla stessa
 * altezza della voce che lo apre.
 *
 * COME SI USA. Il menu e' un widget che si aggiunge per ultimo alla finestra (sta sopra a
 * tutto); nascosto finche' non lo si apre. Aperto, copre tutta la finestra: un clic fuori lo
 * chiude (e il clic finisce li', come sul Mac). Esc lo chiude. Da tastiera: su e giu' scorrono
 * le voci che si possono scegliere, Invio sceglie, destra apre il sottomenu, sinistra lo chiude. Scegliere una voce lo chiude e
 * chiama 'al_scelta' con la sua azione. Se una voce si puo' scegliere, e se ha la spunta, lo
 * chiede a 'attiva' e 'spuntata' (0: sempre attiva, mai spuntata).
 *
 *     static const struct voce_menu_tk voci[] = {
 *         { APRI, "Apri", NESSUNA_ICONA, "Invio" },
 *         { MENU_VOCE_SEPARATORE },
 *         { ELIMINA, "Elimina", NESSUNA_ICONA, "Canc" },
 *     };
 *     menu_prepara(&menu, MISURA_S, 220, attiva, 0, scelta, 0);
 *     menu_aggiungi(&finestra, &menu);
 *     ...
 *     menu_apri(&menu, voci, 3, x, y);              (x, y: l'angolo, nella finestra)
 *
 * Il codice e' in interno/menu.c. */
#ifndef MENU_H
#define MENU_H

#define MENU_VOCE_SEPARATORE    (-1)
#define MENU_VOCE_INTESTAZIONE  (-2)

#define MENU_LARGHEZZA_NORMALE     240           /* quella del componente; Files usa 220, come nel suo disegno */
#define MENU_RIENTRO       SPAZIO_8      /* tra il bordo del menu e la voce */
#define VOCE_MENU_DENTRO   SPAZIO_8      /* dentro la voce, ai lati */
#define VOCE_MENU_GAP      GAP_M         /* tra simbolo, testo e cosa a destra */
#define VOCE_MENU_SIMBOLO  ICONA_M       /* 18 */
#define VOCE_MENU_DESTRA   ICONA_S       /* 16: la freccia del sottomenu, la spunta */
#define MENU_PRIMO_TESTO   SPAZIO_16     /* dal bordo del menu al primo testo, in x e in y */
#define SEPARATORE_MENU_ALTO (SPAZIO_4 + 1 + SPAZIO_4)
#define INTESTAZIONE_MENU_SOPRA SPAZIO_8
#define INTESTAZIONE_MENU_SOTTO SPAZIO_4
#define INTESTAZIONE_MENU_ALTA (INTESTAZIONE_MENU_SOPRA + INTERLINEA_ETICHETTA + INTESTAZIONE_MENU_SOTTO)   /* 28 */
#define INTESTAZIONE_MENU_DENTRO SPAZIO_8
#define SOTTOMENU_STACCO   SPAZIO_4      /* tra il menu e il sottomenu (il disegno non lo dice) */
#define MENU_BORDO         SPAZIO_4      /* il menu non va piu' vicino di cosi' al bordo della finestra */

/* Una voce: un'azione (un numero dell'applicazione, >= 0), MENU_VOCE_SEPARATORE o
 * MENU_VOCE_INTESTAZIONE (il testo e' il titolo). */
struct voce_menu_tk {
    int azione;
    const char *testo;
    int icona;                             /* ICONA_..., o NESSUNA_ICONA (0 vale l'icona 0!) */
    const char *scorciatoia;               /* "Ctrl+D", o 0 */
    const struct voce_menu_tk *sottomenu;  /* le voci del sottomenu, o 0 */
    int quante_sotto;
};

struct menu_tk;
typedef void (*al_scelta_menu)(struct menu_tk *m, int azione, void *dati);
typedef int (*domanda_menu)(int azione, void *dati);

struct pannello_menu_tk {
    const struct voce_menu_tk *voci;
    int quante;
    int x, y;                              /* nella finestra */
};

struct menu_tk {
    struct widget base;                    /* aperto, copre tutta la finestra */
    int misura;                            /* MISURA_S (voci da 32) o MISURA_M (40) */
    int larghezza;
    struct pannello_menu_tk pannelli[2];   /* il menu e il sottomenu */
    int quanti;                            /* 0 chiuso, 1, 2 con il sottomenu */
    int sopra;                             /* la voce sotto il puntatore: pannello * 100 + voce, o -1 */
    int premuta;                           /* quella su cui si e' premuto, o -1 */
    int aperta_da;                         /* la voce del menu che ha aperto il sottomenu, o -1 */
    domanda_menu attiva, spuntata;
    al_scelta_menu al_scelta;
    void *dati;
    /* Facoltativa, la mette chi lo possiede (il menu a tendina): chiamata ogni volta che il
     * menu si chiude, per scelta, Esc o clic fuori, DOPO la chiusura e PRIMA di 'al_scelta'. */
    void (*alla_chiusura)(struct menu_tk *m, void *dati);
    void *dati_chiusura;
};

/* Prepara il menu, chiuso e nascosto: voci di misura 'misura' (MISURA_S o MISURA_M), largo
 * 'larghezza' (0: MENU_LARGHEZZA_NORMALE). 'attiva' e 'spuntata' (anche 0) dicono per ogni
 * azione se la voce si puo' scegliere e se ha la spunta; 'al_scelta' riceve l'azione scelta,
 * con 'dati'. */
void menu_prepara(struct menu_tk *m, int misura, int larghezza, domanda_menu attiva,
                  domanda_menu spuntata, al_scelta_menu al_scelta, void *dati);

/* Il menu sta sopra a tutto: lo si aggiunge alla finestra dopo tutto il resto. */
void menu_aggiungi(struct finestra_tk *f, struct menu_tk *m);

/* 1 se il menu e' aperto, se no 0. */
int menu_aperto(const struct menu_tk *m);

/* Apre il menu con l'angolo nel punto (x, y) della finestra (dove si e' cliccato), spostandolo
 * quanto basta perche' stia tutto nella finestra (sotto la testata). */
void menu_apri(struct menu_tk *m, const struct voce_menu_tk *voci, int quante, int x, int y);

/* Accende la voce 'indice' del menu (la prima, 0), come se il puntatore le fosse sopra: serve
 * a chi lo apre da tastiera. Una voce che non si puo' scegliere non si accende. */
void menu_seleziona(struct menu_tk *m, int indice);

/* Chiude il menu (e il sottomenu) senza scegliere niente; se e' gia' chiuso non fa niente. */
void menu_chiudi(struct menu_tk *m);

#endif
