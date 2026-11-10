/* Il toolkit: il terzo strato della grafica, come AppKit sul Mac o GTK su Linux.
 * Da' alle applicazioni dei pezzi pronti, i WIDGET (etichette, pulsanti, righe,
 * colonne), tutti con lo stile del sistema, e fa da solo le cose noiose: la barra del
 * titolo, il "trascinami", il fuoco, il ciclo degli eventi, il ridisegno.
 *
 *     static struct finestra_tk finestra;
 *     static struct contenitore colonna;
 *     static struct etichetta saluto;
 *     static struct pulsante ok;
 *
 *     static void al_clic(struct pulsante *p, void *dati) { etichetta_testo(&saluto, "Ciao!"); }
 *
 *     int main(void) {
 *         colonna_prepara(&colonna);
 *         etichetta_prepara(&saluto, "Premi il pulsante");
 *         pulsante_prepara(&ok, "OK", al_clic, 0);
 *         widget_aggiungi(&colonna.base, &saluto.base);
 *         widget_aggiungi(&colonna.base, &ok.base);
 *         finestra_tk_prepara(&finestra, "Saluto", &colonna.base);
 *         return toolkit_esegui(&finestra);
 *     }
 *
 * GLI OGGETTI, IN C. Ogni widget comincia con una 'struct widget' (la "base"): li' ci
 * sono il posto, i figli e il TIPO, cioe' le funzioni che sanno misurarlo, sistemarne i
 * figli, disegnarlo e fargli sentire il puntatore. Il toolkit chiama quelle, senza
 * sapere se e' un pulsante o una colonna. E' lo stesso schema di GTK, scritto in C.
 *
 * L'ALBERO. I widget stanno uno dentro l'altro: la finestra contiene una colonna, la
 * colonna un'etichetta e una riga, la riga due pulsanti. Per disegnare si scende
 * dall'alto, ognuno ritagliato nel suo spazio; per un clic si cerca il widget piu'
 * interno che sta sotto il puntatore.
 *
 * L'IMPAGINAZIONE, in due passaggi: prima ognuno dice quanto spazio vorrebbe (misura),
 * poi i contenitori distribuiscono lo spazio vero ai figli (disponi). Righe e colonne
 * mettono i figli uno dopo l'altro; nell'altra direzione li allargano a tutto lo spazio.
 *
 * IL CICLO DEGLI EVENTI E' DEL TOOLKIT. L'applicazione prepara i widget e chiama
 * toolkit_esegui, che non torna finche' la finestra c'e': aspetta le notizie del
 * compositore, le consegna ai widget, e ridisegna quando qualcosa e' cambiato.
 * L'applicazione interviene solo quando succede qualcosa che le interessa: "quando
 * premono questo pulsante, chiama questa funzione" (in GTK i segnali, in AppKit
 * target-action).
 *
 * I WIDGET LI DICHIARA L'APPLICAZIONE, come variabili sue: i programmi non possono
 * ancora chiedere memoria al kernel mentre girano (GTK li crea nella memoria dinamica).
 * Per ora: una finestra per applicazione, che non cambia misura. */
#ifndef TOOLKIT_H
#define TOOLKIT_H

#include "finestra.h"
#include "disegno.h"

/* --- lo stile del sistema: colori e misure, uguali per tutte le applicazioni --- */

#define STILE_BARRA        23        /* la barra del titolo, bordo di sopra compreso */
#define STILE_MARGINE      10        /* tra il bordo della finestra e il contenuto */
#define STILE_SPAZIO        8        /* tra un widget e l'altro in righe e colonne */
#define STILE_PULSANTE_X   14        /* spazio tra il bordo del pulsante e il testo */
#define STILE_PULSANTE_Y    5

#define COLORE_BORDO            0x202020
#define COLORE_BARRA_ATTIVA     0x3A6EA5
#define COLORE_BARRA_INATTIVA   0x707880
#define COLORE_TITOLO           0xFFFFFF
#define COLORE_FONDO            0xECECEC
#define COLORE_TESTO            0x202020
#define COLORE_PULSANTE         0xFAFAFA
#define COLORE_PULSANTE_PREMUTO 0xC8D4E4
#define COLORE_PULSANTE_BORDO   0x8A8A8A

/* --- il widget in generale --- */

struct widget;

struct tipo_widget {
    /* quanto spazio vorrebbe */
    void (*misura)(struct widget *w, int *l, int *a);
    /* il suo posto (x, y, l, a) e' deciso: sistema i figli dentro (0: niente figli) */
    void (*disponi)(struct widget *w);
    /* si disegna nel suo spazio (il pennello e' gia' ritagliato li') */
    void (*disegna)(struct widget *w, struct pennello *p);
    /* il puntatore, in coordinate della finestra (0: non gli interessa). Lo riceve il
     * widget su cui si e' premuto, finche' il pulsante non si lascia, anche da fuori */
    void (*puntatore)(struct widget *w, int x, int y, unsigned int pulsanti);
};

#define MAX_FIGLI 16

struct widget {
    const struct tipo_widget *tipo;
    struct widget *genitore;           /* 0 per la finestra, la radice dell'albero */
    int x, y, l, a;                    /* il suo spazio, in coordinate della finestra */
    struct widget *figli[MAX_FIGLI];
    int quanti_figli;
};

static inline void widget_aggiungi(struct widget *genitore, struct widget *figlio) {
    if (genitore->quanti_figli >= MAX_FIGLI) return;
    genitore->figli[genitore->quanti_figli++] = figlio;
    figlio->genitore = genitore;
}

static inline void widget_misura(struct widget *w, int *l, int *a) {
    *l = *a = 0;
    if (w->tipo->misura) w->tipo->misura(w, l, a);
}

static inline void widget_disponi(struct widget *w) {
    if (w->tipo->disponi) w->tipo->disponi(w);
    for (int i = 0; i < w->quanti_figli; i++) widget_disponi(w->figli[i]);
}

static inline void widget_disegna(struct widget *w, struct pennello *p) {
    pennello_salva(p);
    pennello_ritaglia(p, w->x, w->y, w->l, w->a);   /* nessuno sborda dal suo spazio */
    if (w->tipo->disegna) w->tipo->disegna(w, p);
    for (int i = 0; i < w->quanti_figli; i++) widget_disegna(w->figli[i], p);
    pennello_ripristina(p);
}

static inline int widget_contiene(const struct widget *w, int x, int y) {
    return x >= w->x && x < w->x + w->l && y >= w->y && y < w->y + w->a;
}

/* Il widget piu' interno sotto (x, y) che vuole sentire il puntatore, o 0. */
static inline struct widget *widget_sotto(struct widget *w, int x, int y) {
    if (!widget_contiene(w, x, y)) return 0;
    for (int i = w->quanti_figli - 1; i >= 0; i--) {
        struct widget *trovato = widget_sotto(w->figli[i], x, y);
        if (trovato) return trovato;
    }
    return w->tipo->puntatore ? w : 0;
}

/* Un widget e' cambiato (un testo nuovo, un pulsante premuto): la finestra va
 * ridisegnata. Si risale l'albero fino alla finestra. (Piu' sotto, con la finestra.) */
static inline void widget_cambiato(struct widget *w);

/* --- riga e colonna: il contenitore (la "box" di GTK, gli stack di SwiftUI) --- */

struct contenitore {
    struct widget base;
    int in_riga;                       /* 1 riga (da sinistra a destra), 0 colonna */
};

/* Si somma lungo la direzione del contenitore, si prende il piu' grande nell'altra. */
static inline void contenitore_misura(struct widget *w, int *l, int *a) {
    int in_riga = ((struct contenitore *)w)->in_riga;
    for (int i = 0; i < w->quanti_figli; i++) {
        int fl, fa;
        widget_misura(w->figli[i], &fl, &fa);
        int lungo = in_riga ? fl : fa, largo = in_riga ? fa : fl;
        int *somma = in_riga ? l : a, *massimo = in_riga ? a : l;
        *somma += lungo + (i ? STILE_SPAZIO : 0);
        if (largo > *massimo) *massimo = largo;
    }
}

static inline void contenitore_disponi(struct widget *w) {
    int in_riga = ((struct contenitore *)w)->in_riga;
    int pos = in_riga ? w->x : w->y;
    for (int i = 0; i < w->quanti_figli; i++) {
        struct widget *f = w->figli[i];
        int fl, fa;
        widget_misura(f, &fl, &fa);
        if (in_riga) { f->x = pos;  f->y = w->y; f->l = fl;   f->a = w->a; pos += fl; }
        else         { f->x = w->x; f->y = pos;  f->l = w->l; f->a = fa;   pos += fa; }
        pos += STILE_SPAZIO;
    }
}

static const struct tipo_widget tipo_contenitore = {
    contenitore_misura, contenitore_disponi, 0, 0
};

static inline void riga_prepara(struct contenitore *c) {
    *c = (struct contenitore){ .base = { .tipo = &tipo_contenitore }, .in_riga = 1 };
}

static inline void colonna_prepara(struct contenitore *c) {
    *c = (struct contenitore){ .base = { .tipo = &tipo_contenitore }, .in_riga = 0 };
}

/* --- l'etichetta: un testo --- */

#define MAX_TESTO_WIDGET 64

struct etichetta {
    struct widget base;
    char testo[MAX_TESTO_WIDGET];
};

static inline void copia_testo(char *dove, const char *da, int massimo) {
    int i = 0;
    for (; i < massimo - 1 && da[i]; i++) dove[i] = da[i];
    dove[i] = 0;
}

static inline void etichetta_misura(struct widget *w, int *l, int *a) {
    *l = larghezza_testo(((struct etichetta *)w)->testo);
    *a = altezza_testo();
}

static inline void etichetta_disegna(struct widget *w, struct pennello *p) {
    pennello_colore(p, COLORE_TESTO);
    testo(p, w->x, w->y + (w->a - altezza_testo()) / 2, ((struct etichetta *)w)->testo);
}

static const struct tipo_widget tipo_etichetta = { etichetta_misura, 0, etichetta_disegna, 0 };

static inline void etichetta_prepara(struct etichetta *e, const char *testo) {
    *e = (struct etichetta){ .base = { .tipo = &tipo_etichetta } };
    copia_testo(e->testo, testo, MAX_TESTO_WIDGET);
}

/* Cambia il testo. (Lo spazio dell'etichetta resta quello deciso all'apertura della
 * finestra: un testo piu' lungo si taglia.) */
static inline void etichetta_testo(struct etichetta *e, const char *testo) {
    copia_testo(e->testo, testo, MAX_TESTO_WIDGET);
    widget_cambiato(&e->base);
}

/* --- il pulsante --- */

struct pulsante;
typedef void (*al_clic_pulsante)(struct pulsante *p, void *dati);

struct pulsante {
    struct widget base;
    char testo[MAX_TESTO_WIDGET];
    al_clic_pulsante al_clic;          /* chi chiamare quando viene premuto */
    void *dati;                        /* passato cosi' com'e' ad al_clic */
    int premuto;                       /* il pulsante del mouse e' giu', premuto qui sopra */
    int dentro;                        /* ... e il puntatore e' ancora sopra di lui */
};

static inline void pulsante_misura(struct widget *w, int *l, int *a) {
    *l = larghezza_testo(((struct pulsante *)w)->testo) + 2 * STILE_PULSANTE_X;
    *a = altezza_testo() + 2 * STILE_PULSANTE_Y;
}

static inline void pulsante_disegna(struct widget *w, struct pennello *p) {
    struct pulsante *b = (struct pulsante *)w;
    pennello_colore(p, b->premuto && b->dentro ? COLORE_PULSANTE_PREMUTO : COLORE_PULSANTE);
    riempi_rettangolo(p, w->x, w->y, w->l, w->a);
    pennello_colore(p, COLORE_PULSANTE_BORDO);
    contorna_rettangolo(p, w->x, w->y, w->l, w->a);
    pennello_colore(p, COLORE_TESTO);
    testo(p, w->x + (w->l - larghezza_testo(b->testo)) / 2,
          w->y + (w->a - altezza_testo()) / 2, b->testo);
}

/* Come i pulsanti veri: scatta quando si LASCIA, e solo se il puntatore e' ancora
 * sopra. Chi preme e poi cambia idea esce dal pulsante prima di lasciare. */
static inline void pulsante_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    struct pulsante *b = (struct pulsante *)w;
    int giu = pulsanti & PULSANTE_SINISTRO, sopra = widget_contiene(w, x, y);
    if (giu && !b->premuto) {
        b->premuto = 1;
        b->dentro = sopra;
        widget_cambiato(w);
    } else if (giu && sopra != b->dentro) {
        b->dentro = sopra;
        widget_cambiato(w);
    } else if (!giu && b->premuto) {
        b->premuto = 0;
        widget_cambiato(w);
        if (sopra && b->al_clic) b->al_clic(b, b->dati);
    }
}

static const struct tipo_widget tipo_pulsante = {
    pulsante_misura, 0, pulsante_disegna, pulsante_puntatore
};

static inline void pulsante_prepara(struct pulsante *b, const char *testo,
                                    al_clic_pulsante al_clic, void *dati) {
    *b = (struct pulsante){ .base = { .tipo = &tipo_pulsante }, .al_clic = al_clic, .dati = dati };
    copia_testo(b->testo, testo, MAX_TESTO_WIDGET);
}

/* --- la finestra: la radice dell'albero, con la barra del titolo --- */

struct finestra_tk {
    struct widget base;                /* il suo unico figlio e' il contenuto */
    char titolo[MAX_TITOLO];
    struct finestra finestra;          /* il collegamento con il compositore */
    int fuoco;
    int da_ridisegnare;
    struct widget *catturato;          /* il widget su cui si e' premuto */
    unsigned int pulsanti_prima;
};

static inline void widget_cambiato(struct widget *w) {
    while (w->genitore) w = w->genitore;
    ((struct finestra_tk *)w)->da_ridisegnare = 1;
}

/* Il contenuto, piu' il margine, il bordo e la barra; abbastanza larga per il titolo. */
static inline void finestra_tk_misura(struct widget *w, int *l, int *a) {
    struct finestra_tk *f = (struct finestra_tk *)w;
    int cl = 0, ca = 0;
    if (w->quanti_figli) widget_misura(w->figli[0], &cl, &ca);
    int titolo = larghezza_testo(f->titolo) + 4 * STILE_MARGINE;
    *l = 2 + 2 * STILE_MARGINE + (cl > titolo ? cl : titolo);
    *a = STILE_BARRA + 2 * STILE_MARGINE + ca + 1;
}

static inline void finestra_tk_disponi(struct widget *w) {
    if (!w->quanti_figli) return;
    struct widget *c = w->figli[0];
    c->x = 1 + STILE_MARGINE;
    c->y = STILE_BARRA + STILE_MARGINE;
    c->l = w->l - 2 - 2 * STILE_MARGINE;
    c->a = w->a - STILE_BARRA - 1 - 2 * STILE_MARGINE;
}

static inline void finestra_tk_disegna(struct widget *w, struct pennello *p) {
    struct finestra_tk *f = (struct finestra_tk *)w;
    pennello_colore(p, COLORE_BORDO);
    contorna_rettangolo(p, 0, 0, w->l, w->a);
    pennello_colore(p, f->fuoco ? COLORE_BARRA_ATTIVA : COLORE_BARRA_INATTIVA);
    riempi_rettangolo(p, 1, 1, w->l - 2, STILE_BARRA - 1);
    pennello_colore(p, COLORE_TITOLO);
    testo(p, (w->l - larghezza_testo(f->titolo)) / 2,
          1 + (STILE_BARRA - 1 - altezza_testo()) / 2, f->titolo);
    pennello_colore(p, COLORE_FONDO);
    riempi_rettangolo(p, 1, STILE_BARRA, w->l - 2, w->a - STILE_BARRA - 1);
}

static const struct tipo_widget tipo_finestra_tk = {
    finestra_tk_misura, finestra_tk_disponi, finestra_tk_disegna, 0
};

static inline void finestra_tk_prepara(struct finestra_tk *f, const char *titolo,
                                       struct widget *contenuto) {
    *f = (struct finestra_tk){ .base = { .tipo = &tipo_finestra_tk } };
    copia_testo(f->titolo, titolo, MAX_TITOLO);
    widget_aggiungi(&f->base, contenuto);
}

/* --- il ciclo degli eventi --- */

static inline int toolkit_ridisegna(struct finestra_tk *f) {
    unsigned int *pixel = finestra_tela(&f->finestra);
    if (!pixel) return 0;
    struct tela t = { pixel, f->base.l, f->base.a };
    struct pennello p;
    pennello_prepara(&p, &t);
    widget_disegna(&f->base, &p);
    f->da_ridisegnare = 0;
    return finestra_mostra(&f->finestra) >= 0;
}

/* Il puntatore: un clic sulla barra e' un "trascinami"; un clic nel contenuto cattura
 * il widget sotto, che sentira' il puntatore finche' il pulsante non si lascia. */
static inline void toolkit_puntatore(struct finestra_tk *f, int x, int y, unsigned int pulsanti) {
    int giu = pulsanti & PULSANTE_SINISTRO;
    int appena_premuto = giu && !(f->pulsanti_prima & PULSANTE_SINISTRO);
    f->pulsanti_prima = pulsanti;
    if (appena_premuto) {
        if (y < STILE_BARRA) { finestra_trascinami(&f->finestra); return; }
        f->catturato = widget_sotto(&f->base, x, y);
    }
    if (f->catturato) {
        f->catturato->tipo->puntatore(f->catturato, x, y, pulsanti);
        if (!giu) f->catturato = 0;
    }
}

/* Apre la finestra e la fa vivere: non torna finche' il compositore non la chiude. */
static inline int toolkit_esegui(struct finestra_tk *f) {
    int l, a;
    widget_misura(&f->base, &l, &a);
    f->base.x = f->base.y = 0;
    f->base.l = l;
    f->base.a = a;
    widget_disponi(&f->base);
    if (finestra_apri(&f->finestra, l, a, f->titolo) < 0) return 1;
    if (!toolkit_ridisegna(f)) return 1;

    struct notizia n;
    while (finestra_notizia(&f->finestra, &n)) {
        if (n.tipo == PUNTATORE) toolkit_puntatore(f, n.x, n.y, n.pulsanti);
        else if (n.tipo == USCITO) {
            /* il puntatore non e' piu' nostro (per esempio, il compositore ha cominciato
             * a spostare la finestra): una pressione in corso si annulla, senza scattare */
            if (f->catturato) f->catturato->tipo->puntatore(f->catturato, -1, -1, 0);
            f->catturato = 0;
            f->pulsanti_prima = 0;
        }
        else if (n.tipo == FUOCO) { f->fuoco = n.fuoco; f->da_ridisegnare = 1; }
        if (f->da_ridisegnare && !toolkit_ridisegna(f)) return 1;
    }
    return 0;
}

#endif
