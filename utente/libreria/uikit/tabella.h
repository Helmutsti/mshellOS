/* LA TABELLA, dal file Figma (pagina "Componenti complessi", sezione "Tabella"): la vista a
 * elenco di Files, la tabella dei processi di Attivita, gli elenchi. Composta da:
 *
 *   - l'INTESTAZIONE DI COLONNA: alta ALTEZZA_S (32), il nome della colonna in "etichetta"
 *     TESTO_SECONDARIO, allineata a sinistra (o a destra, per i numeri). Sulla colonna per cui
 *     si ordina, dopo il nome (SPAZIO_4), la freccia da 16: in su Crescente, in giu'
 *     Decrescente. Un clic sull'intestazione ordina per quella colonna (un altro clic gira il
 *     verso);
 *   - una linea BORDO_SEPARATORE da bordo a bordo;
 *   - le RIGHE: SPAZIO_4 sopra la prima, alte ALTEZZA_M (40), a SPAZIO_4 l'una dall'altra (due
 *     fondi arrotondati non si toccano mai), raggio RAGGIO_CONTROLLO_NUOVO, padding SPAZIO_8 ai
 *     lati. Stati: Normale (piatta), Sopra (fondo CONTROLLO_SFONDO), Selezionata (fondo
 *     CONTROLLO_SFONDO_SOPRA). Le celle in "corpo", con "..." se non ci stanno; la prima colonna
 *     puo' avere l'oggetto piccolo (24, oggetti.h) e SPAZIO_8 prima del nome.
 *
 * Tra una colonna e l'altra SPAZIO_24, nell'intestazione e nelle righe (stesso padding: le
 * colonne si allineano). Una colonna ha la sua larghezza, o 0: si divide lo spazio che avanza.
 * La tabella non ha padding suo verso il contenitore: il primo testo e' a 8 dal bordo (il padding
 * della riga, che ha un fondo) e, nell'intestazione, a 8 dalla cima (il testo da 16 centrato
 * nei 32). L'intestazione resta ferma; le righe scorrono con la rotella e la barra di
 * scorrimento di UIKit.
 *
 * I dati li da' l'applicazione, riga per riga, quando servono ('riga'); un clic su una riga (o
 * sul vuoto: riga -1), il doppio clic e il tasto destro arrivano ad 'al_clic'. Lo stato
 * Rinomina (il nome della prima colonna diventa un campo: oggetti.h) si accende con
 * tabella_rinomina. */
#ifndef TABELLA_H
#define TABELLA_H

#define MAX_COLONNE_TABELLA 6
#define MAX_CELLA           96
#define TABELLA_ALTEZZA_TESTATA     ALTEZZA_S
#define TABELLA_ALTEZZA_RIGA        ALTEZZA_M
#define TABELLA_TRA_RIGHE   SPAZIO_4
#define TABELLA_SOPRA_RIGHE SPAZIO_4
#define TABELLA_DENTRO      SPAZIO_8      /* il padding della riga e dell'intestazione, ai lati */
#define TABELLA_TRA_COLONNE SPAZIO_24
#define TABELLA_FRECCIA     ICONA_S
#define TABELLA_OGGETTO_GAP SPAZIO_8

enum { ORDINE_NESSUNO, ORDINE_CRESCENTE, ORDINE_DECRESCENTE };
enum { CLIC_SEMPLICE, CLIC_DOPPIO, CLIC_DESTRO };

struct colonna_tk {
    const char *etichetta;
    int larghezza;                     /* 0: prende lo spazio che avanza */
    int destra;                        /* allineata a destra (i numeri) */
    int primaria;                      /* testo TESTO_PRIMARIO (se no TESTO_SECONDARIO) */
    int ordinabile;                    /* un clic sull'intestazione ordina */
};

/* Quello che c'e' in una riga: un testo per colonna, e l'oggetto della prima (o -1). */
struct riga_tk {
    char testi[MAX_COLONNE_TABELLA][MAX_CELLA];
    int oggetto;
};

struct tabella_tk;
typedef void (*dati_riga_tabella)(struct tabella_tk *t, int riga, struct riga_tk *r, void *dati);
typedef void (*al_clic_tabella)(struct tabella_tk *t, int riga, int come, int x, int y, void *dati);
typedef void (*al_ordine_tabella)(struct tabella_tk *t, void *dati);
typedef void (*al_tasto_tabella)(struct tabella_tk *t, unsigned int tasto, void *dati);

struct tabella_tk {
    struct widget base;
    struct barra_scorrimento barra;    /* sua figlia: sa anche dove si e' scorso */
    const struct colonna_tk *colonne;
    int quante_colonne;
    int righe;
    int scelta;                        /* la riga selezionata, o -1 */
    int sopra;                         /* quella sotto il puntatore, o -1 */
    int ordinata, verso;               /* la colonna per cui si ordina (o -1) e ORDINE_... */
    int rinomina;                      /* la riga il cui nome e' un campo, o -1 */
    const char *testo_rinomina;
    dati_riga_tabella riga;
    al_clic_tabella al_clic;
    al_ordine_tabella al_ordine;
    al_tasto_tabella al_tasto;
    void *dati;
    unsigned int clic_prima, pulsanti_prima;
    int clic_su;
};

/* Dove comincia ogni colonna (x nella finestra) e quanto e' larga. */
static inline void tabella_tk_colonne(const struct tabella_tk *t, int *x, int *l) {
    int resta = t->base.l - 2 * TABELLA_DENTRO - (t->quante_colonne - 1) * TABELLA_TRA_COLONNE, elastiche = 0;
    for (int c = 0; c < t->quante_colonne; c++) {
        resta -= t->colonne[c].larghezza;
        elastiche += !t->colonne[c].larghezza;
    }
    int pos = t->base.x + TABELLA_DENTRO;
    for (int c = 0; c < t->quante_colonne; c++) {
        l[c] = t->colonne[c].larghezza ? t->colonne[c].larghezza : (resta > 0 ? resta / elastiche : 0);
        x[c] = pos;
        pos += l[c] + TABELLA_TRA_COLONNE;
    }
}

/* Dove cominciano le righe (sotto l'intestazione, la linea e lo spazio), sullo schermo. */
static inline int tabella_tk_righe_y(const struct tabella_tk *t) {
    return t->base.y + TABELLA_ALTEZZA_TESTATA + 1 + TABELLA_SOPRA_RIGHE;
}

static inline int tabella_tk_riga_y(const struct tabella_tk *t, int k) {
    return tabella_tk_righe_y(t) + k * (TABELLA_ALTEZZA_RIGA + TABELLA_TRA_RIGHE) - t->barra.scorrimento.posizione;
}

/* La riga nel punto (x, y) dello schermo, o -1. */
static inline int tabella_tk_riga_in(const struct tabella_tk *t, int x, int y) {
    const struct widget *w = &t->base;
    if (y < tabella_tk_righe_y(t) || y >= w->y + w->a || x < w->x || x >= w->x + w->l) return -1;
    int dentro = y - tabella_tk_righe_y(t) + t->barra.scorrimento.posizione;
    int k = dentro / (TABELLA_ALTEZZA_RIGA + TABELLA_TRA_RIGHE);
    if (dentro - k * (TABELLA_ALTEZZA_RIGA + TABELLA_TRA_RIGHE) >= TABELLA_ALTEZZA_RIGA) return -1;
    return k < t->righe ? k : -1;
}

/* Rifa' le misure dello scorrimento (dopo un cambio del numero di righe o della misura). */
static inline void tabella_tk_impagina(struct tabella_tk *t) {
    struct widget *w = &t->base;
    int vista = w->y + w->a - tabella_tk_righe_y(t);
    int totale = t->righe ? t->righe * (TABELLA_ALTEZZA_RIGA + TABELLA_TRA_RIGHE) - TABELLA_TRA_RIGHE : 0;
    barra_scorrimento_posto(&t->barra, w->x, tabella_tk_righe_y(t), w->l, vista > 0 ? vista : 0);
    scorrimento_misure(&t->barra.scorrimento, totale, vista > 0 ? vista : 0);
}

static inline void tabella_tk_misura(struct widget *w, int *l, int *a) { (void)w; *l = 0; *a = 0; }

static inline void tabella_tk_disponi(struct widget *w) { tabella_tk_impagina((struct tabella_tk *)w); }

/* L'INTESTAZIONE DI COLONNA, nel rettangolo (x, y, l, TABELLA_ALTEZZA_TESTATA). */
static inline void intestazione_colonna_disegna(struct pennello *p, int x, int y, int l, const char *etichetta,
                                                int destra, int ordine) {
    int freccia = ordine != ORDINE_NESSUNO ? SPAZIO_4 + TABELLA_FRECCIA : 0;
    int largo = larghezza_testo(FONT_ETICHETTA, etichetta) + freccia;
    int tx = destra ? x + l - largo : x;
    pennello_colore(p, TESTO_SECONDARIO);
    testo_centrato(p, FONT_ETICHETTA, tx, y, TABELLA_ALTEZZA_TESTATA, etichetta);
    if (freccia)
        icona(p, ordine == ORDINE_CRESCENTE ? ICONA_CHEVRON_UP : ICONA_CHEVRON_DOWN, TABELLA_FRECCIA,
              tx + largo - TABELLA_FRECCIA, y + (TABELLA_ALTEZZA_TESTATA - TABELLA_FRECCIA) / 2);
}

static inline void tabella_tk_disegna(struct widget *w, struct pennello *p) {
    struct tabella_tk *t = (struct tabella_tk *)w;
    int cx[MAX_COLONNE_TABELLA], cl[MAX_COLONNE_TABELLA];
    tabella_tk_colonne(t, cx, cl);
    for (int c = 0; c < t->quante_colonne; c++)
        intestazione_colonna_disegna(p, cx[c], w->y, cl[c], t->colonne[c].etichetta, t->colonne[c].destra,
                                     t->ordinata == c ? t->verso : ORDINE_NESSUNO);
    int linea = w->y + TABELLA_ALTEZZA_TESTATA;
    pennello_colore(p, BORDO_SEPARATORE);
    riempi_rettangolo(p, w->x, linea, w->l, 1);

    pennello_salva(p);
    pennello_ritaglia(p, w->x, linea + 1, w->l, w->y + w->a - linea - 1);
    for (int k = 0; k < t->righe; k++) {
        int y = tabella_tk_riga_y(t, k);
        if (y + TABELLA_ALTEZZA_RIGA <= linea || y >= w->y + w->a) continue;
        int scelta = k == t->scelta || k == t->rinomina, sopra = k == t->sopra;
        if (scelta || sopra) {
            pennello_colore(p, scelta ? CONTROLLO_SFONDO_SOPRA : CONTROLLO_SFONDO_NUOVO);
            riempi_tondo(p, w->x, y, w->l, TABELLA_ALTEZZA_RIGA, RAGGIO_CONTROLLO_NUOVO);
        }
        struct riga_tk r = { .oggetto = -1 };
        if (t->riga) t->riga(t, k, &r, t->dati);
        for (int c = 0; c < t->quante_colonne; c++) {
            int x = cx[c], l = cl[c];
            if (c == 0 && r.oggetto >= 0) {
                oggetto(p, r.oggetto, OGGETTO_PICCOLO, x, y + (TABELLA_ALTEZZA_RIGA - OGGETTO_LATO_PICCOLO) / 2);
                x += OGGETTO_LATO_PICCOLO + TABELLA_OGGETTO_GAP;
                l -= OGGETTO_LATO_PICCOLO + TABELLA_OGGETTO_GAP;
            }
            if (c == 0 && k == t->rinomina && t->testo_rinomina) {
                campo_rinomina_disegna(p, x - RINOMINA_DENTRO, y + (TABELLA_ALTEZZA_RIGA - ALTEZZA_S) / 2,
                                       l + 2 * RINOMINA_DENTRO, ALTEZZA_S, RAGGIO_CAMPO_S, t->testo_rinomina, 0);
                continue;
            }
            pennello_colore(p, t->colonne[c].primaria ? TESTO_PRIMARIO : TESTO_SECONDARIO);
            testo_tagliato(p, FONT_CORPO, x, y, TABELLA_ALTEZZA_RIGA, l, r.testi[c], t->colonne[c].destra);
        }
    }
    pennello_ripristina(p);
}

/* Un clic sull'intestazione: ordina per quella colonna (o gira il verso). */
static inline void tabella_tk_clic_testata(struct tabella_tk *t, int x) {
    int cx[MAX_COLONNE_TABELLA], cl[MAX_COLONNE_TABELLA];
    tabella_tk_colonne(t, cx, cl);
    for (int c = 0; c < t->quante_colonne; c++) {
        if (x < cx[c] || x >= cx[c] + cl[c] || !t->colonne[c].ordinabile) continue;
        if (t->ordinata == c) t->verso = t->verso == ORDINE_CRESCENTE ? ORDINE_DECRESCENTE : ORDINE_CRESCENTE;
        else { t->ordinata = c; t->verso = ORDINE_CRESCENTE; }
        widget_cambiato(&t->base);
        if (t->al_ordine) t->al_ordine(t, t->dati);
        return;
    }
}

/* Il clic si sente quando si preme (come nel Finder): sinistro (semplice o doppio) o destro. */
static inline void tabella_tk_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    struct tabella_tk *t = (struct tabella_tk *)w;
    unsigned int giu = pulsanti & (PULSANTE_SINISTRO | PULSANTE_DESTRO);
    int appena = giu && !(t->pulsanti_prima & (PULSANTE_SINISTRO | PULSANTE_DESTRO));
    t->pulsanti_prima = pulsanti;
    if (!appena) return;
    if (y < w->y + TABELLA_ALTEZZA_TESTATA) {
        if (pulsanti & PULSANTE_SINISTRO) tabella_tk_clic_testata(t, x);
        return;
    }
    int k = tabella_tk_riga_in(t, x, y);
    unsigned int adesso = tempo();
    int come = pulsanti & PULSANTE_DESTRO ? CLIC_DESTRO
             : k >= 0 && k == t->clic_su && (int)(adesso - t->clic_prima) < DOPPIO_CLIC ? CLIC_DOPPIO : CLIC_SEMPLICE;
    t->clic_su = come == CLIC_SEMPLICE ? k : -1;
    t->clic_prima = adesso;
    if (t->al_clic) t->al_clic(t, k, come, x, y, t->dati);
}

static inline void tabella_tk_tasto(struct widget *w, unsigned int tasto) {
    struct tabella_tk *t = (struct tabella_tk *)w;
    if (t->al_tasto) t->al_tasto(t, tasto, t->dati);
}

static inline void tabella_tk_rotella(struct widget *w, int sx, int sy) {
    (void)sx;
    barra_scorrimento_rotella(&((struct tabella_tk *)w)->barra, sy);
}

static inline void tabella_tk_movimento(struct widget *w, int x, int y) {
    struct tabella_tk *t = (struct tabella_tk *)w;
    int k = tabella_tk_riga_in(t, x, y);
    if (k != t->sopra) { t->sopra = k; widget_cambiato(w); }
}

static inline void tabella_tk_sopra(struct widget *w) {
    struct tabella_tk *t = (struct tabella_tk *)w;
    if (!w->sopra && t->sopra >= 0) { t->sopra = -1; widget_cambiato(w); }
}

static const struct tipo_widget tipo_tabella_tk = {
    tabella_tk_misura, tabella_tk_disponi, tabella_tk_disegna, tabella_tk_puntatore, tabella_tk_tasto, tabella_tk_rotella,
    tabella_tk_sopra, tabella_tk_movimento, 0
};

/* Una tabella elastica e larga (prende lo spazio che le si da'). 'ordinata' e 'verso': come
 * e' ordinata all'inizio (-1, ORDINE_NESSUNO: per niente). */
static inline void tabella_tk_prepara(struct tabella_tk *t, const struct colonna_tk *colonne, int quante,
                                   dati_riga_tabella riga, al_clic_tabella al_clic, void *dati) {
    *t = (struct tabella_tk){ .base = { .tipo = &tipo_tabella_tk, .elastico = 1, .largo = 1 }, .colonne = colonne,
                              .quante_colonne = quante > MAX_COLONNE_TABELLA ? MAX_COLONNE_TABELLA : quante,
                              .scelta = -1, .sopra = -1, .ordinata = -1, .rinomina = -1, .riga = riga,
                              .al_clic = al_clic, .dati = dati, .clic_su = -1 };
    barra_scorrimento_prepara(&t->barra);
    widget_aggiungi(&t->base, &t->barra.base);
}

/* Le righe sono cambiate (quante, o cosa c'e' dentro). */
static inline void tabella_tk_aggiorna(struct tabella_tk *t, int righe) {
    t->righe = righe;
    if (t->scelta >= righe) t->scelta = -1;
    t->sopra = -1;
    tabella_tk_impagina(t);
    widget_cambiato(&t->base);
}

static inline void tabella_tk_scegli(struct tabella_tk *t, int riga) {
    if (t->scelta == riga) return;
    t->scelta = riga;
    widget_cambiato(&t->base);
}

/* Il nome della prima colonna della riga diventa un campo con 'testo' (che l'applicazione
 * cambia mentre si scrive); -1 lo spegne. */
static inline void tabella_tk_rinomina(struct tabella_tk *t, int riga, const char *testo) {
    t->rinomina = riga;
    t->testo_rinomina = testo;
    widget_cambiato(&t->base);
}

#endif
