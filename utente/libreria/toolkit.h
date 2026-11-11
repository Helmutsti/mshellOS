/* Il toolkit: il terzo strato della grafica, come AppKit sul Mac o GTK su Linux.
 * Da' alle applicazioni dei pezzi pronti, i WIDGET, tutti con lo stile del sistema
 * (stile.h, dal file Figma "mshell - interfaccia"), e fa da solo le cose noiose: la
 * barra del titolo, il "trascinami", il fuoco, la tastiera, il ciclo degli eventi, il
 * ridisegno.
 *
 *     static struct finestra_tk finestra;
 *     static struct contenitore colonna;
 *     static struct etichetta saluto;
 *     static struct pulsante ok;
 *
 *     static void al_clic(struct pulsante *p, void *dati) { etichetta_testo(&saluto, "Ciao!"); }
 *
 *     int main(void) {
 *         colonna_prepara(&colonna, SPAZIO_TRA);
 *         etichetta_prepara(&saluto, "Premi il pulsante", FONT_TESTO);
 *         pulsante_prepara(&ok, "OK", NESSUNA_ICONA, al_clic, 0);
 *         widget_aggiungi(&colonna.base, &saluto.base);
 *         widget_aggiungi(&colonna.base, &ok.base);
 *         finestra_tk_prepara(&finestra, "Saluto", &colonna.base);
 *         return toolkit_esegui(&finestra);
 *     }
 *
 * GLI OGGETTI, IN C. Ogni widget comincia con una 'struct widget' (la "base"): li' ci
 * sono il posto, i figli e il TIPO, cioe' le funzioni che sanno misurarlo, sistemarne i
 * figli, disegnarlo, fargli sentire il puntatore e la tastiera. Il toolkit chiama quelle,
 * senza sapere se e' un pulsante o una colonna. E' lo stesso schema di GTK, in C.
 *
 * L'ALBERO. I widget stanno uno dentro l'altro. Per disegnare si scende dall'alto,
 * ognuno ritagliato nel suo spazio; per un clic si cerca il widget piu' interno che sta
 * sotto il puntatore.
 *
 * L'IMPAGINAZIONE, in due passaggi: prima ognuno dice quanto spazio vorrebbe (misura),
 * poi i contenitori distribuiscono lo spazio vero (disponi). Righe e colonne mettono i
 * figli uno dopo l'altro; lo spazio in piu' va agli SPAZI ELASTICI (come "flex: 1").
 *
 * GLI STATI. Il toolkit segna da solo quale widget ha il puntatore SOPRA (per
 * illuminarlo), quale e' PREMUTO, quale ha il FUOCO della tastiera (il campo di testo
 * su cui si e' cliccato). Un widget DISABILITATO si disegna al 40% e non sente niente.
 *
 * IL CICLO DEGLI EVENTI E' DEL TOOLKIT. toolkit_esegui non torna finche' la finestra
 * c'e': aspetta le notizie del compositore, le consegna ai widget, ridisegna quando
 * qualcosa e' cambiato. Torna quando si preme "chiudi". L'applicazione interviene solo
 * quando succede qualcosa che le interessa ("quando premono questo pulsante, chiama
 * questa funzione": in GTK i segnali, in AppKit target-action).
 *
 * IL TEMPO. Un'applicazione che deve aggiornarsi da sola (un orologio, il task manager)
 * chiede al toolkit di chiamarla ogni tanto: finestra_tk_ogni(&finestra, 1000, f, dati).
 * Il ciclo allora non dorme senza limite: aspetta le notizie fino alla prima scadenza.
 * Le chiamate possono essere piu' d'una (fino a MAX_CHIAMATE): le usano anche i widget
 * che si animano, come la barra di scorrimento, solo finche' l'animazione dura.
 *
 * LO SCORRIMENTO. Di un contenuto piu' alto del suo spazio (una lista, una tabella) se ne
 * mostra un pezzo, la VISTA, e la rotella del mouse lo fa scorrere. Il toolkit da' due
 * pezzi: 'struct scorrimento', i tre numeri (dove si e', quanto e' alto tutto, quanto se
 * ne vede), e la BARRA DI SCORRIMENTO, un widget che sta sopra il contenuto, sul bordo
 * destro, e si vede solo mentre si scorre (come sul Mac). La rotella va al widget sotto
 * il puntatore, o al primo dei suoi genitori che la vuole (il campo 'rotella' del tipo).
 *
 * I WIDGET NASCOSTI non si misurano, non si disegnano e non sentono niente: e' cosi' che
 * si fanno le pagine (piu' contenuti nello stesso posto, se ne vede uno). Dopo averne
 * mostrato o nascosto uno, toolkit_ridisponi rifa' l'impaginazione.
 *
 * I WIDGET LI DICHIARA L'APPLICAZIONE, come variabili sue. Una finestra per
 * applicazione, che non cambia misura. Il pulsante "riduci" la nasconde: torna con un
 * clic sulla sua icona nell'isola della scrivania. */
#ifndef TOOLKIT_H
#define TOOLKIT_H

#include "finestra.h"
#include "stile.h"

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
    /* un tasto, se ha il fuoco (0: non prende il fuoco) */
    void (*tasto)(struct widget *w, unsigned int tasto);
    /* la rotella del mouse, girata di 'sx', 'sy' scatti (sy positivo: in basso) con il
     * puntatore sopra di lui o sopra uno dei suoi figli (0: non gli interessa) */
    void (*rotella)(struct widget *w, int sx, int sy);
    /* il puntatore e' appena arrivato sopra di lui, o se n'e' andato: 'sopra' e' gia'
     * cambiato (0: basta il ridisegno che fa il toolkit) */
    void (*sopra_cambiato)(struct widget *w);
};

#define MAX_FIGLI 16

struct widget {
    const struct tipo_widget *tipo;
    struct widget *genitore;           /* 0 per la finestra, la radice dell'albero */
    int x, y, l, a;                    /* il suo spazio, in coordinate della finestra */
    struct widget *figli[MAX_FIGLI];
    int quanti_figli;
    int sopra;                         /* il puntatore e' sopra di lui (lo segna il toolkit) */
    int fuoco;                         /* ha il fuoco della tastiera (lo segna il toolkit) */
    int disabilitato;                  /* si vede al 40% e non sente niente */
    int elastico;                      /* prende lo spazio in piu' nel suo contenitore */
    int largo;                         /* nell'altra direzione si allarga a tutto lo spazio */
    int nascosto;                      /* non c'e': non si misura, non si vede, non sente */
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
    if (w->nascosto) return;
    pennello_salva(p);
    pennello_ritaglia(p, w->x - 4, w->y - 4, w->l + 8, w->a + 8);   /* anelli e ombre sbordano un po' */
    if (w->disabilitato) pennello_opacita(p, OPACITA_DISABILITATO);
    if (w->tipo->disegna) w->tipo->disegna(w, p);
    for (int i = 0; i < w->quanti_figli; i++) widget_disegna(w->figli[i], p);
    pennello_ripristina(p);
}

static inline int widget_contiene(const struct widget *w, int x, int y) {
    return x >= w->x && x < w->x + w->l && y >= w->y && y < w->y + w->a;
}

/* Il widget piu' interno sotto (x, y) che vuole sentire il puntatore, o 0. I widget
 * disabilitati (e quello che c'e' dentro) non si trovano. */
static inline struct widget *widget_sotto(struct widget *w, int x, int y) {
    if (w->disabilitato || w->nascosto || !widget_contiene(w, x, y)) return 0;
    for (int i = w->quanti_figli - 1; i >= 0; i--) {
        struct widget *trovato = widget_sotto(w->figli[i], x, y);
        if (trovato) return trovato;
    }
    return w->tipo->puntatore ? w : 0;
}

/* Il widget piu' interno sotto (x, y), anche se non sente il puntatore, o 0. */
static inline struct widget *widget_in(struct widget *w, int x, int y) {
    if (w->disabilitato || w->nascosto || !widget_contiene(w, x, y)) return 0;
    for (int i = w->quanti_figli - 1; i >= 0; i--) {
        struct widget *trovato = widget_in(w->figli[i], x, y);
        if (trovato) return trovato;
    }
    return w;
}

/* Un widget e' cambiato (un testo nuovo, un pulsante premuto): la finestra va
 * ridisegnata. Si risale l'albero fino alla finestra. (Piu' sotto, con la finestra.) */
static inline void widget_cambiato(struct widget *w);

static inline void widget_disabilita(struct widget *w, int disabilitato) {
    w->disabilitato = disabilitato;
    widget_cambiato(w);
}

/* Mostra o nasconde un widget. Lo spazio si ridistribuisce con toolkit_ridisponi. */
static inline void widget_nascondi(struct widget *w, int nascosto) {
    w->nascosto = nascosto;
    widget_cambiato(w);
}

static inline void copia_testo(char *dove, const char *da, int massimo) {
    int i = 0;
    for (; i < massimo - 1 && da[i]; i++) dove[i] = da[i];
    dove[i] = 0;
}

#define MAX_TESTO_WIDGET 64
#define NESSUNA_ICONA (-1)

/* Un testo centrato in verticale nel rettangolo (x, y, ..., a), con il colore del pennello. */
static inline int testo_centrato(struct pennello *p, const struct font *f, int x, int y, int a,
                                 const char *s) {
    return testo(p, f, x, y + (a - f->altezza) / 2, s);
}

/* --- riga e colonna: il contenitore (la "box" di GTK, gli stack di SwiftUI) --- */

struct contenitore {
    struct widget base;
    int in_riga;                       /* 1 riga (da sinistra a destra), 0 colonna */
    int spazio;                        /* tra un figlio e l'altro */
};

/* Si somma lungo la direzione del contenitore, si prende il piu' grande nell'altra. */
static inline void contenitore_misura(struct widget *w, int *l, int *a) {
    struct contenitore *c = (struct contenitore *)w;
    int visti = 0;
    for (int i = 0; i < w->quanti_figli; i++) {
        if (w->figli[i]->nascosto) continue;
        int fl, fa;
        widget_misura(w->figli[i], &fl, &fa);
        int lungo = c->in_riga ? fl : fa, largo = c->in_riga ? fa : fl;
        int *somma = c->in_riga ? l : a, *massimo = c->in_riga ? a : l;
        *somma += lungo + (visti++ ? c->spazio : 0);
        if (largo > *massimo) *massimo = largo;
    }
}

/* Ognuno la sua misura; lo spazio in piu' diviso tra gli elastici. Nell'altra
 * direzione: tutto lo spazio a chi e' "largo", agli altri la loro misura (in una riga,
 * centrati in altezza). */
static inline void contenitore_disponi(struct widget *w) {
    struct contenitore *c = (struct contenitore *)w;
    int l = 0, a = 0, elastici = 0;
    contenitore_misura(w, &l, &a);
    for (int i = 0; i < w->quanti_figli; i++)
        if (!w->figli[i]->nascosto) elastici += w->figli[i]->elastico;
    int in_piu = c->in_riga ? w->l - l : w->a - a;
    if (in_piu < 0) in_piu = 0;
    int pos = c->in_riga ? w->x : w->y;
    for (int i = 0; i < w->quanti_figli; i++) {
        struct widget *f = w->figli[i];
        if (f->nascosto) continue;
        int fl, fa;
        widget_misura(f, &fl, &fa);
        int lungo = (c->in_riga ? fl : fa) + (f->elastico ? in_piu / elastici : 0);
        if (c->in_riga) {
            f->x = pos; f->l = lungo;
            f->a = f->largo ? w->a : fa;
            f->y = w->y + (w->a - f->a) / 2;
        } else {
            f->y = pos; f->a = lungo;
            f->l = f->largo ? w->l : fl;
            f->x = w->x;
        }
        pos += lungo + c->spazio;
    }
}

static const struct tipo_widget tipo_contenitore = {
    contenitore_misura, contenitore_disponi, 0, 0, 0, 0, 0
};

static inline void riga_prepara(struct contenitore *c, int spazio) {
    *c = (struct contenitore){ .base = { .tipo = &tipo_contenitore, .largo = 1 },
                               .in_riga = 1, .spazio = spazio };
}

static inline void colonna_prepara(struct contenitore *c, int spazio) {
    *c = (struct contenitore){ .base = { .tipo = &tipo_contenitore, .largo = 1 },
                               .in_riga = 0, .spazio = spazio };
}

/* --- lo spazio elastico: vuoto, prende lo spazio che avanza --- */

struct spazio { struct widget base; };

static const struct tipo_widget tipo_spazio = { 0, 0, 0, 0, 0, 0, 0 };

static inline void spazio_prepara(struct spazio *s) {
    *s = (struct spazio){ .base = { .tipo = &tipo_spazio, .elastico = 1 } };
}

/* --- l'etichetta: un testo --- */

struct etichetta {
    struct widget base;
    char testo[MAX_TESTO_WIDGET];
    const struct font *font;           /* FONT_TESTO, FONT_TITOLO... */
    unsigned int colore;
};

static inline void etichetta_misura(struct widget *w, int *l, int *a) {
    struct etichetta *e = (struct etichetta *)w;
    *l = larghezza_testo(e->font, e->testo);
    *a = e->font->altezza;
}

static inline void etichetta_disegna(struct widget *w, struct pennello *p) {
    struct etichetta *e = (struct etichetta *)w;
    pennello_colore(p, e->colore);
    testo_centrato(p, e->font, w->x, w->y, w->a, e->testo);
}

static const struct tipo_widget tipo_etichetta = { etichetta_misura, 0, etichetta_disegna, 0, 0, 0, 0 };

static inline void etichetta_prepara(struct etichetta *e, const char *testo, const struct font *font) {
    *e = (struct etichetta){ .base = { .tipo = &tipo_etichetta }, .font = font,
                             .colore = CONTENUTO_PRINCIPALE };
    copia_testo(e->testo, testo, MAX_TESTO_WIDGET);
}

/* Cambia il testo. (Lo spazio dell'etichetta resta quello deciso all'apertura della
 * finestra: un testo piu' lungo si taglia.) */
static inline void etichetta_testo(struct etichetta *e, const char *testo) {
    copia_testo(e->testo, testo, MAX_TESTO_WIDGET);
    widget_cambiato(&e->base);
}

/* --- il pulsante: 5 colori, 3 misure, con o senza icona, anche solo icona --- */

enum { NEUTRO, BLU, VERDE, ROSSO, ARANCIO };
enum { PICCOLO, MEDIO, GRANDE };

/* Le misure del disegno: altezza, margine ai lati, spazio icona-testo, lato
 * dell'icona, raggio degli angoli, font. */
struct misure_pulsante { int altezza, margine, spazio, icona, raggio; const struct font *font; };
static const struct misure_pulsante misure_pulsante[] = {
    { 28, 10, 6, 14, RAGGIO_PICCOLO, &inter_medio_13 },
    { 36, 14, 8, 16, RAGGIO_MEDIO,   &inter_medio_14 },
    { 44, 18, 8, 20, RAGGIO_GRANDE,  &inter_medio_16 },
};

/* normale, sopra, premuto (il neutro e' di vetro) */
static const unsigned int colori_pulsante[][3] = {
    { VETRO_SUPERFICIE, VETRO_SOPRA,   VETRO_PREMUTO   },
    { BLU_NORMALE,      BLU_SOPRA,     BLU_PREMUTO     },
    { VERDE_NORMALE,    VERDE_SOPRA,   VERDE_PREMUTO   },
    { ROSSO_NORMALE,    ROSSO_SOPRA,   ROSSO_PREMUTO   },
    { ARANCIO_NORMALE,  ARANCIO_SOPRA, ARANCIO_PREMUTO },
};

struct pulsante;
typedef void (*al_clic_pulsante)(struct pulsante *p, void *dati);

struct pulsante {
    struct widget base;
    char testo[MAX_TESTO_WIDGET];      /* vuoto: pulsante solo icona */
    int icona;                         /* ICONA_..., o NESSUNA_ICONA */
    int colore, dimensione;
    al_clic_pulsante al_clic;          /* chi chiamare quando viene premuto */
    void *dati;                        /* passato cosi' com'e' ad al_clic */
    int premuto;                       /* il pulsante del mouse e' giu', premuto qui sopra */
    int dentro;                        /* ... e il puntatore e' ancora sopra di lui */
};

static inline void pulsante_misura(struct widget *w, int *l, int *a) {
    struct pulsante *b = (struct pulsante *)w;
    const struct misure_pulsante *m = &misure_pulsante[b->dimensione];
    *a = m->altezza;
    if (!b->testo[0]) { *l = m->altezza; return; }             /* solo icona: quadrato */
    *l = 2 * m->margine + larghezza_testo(m->font, b->testo);
    if (b->icona != NESSUNA_ICONA) *l += m->icona + m->spazio;
}

static inline void pulsante_disegna(struct widget *w, struct pennello *p) {
    struct pulsante *b = (struct pulsante *)w;
    const struct misure_pulsante *m = &misure_pulsante[b->dimensione];
    int premuto = b->premuto && b->dentro;
    int stato = premuto ? 2 : w->sopra ? 1 : 0;
    struct tondo t = tondo(w->x, w->y, w->l, w->a, m->raggio);

    if (!premuto) {                    /* premuto, il pulsante "scende" e l'ombra sparisce */
        pennello_colore(p, PULSANTE_OMBRA);
        ombra_forma(p, &t, 0, 2, 3);
    }
    pennello_colore(p, colori_pulsante[b->colore][stato]);
    riempi_forma(p, &t);
    pennello_colore(p, b->colore == NEUTRO ? VETRO_BORDO : PULSANTE_BORDO_COLORATO);
    contorna_forma(p, &t, 256);
    if (!premuto) {
        pennello_colore(p, VETRO_LUCE);
        luce_sopra(p, &t);
    }

    /* icona e testo, centrati insieme */
    int largo = b->testo[0] ? larghezza_testo(m->font, b->testo) : 0;
    if (b->icona != NESSUNA_ICONA) largo += m->icona + (b->testo[0] ? m->spazio : 0);
    int x = w->x + (w->l - largo) / 2;
    pennello_colore(p, CONTENUTO_PRINCIPALE);
    if (b->icona != NESSUNA_ICONA) {
        icona(p, b->icona, m->icona, x, w->y + (w->a - m->icona) / 2);
        x += m->icona + m->spazio;
    }
    if (b->testo[0]) testo_centrato(p, m->font, x, w->y, w->a, b->testo);
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
    pulsante_misura, 0, pulsante_disegna, pulsante_puntatore, 0, 0, 0
};

/* Un pulsante neutro, di misura media. Per un pulsante solo icona, testo "". */
static inline void pulsante_prepara(struct pulsante *b, const char *testo, int icona,
                                    al_clic_pulsante al_clic, void *dati) {
    *b = (struct pulsante){ .base = { .tipo = &tipo_pulsante }, .icona = icona,
                            .colore = NEUTRO, .dimensione = MEDIO, .al_clic = al_clic, .dati = dati };
    copia_testo(b->testo, testo, MAX_TESTO_WIDGET);
}

static inline void pulsante_stile(struct pulsante *b, int colore, int dimensione) {
    b->colore = colore;
    b->dimensione = dimensione;
    widget_cambiato(&b->base);
}

/* --- la casella di spunta e la scelta singola --- */

enum { SPENTO, ACCESO, MISTO };

struct casella;
typedef void (*al_cambio_casella)(struct casella *c, void *dati);

/* Una casella di spunta, o una scelta singola (il tondo: in un gruppo se ne accende
 * una sola). Si comportano allo stesso modo, cambia il disegno. */
struct casella {
    struct widget base;
    char testo[MAX_TESTO_WIDGET];
    int valore;                        /* SPENTO, ACCESO, MISTO (solo la casella) */
    int tonda;                         /* 1: scelta singola */
    void *gruppo;                      /* le scelte con lo stesso gruppo si escludono */
    al_cambio_casella al_cambio;
    void *dati;
    int premuto;
};

static inline void casella_misura(struct widget *w, int *l, int *a) {
    struct casella *c = (struct casella *)w;
    *l = CONTROLLO_LATO + 8 + larghezza_testo(FONT_TESTO, c->testo);
    *a = CONTROLLO_LATO > FONT_TESTO->altezza ? CONTROLLO_LATO : FONT_TESTO->altezza;
}

static inline void casella_disegna(struct widget *w, struct pennello *p) {
    struct casella *c = (struct casella *)w;
    int x = w->x, y = w->y + (w->a - CONTROLLO_LATO) / 2;
    int raggio = c->tonda ? CONTROLLO_LATO / 2 : RAGGIO_CONTROLLO;
    struct tondo t = tondo(x, y, CONTROLLO_LATO, CONTROLLO_LATO, raggio);

    if (w->sopra && !(c->tonda && c->valore == ACCESO)) {   /* l'anello azzurro intorno */
        struct tondo anello = tondo(x - 3, y - 3, CONTROLLO_LATO + 6, CONTROLLO_LATO + 6, raggio + 3);
        pennello_colore(p, ANELLO_SOPRA);
        contorna_forma(p, &anello, 3 * 256);
    }
    if (c->tonda && c->valore == ACCESO) {
        /* la scelta accesa e' l'immagine del disegno (cerchio blu con il punto bianco;
         * quella "sopra" ha gia' l'anello, ed e' piu' grande di 3 pixel per lato) */
        if (w->sopra) disegna_immagine(p, &scelta_accesa_sopra, x - 3, y - 3);
        else disegna_immagine(p, &scelta_accesa, x, y);
    } else if (c->valore != SPENTO) {
        pennello_colore(p, w->sopra ? BLU_SOPRA : BLU_NORMALE);
        riempi_forma(p, &t);
        pennello_colore(p, CAMPO_BORDO);
        contorna_forma(p, &t, 256);
        pennello_colore(p, CONTENUTO_PRINCIPALE);
        icona(p, c->valore == ACCESO ? ICONA_CHECK : ICONA_MINUS, 14, x + 2, y + 2);
    } else {
        pennello_colore(p, w->sopra ? VETRO_SOPRA : CONTROLLO_SFONDO);
        riempi_forma(p, &t);
        pennello_colore(p, CONTROLLO_BORDO);
        contorna_forma(p, &t, 256);
    }
    pennello_colore(p, CONTENUTO_PRINCIPALE);
    testo_centrato(p, FONT_TESTO, x + CONTROLLO_LATO + 8, w->y, w->a, c->testo);
}

static inline void casella_puntatore(struct widget *w, int x, int y, unsigned int pulsanti);

/* Spegne le altre scelte dello stesso gruppo, in tutto l'albero. */
static inline void spegni_gruppo(struct widget *w, struct casella *tranne) {
    for (int i = 0; i < w->quanti_figli; i++) {
        struct widget *f = w->figli[i];
        if (f->tipo->puntatore == casella_puntatore) {
            struct casella *c = (struct casella *)f;
            if (c != tranne && c->tonda && c->gruppo == tranne->gruppo && c->valore) {
                c->valore = SPENTO;
                widget_cambiato(f);
            }
        }
        spegni_gruppo(f, tranne);
    }
}

static inline void casella_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    struct casella *c = (struct casella *)w;
    if (pulsanti & PULSANTE_SINISTRO) { c->premuto = 1; return; }
    if (!c->premuto) return;
    c->premuto = 0;
    if (!widget_contiene(w, x, y)) return;   /* lasciato fuori: ci ha ripensato */
    if (c->tonda) {
        if (c->valore == ACCESO) return;     /* una scelta accesa resta accesa */
        c->valore = ACCESO;
        struct widget *radice = w;
        while (radice->genitore) radice = radice->genitore;
        spegni_gruppo(radice, c);
    } else {
        c->valore = c->valore == ACCESO ? SPENTO : ACCESO;   /* "misto" diventa acceso */
    }
    widget_cambiato(w);
    if (c->al_cambio) c->al_cambio(c, c->dati);
}

static const struct tipo_widget tipo_casella = {
    casella_misura, 0, casella_disegna, casella_puntatore, 0, 0, 0
};

static inline void casella_prepara(struct casella *c, const char *testo, int valore,
                                   al_cambio_casella al_cambio, void *dati) {
    *c = (struct casella){ .base = { .tipo = &tipo_casella }, .valore = valore,
                           .al_cambio = al_cambio, .dati = dati };
    copia_testo(c->testo, testo, MAX_TESTO_WIDGET);
}

static inline void scelta_prepara(struct casella *c, const char *testo, void *gruppo, int accesa,
                                  al_cambio_casella al_cambio, void *dati) {
    casella_prepara(c, testo, accesa ? ACCESO : SPENTO, al_cambio, dati);
    c->tonda = 1;
    c->gruppo = gruppo;
}

/* --- il campo di testo --- */

struct campo;
typedef void (*al_cambio_campo)(struct campo *c, void *dati);

struct campo {
    struct widget base;
    char testo[MAX_TESTO_WIDGET];
    char suggerimento[MAX_TESTO_WIDGET];   /* quello che si vede, piu' chiaro, quando e' vuoto */
    int icona;
    int errore;                            /* bordo rosso */
    al_cambio_campo al_cambio;
    void *dati;
};

static inline void campo_misura(struct widget *w, int *l, int *a) {
    (void)w;
    *l = 280;                              /* la misura del disegno; di solito si allarga */
    *a = CAMPO_ALTEZZA;
}

static inline void campo_disegna(struct widget *w, struct pennello *p) {
    struct campo *c = (struct campo *)w;
    struct tondo t = tondo(w->x, w->y, w->l, w->a, RAGGIO_MEDIO);
    pennello_colore(p, w->fuoco ? CAMPO_SFONDO_ATTIVO : CAMPO_SFONDO);
    riempi_forma(p, &t);
    if (c->errore || w->fuoco) {           /* errore o fuoco: bordo di un pixel e mezzo */
        pennello_colore(p, c->errore ? STATO_ERRORE : STATO_ATTIVO);
        contorna_forma(p, &t, 384);
    } else {
        pennello_colore(p, CAMPO_BORDO);
        contorna_forma(p, &t, 256);
    }

    pennello_salva(p);
    pennello_ritaglia(p, w->x + 1, w->y + 1, w->l - 2, w->a - 2);
    int x = w->x + 10;
    pennello_colore(p, CONTENUTO_PRINCIPALE);
    if (c->icona != NESSUNA_ICONA) {
        icona(p, c->icona, 16, x, w->y + (w->a - 16) / 2);
        x += 16 + 8;
    }
    if (c->testo[0] || w->fuoco) {
        /* LO SCORRIMENTO. Il testo si vede in una striscia, dal suo inizio a 10 pixel dal
         * bordo destro (lo stesso margine di sinistra). Se e' piu' lungo, si fa scorrere
         * a sinistra quanto basta perche' il cursore resti dentro, come in ogni campo di
         * testo. Il cursore e' sempre in fondo (la tastiera non manda ancora le frecce),
         * quindi basta guardare la fine del testo: scrivendo il testo va a sinistra,
         * cancellando torna a destra. Senza fuoco si vede l'inizio, come sul Mac. */
        int destra = w->x + w->l - 10;
        int fine = larghezza_testo(FONT_TESTO, c->testo) + 8 + 2;   /* il cursore compreso */
        int scorrimento = w->fuoco && x + fine > destra ? x + fine - destra : 0;
        pennello_ritaglia(p, x - 2, w->y, destra - (x - 2), w->a);  /* 2: le lettere che sbordano */
        x = testo_centrato(p, FONT_TESTO, x - scorrimento, w->y, w->a, c->testo);
        if (w->fuoco) {                    /* il cursore: una barretta alta 18 */
            struct tondo cursore = tondo(x + 8, w->y + (w->a - 18) / 2, 2, 18, 0);
            riempi_forma(p, &cursore);
        }
    } else {
        pennello_colore(p, CONTENUTO_SECONDARIO);
        testo_centrato(p, FONT_TESTO, x, w->y, w->a, c->suggerimento);
    }
    pennello_ripristina(p);
}

/* Il clic gli da' il fuoco (lo fa il toolkit): qui non serve altro. */
static inline void campo_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    (void)w; (void)x; (void)y; (void)pulsanti;
}

/* I tasti: le lettere si aggiungono in fondo, "cancella" toglie l'ultima. */
static inline void campo_tasto(struct widget *w, unsigned int tasto) {
    struct campo *c = (struct campo *)w;
    int n = lunghezza(c->testo);
    if (tasto == '\b') {
        if (!n) return;
        c->testo[n - 1] = 0;
    } else if (tasto >= ' ' && tasto != 0x7F) {
        if (n >= MAX_TESTO_WIDGET - 1) return;
        c->testo[n] = (char)tasto;
        c->testo[n + 1] = 0;
    } else {
        return;                            /* Invio, Tab...: non sono per lui */
    }
    widget_cambiato(w);
    if (c->al_cambio) c->al_cambio(c, c->dati);
}

static const struct tipo_widget tipo_campo = {
    campo_misura, 0, campo_disegna, campo_puntatore, campo_tasto, 0, 0
};

static inline void campo_prepara(struct campo *c, const char *testo, const char *suggerimento,
                                 int icona, al_cambio_campo al_cambio, void *dati) {
    *c = (struct campo){ .base = { .tipo = &tipo_campo, .largo = 1 }, .icona = icona,
                         .al_cambio = al_cambio, .dati = dati };
    copia_testo(c->testo, testo, MAX_TESTO_WIDGET);
    copia_testo(c->suggerimento, suggerimento, MAX_TESTO_WIDGET);
}

static inline void campo_errore(struct campo *c, int errore) {
    c->errore = errore;
    widget_cambiato(&c->base);
}

/* --- la finestra: la radice dell'albero, con la barra del titolo --- */

struct finestra_tk;

/* Una chiamata "ogni tanto" (finestra_tk_ogni). */
#define MAX_CHIAMATE 4
struct chiamata_tk {
    void (*funzione)(struct finestra_tk *f, void *dati);   /* 0: posto libero */
    void *dati;
    unsigned int ms;
    unsigned int scadenza;             /* quando chiamarla, in tempo() */
};

struct finestra_tk {
    struct widget base;                /* il suo unico figlio e' il contenuto */
    char titolo[MAX_TITOLO];
    int l_fissa, a_fissa;              /* 0: la misura la decide il contenuto */
    struct finestra finestra;          /* il collegamento con il compositore */
    int attiva;                        /* ha il fuoco: e' in primo piano */
    int da_ridisegnare;
    struct widget *catturato;          /* il widget su cui si e' premuto */
    struct widget *sotto;              /* quello che ha il puntatore sopra */
    struct widget *con_fuoco;          /* quello che riceve i tasti */
    int chiudi_premuto;
    int riduci_premuto;
    int da_chiudere;
    unsigned int pulsanti_prima;
    struct chiamata_tk chiamate[MAX_CHIAMATE];   /* i "chiamami ogni tanto" */
};

/* La finestra in cui sta un widget: la radice del suo albero. */
static inline struct finestra_tk *widget_finestra(struct widget *w) {
    while (w->genitore) w = w->genitore;
    return (struct finestra_tk *)w;
}

static inline void widget_cambiato(struct widget *w) {
    widget_finestra(w)->da_ridisegnare = 1;
}

/* Dove stanno i due comandi nella barra: riduci e chiudi, a destra. */
static inline int comando_x(const struct widget *w, int chiudi) {
    return w->l - BARRA_MARGINE - BARRA_COMANDO - (chiudi ? 0 : BARRA_COMANDO + 6);
}
#define COMANDO_Y ((BARRA_ALTEZZA - BARRA_COMANDO) / 2)

static inline int su_comando(const struct widget *w, int chiudi, int x, int y) {
    int cx = comando_x(w, chiudi);
    return x >= cx && x < cx + BARRA_COMANDO && y >= COMANDO_Y && y < COMANDO_Y + BARRA_COMANDO;
}

/* Il contenuto, piu' il margine e la barra; abbastanza larga per il titolo e i comandi. */
static inline void finestra_tk_misura(struct widget *w, int *l, int *a) {
    struct finestra_tk *f = (struct finestra_tk *)w;
    if (f->l_fissa) { *l = f->l_fissa; *a = f->a_fissa; return; }
    int cl = 0, ca = 0;
    if (w->quanti_figli) widget_misura(w->figli[0], &cl, &ca);
    int barra = 2 * (BARRA_MARGINE + BARRA_EQUILIBRIO + 8) + larghezza_testo(FONT_BARRA, f->titolo);
    *l = 2 * FINESTRA_MARGINE + cl;
    if (*l < barra) *l = barra;
    *a = BARRA_ALTEZZA + 2 * FINESTRA_MARGINE + ca;
}

static inline void finestra_tk_disponi(struct widget *w) {
    if (!w->quanti_figli) return;
    struct widget *c = w->figli[0];
    c->x = FINESTRA_MARGINE;
    c->y = BARRA_ALTEZZA + FINESTRA_MARGINE;
    c->l = w->l - 2 * FINESTRA_MARGINE;
    c->a = w->a - BARRA_ALTEZZA - 2 * FINESTRA_MARGINE;
}

/* La finestra disegna il vetro (la barra un po' piu' chiara del corpo), il bordo, il
 * titolo al centro e i comandi. L'ombra intorno la disegna il compositore: e' fuori
 * dalla finestra. */
static inline void finestra_tk_disegna(struct widget *w, struct pennello *p) {
    struct finestra_tk *f = (struct finestra_tk *)w;
    pulisci(p);
    struct tondo barra = { 0, 0, w->l, BARRA_ALTEZZA, { RAGGIO_FINESTRA, RAGGIO_FINESTRA, 0, 0 } };
    struct tondo corpo = { 0, BARRA_ALTEZZA, w->l, w->a - BARRA_ALTEZZA,
                           { 0, 0, RAGGIO_FINESTRA, RAGGIO_FINESTRA } };
    struct tondo tutta = tondo(0, 0, w->l, w->a, RAGGIO_FINESTRA);
    pennello_colore(p, VETRO_FONDO);          /* sotto il vetro, il fondo scuro */
    riempi_forma(p, &tutta);
    pennello_colore(p, f->attiva ? BARRA_ATTIVA : BARRA_INATTIVA);
    riempi_forma(p, &barra);
    pennello_colore(p, CAMPO_SFONDO);
    riempi_forma(p, &corpo);
    if (f->attiva) {
        pennello_colore(p, BARRA_BORDO_ATTIVA);
        contorna_forma(p, &tutta, 256);
    }

    /* il titolo, al centro della barra (lo spazio vuoto a sinistra pareggia i comandi) */
    pennello_salva(p);
    int margine = BARRA_MARGINE + BARRA_EQUILIBRIO + 8;
    pennello_ritaglia(p, margine, 0, w->l - 2 * margine, BARRA_ALTEZZA);
    pennello_colore(p, f->attiva ? CONTENUTO_PRINCIPALE : CONTENUTO_SECONDARIO);
    testo_centrato(p, FONT_BARRA, (w->l - larghezza_testo(FONT_BARRA, f->titolo)) / 2, 0,
                   BARRA_ALTEZZA, f->titolo);
    pennello_ripristina(p);

    /* i comandi: due tondi di vetro con l'icona */
    pennello_salva(p);
    if (!f->attiva) pennello_opacita(p, OPACITA_INATTIVO);
    for (int chiudi = 0; chiudi < 2; chiudi++) {
        int cx = comando_x(w, chiudi);
        pennello_colore(p, VETRO_SUPERFICIE);
        riempi_tondo(p, cx, COMANDO_Y, BARRA_COMANDO, BARRA_COMANDO, BARRA_COMANDO / 2);
        pennello_colore(p, CONTENUTO_PRINCIPALE);
        icona(p, chiudi ? ICONA_X : ICONA_MINUS, 12, cx + 5, COMANDO_Y + 5);
    }
    pennello_ripristina(p);
}

static const struct tipo_widget tipo_finestra_tk = {
    finestra_tk_misura, finestra_tk_disponi, finestra_tk_disegna, 0, 0, 0, 0
};

static inline void finestra_tk_prepara(struct finestra_tk *f, const char *titolo,
                                       struct widget *contenuto) {
    *f = (struct finestra_tk){ .base = { .tipo = &tipo_finestra_tk } };
    copia_testo(f->titolo, titolo, MAX_TITOLO);
    widget_aggiungi(&f->base, contenuto);
}

/* Chiude la finestra, come il pulsante "chiudi": toolkit_esegui torna. */
static inline void finestra_tk_chiudi(struct finestra_tk *f) { f->da_chiudere = 1; }

/* Una misura fissa per la finestra, invece di quella del contenuto. */
static inline void finestra_tk_dimensione(struct finestra_tk *f, int l, int a) {
    f->l_fissa = l;
    f->a_fissa = a;
}

/* Chiama 'funzione' ogni 'ms' millisecondi, finche' la finestra e' aperta (0: smette).
 * La prima volta dopo 'ms'. La stessa funzione con gli stessi dati e' la stessa chiamata:
 * chiederla di nuovo cambia il suo ritmo (e la sua prossima volta), non ne aggiunge
 * un'altra. Cosi' una funzione puo' anche decidere da se' quando essere richiamata. */
static inline void finestra_tk_ogni(struct finestra_tk *f, unsigned int ms,
                                    void (*funzione)(struct finestra_tk *f, void *dati), void *dati) {
    struct chiamata_tk *posto = 0;
    for (int i = 0; i < MAX_CHIAMATE; i++) {
        struct chiamata_tk *c = &f->chiamate[i];
        if (c->funzione == funzione && c->dati == dati) { posto = c; break; }
        if (!c->funzione && !posto) posto = c;
    }
    if (!posto) return;                         /* tutte prese: non si puo' */
    if (!ms) {
        if (posto->funzione == funzione && posto->dati == dati) posto->funzione = 0;
        return;
    }
    *posto = (struct chiamata_tk){ funzione, dati, ms, tempo() + ms };
}

/* La chiamata che scade per prima, o 0 se non ce ne sono. */
static inline struct chiamata_tk *toolkit_prima_chiamata(struct finestra_tk *f) {
    struct chiamata_tk *prima = 0;
    for (int i = 0; i < MAX_CHIAMATE; i++) {
        struct chiamata_tk *c = &f->chiamate[i];
        if (c->funzione && (!prima || (int)(c->scadenza - prima->scadenza) < 0)) prima = c;
    }
    return prima;
}

/* Fa le chiamate scadute. Ognuna prima si rimette in fila (la prossima volta tra 'ms'),
 * poi si chiama: cosi' puo' cambiare il suo ritmo, o smettere. */
static inline void toolkit_chiamate_scadute(struct finestra_tk *f) {
    unsigned int ora = tempo();
    for (int i = 0; i < MAX_CHIAMATE; i++) {
        struct chiamata_tk *c = &f->chiamate[i];
        if (!c->funzione || (int)(c->scadenza - ora) > 0) continue;
        c->scadenza = ora + c->ms;
        c->funzione(f, c->dati);
    }
}

/* Rifa' l'impaginazione dentro la finestra (dopo aver mostrato o nascosto dei widget). */
static inline void toolkit_ridisponi(struct finestra_tk *f) {
    widget_disponi(&f->base);
    f->da_ridisegnare = 1;
}

/* --- lo scorrimento e la barra di scorrimento --- */

/* Dove si e' in un contenuto piu' alto della sua vista, in pixel. Chi disegna il
 * contenuto lo sposta in su di 'posizione'; chi riceve un clic gliela somma. Gli ultimi
 * due campi servono alla rotella (scorrimento_rotella). */
struct scorrimento {
    int posizione;                     /* i pixel del contenuto passati sopra la vista */
    int totale;                        /* l'altezza di tutto il contenuto */
    int vista;                         /* l'altezza della parte che si vede */
    unsigned int rotella_prima;        /* tempo() dello scatto di prima */
    int raffica;                       /* gli scatti della raffica in corso (col segno del verso) */
};

/* Fin dove si puo' scorrere: 0 se il contenuto ci sta tutto. */
static inline int scorrimento_massimo(const struct scorrimento *s) {
    return s->totale > s->vista ? s->totale - s->vista : 0;
}

/* Va a 'posizione' (dentro i limiti). 1 se si e' spostato. */
static inline int scorrimento_metti(struct scorrimento *s, int posizione) {
    int massimo = scorrimento_massimo(s);
    if (posizione > massimo) posizione = massimo;
    if (posizione < 0) posizione = 0;
    if (posizione == s->posizione) return 0;
    s->posizione = posizione;
    return 1;
}

/* Il contenuto o la vista hanno cambiato altezza: la posizione resta dentro i limiti. */
static inline void scorrimento_misure(struct scorrimento *s, int totale, int vista) {
    s->totale = totale;
    s->vista = vista;
    scorrimento_metti(s, s->posizione);
}

/* La rotella ha girato di 'scatti' (positivi: in basso, o a destra): si scorre come sul
 * Mac, con l'accelerazione spiegata in stile.h (SCORRIMENTO_RIGA). Basta l'ora di ogni
 * scatto: niente attese, niente chiamate "ogni tanto". 1 se si e' spostato. */
static inline int scorrimento_rotella(struct scorrimento *s, int scatti) {
    if (!scatti) return 0;
    int verso = scatti > 0 ? 1 : -1;
    unsigned int ora = tempo();
    if (ora - s->rotella_prima >= SCORRIMENTO_PAUSA || s->raffica * verso <= 0)
        s->raffica = 0;                /* dopo una pausa, o girando al contrario: da capo */
    s->rotella_prima = ora;
    int pixel = 0;
    for (int i = 0; i < scatti * verso; i++) {
        if (s->raffica * verso < SCORRIMENTO_SLANCIO) s->raffica += verso;
        pixel += s->raffica * SCORRIMENTO_RIGA;   /* il k-esimo scatto vale k righe */
    }
    return scorrimento_metti(s, s->posizione + pixel);
}

/* LA BARRA DI SCORRIMENTO, dal file Figma (il componente "Barra di scorrimento"). Sta
 * SOPRA il contenuto, in una striscia larga 14 pixel lungo il suo bordo destro, e:
 *   - a riposo non si vede (e non si sente: i clic vanno al contenuto sotto);
 *   - quando si scorre compare (in 100 ms): il cursore, sottile (6 pixel, a 4 dal bordo),
 *     senza guida; resta un secondo dopo l'ultimo scatto, poi svanisce (in 300 ms);
 *   - con il puntatore sopra, o mentre la si trascina, il cursore si allarga a 10 pixel
 *     (in 150 ms) e dietro compare la guida, appena chiara. Finche' il puntatore e' sopra
 *     non svanisce.
 * Il cursore e' lungo quanto la parte che si vede, in proporzione (almeno 32 pixel), e sta
 * dove si e' nel contenuto. Trascinandolo il contenuto scorre; un clic sulla guida porta
 * li' il centro del cursore. Se il contenuto ci sta tutto, la barra non compare mai.
 *
 * LE ANIMAZIONI. A ogni passo si guarda quanto tempo e' passato, e opacita' e spessore si
 * avvicinano al loro obiettivo di conseguenza: anche se un passo arriva in ritardo,
 * l'animazione dura quanto deve. I passi li fa una chiamata "ogni tanto" del toolkit, che
 * c'e' solo mentre qualcosa si muove: con la barra ferma, la finestra non si ridisegna.
 * Mentre la barra aspetta il secondo prima di svanire, la chiamata e' una sola, alla fine.
 *
 * Per usarla: il widget che scorre la tiene tra i suoi figli (widget_aggiungi), le dice
 * dov'e' la vista (barra_scorrimento_posto) e quanto e' alto il contenuto
 * (scorrimento_misure su b->scorrimento), e le passa la rotella
 * (barra_scorrimento_rotella). Disegnando, sposta il contenuto di b->scorrimento.posizione. */

#define SCORRIMENTO_FOTOGRAMMA 20      /* ms tra un passo e l'altro dell'animazione */

struct barra_scorrimento {
    struct widget base;
    struct scorrimento scorrimento;
    int opacita;                       /* 0 nascosta, 255 del tutto visibile */
    int spessore;                      /* 0 sottile (6 pixel), 255 spessa (10) */
    unsigned int ultimo;               /* tempo() dell'ultimo scatto o trascinamento */
    unsigned int passo_prima;          /* tempo() del passo di animazione di prima */
    unsigned int attesa;               /* ogni quanti ms la si chiama ora (0: non la si chiama) */
    int trascinata;                    /* il cursore e' preso con il mouse */
    int presa;                         /* dove lo si e' preso, dal suo inizio */
};

/* Il cursore: dove comincia (y) e quanto e' lungo. */
static inline void barra_scorrimento_cursore(const struct barra_scorrimento *b, int *y, int *lungo) {
    const struct scorrimento *s = &b->scorrimento;
    int corsa = b->base.a - 2 * SCORRIMENTO_MARGINE;     /* dove il cursore si muove */
    int l = s->totale > 0 ? (int)((long long)corsa * s->vista / s->totale) : corsa;
    if (l < SCORRIMENTO_MINIMO) l = SCORRIMENTO_MINIMO;
    if (l > corsa) l = corsa;
    int massimo = scorrimento_massimo(s);
    *lungo = l;
    *y = b->base.y + SCORRIMENTO_MARGINE +
         (massimo ? (int)((long long)(corsa - l) * s->posizione / massimo) : 0);
}

static inline void barra_scorrimento_disegna(struct widget *w, struct pennello *p) {
    struct barra_scorrimento *b = (struct barra_scorrimento *)w;
    if (!b->opacita || !scorrimento_massimo(&b->scorrimento)) return;
    pennello_opacita(p, b->opacita);
    if (b->spessore) {                 /* la guida: tutta la striscia, compare allargandosi */
        pennello_salva(p);
        pennello_opacita(p, b->spessore);
        pennello_colore(p, VETRO_PREMUTO);
        riempi_tondo(p, w->x, w->y, w->l, w->a, w->l / 2);
        pennello_ripristina(p);
    }
    int spesso = SCORRIMENTO_SOTTILE + (SCORRIMENTO_SPESSO - SCORRIMENTO_SOTTILE) * b->spessore / 255;
    int y, lungo;
    barra_scorrimento_cursore(b, &y, &lungo);
    pennello_colore(p, b->trascinata ? SCORRIMENTO_CURSORE_TRASCINATO : CONTENUTO_SECONDARIO);
    riempi_tondo(p, w->x + (w->l - spesso) / 2, y, spesso, lungo, spesso / 2);
}

/* Porta 'valore' verso 'obiettivo' (0 o 255) di quanto basta perche' tutta la strada
 * duri 'durata' ms; 'passati' sono i ms dall'ultima volta. */
static inline int avvicina(int valore, int obiettivo, unsigned int passati, int durata) {
    int passo = (int)(255 * passati / (unsigned int)durata);
    if (passo < 1) passo = 1;
    if (valore < obiettivo) return valore + passo > obiettivo ? obiettivo : valore + passo;
    return valore - passo < obiettivo ? obiettivo : valore - passo;
}

static inline void barra_scorrimento_passo(struct finestra_tk *f, void *dati);

/* La prossima chiamata tra 'ms' millisecondi (0: nessuna). */
static inline void barra_scorrimento_richiama(struct barra_scorrimento *b, unsigned int ms) {
    if (b->attesa == ms && ms == SCORRIMENTO_FOTOGRAMMA) return;   /* gia' in moto */
    b->attesa = ms;
    finestra_tk_ogni(widget_finestra(&b->base), ms, barra_scorrimento_passo, b);
}

/* Qualcosa e' cambiato (uno scatto, il puntatore sopra): l'animazione riparte. */
static inline void barra_scorrimento_sveglia(struct barra_scorrimento *b) {
    if (b->attesa != SCORRIMENTO_FOTOGRAMMA) b->passo_prima = tempo();
    barra_scorrimento_richiama(b, SCORRIMENTO_FOTOGRAMMA);
}

/* Un passo dell'animazione: e' la chiamata "ogni tanto". */
static inline void barra_scorrimento_passo(struct finestra_tk *f, void *dati) {
    (void)f;
    struct barra_scorrimento *b = dati;
    unsigned int ora = tempo(), passati = ora - b->passo_prima;
    b->passo_prima = ora;
    int spessa = b->trascinata || b->base.sopra;
    int fresca = (int)(ora - b->ultimo) < SCORRIMENTO_RESTA;
    int vista = (spessa || fresca) && scorrimento_massimo(&b->scorrimento);
    int opacita = avvicina(b->opacita, vista ? 255 : 0, passati,
                           vista ? SCORRIMENTO_COMPARE : SCORRIMENTO_SVANISCE);
    int spessore = avvicina(b->spessore, spessa ? 255 : 0, passati, SCORRIMENTO_ALLARGA);
    if (opacita != b->opacita || spessore != b->spessore) {
        b->opacita = opacita;
        b->spessore = spessore;
        b->base.nascosto = !opacita;   /* nascosta non si disegna e non prende i clic */
        widget_cambiato(&b->base);
    }
    if (opacita != (vista ? 255 : 0) || spessore != (spessa ? 255 : 0))
        barra_scorrimento_richiama(b, SCORRIMENTO_FOTOGRAMMA);              /* ancora in moto */
    else if (vista && fresca && !spessa)
        barra_scorrimento_richiama(b, SCORRIMENTO_RESTA - (ora - b->ultimo)); /* poi svanisce */
    else
        barra_scorrimento_richiama(b, 0);   /* ferma: la risveglia uno scatto o il puntatore */
}

/* Il contenuto e' stato scorso (rotella o trascinamento): la barra si mostra. */
static inline void barra_scorrimento_usata(struct barra_scorrimento *b, int spostato) {
    b->ultimo = tempo();
    if (spostato) widget_cambiato(&b->base);
    barra_scorrimento_sveglia(b);
}

/* La rotella sopra il contenuto: scorre di 'scatti' (positivi: in basso). 0 se il
 * contenuto ci sta tutto e non c'e' niente da scorrere. */
static inline int barra_scorrimento_rotella(struct barra_scorrimento *b, int scatti) {
    struct scorrimento *s = &b->scorrimento;
    if (!scorrimento_massimo(s)) return 0;
    barra_scorrimento_usata(b, scorrimento_rotella(s, scatti));
    return 1;
}

/* Il cursore segue il puntatore, tenuto per il punto in cui lo si e' preso. */
static inline void barra_scorrimento_segui(struct barra_scorrimento *b, int y) {
    int inizio, lungo;
    barra_scorrimento_cursore(b, &inizio, &lungo);
    int corsa = b->base.a - 2 * SCORRIMENTO_MARGINE - lungo;
    if (corsa <= 0) return;
    int da_inizio = y - b->presa - (b->base.y + SCORRIMENTO_MARGINE);
    int posizione = (int)((long long)da_inizio * scorrimento_massimo(&b->scorrimento) / corsa);
    barra_scorrimento_usata(b, scorrimento_metti(&b->scorrimento, posizione));
}

/* Premuto sul cursore: lo si prende li'. Premuto sulla guida: il centro del cursore
 * salta li', e lo si tiene per il centro. Poi segue il puntatore finche' non si lascia. */
static inline void barra_scorrimento_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    struct barra_scorrimento *b = (struct barra_scorrimento *)w;
    (void)x;
    int giu = pulsanti & PULSANTE_SINISTRO;
    if (giu && !b->trascinata) {
        int inizio, lungo;
        barra_scorrimento_cursore(b, &inizio, &lungo);
        b->presa = y >= inizio && y < inizio + lungo ? y - inizio : lungo / 2;
        b->trascinata = 1;
        widget_cambiato(w);            /* il cursore cambia colore */
        barra_scorrimento_segui(b, y);
    } else if (giu) {
        barra_scorrimento_segui(b, y);
    } else if (b->trascinata) {
        b->trascinata = 0;
        widget_cambiato(w);
        barra_scorrimento_usata(b, 0);  /* resta ancora un secondo, poi svanisce */
    }
}

static inline void barra_scorrimento_su_rotella(struct widget *w, int sx, int sy) {
    (void)sx;
    barra_scorrimento_rotella((struct barra_scorrimento *)w, sy);
}

static inline void barra_scorrimento_sopra(struct widget *w) {
    barra_scorrimento_sveglia((struct barra_scorrimento *)w);
}

static const struct tipo_widget tipo_barra_scorrimento = {
    0, 0, barra_scorrimento_disegna, barra_scorrimento_puntatore, 0,
    barra_scorrimento_su_rotella, barra_scorrimento_sopra
};

static inline void barra_scorrimento_prepara(struct barra_scorrimento *b) {
    *b = (struct barra_scorrimento){ .base = { .tipo = &tipo_barra_scorrimento, .nascosto = 1 } };
}

/* La vista del contenuto e' il rettangolo (x, y, l, a): la barra va lungo il suo bordo
 * destro. Da chiamare quando il widget che scorre viene disposto. */
static inline void barra_scorrimento_posto(struct barra_scorrimento *b, int x, int y, int l, int a) {
    b->base.x = x + l - SCORRIMENTO_LARGHEZZA;
    b->base.y = y;
    b->base.l = SCORRIMENTO_LARGHEZZA;
    b->base.a = a;
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

/* Il puntatore sopra un widget nuovo (o su nessuno): si spegne il vecchio, si accende
 * il nuovo. */
static inline void toolkit_sotto(struct finestra_tk *f, struct widget *w) {
    if (w == f->sotto) return;
    struct widget *prima = f->sotto;
    if (prima) { prima->sopra = 0; widget_cambiato(prima); }
    f->sotto = w;
    if (w) { w->sopra = 1; widget_cambiato(w); }
    if (prima && prima->tipo->sopra_cambiato) prima->tipo->sopra_cambiato(prima);
    if (w && w->tipo->sopra_cambiato) w->tipo->sopra_cambiato(w);
}

/* La rotella: al widget sotto il puntatore, o al primo dei suoi genitori che la vuole. */
static inline void toolkit_rotella(struct finestra_tk *f, int x, int y, int sx, int sy) {
    if (y < BARRA_ALTEZZA) return;
    for (struct widget *w = widget_in(&f->base, x, y); w; w = w->genitore)
        if (w->tipo->rotella) { w->tipo->rotella(w, sx, sy); return; }
}

static inline void toolkit_fuoco(struct finestra_tk *f, struct widget *w) {
    if (w == f->con_fuoco) return;
    if (f->con_fuoco) { f->con_fuoco->fuoco = 0; widget_cambiato(f->con_fuoco); }
    f->con_fuoco = w;
    if (w) { w->fuoco = 1; widget_cambiato(w); }
}

/* Il puntatore: un clic su "chiudi" chiude; sulla barra e' un "trascinami"; nel
 * contenuto cattura il widget sotto, che sentira' il puntatore finche' il pulsante non
 * si lascia, e se prende i tasti gli da' il fuoco. */
static inline void toolkit_puntatore(struct finestra_tk *f, int x, int y, unsigned int pulsanti) {
    int giu = pulsanti & PULSANTE_SINISTRO;
    int appena_premuto = giu && !(f->pulsanti_prima & PULSANTE_SINISTRO);
    f->pulsanti_prima = pulsanti;

    if (appena_premuto) {
        if (su_comando(&f->base, 1, x, y)) { f->chiudi_premuto = 1; return; }
        if (su_comando(&f->base, 0, x, y)) { f->riduci_premuto = 1; return; }
        if (y < BARRA_ALTEZZA) { finestra_trascinami(&f->finestra); return; }
        f->catturato = widget_sotto(&f->base, x, y);
        toolkit_fuoco(f, f->catturato && f->catturato->tipo->tasto ? f->catturato : 0);
    }
    if (f->chiudi_premuto) {
        if (!giu) {
            f->chiudi_premuto = 0;
            if (su_comando(&f->base, 1, x, y)) f->da_chiudere = 1;
        }
        return;
    }
    if (f->riduci_premuto) {                /* scatta quando si lascia, se si e' ancora sopra */
        if (!giu) {
            f->riduci_premuto = 0;
            if (su_comando(&f->base, 0, x, y)) finestra_riducimi(&f->finestra);
        }
        return;
    }
    if (f->catturato) {
        f->catturato->tipo->puntatore(f->catturato, x, y, pulsanti);
        toolkit_sotto(f, widget_contiene(f->catturato, x, y) ? f->catturato : 0);
        if (giu) return;
        f->catturato = 0;
    }
    toolkit_sotto(f, widget_sotto(&f->base, x, y));
}

/* Apre la finestra e la fa vivere: torna quando si preme "chiudi" (0) o se il
 * compositore la chiude (1). */
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
    for (;;) {
        /* Con i "chiamami ogni tanto": se non c'e' niente da parte si aspetta il
         * compositore solo fino alla prima scadenza. Scaduta, si chiama chi l'ha chiesto.
         * Ma prima le notizie gia' arrivate: se mentre si ridisegnava e' arrivato un clic o
         * uno scatto della rotella, si serve prima lui e poi l'animazione (come in GTK, dove
         * l'input passa davanti ai ridisegni). Cosi' due scatti veloci restano una raffica
         * anche quando ridisegnare costa. */
        struct chiamata_tk *prima = toolkit_prima_chiamata(f);
        if (prima && !f->finestra.quante) {
            int resta = (int)(prima->scadenza - tempo());
            struct da_guardare g = { .descrittore = f->finestra.collegamento };
            if (aspetta(&g, 1, resta > 0 ? resta : 0) == 0) {
                toolkit_chiamate_scadute(f);
                if (f->da_chiudere) return 0;
                if (f->da_ridisegnare && !toolkit_ridisegna(f)) return 1;
                continue;
            }
            if (!finestra_ricevi(&f->finestra, &n)) return 1;
            if (finestra_di_servizio(&n)) continue;
        } else if (!finestra_notizia(&f->finestra, &n)) {
            return 1;
        }
        if (n.tipo == PUNTATORE) {
            toolkit_puntatore(f, n.x, n.y, n.pulsanti);
        } else if (n.tipo == USCITO) {
            /* il puntatore non e' piu' nostro (per esempio, il compositore ha cominciato
             * a spostare la finestra): una pressione in corso si annulla, senza scattare */
            if (f->catturato) f->catturato->tipo->puntatore(f->catturato, -1, -1, 0);
            f->catturato = 0;
            f->chiudi_premuto = 0;
            f->riduci_premuto = 0;
            f->pulsanti_prima = 0;
            toolkit_sotto(f, 0);
        } else if (n.tipo == ROTELLA) {
            toolkit_rotella(f, n.x, n.y, n.scatti_x, n.scatti_y);
        } else if (n.tipo == TASTO && f->con_fuoco) {
            f->con_fuoco->tipo->tasto(f->con_fuoco, n.tasto);
        } else if (n.tipo == FUOCO) {
            f->attiva = n.fuoco;
            f->da_ridisegnare = 1;
        }
        if (f->da_chiudere) return 0;
        if (f->da_ridisegnare && !toolkit_ridisegna(f)) return 1;
    }
}

#endif
