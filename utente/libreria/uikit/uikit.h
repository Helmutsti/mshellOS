/* UIKit, l'interfaccia (vedi LIBRERIE.md): come AppKit sul Mac o GTK su Linux, costruita
 * sopra GKit. Da' alle applicazioni dei pezzi pronti, i WIDGET, tutti con l'aspetto del
 * sistema, e fa da solo le cose noiose: la barra del titolo, il "trascinami", il fuoco, la
 * tastiera, il ciclo degli eventi, il ridisegno. Un'applicazione include solo questo file:
 * dentro c'e' tutto quello che le serve.
 *
 * I PEZZI DI UIKIT, tutti in questa cartella:
 *   - lo stile e i temi (stile.h, tema.h, dal file Figma "mshell - interfaccia"): colori
 *     semantici Chiaro/Scuro, misure, raggi, ombre;
 *   - i materiali del sistema (materiali.h; i dati in file su disco, /system/resources,
 *     caricati la prima volta che servono): il font, le icone Lucide, le icone di file e
 *     cartelle;
 *   - la finestra dell'applicazione come la decide il design, i componenti e il ciclo degli
 *     eventi: qui sotto (e l'elemento di un file in elemento_file.h; la userbar e il vassoio,
 *     della scrivania, in userbar.h);
 *   - quello che serve al compositore del design (uikit_compositore.h).
 * Sotto, GKit (utente/libreria/gkit/): la finestra (gkit.h) e gli strumenti di disegno
 * (gkit_disegno.h).
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
 *         return uikit_esegui(&finestra);
 *     }
 *
 * GLI OGGETTI, IN C. Ogni widget comincia con una 'struct widget' (la "base"): li' ci
 * sono il posto, i figli e il TIPO, cioe' le funzioni che sanno misurarlo, sistemarne i
 * figli, disegnarlo, fargli sentire il puntatore e la tastiera. UIKit chiama quelle,
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
 * GLI STATI. UIKit segna da solo quale widget ha il puntatore SOPRA (per
 * illuminarlo), quale e' PREMUTO, quale ha il FUOCO della tastiera (il campo di testo
 * su cui si e' cliccato). Un widget DISABILITATO si disegna al 40% e non sente niente.
 *
 * IL CICLO DEGLI EVENTI E' DI UIKIT. uikit_esegui non torna finche' la finestra
 * c'e': aspetta le notizie del compositore, le consegna ai widget, ridisegna quando
 * qualcosa e' cambiato. Torna quando si preme "chiudi". L'applicazione interviene solo
 * quando succede qualcosa che le interessa ("quando premono questo pulsante, chiama
 * questa funzione": in GTK i segnali, in AppKit target-action).
 *
 * SI RIDISEGNA SOLO QUELLO CHE E' CAMBIATO (le "zone rovinate" di GTK, AppKit e Wayland). Un
 * widget che cambia lo dice con widget_cambiato (tutto il suo spazio, con i 24 pixel che puo'
 * sbordare) o widget_cambiato_zona (solo un pezzo: la riga di una tabella sotto il
 * puntatore). La finestra raccoglie queste zone e al ridisegno rifa' SOLO LORO: per ognuna si
 * ritaglia il pennello li' e si disegna tutto l'albero, dal fondo della finestra in su, ma i
 * widget che non toccano la zona si saltano e le forme grandi (il vetro, i pannelli) costano
 * solo nel ritaglio. Il risultato e' lo stesso, pixel per pixel, di un ridisegno intero,
 * perche' ogni pixel dipende solo da quello che c'e' in lui. Poi la finestra dice al
 * compositore quali zone sono cambiate (finestra_mostra_zone), e lui ricompone solo li'.
 * Quando cambia l'impaginazione (uikit_ridisponi) si rifa' tutto.
 * Le notizie si consegnano TUTTE prima di ridisegnare: quelle arrivate mentre si disegnava si
 * servono insieme, e si ridisegna una volta sola (i movimenti del puntatore di mezzo non si
 * vedrebbero: conta l'ultimo).
 *
 * IL TEMPO. Un'applicazione che deve aggiornarsi da sola (un orologio, il task manager)
 * chiede a UIKit di chiamarla ogni tanto: finestra_tk_ogni(&finestra, 1000, f, dati).
 * Il ciclo allora non dorme senza limite: aspetta le notizie fino alla prima scadenza.
 * Le chiamate possono essere piu' d'una (fino a MAX_CHIAMATE): le usano anche i widget
 * che si animano, come la barra di scorrimento, solo finche' l'animazione dura.
 *
 * LO SCORRIMENTO. Di un contenuto piu' alto del suo spazio (una lista, una tabella) se ne
 * mostra un pezzo, la VISTA, e la rotella del mouse lo fa scorrere. UIKit da' due
 * pezzi: 'struct scorrimento', i tre numeri (dove si e', quanto e' alto tutto, quanto se
 * ne vede), e la BARRA DI SCORRIMENTO, un widget che sta sopra il contenuto, sul bordo
 * destro, e si vede solo mentre si scorre (come sul Mac). La rotella va al widget sotto
 * il puntatore, o al primo dei suoi genitori che la vuole (il campo 'rotella' del tipo).
 *
 * I WIDGET NASCOSTI non si misurano, non si disegnano e non sentono niente: e' cosi' che
 * si fanno le pagine (piu' contenuti nello stesso posto, se ne vede uno). Dopo averne
 * mostrato o nascosto uno, uikit_ridisponi rifa' l'impaginazione.
 *
 * I WIDGET LI DICHIARA L'APPLICAZIONE, come variabili sue. Una finestra per
 * applicazione, che non cambia misura. Il pulsante "riduci" la nasconde: torna con un
 * clic sulla sua icona nella userbar della scrivania. */
#ifndef UIKIT_H
#define UIKIT_H

#include "gkit/gkit.h"
#include "gkit/gkit_disegno.h"
#include "stile.h"           /* con i temi (tema.h) e i materiali (materiali.h) */
#include "elemento_file.h"   /* testo_a_capo, per il messaggio dell'avviso */
#include "tasti.h"           /* i codici dei tasti fisici e gli ascoltatori delle scorciatoie */

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
     * cambiato (0: basta il ridisegno che fa UIKit) */
    void (*sopra_cambiato)(struct widget *w);
    /* il puntatore si muove sopra di lui senza pulsanti premuti, in (x, y) (0: non gli
     * interessa). Serve a chi deve sapere SOPRA QUALE SUA PARTE e' il puntatore (i file
     * di una cartella, per illuminare quello li' sotto), non solo se e' sopra di lui */
    void (*movimento)(struct widget *w, int x, int y);
    /* 1: il widget sa disegnarsi disabilitato (piatto, con i colori _DISABILITATO del
     * tema) e UIKit NON lo sbiadisce al 40%. 0 (gli altri): il vecchio modo, 40%. */
    int disegna_disabilitato;
};

#define MAX_FIGLI 16

struct widget {
    const struct tipo_widget *tipo;
    struct widget *genitore;           /* 0 per la finestra, la radice dell'albero */
    int x, y, l, a;                    /* il suo spazio, in coordinate della finestra */
    struct widget *figli[MAX_FIGLI];
    int quanti_figli;
    int sopra;                         /* il puntatore e' sopra di lui (lo segna UIKit) */
    int fuoco;                         /* ha il fuoco della tastiera (lo segna UIKit) */
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
    /* tutto quello che il widget (e i suoi figli) disegna sta nel suo spazio piu' lo sbordo:
     * se quello non tocca il ritaglio (la zona che si sta ridisegnando), non c'e' niente da fare */
    if (w->x + w->l + WIDGET_SBORDO <= p->s.rx || w->x - WIDGET_SBORDO >= p->s.rx2 ||
        w->y + w->a + WIDGET_SBORDO <= p->s.ry || w->y - WIDGET_SBORDO >= p->s.ry2) return;
    pennello_salva(p);
    pennello_ritaglia(p, w->x - WIDGET_SBORDO, w->y - WIDGET_SBORDO,
                     w->l + 2 * WIDGET_SBORDO, w->a + 2 * WIDGET_SBORDO);   /* anelli e ombre sbordano */
    if (w->disabilitato && !w->tipo->disegna_disabilitato) pennello_opacita(p, OPACITA_DISABILITATO);
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

/* Un widget e' cambiato (un testo nuovo, un pulsante premuto): va ridisegnato il suo spazio
 * (con lo sbordo). Si risale l'albero fino alla finestra, che si segna la zona. Un widget
 * nascosto (o dentro uno nascosto) non si vede: niente da ridisegnare. (Piu' sotto, con la
 * finestra.) */
static inline void widget_cambiato(struct widget *w);

/* Solo un pezzo del widget e' cambiato: il rettangolo (x, y, l, a), in coordinate della
 * finestra (la parte fuori dal widget e dal suo sbordo non conta). */
static inline void widget_cambiato_zona(struct widget *w, int x, int y, int l, int a);

/* Il widget si vede: ne' lui ne' chi lo contiene e' nascosto. */
static inline int widget_visibile(const struct widget *w) {
    for (; w; w = w->genitore)
        if (w->nascosto) return 0;
    return 1;
}

static inline void widget_disabilita(struct widget *w, int disabilitato) {
    w->disabilitato = disabilitato;
    widget_cambiato(w);
}

static inline void widget_segna(struct widget *w);   /* piu' sotto: il suo spazio, sempre */

/* Mostra o nasconde un widget. Lo spazio si ridistribuisce con uikit_ridisponi. Il posto
 * dove stava (o dove sta) si ridisegna anche se adesso e' nascosto. */
static inline void widget_nascondi(struct widget *w, int nascosto) {
    w->nascosto = nascosto;
    if (!w->genitore || widget_visibile(w->genitore)) widget_segna(w);
}

/* Di quanto un'ombra a strati esce dalla sua forma, al massimo (per sapere cosa ridisegnare
 * quando la forma compare o sparisce). */
static inline int ombra_sporgenza(const struct strato_ombra *strati, int n) {
    int massimo = 0;
    for (int i = 0; i < n; i++) {
        if (strati[i].tipo != OMBRA_ESTERNA) continue;
        int dx = strati[i].dx < 0 ? -strati[i].dx : strati[i].dx, dy = strati[i].dy < 0 ? -strati[i].dy : strati[i].dy;
        int s = (dx > dy ? dx : dy) + strati[i].sfocatura + (strati[i].diffusione > 0 ? strati[i].diffusione : 0) + 2;
        if (s > massimo) massimo = s;
    }
    return massimo;
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
    contenitore_misura, contenitore_disponi, 0, 0, 0, 0, 0, 0, 0
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

static const struct tipo_widget tipo_spazio = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static inline void spazio_prepara(struct spazio *s) {
    *s = (struct spazio){ .base = { .tipo = &tipo_spazio, .elastico = 1 } };
}

/* Lo SPAZIO FISSO: vuoto, largo 'larghezza' (in una riga) o alto (in una colonna). Serve ad
 * allineare: per esempio i controlli della barra degli strumenti di Files cominciano dove
 * finisce la barra laterale. */
struct spazio_fisso { struct widget base; int misura; };

static inline void spazio_fisso_misura(struct widget *w, int *l, int *a) {
    *l = *a = ((struct spazio_fisso *)w)->misura;
}

static const struct tipo_widget tipo_spazio_fisso = { spazio_fisso_misura, 0, 0, 0, 0, 0, 0, 0, 0 };

static inline void spazio_fisso_prepara(struct spazio_fisso *s, int misura) {
    *s = (struct spazio_fisso){ .base = { .tipo = &tipo_spazio_fisso }, .misura = misura };
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

static const struct tipo_widget tipo_etichetta = { etichetta_misura, 0, etichetta_disegna, 0, 0, 0, 0, 0, 0 };

static inline void etichetta_prepara(struct etichetta *e, const char *testo, const struct font *font) {
    *e = (struct etichetta){ .base = { .tipo = &tipo_etichetta }, .font = font,
                             .colore = TESTO_PRIMARIO };
    copia_testo(e->testo, testo, MAX_TESTO_WIDGET);
}

/* Cambia il testo. (Lo spazio dell'etichetta resta quello deciso all'apertura della
 * finestra: un testo piu' lungo si taglia.) */
static inline void etichetta_testo(struct etichetta *e, const char *testo) {
    copia_testo(e->testo, testo, MAX_TESTO_WIDGET);
    widget_cambiato(&e->base);
}

/* --- il pulsante: 3 tipi, 3 misure, con o senza icona, anche solo icona --- */

/* I TIPI. Primario (l'azione principale: fondo scuro nel chiaro, chiaro nello scuro, ombra
 * "sollevato-pieno"), Secondario (fondo di controllo, ombra "sollevato"), Ghost (senza
 * fondo: compare solo sotto il puntatore, premuto e disabilitato sono piatti come gli
 * altri). Il colore NON dice piu' il significato (blu/verde/rosso/arancio non esistono
 * nella scala di grigi): chi vuole un'azione di rilievo usa il Primario.
 * I tipi COLORATI dicono il significato dell'azione, con i colori di ruolo (tema.h):
 * Pericolo (rosso: terminare, cancellare, cose che non si annullano), Successo (verde),
 * Avviso (giallo, testo scuro), Informazione (blu). Si comportano come il Primario
 * (ombra "sollevato-pieno", testo e icona nel colore *_TESTO del ruolo). */
enum { PRIMARIO, SECONDARIO, GHOST, PERICOLO, SUCCESSO, AVVISO, INFORMAZIONE };
/* Le MISURE: S 32, M 40, L 48 (ALTEZZA_S/M/L in stile.h); XS 24 solo per i pulsanti icona
 * (la chiusura della mini finestra). MISURA_XS e' in fondo per non cambiare gli indici. */
enum { MISURA_S, MISURA_M, MISURA_L, MISURA_XS };
/* VECCHIO: da eliminare quando le app saranno migrate. I nomi di prima restano come
 * sinonimi, cosi' Files, Impostazioni, Attivita e Contatore compilano senza toccarli:
 * NEUTRO e' il Secondario, tutti i colori (blu, verde, rosso, arancio) il Primario;
 * PICCOLO, MEDIO e GRANDE sono S, M e L. */
enum { NEUTRO = SECONDARIO, BLU = PRIMARIO, VERDE = PRIMARIO, ROSSO = PRIMARIO, ARANCIO = PRIMARIO };
enum { PICCOLO = MISURA_S, MEDIO = MISURA_M, GRANDE = MISURA_L };

/* Le misure del disegno: altezza, margine ai lati, spazio icona-testo, lato dell'icona,
 * font (il suo numero: font_del_sistema, materiali.h). Il raggio non c'e': i pulsanti sono
 * pillole (raggio = meta' altezza). */
struct misure_pulsante { int altezza, margine, spazio, icona, font; };
static const struct misure_pulsante misure_pulsante[] = {
    { ALTEZZA_S, PADDING_S, GAP_S, ICONA_S, FONT_PULSANTE_S_NUMERO },
    { ALTEZZA_M, PADDING_M, GAP_M, ICONA_M, FONT_PULSANTE_M_NUMERO },
    { ALTEZZA_L, PADDING_L, GAP_L, ICONA_L, FONT_PULSANTE_L_NUMERO },
    { ALTEZZA_XS, PADDING_S, GAP_S, ICONA_XS, FONT_PULSANTE_S_NUMERO },   /* XS: pensato solo icona */
};

struct pulsante;
typedef void (*al_clic_pulsante)(struct pulsante *p, void *dati);

struct pulsante {
    struct widget base;
    char testo[MAX_TESTO_WIDGET];      /* vuoto: pulsante solo icona (tondo) */
    int icona;                         /* ICONA_..., o NESSUNA_ICONA */
    int colore, dimensione;            /* il tipo (PRIMARIO...) e la misura (MISURA_S...); 'colore' per compatibilita' */
    al_clic_pulsante al_clic;          /* chi chiamare quando viene premuto */
    void *dati;                        /* passato cosi' com'e' ad al_clic */
    int premuto;                       /* il pulsante del mouse e' giu', premuto qui sopra */
    int dentro;                        /* ... e il puntatore e' ancora sopra di lui */
};

static inline void pulsante_misura(struct widget *w, int *l, int *a) {
    struct pulsante *b = (struct pulsante *)w;
    const struct misure_pulsante *m = &misure_pulsante[b->dimensione];
    *a = m->altezza;
    if (!b->testo[0]) { *l = m->altezza; return; }             /* solo icona: tondo, lato = altezza */
    *l = 2 * m->margine + larghezza_testo(font_del_sistema(m->font), b->testo);
    if (b->icona != NESSUNA_ICONA) *l += m->icona + m->spazio;
}

/* LA PROFONDITA' del pulsante. Si preme, quindi e' SOLLEVATO: il Primario ha l'ombra
 * "sollevato-pieno", il Secondario "sollevato", e le ombre ci sono sia a riposo sia sotto
 * il puntatore. Premuto "scende" e disabilitato "si spegne": tutti e due PIATTI, senza
 * ombra. Ghost e' sempre piatto. L'ordine e' quello di gkit_disegno.h: ombre esterne, fondo,
 * ombre interne (nel tema scuro c'e' un filo di luce sul bordo alto), poi icona e testo. */
static inline void pulsante_disegna(struct widget *w, struct pennello *p) {
    struct pulsante *b = (struct pulsante *)w;
    const struct misure_pulsante *m = &misure_pulsante[b->dimensione];
    int premuto = b->premuto && b->dentro;
    int sopra = w->sopra, spento = w->disabilitato;
    if (spento) premuto = sopra = 0;
    struct tondo t = tondo(w->x, w->y, w->l, w->a, w->a / 2);
    const struct strato_ombra *ombre = 0;
    int quante = 0;
    int pieno = b->colore == PRIMARIO || b->colore >= PERICOLO;   /* Primario e i colorati */
    if (!premuto && !spento && pieno) {
        ombre = OMBRA_TAB(sollevato_pieno); quante = OMBRA_N(sollevato_pieno);
    } else if (!premuto && !spento && b->colore == SECONDARIO) {
        ombre = OMBRA_TAB(sollevato); quante = OMBRA_N(sollevato);
    }
    if (ombre) ombre_strati_fuori(p, &t, ombre, quante);

    /* il testo e l'icona: l'icona prende il colore dalla raccolta "Colore icona" (tema.h),
     * con il modo che il disegno da' a ogni tipo di pulsante */
    unsigned int fondo = 0, testo = TESTO_PRIMARIO;
    int modo_icona = COLORE_ICONA_PRIMARIO;
    if (spento) {
        if (b->colore != GHOST) fondo = CONTROLLO_SFONDO_DISABILITATO;
        testo = TESTO_DISABILITATO;
        modo_icona = COLORE_ICONA_ATTENUATO;
    } else if (b->colore == PRIMARIO) {
        fondo = premuto ? AZIONE_SFONDO_PREMUTO : sopra ? AZIONE_SFONDO_SOPRA : AZIONE_SFONDO;
        testo = AZIONE_TESTO;
        modo_icona = COLORE_ICONA_INVERSO;
    } else if (b->colore == PERICOLO) {
        fondo = premuto ? PERICOLO_SFONDO_PREMUTO : sopra ? PERICOLO_SFONDO_SOPRA : PERICOLO_SFONDO;
        testo = PERICOLO_TESTO;
        modo_icona = COLORE_ICONA_PERICOLO;
    } else if (b->colore == SUCCESSO) {
        fondo = premuto ? SUCCESSO_SFONDO_PREMUTO : sopra ? SUCCESSO_SFONDO_SOPRA : SUCCESSO_SFONDO;
        testo = SUCCESSO_TESTO;
        modo_icona = COLORE_ICONA_SUCCESSO;
    } else if (b->colore == AVVISO) {
        fondo = premuto ? AVVISO_SFONDO_PREMUTO : sopra ? AVVISO_SFONDO_SOPRA : AVVISO_SFONDO;
        testo = AVVISO_TESTO;
        modo_icona = COLORE_ICONA_AVVISO;
    } else if (b->colore == INFORMAZIONE) {
        fondo = premuto ? INFORMAZIONE_SFONDO_PREMUTO : sopra ? INFORMAZIONE_SFONDO_SOPRA : INFORMAZIONE_SFONDO;
        testo = INFORMAZIONE_TESTO;
        modo_icona = COLORE_ICONA_INFORMAZIONE;
    } else if (b->colore == SECONDARIO) {
        fondo = premuto ? CONTROLLO_SFONDO_PREMUTO : sopra ? CONTROLLO_SFONDO_SOPRA : CONTROLLO_SFONDO_NUOVO;
    } else {                           /* Ghost: nessun fondo a riposo */
        fondo = premuto ? CONTROLLO_SFONDO_SOPRA : sopra ? CONTROLLO_SFONDO_NUOVO : 0;
    }
    if (fondo) {
        pennello_colore(p, fondo);
        riempi_forma(p, &t);
    }
    if (ombre) ombre_strati_dentro(p, &t, ombre, quante);

    /* icona e testo, centrati insieme */
    const struct font *font = font_del_sistema(m->font);
    int largo = b->testo[0] ? larghezza_testo(font, b->testo) : 0;
    if (b->icona != NESSUNA_ICONA) largo += m->icona + (b->testo[0] ? m->spazio : 0);
    int x = w->x + (w->l - largo) / 2;
    if (b->icona != NESSUNA_ICONA) {
        pennello_colore(p, COLORE_ICONA(modo_icona));
        icona(p, b->icona, m->icona, x, w->y + (w->a - m->icona) / 2);
        x += m->icona + m->spazio;
    }
    pennello_colore(p, testo);
    if (b->testo[0]) testo_centrato(p, font, x, w->y, w->a, b->testo);
}

/* Come i pulsanti veri: scatta quando si LASCIA, e solo se il puntatore e' ancora
 * sopra. Chi preme e poi cambia idea esce dal pulsante prima di lasciare. Tiene il conto
 * in 'premuto' e 'dentro' (del widget che lo usa) e dice 1 quando il clic e' fatto. Lo
 * usano tutti i pulsanti: questo e quelli della barra degli strumenti. */
static inline int pressione(struct widget *w, int *premuto, int *dentro, int x, int y,
                            unsigned int pulsanti) {
    int giu = pulsanti & PULSANTE_SINISTRO, sopra = widget_contiene(w, x, y);
    if (giu && !*premuto) {
        *premuto = 1;
        *dentro = sopra;
        widget_cambiato(w);
    } else if (giu && sopra != *dentro) {
        *dentro = sopra;
        widget_cambiato(w);
    } else if (!giu && *premuto) {
        *premuto = 0;
        widget_cambiato(w);
        return sopra;
    }
    return 0;
}

static inline void pulsante_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    struct pulsante *b = (struct pulsante *)w;
    if (pressione(w, &b->premuto, &b->dentro, x, y, pulsanti) && b->al_clic) b->al_clic(b, b->dati);
}

static const struct tipo_widget tipo_pulsante = {
    pulsante_misura, 0, pulsante_disegna, pulsante_puntatore, 0, 0, 0, 0, 1
};

/* Un pulsante secondario, di misura media. Per un pulsante solo icona, testo "". */
static inline void pulsante_prepara(struct pulsante *b, const char *testo, int icona,
                                    al_clic_pulsante al_clic, void *dati) {
    *b = (struct pulsante){ .base = { .tipo = &tipo_pulsante }, .icona = icona,
                            .colore = SECONDARIO, .dimensione = MISURA_M, .al_clic = al_clic, .dati = dati };
    copia_testo(b->testo, testo, MAX_TESTO_WIDGET);
}

/* Il tipo (PRIMARIO, SECONDARIO, GHOST, PERICOLO, SUCCESSO, AVVISO, INFORMAZIONE) e la
 * misura (MISURA_S, _M, _L, e _XS per i pulsanti solo icona). Accetta anche i
 * vecchi nomi (NEUTRO, BLU..., PICCOLO...): sono sinonimi. */
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
    *l = CASELLA_LATO + CASELLA_GAP + larghezza_testo(FONT_CORPO, c->testo);
    *a = CASELLA_LATO > FONT_CORPO->altezza ? CASELLA_LATO : FONT_CORPO->altezza;
}

/* La casella e la scelta CONTENGONO una risposta, quindi sono INCASSATE: fondo
 * SUPERFICIE_INCASSATA, ombra interna (e una sottile esterna), mai bordo. Accese cambiano
 * solo il colore del fondo (AZIONE_SFONDO, come un'azione) e mettono il segno sopra; il
 * livello non cambia. Disabilitate sono piatte: fondo CONTROLLO_SFONDO_DISABILITATO, niente
 * ombra, segno e testo TESTO_DISABILITATO. Il tondo della scelta accesa e' un punto di 8 px
 * disegnato con riempi_cerchio (non piu' un'immagine precalcolata). */
static inline void casella_disegna(struct widget *w, struct pennello *p) {
    struct casella *c = (struct casella *)w;
    int spento = w->disabilitato, acceso = c->valore != SPENTO;
    int x = w->x, y = w->y + (w->a - CASELLA_LATO) / 2;
    struct tondo t = tondo(x, y, CASELLA_LATO, CASELLA_LATO, c->tonda ? CASELLA_LATO / 2 : RAGGIO_CASELLA);
    const struct strato_ombra *ombre = OMBRA_TAB(incassato);
    int quante = OMBRA_N(incassato);

    if (!spento) ombre_strati_fuori(p, &t, ombre, quante);
    unsigned int fondo = SUPERFICIE_INCASSATA;
    if (spento) fondo = CONTROLLO_SFONDO_DISABILITATO;
    else if (acceso && !c->tonda) fondo = w->sopra ? AZIONE_SFONDO_SOPRA : AZIONE_SFONDO;
    pennello_colore(p, fondo);
    riempi_forma(p, &t);
    if (!spento) ombre_strati_dentro(p, &t, ombre, quante);

    unsigned int segno = spento ? TESTO_DISABILITATO : AZIONE_SFONDO;
    if (acceso && c->tonda) {
        pennello_colore(p, segno);
        riempi_cerchio(p, x + CASELLA_LATO / 2, y + CASELLA_LATO / 2, SCELTA_PUNTO / 2);
    } else if (acceso) {
        pennello_colore(p, COLORE_ICONA(spento ? COLORE_ICONA_ATTENUATO : COLORE_ICONA_INVERSO));
        int lato = CASELLA_LATO - 4;                     /* il segno, 2 px di margine */
        icona(p, c->valore == ACCESO ? ICONA_CHECK : ICONA_MINUS, lato, x + 2, y + 2);
    }
    pennello_colore(p, spento ? TESTO_DISABILITATO : TESTO_PRIMARIO);
    testo_centrato(p, FONT_CORPO, x + CASELLA_LATO + CASELLA_GAP, w->y, w->a, c->testo);
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
    casella_misura, 0, casella_disegna, casella_puntatore, 0, 0, 0, 0, 1
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
    int errore;                            /* bordo del fuoco + triangolo di avviso (stato di errore) */
    int piccolo;                           /* 1: la RICERCA (pillola, 180 di larghezza, altezza S) */
    int misura;                            /* MISURA_S, _M (default) o _L: altezza, margini, font */
    int larghezza;                         /* 0: quella del disegno (280, la ricerca 180); se no questa */
    al_cambio_campo al_cambio;
    void *dati;
};

/* Le misure del disegno, una riga per misura: altezza, margine ai lati (dentro), lato
 * dell'icona, spazio dopo di lei, raggio degli angoli (RAGGIO_CAMPO_S/M/L: il campo e' un
 * rettangolo arrotondato, NON una pillola; la ricerca ha il raggio = meta' altezza) e font.
 * Il font e' Plex Regular (non Medium come i pulsanti: si scrive, non si preme) della stessa
 * dimensione del testo dei pulsanti: 13, 14, 16. Larghezza: 280 il campo (di solito si
 * allarga), 180 la ricerca. Il font per numero, come in misure_pulsante. */
struct misure_campo { int altezza, margine, icona, spazio, raggio, font; };
static const struct misure_campo misure_campo[] = {
    { ALTEZZA_S, PADDING_S, ICONA_S, GAP_S, RAGGIO_CAMPO_S, MATERIALE_plex_13 },
    { ALTEZZA_M, PADDING_M, ICONA_M, GAP_M, RAGGIO_CAMPO_M, MATERIALE_plex_14 },
    { ALTEZZA_L, PADDING_L, ICONA_L, GAP_L, RAGGIO_CAMPO_L, MATERIALE_plex_16 },
};
#define CAMPO_LARGHEZZA_NORMALE 280

static inline void campo_misura(struct widget *w, int *l, int *a) {
    struct campo *c = (struct campo *)w;
    *l = c->larghezza ? c->larghezza : c->piccolo ? RICERCA_LARGHEZZA_NUOVA : CAMPO_LARGHEZZA_NORMALE;   /* il campo normale di solito si allarga */
    *a = misure_campo[c->misura].altezza;
}

/* Un campo CONTIENE quello che si scrive, quindi e' INCASSATO: fondo SUPERFICIE_INCASSATA,
 * ombra interna e una sottile esterna, mai bordo a riposo. Il fuoco NON cambia livello: il
 * campo resta incassato e aggiunge il bordo BORDO_FOCUS di 1,5 px (dentro) e l'anello
 * "focus" (la tabella ombra_focus_*: stessa ombra interna, ma la esterna e' un anello di
 * 3 px). L'errore non ha rosso (scala di grigi): ha lo stesso bordo del fuoco, piu' il
 * triangolo di avviso a destra e il testo secondario. Disabilitato e' piatto. */
static inline void campo_disegna(struct widget *w, struct pennello *p) {
    struct campo *c = (struct campo *)w;
    const struct misure_campo *m = &misure_campo[c->misura];
    const struct font *font = font_del_sistema(m->font);
    int spento = w->disabilitato;
    struct tondo t = tondo(w->x, w->y, w->l, w->a, c->piccolo ? w->a / 2 : m->raggio);
    if (!spento) {
        const struct strato_ombra *ombre = w->fuoco ? OMBRA_TAB(focus) : OMBRA_TAB(incassato);
        int quante = w->fuoco ? OMBRA_N(focus) : OMBRA_N(incassato);
        ombre_strati_fuori(p, &t, ombre, quante);
        pennello_colore(p, SUPERFICIE_INCASSATA);
        riempi_forma(p, &t);
        ombre_strati_dentro(p, &t, ombre, quante);
        if (c->errore || w->fuoco) {       /* errore o fuoco: bordo di un pixel e mezzo */
            pennello_colore(p, BORDO_FOCUS);
            contorna_forma(p, &t, BORDO_FOCUS_SPESSORE);
        }
    } else {
        pennello_colore(p, CONTROLLO_SFONDO_DISABILITATO);
        riempi_forma(p, &t);
    }

    pennello_salva(p);
    pennello_ritaglia(p, w->x + 1, w->y + 1, w->l - 2, w->a - 2);
    unsigned int colore_testo = spento ? TESTO_DISABILITATO : c->errore ? TESTO_SECONDARIO : TESTO_PRIMARIO;
    int x = w->x + m->margine;
    int destra = w->x + w->l - m->margine;
    if (c->errore && !spento) {            /* il triangolo di avviso, a destra */
        pennello_colore(p, COLORE_ICONA(COLORE_ICONA_SECONDARIO));
        icona(p, ICONA_TRIANGLE_ALERT, m->icona, destra - m->icona, w->y + (w->a - m->icona) / 2);
        destra -= m->icona + m->spazio;
    }
    if (c->icona != NESSUNA_ICONA) {   /* come nel disegno: secondaria, primaria con il fuoco */
        pennello_colore(p, COLORE_ICONA(spento ? COLORE_ICONA_ATTENUATO
                                        : w->fuoco ? COLORE_ICONA_PRIMARIO : COLORE_ICONA_SECONDARIO));
        icona(p, c->icona, m->icona, x, w->y + (w->a - m->icona) / 2);
        x += m->icona + m->spazio;
    }
    pennello_colore(p, colore_testo);
    if (c->testo[0] || w->fuoco) {
        /* LO SCORRIMENTO. Il testo si vede in una striscia, dal suo inizio fino al margine
         * destro (lo stesso di sinistra). Se e' piu' lungo, si fa scorrere
         * a sinistra quanto basta perche' il cursore resti dentro, come in ogni campo di
         * testo. Il cursore e' sempre in fondo (la tastiera non manda ancora le frecce),
         * quindi basta guardare la fine del testo: scrivendo il testo va a sinistra,
         * cancellando torna a destra. Senza fuoco si vede l'inizio, come sul Mac. */
        int fine = larghezza_testo(font, c->testo) + CAMPO_CURSORE_SPAZIO + 2;   /* il cursore compreso */
        int scorrimento = w->fuoco && x + fine > destra ? x + fine - destra : 0;
        pennello_ritaglia(p, x - 2, w->y, destra - (x - 2), w->a);  /* 2: le lettere che sbordano */
        x = testo_centrato(p, font, x - scorrimento, w->y, w->a, c->testo);
        if (w->fuoco) {                    /* il cursore: una barretta alta quanto la riga */
            int alto = font->altezza;
            struct tondo cursore = tondo(x + CAMPO_CURSORE_SPAZIO, w->y + (w->a - alto) / 2, 2, alto, 0);
            riempi_forma(p, &cursore);
        }
    } else {
        pennello_colore(p, spento ? TESTO_DISABILITATO : TESTO_SECONDARIO);
        testo_centrato(p, font, x, w->y, w->a, c->suggerimento);
    }
    pennello_ripristina(p);
}

/* Il clic gli da' il fuoco (lo fa UIKit): qui non serve altro. */
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
    campo_misura, 0, campo_disegna, campo_puntatore, campo_tasto, 0, 0, 0, 1
};

static inline void campo_prepara(struct campo *c, const char *testo, const char *suggerimento,
                                 int icona, al_cambio_campo al_cambio, void *dati) {
    *c = (struct campo){ .base = { .tipo = &tipo_campo, .largo = 1 }, .icona = icona, .misura = MISURA_M,
                         .al_cambio = al_cambio, .dati = dati };
    copia_testo(c->testo, testo, MAX_TESTO_WIDGET);
    copia_testo(c->suggerimento, suggerimento, MAX_TESTO_WIDGET);
}

/* La misura del campo o della ricerca: MISURA_S (32), MISURA_M (40, il default del campo), MISURA_L (48). */
static inline void campo_stile(struct campo *c, int misura) {
    c->misura = misura;
    widget_cambiato(&c->base);
}

static inline void campo_errore(struct campo *c, int errore) {
    c->errore = errore;
    widget_cambiato(&c->base);
}

/* La ricerca piccola, per la barra degli strumenti: un campo di testo come gli altri
 * (si scrive, si cancella, al_cambio a ogni tasto), ma a PILLOLA, alta 32, larga 180, con la
 * lente. Come il campo e' incassata, e col fuoco ha bordo e anello. Non si allarga da solo (se deve, 'elastico'). */
static inline void ricerca_prepara(struct campo *c, const char *suggerimento,
                                   al_cambio_campo al_cambio, void *dati) {
    campo_prepara(c, "", suggerimento, ICONA_SEARCH, al_cambio, dati);
    c->piccolo = 1;
    c->misura = MISURA_S;
    c->base.largo = 0;
}

/* --- lo stato vuoto: un'area senza contenuto, o in errore --- */

/* LO STATO VUOTO, dal file Figma ("Stato vuoto"): al centro dello spazio che riceve, in
 * colonna, un'icona (facoltativa) di ICONA_OGGETTO (48) nel colore d'icona Secondario (le
 * schermate d'errore del disegno la vogliono Primario: stato_vuoto_colore_icona), il titolo
 * (FONT_SEZIONE_NUOVO, TESTO_PRIMARIO) e la spiegazione (FONT_CORPO, TESTO_SECONDARIO, a
 * capo da sola, al massimo STATO_VUOTO_RIGHE righe), tutto centrato; SPAZIO_8 tra un pezzo
 * e l'altro, FINESTRA_PADDING (24) di margine. Per una cartella vuota (senza icona), una
 * cartella che non si legge (ICONA_TRIANGLE_ALERT), un permesso negato (ICONA_LOCK).
 * Prende tutto lo spazio che avanza (elastico e largo). */
#define STATO_VUOTO_RIGHE 3

struct stato_vuoto {
    struct widget base;
    int icona;                          /* ICONA_..., o NESSUNA_ICONA */
    int colore_icona;                   /* COLORE_ICONA_... (tema.h): Secondario se non si dice */
    char titolo[MAX_TESTO_WIDGET];
    char spiegazione[STATO_VUOTO_RIGHE * MAX_TESTO_WIDGET];
};

static inline int stato_vuoto_righe(struct stato_vuoto *s, int largo, char righe[][MAX_RIGA_NOME]) {
    if (!s->spiegazione[0]) return 0;
    return testo_a_capo(FONT_CORPO, s->spiegazione, largo, righe, STATO_VUOTO_RIGHE);
}

static inline void stato_vuoto_misura(struct widget *w, int *l, int *a) {
    struct stato_vuoto *s = (struct stato_vuoto *)w;
    char righe[STATO_VUOTO_RIGHE][MAX_RIGA_NOME];
    int n = stato_vuoto_righe(s, AVVISO_LARGHEZZA, righe);
    int t = larghezza_testo(FONT_SEZIONE_NUOVO, s->titolo);
    *l = 2 * FINESTRA_PADDING + (t > AVVISO_LARGHEZZA ? t : AVVISO_LARGHEZZA);
    *a = 2 * FINESTRA_PADDING + INTERLINEA_SEZIONE +
         (s->icona != NESSUNA_ICONA ? ICONA_OGGETTO + SPAZIO_8 : 0) + (n ? SPAZIO_8 + n * INTERLINEA_CORPO : 0);
}

static inline void stato_vuoto_disegna(struct widget *w, struct pennello *p) {
    struct stato_vuoto *s = (struct stato_vuoto *)w;
    int largo = w->l - 2 * FINESTRA_PADDING;
    char righe[STATO_VUOTO_RIGHE][MAX_RIGA_NOME];
    int n = stato_vuoto_righe(s, largo, righe);
    int alto = INTERLINEA_SEZIONE + (s->icona != NESSUNA_ICONA ? ICONA_OGGETTO + SPAZIO_8 : 0) +
               (n ? SPAZIO_8 + n * INTERLINEA_CORPO : 0);
    int y = w->y + (w->a - alto) / 2, cx = w->x + w->l / 2;
    if (s->icona != NESSUNA_ICONA) {
        pennello_colore(p, COLORE_ICONA(s->colore_icona));
        icona(p, s->icona, ICONA_OGGETTO, cx - ICONA_OGGETTO / 2, y);
        y += ICONA_OGGETTO + SPAZIO_8;
    }
    pennello_colore(p, TESTO_PRIMARIO);
    testo_centrato(p, FONT_SEZIONE_NUOVO, cx - larghezza_testo(FONT_SEZIONE_NUOVO, s->titolo) / 2, y,
                   INTERLINEA_SEZIONE, s->titolo);
    y += INTERLINEA_SEZIONE + SPAZIO_8;
    pennello_colore(p, TESTO_SECONDARIO);
    for (int i = 0; i < n; i++, y += INTERLINEA_CORPO)
        testo_centrato(p, FONT_CORPO, cx - larghezza_testo(FONT_CORPO, righe[i]) / 2, y, INTERLINEA_CORPO, righe[i]);
}

static const struct tipo_widget tipo_stato_vuoto = { stato_vuoto_misura, 0, stato_vuoto_disegna, 0, 0, 0, 0, 0, 0 };

static inline void stato_vuoto_prepara(struct stato_vuoto *s, int icona_, const char *titolo,
                                       const char *spiegazione) {
    *s = (struct stato_vuoto){ .base = { .tipo = &tipo_stato_vuoto, .elastico = 1, .largo = 1 },
                               .icona = icona_, .colore_icona = COLORE_ICONA_SECONDARIO };
    copia_testo(s->titolo, titolo, MAX_TESTO_WIDGET);
    copia_testo(s->spiegazione, spiegazione ? spiegazione : "", sizeof s->spiegazione);
}

/* Il colore dell'icona (COLORE_ICONA_..., tema.h). Nel disegno lo stato vuoto ha l'icona
 * Secondario; le schermate d'errore (cartella che non si legge, permesso negato, pacchetto
 * illeggibile) la mettono Primario. */
static inline void stato_vuoto_colore_icona(struct stato_vuoto *s, int modo) { s->colore_icona = modo; }

/* Cambia icona e testi (per esempio, da "cartella vuota" a "impossibile leggere"). */
static inline void stato_vuoto_cambia(struct stato_vuoto *s, int icona_, const char *titolo,
                                      const char *spiegazione) {
    s->icona = icona_;
    copia_testo(s->titolo, titolo, MAX_TESTO_WIDGET);
    copia_testo(s->spiegazione, spiegazione ? spiegazione : "", sizeof s->spiegazione);
    widget_cambiato(&s->base);
}

/* --- l'avviso: un messaggio che interrompe e chiede conferma --- */

/* L'AVVISO, dal file Figma ("Avviso" e "Velo della finestra"). Un riquadro di vetro largo
 * AVVISO_LARGHEZZA, al centro della finestra che lo apre: un titolo, un messaggio (a capo
 * da solo, al massimo AVVISO_RIGHE righe) e in fondo a destra due pulsanti, Annulla
 * (Secondario) e la conferma (di solito Pericolo, per le cose che non si annullano).
 * Margine FINESTRA_PADDING (24), SPAZIO_8 tra titolo e messaggio, SPAZIO_24 prima dei
 * pulsanti; ombra "finestra".
 *
 * BLOCCANTE o no:
 *   - bloccante: la finestra sotto si spegne (titolo come una finestra sullo sfondo) e
 *     riceve il VELO (SUPERFICIE_VELO, con gli angoli della finestra); finche' non si
 *     risponde non si puo' usare niente, nemmeno chiudere o ridurre la finestra;
 *   - non bloccante: niente velo, la finestra resta attiva e usabile.
 * Invio = conferma, Esc = annulla.
 *
 *     static struct avviso riavvia;
 *     avviso_prepara(&riavvia, "Riavviare \"contatore\"?",
 *                    "Il processo viene chiuso e aperto di nuovo.", "Riavvia", PERICOLO, 1,
 *                    al_riavvia, 0);
 *     finestra_tk_avviso(&finestra, &riavvia);     // una volta, prima di uikit_esegui
 *     ...
 *     avviso_apri(&riavvia);                        // quando serve
 *
 * 'al_risposta' riceve la scelta: AVVISO_CONFERMA (1), AVVISO_ANNULLA (0) o, se c'e' il terzo
 * pulsante, AVVISO_ALTERNATIVA (2); l'avviso si chiude da solo.
 *
 * TRE SCELTE ("Scelte=Tre" in Figma): avviso_alternativa(&v, "Togli solo l'app", SECONDARIO)
 * aggiunge un terzo pulsante tra Annulla e la conferma; il riquadro diventa largo
 * AVVISO_LARGHEZZA_TRE (480). Per esempio Annulla / "Togli solo l'app" / "Togli app e dati". */
#define AVVISO_RIGHE 4
enum { AVVISO_ANNULLA, AVVISO_CONFERMA, AVVISO_ALTERNATIVA };

struct avviso;
typedef void (*al_risposta_avviso)(struct avviso *a, int scelta, void *dati);

/* L'avviso NON e' un figlio della finestra (non sta nel suo albero): la finestra lo tiene a
 * parte (finestra_tk_avviso), lo disegna dopo tutto il resto e gli da' i clic per primo.
 * Il suo 'genitore' e' comunque la finestra, cosi' widget_cambiato la trova. Il velo e
 * l'ombra (che escono dal riquadro piu' dei 24 pixel che un widget puo' sbordare) li
 * disegna la finestra (avviso_sotto); il riquadro, i testi e i pulsanti li disegna lui. */
struct avviso {
    struct widget base;                /* il riquadro */
    char titolo[MAX_TESTO_WIDGET];
    char messaggio[AVVISO_RIGHE * MAX_TESTO_WIDGET];
    int bloccante;
    int x, y, l, a;                    /* il riquadro, in coordinate della finestra */
    struct pulsante annulla, conferma;
    struct pulsante alternativa;       /* il terzo pulsante (avviso_alternativa), se 'tre' */
    int tre;
    al_risposta_avviso al_risposta;
    void *dati;
};

static inline int avviso_larghezza(const struct avviso *v) {
    return v->tre ? AVVISO_LARGHEZZA_TRE : AVVISO_LARGHEZZA;
}

static inline int avviso_righe(struct avviso *v, char righe[][MAX_RIGA_NOME]) {
    return testo_a_capo(FONT_CORPO, v->messaggio, avviso_larghezza(v) - 2 * FINESTRA_PADDING,
                        righe, AVVISO_RIGHE);
}

/* L'altezza del riquadro: margine, titolo, messaggio, spazio, pulsanti, margine. */
static inline int avviso_altezza(struct avviso *v) {
    char righe[AVVISO_RIGHE][MAX_RIGA_NOME];
    int n = v->messaggio[0] ? avviso_righe(v, righe) : 0;
    int pl, pa;
    widget_misura(&v->conferma.base, &pl, &pa);
    return 2 * FINESTRA_PADDING + INTERLINEA_SEZIONE + (n ? SPAZIO_8 + n * INTERLINEA_CORPO : 0) +
           SPAZIO_24 + pa;
}

/* Dispone il riquadro al centro della finestra (larga fl, alta fa) e i due pulsanti in
 * fondo a destra. */
static inline void avviso_sistema(struct avviso *v, int fl, int fa) {
    v->l = avviso_larghezza(v);
    v->a = avviso_altezza(v);
    v->x = (fl - v->l) / 2;
    v->y = (fa - v->a) / 2;
    v->base.x = v->x; v->base.y = v->y; v->base.l = v->l; v->base.a = v->a;
    /* da destra: la conferma, poi (se c'e') l'alternativa, poi Annulla, a SPAZIO_8 */
    struct pulsante *fila[3] = { &v->conferma, v->tre ? &v->alternativa : 0, &v->annulla };
    int ca, x = v->x + v->l - FINESTRA_PADDING;
    int dummy;
    widget_misura(&v->conferma.base, &dummy, &ca);
    int y = v->y + v->a - FINESTRA_PADDING - ca;
    for (int i = 0; i < 3; i++) {
        if (!fila[i]) continue;
        int bl, ba;
        widget_misura(&fila[i]->base, &bl, &ba);
        x -= bl;
        fila[i]->base.x = x; fila[i]->base.y = y; fila[i]->base.l = bl; fila[i]->base.a = ba;
        x -= SPAZIO_8;
    }
}

/* Sotto il riquadro: il velo (se bloccante) su tutta la finestra (larga fl, alta fa), con i
 * suoi angoli, e l'ombra del riquadro. Lo chiama la finestra, senza ritagli. */
static inline void avviso_sotto(struct avviso *v, struct pennello *p, int fl, int fa) {
    if (v->bloccante) {
        struct tondo velo = tondo(0, 0, fl, fa, RAGGIO_FINESTRA_NUOVO);
        pennello_colore(p, SUPERFICIE_VELO);
        riempi_forma(p, &velo);
    }
    struct tondo r = tondo(v->x, v->y, v->l, v->a, RAGGIO_FINESTRA_NUOVO);
    ombre_strati_fuori(p, &r, OMBRA_TAB(finestra), OMBRA_N(finestra));
}

static inline void avviso_disegna(struct widget *w, struct pennello *p) {
    struct avviso *v = (struct avviso *)w;
    struct tondo r = tondo(v->x, v->y, v->l, v->a, RAGGIO_FINESTRA_NUOVO);
    pennello_salva(p);
    /* Pieno, non vetro: in Figma l'avviso e' di vetro con la sfocatura dello sfondo, ma
     * dentro una finestra nessuno sfoca quello che sta sotto (lo fa solo il compositore,
     * sotto le finestre): il vetro lascerebbe leggere il testo di sotto. Come i menu di Files. */
    pennello_colore(p, SUPERFICIE_ELEVATA);
    riempi_forma(p, &r);
    ombre_strati_dentro(p, &r, OMBRA_TAB(finestra), OMBRA_N(finestra));
    int x = v->x + FINESTRA_PADDING, y = v->y + FINESTRA_PADDING;
    pennello_colore(p, TESTO_PRIMARIO);
    testo_centrato(p, FONT_SEZIONE_NUOVO, x, y, INTERLINEA_SEZIONE, v->titolo);
    y += INTERLINEA_SEZIONE;
    if (v->messaggio[0]) {
        char righe[AVVISO_RIGHE][MAX_RIGA_NOME];
        int n = avviso_righe(v, righe);
        y += SPAZIO_8;
        pennello_colore(p, TESTO_SECONDARIO);
        for (int i = 0; i < n; i++, y += INTERLINEA_CORPO)
            testo_centrato(p, FONT_CORPO, x, y, INTERLINEA_CORPO, righe[i]);
    }
    pennello_ripristina(p);
}

/* Il riquadro prende i clic che non vanno ai pulsanti: cosi' sotto non arriva niente. (Se
 * l'avviso e' bloccante, i clic fuori dal riquadro li butta via la finestra.) */
static inline void avviso_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    (void)w; (void)x; (void)y; (void)pulsanti;
}

static inline void avviso_chiudi(struct avviso *v);

static inline void avviso_rispondi(struct avviso *v, int scelta) {
    avviso_chiudi(v);
    if (v->al_risposta) v->al_risposta(v, scelta, v->dati);
}

static inline void avviso_tasto(struct widget *w, unsigned int tasto) {
    struct avviso *v = (struct avviso *)w;
    if (tasto == '\n' || tasto == '\r') avviso_rispondi(v, AVVISO_CONFERMA);
    else if (tasto == 27) avviso_rispondi(v, AVVISO_ANNULLA);
}

static const struct tipo_widget tipo_avviso = {
    0, 0, avviso_disegna, avviso_puntatore, avviso_tasto, 0, 0, 0, 1
};

static inline void avviso_al_annulla(struct pulsante *b, void *dati) { (void)b; avviso_rispondi(dati, AVVISO_ANNULLA); }
static inline void avviso_al_conferma(struct pulsante *b, void *dati) { (void)b; avviso_rispondi(dati, AVVISO_CONFERMA); }
static inline void avviso_al_alternativa(struct pulsante *b, void *dati) { (void)b; avviso_rispondi(dati, AVVISO_ALTERNATIVA); }

/* Prepara l'avviso (chiuso). 'tipo' e' il tipo del pulsante di conferma (PERICOLO,
 * PRIMARIO...), 'bloccante' 1 o 0. Annulla si puo' rinominare con avviso_annulla. */
static inline void avviso_prepara(struct avviso *v, const char *titolo, const char *messaggio,
                                  const char *conferma, int tipo, int bloccante,
                                  al_risposta_avviso al_risposta, void *dati) {
    *v = (struct avviso){ .base = { .tipo = &tipo_avviso, .nascosto = 1 }, .bloccante = bloccante,
                          .al_risposta = al_risposta, .dati = dati };
    copia_testo(v->titolo, titolo, MAX_TESTO_WIDGET);
    copia_testo(v->messaggio, messaggio ? messaggio : "", sizeof v->messaggio);
    pulsante_prepara(&v->annulla, "Annulla", NESSUNA_ICONA, avviso_al_annulla, v);
    pulsante_prepara(&v->conferma, conferma, NESSUNA_ICONA, avviso_al_conferma, v);
    pulsante_stile(&v->conferma, tipo, MISURA_M);
    widget_aggiungi(&v->base, &v->annulla.base);
    widget_aggiungi(&v->base, &v->conferma.base);
}

/* Cambia il testo di Annulla (per esempio "Apri cartella" in un avviso non bloccante). */
static inline void avviso_annulla(struct avviso *v, const char *testo) {
    copia_testo(v->annulla.testo, testo, MAX_TESTO_WIDGET);
}

/* Aggiunge il terzo pulsante, tra Annulla e la conferma (di solito SECONDARIO): la risposta
 * sara' AVVISO_ALTERNATIVA. Prima di avviso_apri. */
static inline void avviso_alternativa(struct avviso *v, const char *testo, int tipo) {
    if (!v->tre) {
        pulsante_prepara(&v->alternativa, testo, NESSUNA_ICONA, avviso_al_alternativa, v);
        widget_aggiungi(&v->base, &v->alternativa.base);
        v->tre = 1;
    } else {
        copia_testo(v->alternativa.testo, testo, MAX_TESTO_WIDGET);
    }
    pulsante_stile(&v->alternativa, tipo, MISURA_M);
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
    struct widget base;                /* il primo figlio e' il contenuto, il secondo (se
                                        * c'e') la barra degli strumenti */
    struct widget *strumenti;          /* la barra degli strumenti, o 0 (finestra_tk_barra) */
    char titolo[MAX_TITOLO];
    int l_fissa, a_fissa;              /* 0: la misura la decide il contenuto */
    int senza_margine;                 /* 1: il contenuto arriva fino ai bordi (finestra_tk_bordo) */
    struct finestra finestra;          /* il collegamento con il compositore */
    int attiva;                        /* ha il fuoco: e' in primo piano */
    int da_ridisegnare;                /* c'e' qualcosa da ridisegnare: le zone qui sotto */
    struct zona_finestra cambiate[MAX_ZONE_MOSTRA];   /* le zone cambiate dall'ultimo ridisegno */
    int quante_cambiate;
    int tutta_cambiata;                /* tutta la finestra (l'impaginazione e' cambiata) */
    struct widget *catturato;          /* il widget su cui si e' premuto */
    struct widget *sotto;              /* quello che ha il puntatore sopra */
    struct widget *con_fuoco;          /* quello che riceve i tasti */
    int chiudi_premuto;
    int riduci_premuto;
    int comando_sopra;                 /* il puntatore e' sopra: 0 nessuno, 1 riduci, 2 chiudi */
    int da_chiudere;
    unsigned int pulsanti_prima;
    struct chiamata_tk chiamate[MAX_CHIAMATE];   /* i "chiamami ogni tanto" */
    struct avviso *avviso;             /* l'avviso della finestra (finestra_tk_avviso), o 0 */
    int mini;                          /* 1: mini finestra (finestra_tk_mini) */
    /* la tastiera fisica (tasti.h): chi ascolta le scorciatoie, e i modificatori premuti */
    struct ascoltatore_tk ascoltatori[MAX_ASCOLTATORI];
    unsigned int modificatori;         /* MOD_CTRL | MOD_MAIUSC | MOD_ALT, tenuti giu' adesso */
    /* L'ALTRA FINESTRA dell'applicazione (una sola, per le cose secondarie: le informazioni di
     * un file in una mini finestra), aperta con finestra_tk_apri_altra: il ciclo degli eventi
     * di questa finestra serve anche lei. 0 se non c'e'. */
    struct finestra_tk *altra;
    int aperta;                        /* 1 mentre la finestra c'e' sullo schermo */
};

/* L'altezza della testata: TITOLO_ALTEZZA (40), o ALTEZZA_BARRA_MINI (28) nella mini finestra. */
static inline int testata_altezza(const struct finestra_tk *f) {
    return f->mini ? ALTEZZA_BARRA_MINI : TITOLO_ALTEZZA;
}
/* Il lato dei comandi (riduci, chiudi): S (32), o XS (24) nella mini finestra. */
static inline int comando_lato(const struct finestra_tk *f) { return f->mini ? ALTEZZA_XS : ALTEZZA_S; }
/* Lo spazio dei comandi a destra (e, uguale, quello vuoto a sinistra che centra il titolo):
 * riduci e chiudi con i margini (76), o nella mini finestra il solo chiudi (4 + 24 = 28). */
static inline int comandi_larghezza(const struct finestra_tk *f) {
    return f->mini ? TITOLO_MARGINE + ALTEZZA_XS : TITOLO_COMANDI_LARGHEZZA;
}
/* Il margine intorno al contenuto: FINESTRA_PADDING (24), SPAZIO_16 nella mini finestra. */
static inline int finestra_padding(const struct finestra_tk *f) { return f->mini ? SPAZIO_16 : FINESTRA_PADDING; }

/* L'avviso e' aperto. Se bloccante, la finestra sotto non si usa. */
static inline struct avviso *finestra_tk_avviso_aperto(const struct finestra_tk *f) {
    return f->avviso && !f->avviso->base.nascosto ? f->avviso : 0;
}
static inline int finestra_tk_bloccata(const struct finestra_tk *f) {
    struct avviso *v = finestra_tk_avviso_aperto(f);
    return v && v->bloccante;
}

/* La finestra in cui sta un widget: la radice del suo albero. */
static inline struct finestra_tk *widget_finestra(struct widget *w) {
    while (w->genitore) w = w->genitore;
    return (struct finestra_tk *)w;
}

static const struct tipo_widget tipo_finestra_tk;   /* piu' sotto */

/* LE ZONE DA RIDISEGNARE della finestra (vedi in cima, "si ridisegna solo quello che e'
 * cambiato"): un rettangolo in piu', in coordinate della finestra. */
static inline void uikit_cambiata_zona(struct finestra_tk *f, int x, int y, int l, int a) {
    struct zona_finestra z = zona_dentro((struct zona_finestra){ x, y, l, a },
                                         (struct zona_finestra){ 0, 0, f->base.l, f->base.a });
    if (zona_vuota(z)) return;
    f->da_ridisegnare = 1;
    if (!f->tutta_cambiata) zone_aggiungi(f->cambiate, &f->quante_cambiate, MAX_ZONE_MOSTRA, z);
}

/* Tutta la finestra (l'impaginazione e' cambiata, o il tema). */
static inline void uikit_cambiata_tutta(struct finestra_tk *f) {
    f->da_ridisegnare = 1;
    f->tutta_cambiata = 1;
}

/* La testata (il titolo, i comandi riduci e chiudi), con il suo divisore. */
static inline void uikit_cambiata_testata(struct finestra_tk *f) {
    uikit_cambiata_zona(f, 0, 0, f->base.l, testata_altezza(f) + TITOLO_DIVISORE);
}

/* Se il widget non e' ancora in una finestra (lo si sta preparando, prima di
 * uikit_esegui), non c'e' niente da ridisegnare: la cima del suo albero non e' una
 * finestra, e non la si tocca. La zona si taglia sullo spazio del widget con il suo sbordo:
 * fuori di li' il widget non disegna. */
static inline void widget_segna_zona(struct widget *w, int x, int y, int l, int a) {
    struct finestra_tk *f = widget_finestra(w);
    if (f->base.tipo != &tipo_finestra_tk) return;
    struct zona_finestra z = zona_dentro((struct zona_finestra){ x, y, l, a },
                                         (struct zona_finestra){ w->x - WIDGET_SBORDO, w->y - WIDGET_SBORDO,
                                                                 w->l + 2 * WIDGET_SBORDO, w->a + 2 * WIDGET_SBORDO });
    uikit_cambiata_zona(f, z.x, z.y, z.l, z.a);
}

static inline void widget_segna(struct widget *w) {
    widget_segna_zona(w, w->x - WIDGET_SBORDO, w->y - WIDGET_SBORDO, w->l + 2 * WIDGET_SBORDO,
                      w->a + 2 * WIDGET_SBORDO);
}

static inline void widget_cambiato(struct widget *w) {
    if (widget_visibile(w)) widget_segna(w);
}

static inline void widget_cambiato_zona(struct widget *w, int x, int y, int l, int a) {
    if (widget_visibile(w)) widget_segna_zona(w, x, y, l, a);
}

/* LA FINESTRA, dal file Figma. Sono DUE pezzi separati, uno sopra l'altro:
 *
 *   - la TESTATA (il titolo): alta TITOLO_ALTEZZA (40), con i soli angoli in ALTO
 *     arrotondati. Dentro, tre cose in fila: a sinistra uno spazio vuoto, al centro il
 *     titolo, a destra i comandi riduci e chiudi. I comandi sono pulsanti S (ALTEZZA_S, 32)
 *     tondi e "ghost" (senza fondo, il fondo compare solo sotto il puntatore), a
 *     TITOLO_MARGINE (4) dal bordo e l'uno dall'altro: 4 + 32 + 4 = 40, cioe' proprio
 *     l'altezza della testata. Lo spazio vuoto a sinistra e' largo quanto il gruppo dei
 *     comandi (TITOLO_COMANDI_LARGHEZZA), cosi' il titolo resta centrato nella finestra.
 *     Sotto la testata c'e' un divisore di 1 pixel (BORDO_SEPARATORE) da bordo a bordo,
 *     senza margini ai lati; se sotto c'e' la barra degli strumenti il divisore e' di vetro
 *     (non si vede): testata e barra sono una superficie sola;
 *   - il CONTENUTO: tutto quello che sta sotto il divisore, con i soli angoli in BASSO
 *     arrotondati. Comincia con la barra degli strumenti, se c'e', poi il contenuto
 *     dell'applicazione, con FINESTRA_PADDING (24) intorno (a meno di finestra_tk_bordo).
 *
 * La testata non e' contenuto: ha la sua funzione di disegno e le sue misure, e quello che
 * l'applicazione mette nella finestra comincia sempre sotto di lei (finestra_tk_corpo).
 *
 * Dove stanno i due comandi nella testata: riduci e chiudi, a destra. */
static inline int comando_x(const struct finestra_tk *f, int chiudi) {
    int lato = comando_lato(f);
    return f->base.l - TITOLO_MARGINE - lato - (chiudi ? 0 : lato + TITOLO_MARGINE);
}
/* centrati in altezza nella testata: 4 nella normale (4 + 32 + 4), 2 nella mini (2 + 24 + 2) */
static inline int comando_y(const struct finestra_tk *f) { return (testata_altezza(f) - comando_lato(f)) / 2; }

/* Il puntatore e' sopra il comando? La mini finestra non si riduce: ha solo chiudi. */
static inline int su_comando(const struct finestra_tk *f, int chiudi, int x, int y) {
    if (f->mini && !chiudi) return 0;
    int cx = comando_x(f, chiudi), cy = comando_y(f), lato = comando_lato(f);
    return x >= cx && x < cx + lato && y >= cy && y < cy + lato;
}

/* Dove comincia il corpo della finestra: sotto la testata col suo divisore e, se c'e' (e non
 * e' nascosta), sotto la barra degli strumenti. */
static inline int finestra_tk_corpo(const struct finestra_tk *f) {
    return testata_altezza(f) + TITOLO_DIVISORE +
           (f->strumenti && !f->strumenti->nascosto ? STRUMENTI_ALTEZZA : 0);
}

/* Il contenuto, piu' il padding e le barre; abbastanza larga per il titolo e i comandi, e
 * per quello che sta nella barra degli strumenti. Con una misura fissa, la misura e' di
 * tutta la finestra: la barra degli strumenti la prende al contenuto. */
static inline void finestra_tk_misura(struct widget *w, int *l, int *a) {
    struct finestra_tk *f = (struct finestra_tk *)w;
    if (f->l_fissa) { *l = f->l_fissa; *a = f->a_fissa; return; }
    int cl = 0, ca = 0;
    if (w->quanti_figli) widget_misura(w->figli[0], &cl, &ca);
    /* il titolo sta tra i due gruppi (lo spazio vuoto e i comandi), con un po' d'aria */
    int barra = 2 * (comandi_larghezza(f) + SPAZIO_8) +
                larghezza_testo(f->mini ? FONT_ETICHETTA : FONT_TITOLO_FINESTRA, f->titolo);
    *l = 2 * finestra_padding(f) + cl;
    if (*l < barra) *l = barra;
    if (f->strumenti && !f->strumenti->nascosto) {
        int sl, sa;
        widget_misura(f->strumenti, &sl, &sa);
        if (*l < sl) *l = sl;
    }
    *a = finestra_tk_corpo(f) + 2 * finestra_padding(f) + ca;
}

static inline void finestra_tk_disponi(struct widget *w) {
    struct finestra_tk *f = (struct finestra_tk *)w;
    if (!w->quanti_figli) return;
    if (f->strumenti) {                     /* tutta la larghezza, subito sotto il divisore */
        f->strumenti->x = 0;
        f->strumenti->y = testata_altezza(f) + TITOLO_DIVISORE;
        f->strumenti->l = w->l;
        f->strumenti->a = STRUMENTI_ALTEZZA;
    }
    int corpo = finestra_tk_corpo(f);
    int margine = f->senza_margine ? 0 : finestra_padding(f);
    struct widget *c = w->figli[0];
    c->x = margine;
    c->y = corpo + margine;
    c->l = w->l - 2 * margine;
    c->a = w->a - corpo - 2 * margine;
}

/* IL CONTENUTO della finestra, cioe' il suo fondo: il vetro (SUPERFICIE_VETRO, bianco o
 * grafite al 72%) che lascia vedere lo sfondo che il compositore ha sfocato qui sotto, con i
 * soli angoli in basso arrotondati. Quello che sta nel contenuto (la barra degli strumenti,
 * il widget dell'applicazione) lo disegnano i figli, dopo. */
static inline void finestra_tk_contenuto_disegna(struct finestra_tk *f, struct pennello *p) {
    int inizio = testata_altezza(f) + TITOLO_DIVISORE;
    struct tondo contenuto = { 0, inizio, f->base.l, f->base.a - inizio,
                               { 0, 0, RAGGIO_FINESTRA_NUOVO, RAGGIO_FINESTRA_NUOVO } };
    pennello_colore(p, SUPERFICIE_VETRO);
    riempi_forma(p, &contenuto);
}

/* LA TESTATA: il fondo (lo stesso vetro, soli angoli in alto arrotondati), il divisore di
 * 1 pixel da bordo a bordo (solo senza barra degli strumenti: vedi sotto), il titolo centrato e i comandi. Una finestra che non ha il
 * fuoco (non e' in primo piano) ha il titolo in TESTO_SECONDARIO e i comandi al 50%.
 *
 * Il VETRO e' lo stesso del contenuto: i due pezzi si toccano lungo il divisore, e le
 * coperture dei loro bordi dritti sono piene, quindi in mezzo non resta nessuna cucitura. */
static inline void finestra_tk_titolo_disegna(struct finestra_tk *f, struct pennello *p) {
    struct widget *w = &f->base;
    int alta = testata_altezza(f);
    struct tondo testata = { 0, 0, w->l, alta,
                             { RAGGIO_FINESTRA_NUOVO, RAGGIO_FINESTRA_NUOVO, 0, 0 } };
    pennello_colore(p, SUPERFICIE_VETRO);
    riempi_forma(p, &testata);
    /* Il divisore. Con la barra degli strumenti sotto, testata e barra sono UNA superficie
     * di vetro (deciso dalla persona: la linea li' non ci va, anche se il componente "Titolo
     * finestra" del disegno la ha): la riga del divisore resta (le misure non cambiano) ma e'
     * dello stesso vetro. Senza barra, il divisore BORDO_SEPARATORE stacca la testata dal
     * contenuto, come nel disegno. */
    int con_barra = f->strumenti && !f->strumenti->nascosto;
    pennello_colore(p, con_barra ? SUPERFICIE_VETRO : BORDO_SEPARATORE);
    riempi_rettangolo(p, 0, alta, w->l, TITOLO_DIVISORE);

    /* il titolo, al centro della finestra: tra lo spazio vuoto a sinistra (largo quanto i
     * comandi) e i comandi non entra mai nei comandi, un titolo lungo si taglia. Nella mini
     * finestra e' piu' piccolo (l'etichetta, 12). */
    const struct font *font = f->mini ? FONT_ETICHETTA : FONT_TITOLO_FINESTRA;
    pennello_salva(p);
    int margine = comandi_larghezza(f) + SPAZIO_8;
    pennello_ritaglia(p, margine, 0, w->l - 2 * margine, alta);
    int attiva = f->attiva && !finestra_tk_bloccata(f);   /* sotto un avviso bloccante si spegne */
    pennello_colore(p, attiva ? TESTO_PRIMARIO : TESTO_SECONDARIO);
    testo_centrato(p, font, (w->l - larghezza_testo(font, f->titolo)) / 2, 0, alta, f->titolo);
    pennello_ripristina(p);

    /* i comandi: pulsanti ghost tondi (S, o XS nella mini finestra, che ha solo chiudi).
     * Senza fondo a riposo; sotto il puntatore CONTROLLO_SFONDO, premuto
     * CONTROLLO_SFONDO_SOPRA (come ogni ghost di UIKit). */
    int lato = comando_lato(f), segno = f->mini ? ICONA_XS : ICONA_S, cy = comando_y(f);
    pennello_salva(p);
    if (!attiva) pennello_opacita(p, OPACITA_COMANDI_INATTIVI);
    for (int chiudi = f->mini ? 1 : 0; chiudi < 2; chiudi++) {
        int cx = comando_x(f, chiudi);
        int sopra = f->comando_sopra == 1 + chiudi;
        int premuto = sopra && (chiudi ? f->chiudi_premuto : f->riduci_premuto);
        if (sopra) {
            pennello_colore(p, premuto ? CONTROLLO_SFONDO_SOPRA : CONTROLLO_SFONDO_NUOVO);
            riempi_cerchio(p, cx + lato / 2, cy + lato / 2, lato / 2);
        }
        pennello_colore(p, COLORE_ICONA(COLORE_ICONA_PRIMARIO));
        icona(p, chiudi ? ICONA_X : ICONA_MINUS, segno, cx + (lato - segno) / 2, cy + (lato - segno) / 2);
    }
    pennello_ripristina(p);
}

/* La finestra: pulisce, disegna il contenuto e la testata (due pezzi separati, vedi sopra),
 * e infine il filo di luce interno del tema scuro (gli strati INTERNI della tabella
 * "finestra": nel chiaro non ce ne sono). Le ombre ESTERNE e la sfocatura dello sfondo non
 * sono qui: stanno fuori dalla finestra, le fa il compositore. Non c'e' nessun bordo.
 *
 * IL FONDO SCURO. Prima, sotto il vetro, c'era VETRO_FONDO (un blu scuro al 50%) perche' il
 * testo era bianco e sopra uno sfondo chiaro non si sarebbe letto. Ora il testo cambia col
 * tema: scuro sul vetro bianco (tema chiaro), chiaro sul vetro grafite (tema scuro). Il vetro
 * bianco al 72% sopra lo sfondo sfocato resta chiaro (il testo scuro si legge: sotto, il
 * colore dello sfondo conta per il 28%); il vetro grafite al 72% resta scuro. Un fondo scuro
 * in piu' renderebbe grigio il vetro chiaro, e non c'e' nel disegno: tolto in tutti e due i
 * temi. */
static inline void finestra_tk_disegna(struct widget *w, struct pennello *p) {
    struct finestra_tk *f = (struct finestra_tk *)w;
    pulisci(p);
    finestra_tk_contenuto_disegna(f, p);
    finestra_tk_titolo_disegna(f, p);
    struct tondo tutta = tondo(0, 0, w->l, w->a, RAGGIO_FINESTRA_NUOVO);
    ombre_strati_dentro(p, &tutta, OMBRA_TAB(finestra), OMBRA_N(finestra));
}

static const struct tipo_widget tipo_finestra_tk = {
    finestra_tk_misura, finestra_tk_disponi, finestra_tk_disegna, 0, 0, 0, 0, 0, 0
};

static inline void finestra_tk_prepara(struct finestra_tk *f, const char *titolo,
                                       struct widget *contenuto) {
    *f = (struct finestra_tk){ .base = { .tipo = &tipo_finestra_tk } };
    copia_testo(f->titolo, titolo, MAX_TITOLO);
    widget_aggiungi(&f->base, contenuto);
}

/* Mette una barra degli strumenti sotto la barra del titolo (di solito una 'struct
 * barra_strumenti', piu' sotto): la finestra si allunga della sua altezza. Da chiamare
 * prima di uikit_esegui, dopo finestra_tk_prepara. */
static inline void finestra_tk_barra(struct finestra_tk *f, struct widget *strumenti) {
    f->strumenti = strumenti;
    widget_aggiungi(&f->base, strumenti);
}

/* Il contenuto senza il margine intorno: arriva fino ai bordi della finestra e fin sotto
 * la barra (degli strumenti, se c'e'). Per le finestre fatte a "pannelli", come Files con
 * la sua barra laterale: i margini, dentro, li decide l'applicazione. Prima di
 * uikit_esegui. */
static inline void finestra_tk_bordo(struct finestra_tk *f) { f->senza_margine = 1; }

/* Cambia il titolo della finestra (per esempio, la cartella in cui si e'). Lo disegna la
 * finestra stessa, nella sua barra: basta ridisegnarla. */
static inline void finestra_tk_titolo(struct finestra_tk *f, const char *titolo) {
    copia_testo(f->titolo, titolo, MAX_TITOLO);
    uikit_cambiata_testata(f);
}

/* LA MINI FINESTRA, per le cose secondarie (informazioni su un file, un avanzamento, uno
 * strumento): testata da ALTEZZA_BARRA_MINI (28) con il titolo piccolo (FONT_ETICHETTA) e il
 * solo pulsante di chiusura XS (24): non si riduce. Il contenuto ha SPAZIO_16 di margine.
 * Prima di uikit_esegui, dopo finestra_tk_prepara. */
static inline void finestra_tk_mini(struct finestra_tk *f) { f->mini = 1; }

/* Da' alla finestra il suo avviso (chiuso): si apre con avviso_apri. Prima di
 * uikit_esegui, dopo finestra_tk_prepara. Una finestra ha un solo avviso alla volta. */
static inline void finestra_tk_avviso(struct finestra_tk *f, struct avviso *v) {
    f->avviso = v;
    v->base.genitore = &f->base;
}

/* Chiude la finestra, come il pulsante "chiudi": uikit_esegui torna. */
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
static inline struct chiamata_tk *uikit_prima_chiamata(struct finestra_tk *f) {
    struct chiamata_tk *prima = 0;
    for (int i = 0; i < MAX_CHIAMATE; i++) {
        struct chiamata_tk *c = &f->chiamate[i];
        if (c->funzione && (!prima || (int)(c->scadenza - prima->scadenza) < 0)) prima = c;
    }
    return prima;
}

/* Fa le chiamate scadute. Ognuna prima si rimette in fila (la prossima volta tra 'ms'),
 * poi si chiama: cosi' puo' cambiare il suo ritmo, o smettere. */
static inline void uikit_chiamate_scadute(struct finestra_tk *f) {
    unsigned int ora = tempo();
    for (int i = 0; i < MAX_CHIAMATE; i++) {
        struct chiamata_tk *c = &f->chiamate[i];
        if (!c->funzione || (int)(c->scadenza - ora) > 0) continue;
        c->scadenza = ora + c->ms;
        c->funzione(f, c->dati);
    }
}

/* Rifa' l'impaginazione dentro la finestra (dopo aver mostrato o nascosto dei widget). */
static inline void uikit_ridisponi(struct finestra_tk *f) {
    widget_disponi(&f->base);
    uikit_cambiata_tutta(f);
}

/* --- la barra degli strumenti --- */

/* LA BARRA DEGLI STRUMENTI, dal file Figma. Una riga alta STRUMENTI_ALTEZZA (48) sotto la
 * testata, DENTRO la finestra: e' parte del contenuto e non ha un fondo suo (si vede il
 * vetro della finestra), larga quanto tutta la finestra (non sta nel padding del
 * contenuto), come nel Finder del Mac. Dentro, da sinistra a destra, le sue cose: controlli
 * S (32 di altezza) con STRUMENTI_MARGINE_SOPRA (8) sopra e sotto, a GAP_M (8) l'una
 * dall'altra, a STRUMENTI_MARGINE_LATI (8) dai bordi: come nelle schermate di Figma. Una cosa 'elastica' (il percorso, o
 * uno spazio) prende la larghezza che avanza.
 *
 * Un clic sullo spazio vuoto della barra trascina la finestra, come sulla testata (cosi'
 * fa il Mac): vedi uikit_puntatore.
 *
 *     barra_strumenti_prepara(&strumenti);
 *     barra_strumenti_aggiungi(&strumenti, &frecce.base);
 *     barra_strumenti_aggiungi(&strumenti, &dove.base);
 *     finestra_tk_barra(&finestra, &strumenti.base);
 *
 * Dentro c'e' una riga (il suo unico figlio): la barra le lascia lo spazio meno i
 * margini, e la riga mette in fila le cose. */
struct barra_strumenti {
    struct widget base;
    struct contenitore riga;
};

static inline void barra_strumenti_misura(struct widget *w, int *l, int *a) {
    struct barra_strumenti *b = (struct barra_strumenti *)w;
    widget_misura(&b->riga.base, l, a);
    *l += 2 * STRUMENTI_MARGINE_LATI;
    *a = STRUMENTI_ALTEZZA;
}

static inline void barra_strumenti_disponi(struct widget *w) {
    struct barra_strumenti *b = (struct barra_strumenti *)w;
    b->riga.base.x = w->x + STRUMENTI_MARGINE_LATI;
    b->riga.base.y = w->y + STRUMENTI_MARGINE_SOPRA;
    b->riga.base.l = w->l - 2 * STRUMENTI_MARGINE_LATI;
    b->riga.base.a = ALTEZZA_S;
}

static const struct tipo_widget tipo_barra_strumenti = {
    barra_strumenti_misura, barra_strumenti_disponi, 0, 0, 0, 0, 0, 0, 0
};

static inline void barra_strumenti_prepara(struct barra_strumenti *b) {
    *b = (struct barra_strumenti){ .base = { .tipo = &tipo_barra_strumenti } };
    riga_prepara(&b->riga, GAP_M);
    widget_aggiungi(&b->base, &b->riga.base);
}

static inline void barra_strumenti_aggiungi(struct barra_strumenti *b, struct widget *w) {
    widget_aggiungi(&b->riga.base, w);
}

/* Lo spazio vuoto della barra (non una delle sue cose): da li' si trascina la finestra. */
static inline int barra_strumenti_vuota(struct finestra_tk *f, int x, int y) {
    struct widget *s = f->strumenti;
    if (!s || s->nascosto || !widget_contiene(s, x, y)) return 0;
    struct widget *w = widget_in(s, x, y);
    return !w || w == s || w == s->figli[0] || w->tipo == &tipo_spazio;
}

/* IL PULSANTE DELLA BARRA, dal file Figma. Fuori da un gruppo e' un pulsante GHOST S: senza
 * fondo a riposo, CONTROLLO_SFONDO sotto il puntatore, CONTROLLO_SFONDO_SOPRA premuto (gli
 * stessi stati del pulsante ghost). Solo icona e' TONDO (32 x 32, icona 16); con un testo
 * (pulsante_barra_testo) e' una PILLOLA, larga quanto il testo piu' PADDING_S per lato.
 * Scatta come i pulsanti normali, quando si lascia. Disabilitato (per esempio "avanti"
 * quando non c'e' dove andare) non sente niente e ha l'icona (o il testo) nel colore
 * Attenuato, come l'"Azione di gruppo" disabilitata del disegno; nessun fondo.
 *
 * Dentro una PILLOLA (il gruppo, sotto) cambia misura e aspetto: e' una voce alta 24
 * (GRUPPO_ELEMENTO). Se il gruppo e' a scelta, la voce accesa e' SOLLEVATA (SUPERFICIE_ELEVATA
 * con l'ombra "sollevato", testo e icona TESTO_PRIMARIO) e le altre sono senza fondo con
 * TESTO_SECONDARIO; sotto il puntatore, una voce spenta prende un fondo piatto leggero
 * (CONTROLLO_SFONDO). Nei gruppi non a scelta (indietro / avanti: il "Gruppo di azioni" del
 * disegno) le voci non si accendono mai: sono ghost di 24 con testo e icona TESTO_SECONDARIO,
 * CONTROLLO_SFONDO sotto il puntatore e CONTROLLO_SFONDO_SOPRA premute. */
struct pulsante_barra;
typedef void (*al_clic_pulsante_barra)(struct pulsante_barra *b, void *dati);

struct pulsante_barra {
    struct widget base;
    int icona;
    char testo[MAX_TESTO_WIDGET];      /* vuoto: si vede l'icona; se no il testo, senza icona */
    int acceso;                        /* in una pillola a scelta: e' quello scelto */
    al_clic_pulsante_barra al_clic;
    void *dati;
    int premuto, dentro;               /* come il pulsante (vedi pressione) */
};

static const struct tipo_widget tipo_pillola;   /* piu' sotto */

/* La pillola (il gruppo, descritto sotto): qui serve solo sapere se e' a scelta. */
struct pillola {
    struct widget base;
    int a_scelta;
};

/* Il gruppo (la pillola) in cui sta il pulsante, o 0 se sta da solo nella barra. */
static inline struct widget *pulsante_barra_gruppo(const struct widget *w) {
    return w->genitore && w->genitore->tipo == &tipo_pillola ? w->genitore : 0;
}

static inline void pulsante_barra_misura(struct widget *w, int *l, int *a) {
    struct pulsante_barra *b = (struct pulsante_barra *)w;
    *a = pulsante_barra_gruppo(w) ? GRUPPO_ELEMENTO : ALTEZZA_S;
    *l = *a;                                                  /* solo icona: tondo */
    if (b->testo[0]) *l = 2 * PADDING_S + larghezza_testo(FONT_PULSANTE_S, b->testo);
}

static inline void pulsante_barra_disegna(struct widget *w, struct pennello *p) {
    struct pulsante_barra *b = (struct pulsante_barra *)w;
    struct widget *gruppo = pulsante_barra_gruppo(w);
    int a_scelta = gruppo && ((struct pillola *)gruppo)->a_scelta;
    int premuto = b->premuto && b->dentro;
    int sopra = w->sopra && !w->disabilitato;
    struct tondo t = tondo(w->x, w->y, w->l, w->a, w->a / 2);
    /* il modo del colore d'icona (tema.h): nel gruppo secondario (la voce scelta, e sotto il
     * puntatore in un gruppo a scelta, primario); fuori da un gruppo un ghost normale,
     * primario; disabilitato attenuato. Il testo (pulsante_barra_testo) segue lo stesso modo:
     * i colori sono gli stessi (Attenuato = TESTO_DISABILITATO...). */
    int modo = w->disabilitato ? COLORE_ICONA_ATTENUATO
             : gruppo ? COLORE_ICONA_SECONDARIO : COLORE_ICONA_PRIMARIO;

    if (a_scelta && b->acceso) {
        /* la voce scelta: SOLLEVATA dentro il binario. Le ombre non devono uscire dalla
         * pillola (nel disegno il binario ritaglia): si ritaglia sul suo rettangolo. */
        pennello_salva(p);
        pennello_ritaglia(p, gruppo->x, gruppo->y, gruppo->l, gruppo->a);
        if (!premuto) ombre_strati_fuori(p, &t, OMBRA_TAB(sollevato), OMBRA_N(sollevato));
        pennello_colore(p, SUPERFICIE_ELEVATA);
        riempi_forma(p, &t);
        if (!premuto) ombre_strati_dentro(p, &t, OMBRA_TAB(sollevato), OMBRA_N(sollevato));
        pennello_ripristina(p);
        if (!w->disabilitato) modo = COLORE_ICONA_PRIMARIO;
    } else if (!w->disabilitato && (premuto || sopra)) {
        pennello_colore(p, premuto ? CONTROLLO_SFONDO_SOPRA : CONTROLLO_SFONDO_NUOVO);
        riempi_forma(p, &t);
        if (a_scelta) modo = COLORE_ICONA_PRIMARIO;
    }
    pennello_colore(p, COLORE_ICONA(modo));
    if (b->testo[0]) {
        testo_centrato(p, FONT_PULSANTE_S, w->x + PADDING_S, w->y, w->a, b->testo);
        return;
    }
    icona(p, b->icona, ICONA_S, w->x + (w->l - ICONA_S) / 2, w->y + (w->a - ICONA_S) / 2);
}

/* LA PILLOLA: un gruppo di pulsanti, il "gruppo di pulsanti" del disegno. E' un binario
 * INCASSATO (contiene e riceve): fondo SUPERFICIE_INCASSATA con l'ombra "incassato" (la
 * parte interna, che lo scava, e un filo esterno), MAI un bordo. Alto ALTEZZA_S (32),
 * pillola (raggio = meta' altezza), con GRUPPO_PADDING (4) intorno alle voci, che sono alte
 * GRUPPO_ELEMENTO (24): 4 + 24 + 4. Le voci con l'icona sono tonde (24 x 24), quelle con un
 * testo larghe quanto il testo piu' PADDING_S per lato. Puo' essere:
 *   - di pulsanti semplici: ognuno fa la sua cosa (indietro, avanti);
 *   - a SCELTA: ne e' acceso uno solo, quello premuto per ultimo (griglia o lista), come
 *     le scelte singole; la voce accesa e' il pezzo mobile, SOLLEVATO dentro il binario. Il
 *     clic si fa sentire anche sul pulsante gia' acceso. */
static inline void pillola_misura(struct widget *w, int *l, int *a) {
    int visti = 0, largo = 0;
    for (int i = 0; i < w->quanti_figli; i++) {
        if (w->figli[i]->nascosto) continue;
        int fl, fa;
        widget_misura(w->figli[i], &fl, &fa);
        largo += fl;
        visti++;
    }
    *l = 2 * GRUPPO_PADDING + largo + (visti ? visti - 1 : 0) * PILLOLA_SPAZIO;
    *a = ALTEZZA_S;
}

static inline void pillola_disponi(struct widget *w) {
    int x = w->x + GRUPPO_PADDING;
    for (int i = 0; i < w->quanti_figli; i++) {
        struct widget *f = w->figli[i];
        if (f->nascosto) continue;
        int fl, fa;
        widget_misura(f, &fl, &fa);
        f->x = x;
        f->y = w->y + GRUPPO_PADDING;
        f->l = fl;
        f->a = GRUPPO_ELEMENTO;
        x += fl + PILLOLA_SPAZIO;
    }
}

static inline void pillola_disegna(struct widget *w, struct pennello *p) {
    struct tondo t = tondo(w->x, w->y, w->l, w->a, w->a / 2);
    ombre_strati_fuori(p, &t, OMBRA_TAB(incassato), OMBRA_N(incassato));
    pennello_colore(p, SUPERFICIE_INCASSATA);
    riempi_forma(p, &t);
    ombre_strati_dentro(p, &t, OMBRA_TAB(incassato), OMBRA_N(incassato));
}

static const struct tipo_widget tipo_pillola = {
    pillola_misura, pillola_disponi, pillola_disegna, 0, 0, 0, 0, 0, 0
};

/* Accende o spegne un pulsante; in una pillola a scelta, accenderlo spegne gli altri. */
static inline void pulsante_barra_accendi(struct pulsante_barra *b, int acceso) {
    struct widget *g = b->base.genitore;
    if (acceso && g && g->tipo == &tipo_pillola && ((struct pillola *)g)->a_scelta)
        for (int i = 0; i < g->quanti_figli; i++) {
            struct pulsante_barra *altro = (struct pulsante_barra *)g->figli[i];
            if (altro != b && altro->acceso) { altro->acceso = 0; widget_cambiato(&altro->base); }
        }
    b->acceso = acceso;
    widget_cambiato(&b->base);
}

static inline void pulsante_barra_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    struct pulsante_barra *b = (struct pulsante_barra *)w;
    if (!pressione(w, &b->premuto, &b->dentro, x, y, pulsanti)) return;
    struct widget *g = w->genitore;
    if (g && g->tipo == &tipo_pillola && ((struct pillola *)g)->a_scelta) pulsante_barra_accendi(b, 1);
    if (b->al_clic) b->al_clic(b, b->dati);
}

static const struct tipo_widget tipo_pulsante_barra = {
    pulsante_barra_misura, 0, pulsante_barra_disegna, pulsante_barra_puntatore, 0, 0, 0, 0, 1
};

static inline void pulsante_barra_prepara(struct pulsante_barra *b, int icona,
                                          al_clic_pulsante_barra al_clic, void *dati) {
    *b = (struct pulsante_barra){ .base = { .tipo = &tipo_pulsante_barra }, .icona = icona,
                                  .al_clic = al_clic, .dati = dati };
}

/* Un pulsante della barra con un testo invece dell'icona: una scheda (vedi sopra). */
static inline void pulsante_barra_testo(struct pulsante_barra *b, const char *testo,
                                        al_clic_pulsante_barra al_clic, void *dati) {
    pulsante_barra_prepara(b, NESSUNA_ICONA, al_clic, dati);
    copia_testo(b->testo, testo, MAX_TESTO_WIDGET);
}

/* 'a_scelta': 1 se ne resta acceso uno solo (poi pulsante_barra_accendi per il primo). */
static inline void pillola_prepara(struct pillola *p, int a_scelta) {
    *p = (struct pillola){ .base = { .tipo = &tipo_pillola }, .a_scelta = a_scelta };
}

static inline void pillola_aggiungi(struct pillola *p, struct pulsante_barra *b) {
    widget_aggiungi(&p->base, &b->base);
}

/* IL PERCORSO: dove si e', come nel Finder. I nomi delle cartelle, dalla piu' esterna a
 * quella in cui si e', separati da una barra "/" (FONT_CORPO, TESTO_SECONDARIO), con
 * PERCORSO_SPAZIO (4) prima e dopo, come nella barra di Files del file Figma. I nomi sono in
 * FONT_CORPO e TESTO_SECONDARIO, e TESTO_PRIMARIO sotto il puntatore: un clic su un nome
 * chiama al_clic con il suo numero (0 il primo). L'ultimo, la cartella in cui si e', e' in
 * FONT_CORPO_MEDIO e TESTO_PRIMARIO, e non si clicca.
 * Il percorso e' elastico: prende la larghezza che avanza nella barra. Se non ci sta,
 * si tolgono i nomi dal principio (restano gli ultimi, i piu' utili); se non ci sta
 * neanche l'ultimo, lo si taglia.
 *
 * Ogni nome e' un widget figlio: cosi' UIKit sa sopra quale nome e' il puntatore
 * (lo accende) e a quale mandare il clic. */
#define MAX_NOMI_PERCORSO 8   /* le cartelle che il percorso sa mostrare */

struct percorso;
typedef void (*al_clic_percorso)(struct percorso *p, int indice, void *dati);

struct nome_percorso {
    struct widget base;
    char nome[MAX_TESTO_WIDGET];
    int indice;
    int premuto, dentro;
};

struct percorso {
    struct widget base;
    struct nome_percorso nomi[MAX_NOMI_PERCORSO];
    int quanti;
    al_clic_percorso al_clic;
    void *dati;
};

static inline const struct font *percorso_font(const struct percorso *p, int i) {
    return i == p->quanti - 1 ? FONT_CORPO_MEDIO : FONT_CORPO;
}

static inline int percorso_largo(const struct percorso *p, int i) {
    return larghezza_testo(percorso_font(p, i), p->nomi[i].nome);
}

/* Il separatore tra un nome e l'altro, e quanto e' largo. */
#define PERCORSO_SEPARATORE "/"
static inline int percorso_separatore(void) { return larghezza_testo(FONT_CORPO, PERCORSO_SEPARATORE); }

/* Al minimo: il nome dell'ultima cartella (ma non piu' di
 * PERCORSO_ULTIMO_MINIMO: un nome lunghissimo si taglia, invece di spingere fuori dalla
 * barra le cose dopo di lui). Il resto viene dall'elastico. */
#define PERCORSO_ULTIMO_MINIMO 120

static inline void percorso_misura(struct widget *w, int *l, int *a) {
    struct percorso *p = (struct percorso *)w;
    int ultimo = p->quanti ? percorso_largo(p, p->quanti - 1) : 0;
    if (ultimo > PERCORSO_ULTIMO_MINIMO) ultimo = PERCORSO_ULTIMO_MINIMO;
    *l = ultimo;
    *a = ALTEZZA_S;
}

/* Da destra a sinistra: si mettono i nomi finche' ci stanno. (Il primo nome non ha uno
 * spazio davanti: l'inizio lo comprende gia'.) */
static inline void percorso_disponi(struct widget *w) {
    struct percorso *p = (struct percorso *)w;
    int resta = w->l + PERCORSO_SPAZIO, primo = p->quanti;
    for (int i = p->quanti - 1; i >= 0; i--) {
        int serve = PERCORSO_SPAZIO + percorso_largo(p, i) +
                    (i < p->quanti - 1 ? percorso_separatore() + PERCORSO_SPAZIO : 0);
        if (serve > resta && i < p->quanti - 1) break;   /* l'ultimo resta sempre */
        resta -= serve;
        primo = i;
    }
    int x = w->x;
    for (int i = 0; i < p->quanti; i++) {
        struct widget *n = &p->nomi[i].base;
        n->nascosto = i < primo;
        if (n->nascosto) continue;
        int largo = percorso_largo(p, i);
        int fine = w->x + w->l;
        n->x = x;
        n->y = w->y;
        n->l = x + largo > fine ? fine - x : largo;      /* l'ultimo, se non ci sta, si taglia */
        n->a = w->a;
        x += largo + PERCORSO_SPAZIO + percorso_separatore() + PERCORSO_SPAZIO;
    }
}

/* I separatori; i nomi si disegnano da se'. */
static inline void percorso_disegna(struct widget *w, struct pennello *p) {
    struct percorso *pc = (struct percorso *)w;
    pennello_colore(p, TESTO_SECONDARIO);
    for (int i = 0; i < pc->quanti - 1; i++) {
        struct widget *n = &pc->nomi[i].base;
        if (n->nascosto) continue;
        testo_centrato(p, FONT_CORPO, n->x + n->l + PERCORSO_SPAZIO, w->y, w->a, PERCORSO_SEPARATORE);
    }
}

static const struct tipo_widget tipo_percorso = {
    percorso_misura, percorso_disponi, percorso_disegna, 0, 0, 0, 0, 0, 0
};

static inline void nome_percorso_disegna(struct widget *w, struct pennello *p) {
    struct nome_percorso *n = (struct nome_percorso *)w;
    struct percorso *pc = (struct percorso *)w->genitore;
    int qui = n->indice == pc->quanti - 1;
    pennello_salva(p);
    pennello_ritaglia(p, w->x, w->y, w->l, w->a);       /* l'ultimo puo' essere tagliato */
    pennello_colore(p, qui || w->sopra ? TESTO_PRIMARIO : TESTO_SECONDARIO);
    const struct font *f = percorso_font(pc, n->indice);
    const char *testo = n->nome;
    char tagliato[MAX_TESTO_WIDGET + 4];
    if (larghezza_testo(f, n->nome) > w->l) {            /* non ci sta: finisce con tre punti (il font non ha il carattere dei puntini) */
        int lungo = 0;                                   /* quante lettere del nome stanno, con i puntini */
        for (int k = 0; n->nome[k]; k++) {
            if (((unsigned char)n->nome[k] & 0xC0) == 0x80) continue;   /* in mezzo a una lettera UTF-8 */
            for (int j = 0; j < k; j++) tagliato[j] = n->nome[j];
            tagliato[k] = '.'; tagliato[k + 1] = '.'; tagliato[k + 2] = '.'; tagliato[k + 3] = 0;
            if (larghezza_testo(f, tagliato) > w->l) break;
            lungo = k;
        }
        for (int j = 0; j < lungo; j++) tagliato[j] = n->nome[j];
        tagliato[lungo] = '.'; tagliato[lungo + 1] = '.'; tagliato[lungo + 2] = '.'; tagliato[lungo + 3] = 0;
        testo = tagliato;
    }
    testo_centrato(p, f, w->x, w->y, w->a, testo);
    pennello_ripristina(p);
}

static inline void nome_percorso_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    struct nome_percorso *n = (struct nome_percorso *)w;
    struct percorso *pc = (struct percorso *)w->genitore;
    if (pressione(w, &n->premuto, &n->dentro, x, y, pulsanti) && pc->al_clic)
        pc->al_clic(pc, n->indice, pc->dati);
}

/* Due tipi: i nomi che si cliccano, e l'ultimo (dove si e') che non sente il puntatore. */
static const struct tipo_widget tipo_nome_percorso = {
    0, 0, nome_percorso_disegna, nome_percorso_puntatore, 0, 0, 0, 0, 0
};
static const struct tipo_widget tipo_nome_percorso_qui = {
    0, 0, nome_percorso_disegna, 0, 0, 0, 0, 0, 0
};

static inline void percorso_prepara(struct percorso *p, al_clic_percorso al_clic, void *dati) {
    *p = (struct percorso){ .base = { .tipo = &tipo_percorso, .elastico = 1 },
                            .al_clic = al_clic, .dati = dati };
}

/* Cambia il percorso: 'quanti' nomi, dal piu' esterno a quello in cui si e'. Si puo'
 * chiamare anche con la finestra aperta (per esempio dopo un clic su un nome). */
static inline void percorso_metti(struct percorso *p, const char *const *nomi, int quanti) {
    if (quanti > MAX_NOMI_PERCORSO) quanti = MAX_NOMI_PERCORSO;
    p->base.quanti_figli = 0;
    p->quanti = quanti;
    for (int i = 0; i < quanti; i++) {
        struct nome_percorso *n = &p->nomi[i];
        *n = (struct nome_percorso){ .base = { .tipo = i == quanti - 1 ? &tipo_nome_percorso_qui
                                                                       : &tipo_nome_percorso },
                                     .indice = i };
        copia_testo(n->nome, nomi[i], MAX_TESTO_WIDGET);
        widget_aggiungi(&p->base, &n->base);
    }
    struct finestra_tk *f = widget_finestra(&p->base);
    if (f->base.tipo == &tipo_finestra_tk) {   /* gia' in una finestra: si rifa' il suo posto */
        /* i nomi sono stati rifatti da capo (spenti): UIKit deve riaccendere quello
         * sotto il puntatore. (Il widget catturato, se era un nome, resta valido: i nomi
         * stanno sempre negli stessi posti.) */
        if (f->sotto && f->sotto->genitore == &p->base) f->sotto = 0;
        percorso_disponi(&p->base);
        widget_cambiato(&p->base);
    }
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
 *   - quando si scorre compare (in 100 ms): il cursore, sottile (8 pixel), da solo, senza
 *     binario; resta un secondo dopo l'ultimo scatto, poi svanisce (in 300 ms);
 *   - con il puntatore sopra, o mentre la si trascina, dietro al cursore compare (in 150 ms)
 *     il binario da 12 pixel, incassato; il cursore resta dov'e', con 2 pixel di margine per
 *     lato. Finche' il puntatore e' sopra non svanisce.
 * Il cursore e' lungo quanto la parte che si vede, in proporzione (almeno SCORRIMENTO_CURSORE_LUNGHEZZA, 64 pixel), e sta
 * dove si e' nel contenuto. Trascinandolo il contenuto scorre; un clic sul binario porta
 * li' il centro del cursore. Se il contenuto ci sta tutto, la barra non compare mai.
 *
 * LE ANIMAZIONI. A ogni passo si guarda quanto tempo e' passato, e opacita' e spessore si
 * avvicinano al loro obiettivo di conseguenza: anche se un passo arriva in ritardo,
 * l'animazione dura quanto deve. I passi li fa una chiamata "ogni tanto" di UIKit, che
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
    int spessore;                      /* quanto si vede il binario: 0 niente, 255 tutto */
    unsigned int ultimo;               /* tempo() dell'ultimo scatto o trascinamento */
    unsigned int passo_prima;          /* tempo() del passo di animazione di prima */
    unsigned int attesa;               /* ogni quanti ms la si chiama ora (0: non la si chiama) */
    int trascinata;                    /* il cursore e' preso con il mouse */
    int presa;                         /* dove lo si e' preso, dal suo inizio */
    int fuori;                         /* gli ultimi pixel a destra del widget che scorre sono
                                        * il padding del contenitore (barra_scorrimento_riserva) */
};

/* Il cursore: dove comincia (y) e quanto e' lungo. */
static inline void barra_scorrimento_cursore(const struct barra_scorrimento *b, int *y, int *lungo) {
    const struct scorrimento *s = &b->scorrimento;
    int corsa = b->base.a - 2 * SCORRIMENTO_CURSORE_MARGINE;     /* dove il cursore si muove */
    int l = s->totale > 0 ? (int)((long long)corsa * s->vista / s->totale) : corsa;
    if (l < SCORRIMENTO_CURSORE_LUNGHEZZA) l = SCORRIMENTO_CURSORE_LUNGHEZZA;
    if (l > corsa) l = corsa;
    int massimo = scorrimento_massimo(s);
    *lungo = l;
    *y = b->base.y + SCORRIMENTO_CURSORE_MARGINE +
         (massimo ? (int)((long long)(corsa - l) * s->posizione / massimo) : 0);
}

/* Come si disegna, dal componente "Barra di scorrimento" del Figma. Il CURSORE e' una pillola
 * di SCORRIMENTO_CURSORE_NUOVO (8) pixel, al centro della striscia di 14, nel colore
 * SCORRIMENTO_CURSORE (a riposo), _SOPRA (puntatore sopra la barra) o _TRASCINATO_NUOVO
 * (preso col mouse), con l'ombra "sollevato" ridotta a un velo (e' un pezzo che si sposta,
 * quindi SOLLEVATO, ma deve restare leggerissima: e' sottile). Il BINARIO e' INCASSATO (riceve
 * il cursore): una pillola di SCORRIMENTO_BINARIO (12) per tutta la lunghezza, fondo
 * SUPERFICIE_INCASSATA con l'ombra "incassato", senza bordo. Il cursore sta dentro con
 * SCORRIMENTO_CURSORE_MARGINE (2) per lato: 2 + 8 + 2 = 12, e' lo stesso posto di prima,
 * quindi quando il binario compare il cursore non si muove. Compare (b->spessore, 0-255, la
 * stessa animazione di prima) quando il puntatore e' sopra o si trascina; mentre si scorre
 * con la rotella c'e' SOLO il cursore. */
static inline void barra_scorrimento_disegna(struct widget *w, struct pennello *p) {
    struct barra_scorrimento *b = (struct barra_scorrimento *)w;
    if (!b->opacita || !scorrimento_massimo(&b->scorrimento)) return;
    pennello_opacita(p, b->opacita);
    if (b->spessore) {                 /* il binario: tutta la striscia, compare sfumando */
        pennello_salva(p);
        pennello_opacita(p, b->spessore);
        struct tondo binario = tondo(w->x + (w->l - SCORRIMENTO_BINARIO) / 2, w->y,
                                     SCORRIMENTO_BINARIO, w->a, SCORRIMENTO_BINARIO / 2);
        ombre_strati_fuori(p, &binario, OMBRA_TAB(incassato), OMBRA_N(incassato));
        pennello_colore(p, SUPERFICIE_INCASSATA);
        riempi_forma(p, &binario);
        ombre_strati_dentro(p, &binario, OMBRA_TAB(incassato), OMBRA_N(incassato));
        pennello_ripristina(p);
    }
    int y, lungo;
    barra_scorrimento_cursore(b, &y, &lungo);
    struct tondo cursore = tondo(w->x + (w->l - SCORRIMENTO_CURSORE_NUOVO) / 2, y,
                                 SCORRIMENTO_CURSORE_NUOVO, lungo, SCORRIMENTO_CURSORE_NUOVO / 2);
    pennello_salva(p);
    pennello_opacita(p, 90);           /* l'ombra "sollevato" ridotta a un velo */
    ombre_strati_fuori(p, &cursore, OMBRA_TAB(sollevato), OMBRA_N(sollevato));
    pennello_ripristina(p);
    pennello_colore(p, b->trascinata ? SCORRIMENTO_CURSORE_TRASCINATO_NUOVO
                                     : b->base.sopra ? SCORRIMENTO_CURSORE_SOPRA : SCORRIMENTO_CURSORE);
    riempi_forma(p, &cursore);
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
        widget_nascondi(&b->base, !opacita);   /* nascosta non si disegna e non prende i clic */
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
    if (spostato) {                    /* si e' mosso il contenuto: tutto il widget che scorre */
        widget_cambiato(&b->base);
        if (b->base.genitore) widget_cambiato(b->base.genitore);
    }
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
    int corsa = b->base.a - 2 * SCORRIMENTO_CURSORE_MARGINE - lungo;
    if (corsa <= 0) return;
    int da_inizio = y - b->presa - (b->base.y + SCORRIMENTO_CURSORE_MARGINE);
    int posizione = (int)((long long)da_inizio * scorrimento_massimo(&b->scorrimento) / corsa);
    barra_scorrimento_usata(b, scorrimento_metti(&b->scorrimento, posizione));
}

/* Premuto sul cursore: lo si prende li'. Premuto sul binario: il centro del cursore
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
    barra_scorrimento_su_rotella, barra_scorrimento_sopra, 0, 0
};

static inline void barra_scorrimento_prepara(struct barra_scorrimento *b) {
    *b = (struct barra_scorrimento){ .base = { .tipo = &tipo_barra_scorrimento, .nascosto = 1 } };
}

/* LA CORSIA DELLA BARRA: la barra non va mai sopra il contenuto. Sta nella striscia di
 * SCORRIMENTO_ZONA (14) lungo il bordo destro del widget che scorre, e quella striscia deve
 * essere vuota: e' il padding a destra.
 *   - 'fuori': se il widget sta in una pagina con un margine, la pagina gli allarga lo spazio
 *     fino al proprio bordo e gli dice quanto (pagina_disponi): quei pixel sono il padding del
 *     contenitore, il contenuto non li usa mai e la barra ci sta dentro (se sono almeno 14);
 *   - 'libero': il padding che il contenuto ha gia' a destra, suo (la griglia: 8);
 *   - se il contenuto scorre e i due insieme non bastano, il contenuto lascia libero quello
 *     che manca: il padding a destra diventa di 14, e li' sta la barra.
 * Dice quanti pixel a destra del widget il contenuto non usa. Se il contenuto ci sta tutto
 * (la barra non c'e') resta solo 'fuori': il padding e' quello di sempre. */
static inline int barra_scorrimento_riserva(const struct barra_scorrimento *b, int libero) {
    int manca = SCORRIMENTO_ZONA - b->fuori - libero;
    return b->fuori + (scorrimento_massimo(&b->scorrimento) && manca > 0 ? manca : 0);
}

/* La vista del contenuto e' il rettangolo (x, y, l, a): la barra va lungo il suo bordo
 * destro, nella corsia (barra_scorrimento_riserva). Da chiamare quando il widget che scorre
 * viene disposto. */
static inline void barra_scorrimento_posto(struct barra_scorrimento *b, int x, int y, int l, int a) {
    b->base.x = x + l - SCORRIMENTO_ZONA;
    b->base.y = y;
    b->base.l = SCORRIMENTO_ZONA;
    b->base.a = a;
}

/* --- il ciclo degli eventi --- */

/* Disegna tutta la finestra: l'albero e, sopra a tutto, l'avviso aperto (velo, ombra,
 * riquadro). La usa anche l'anteprima (strumenti/anteprima). */
static inline void uikit_disegna_tutto(struct finestra_tk *f, struct pennello *p) {
    widget_disegna(&f->base, p);
    struct avviso *v = finestra_tk_avviso_aperto(f);
    if (v) {
        avviso_sotto(v, p, f->base.l, f->base.a);
        widget_disegna(&v->base, p);
    }
}

/* Ridisegna le zone cambiate e le mostra. L'immagine libera e' quella di due fotogrammi fa:
 * oltre alle zone cambiate adesso si rifanno quelle che le mancano (finestra_arretrate).
 * Ogni zona si ridisegna da capo, con il pennello ritagliato li': il fondo della finestra
 * comincia pulendo (pulisci), quindi rifare una zona da' gli stessi pixel di un ridisegno
 * intero. Al compositore si dicono solo le zone cambiate adesso. */
static inline int uikit_ridisegna(struct finestra_tk *f) {
    if (!f->tutta_cambiata && !f->quante_cambiate) { f->da_ridisegnare = 0; return 1; }
    unsigned int *pixel = finestra_tela(&f->finestra);
    if (!pixel) return 0;
    struct tela t = { pixel, f->base.l, f->base.a };
    struct pennello p;
    struct zona_finestra zone[MAX_ZONE_MOSTRA];
    int n = finestra_arretrate(&f->finestra, zone);
    int tutta = f->tutta_cambiata || n < 0;
    for (int k = 0; !tutta && k < f->quante_cambiate; k++)
        zone_aggiungi(zone, &n, MAX_ZONE_MOSTRA, f->cambiate[k]);
    if (tutta) {
        pennello_prepara(&p, &t);
        uikit_disegna_tutto(f, &p);
    } else {
        for (int k = 0; k < n; k++) {
            pennello_prepara(&p, &t);
            pennello_ritaglia(&p, zone[k].x, zone[k].y, zone[k].l, zone[k].a);
            uikit_disegna_tutto(f, &p);
        }
    }
    int fatto = (f->tutta_cambiata ? finestra_mostra(&f->finestra)
                                   : finestra_mostra_zone(&f->finestra, f->cambiate, f->quante_cambiate)) >= 0;
    f->da_ridisegnare = f->tutta_cambiata = f->quante_cambiate = 0;
    return fatto;
}

/* Il puntatore sopra un widget nuovo (o su nessuno): si spegne il vecchio, si accende
 * il nuovo. */
static inline void uikit_sotto(struct finestra_tk *f, struct widget *w) {
    if (w == f->sotto) return;
    struct widget *prima = f->sotto;
    /* un widget che segue il puntatore pezzo per pezzo (ha 'movimento': una tabella, una
     * griglia, un menu) accende le sue parti e ridisegna solo quelle: tutto il widget no */
    if (prima) { prima->sopra = 0; if (!prima->tipo->movimento) widget_cambiato(prima); }
    f->sotto = w;
    if (w) { w->sopra = 1; if (!w->tipo->movimento) widget_cambiato(w); }
    if (prima && prima->tipo->sopra_cambiato) prima->tipo->sopra_cambiato(prima);
    if (w && w->tipo->sopra_cambiato) w->tipo->sopra_cambiato(w);
}

/* Il widget sotto il puntatore, con l'avviso: prima i suoi pulsanti; se e' bloccante, fuori
 * dal riquadro non c'e' niente. */
static inline struct widget *uikit_widget_sotto(struct finestra_tk *f, int x, int y) {
    struct avviso *v = finestra_tk_avviso_aperto(f);
    if (v) {
        struct widget *w = widget_sotto(&v->base, x, y);
        if (w || v->bloccante) return w;
    }
    return widget_sotto(&f->base, x, y);
}

/* Il widget piu' interno sotto (x, y) che vuole la rotella, o 0. Come widget_sotto: un widget
 * messo sopra agli altri che la rotella non la vuole (l'avviso in basso di Files, grande
 * quanto il pannello ma disegnato solo in fondo) non la ferma, passa a quelli sotto. */
static inline struct widget *widget_rotella_sotto(struct widget *w, int x, int y) {
    if (w->disabilitato || w->nascosto || !widget_contiene(w, x, y)) return 0;
    for (int i = w->quanti_figli - 1; i >= 0; i--) {
        struct widget *trovato = widget_rotella_sotto(w->figli[i], x, y);
        if (trovato) return trovato;
    }
    return w->tipo->rotella ? w : 0;
}

/* La rotella: al widget piu' interno sotto il puntatore che la vuole. */
static inline void uikit_rotella(struct finestra_tk *f, int x, int y, int sx, int sy) {
    if (y < testata_altezza(f) || finestra_tk_bloccata(f)) return;
    struct widget *w = widget_rotella_sotto(&f->base, x, y);
    if (w) w->tipo->rotella(w, sx, sy);
}

static inline void uikit_fuoco(struct finestra_tk *f, struct widget *w) {
    if (w == f->con_fuoco) return;
    if (f->con_fuoco) { f->con_fuoco->fuoco = 0; widget_cambiato(f->con_fuoco); }
    f->con_fuoco = w;
    if (w) { w->fuoco = 1; widget_cambiato(w); }
}

/* Il puntatore: un clic su "chiudi" chiude; sulla barra (o sul vuoto della barra degli
 * strumenti) e' un "trascinami"; nel contenuto e tra gli strumenti cattura il widget
 * sotto, che sentira' il puntatore finche' il pulsante non si lascia, e se prende i
 * tasti gli da' il fuoco.
 *
 * IL TASTO DESTRO cattura il widget sotto come il sinistro (e gli da' il fuoco): sara'
 * lui, guardando PULSANTE_DESTRO in 'pulsanti', a decidere cosa farne (di solito, un
 * menu). Barra del titolo, "trascinami", riduci e chiudi restano del solo sinistro. I
 * widget che guardano solo il sinistro, al destro non fanno niente.
 *
 * Senza pulsanti premuti, il widget sotto il puntatore che lo vuole sapere riceve il
 * MOVIMENTO (il campo 'movimento' del suo tipo). */
static inline void uikit_puntatore(struct finestra_tk *f, int x, int y, unsigned int pulsanti) {
    const unsigned int cattura = PULSANTE_SINISTRO | PULSANTE_DESTRO;
    int giu = pulsanti & cattura;
    int appena_premuto = giu && !(f->pulsanti_prima & cattura);
    int sinistro = pulsanti & PULSANTE_SINISTRO;
    f->pulsanti_prima = pulsanti;

    /* riduci e chiudi si accendono sotto il puntatore (non mentre si tiene premuto
     * altrove, su un widget) */
    int bloccata = finestra_tk_bloccata(f);   /* un avviso bloccante: niente comandi ne' trascinami */
    int comando = f->catturato || bloccata ? 0 : su_comando(f, 1, x, y) ? 2 : su_comando(f, 0, x, y) ? 1 : 0;
    if (comando != f->comando_sopra) { f->comando_sopra = comando; uikit_cambiata_testata(f); }

    if (appena_premuto) {
        if (comando == 2 && sinistro) { f->chiudi_premuto = 1; uikit_cambiata_testata(f); return; }
        if (comando == 1 && sinistro) { f->riduci_premuto = 1; uikit_cambiata_testata(f); return; }
        /* la barra del titolo, e lo spazio vuoto della barra degli strumenti: "trascinami" */
        if (!bloccata && (y < testata_altezza(f) || barra_strumenti_vuota(f, x, y))) {
            if (sinistro) finestra_trascinami(&f->finestra);
            return;
        }
        f->catturato = uikit_widget_sotto(f, x, y);
        uikit_fuoco(f, f->catturato && f->catturato->tipo->tasto ? f->catturato : 0);
    }
    if (f->chiudi_premuto) {
        if (!sinistro) {
            f->chiudi_premuto = 0;
            uikit_cambiata_testata(f);
            if (su_comando(f, 1, x, y)) f->da_chiudere = 1;
        }
        return;
    }
    if (f->riduci_premuto) {                /* scatta quando si lascia, se si e' ancora sopra */
        if (!sinistro) {
            f->riduci_premuto = 0;
            uikit_cambiata_testata(f);
            if (su_comando(f, 0, x, y)) finestra_riducimi(&f->finestra);
        }
        return;
    }
    if (f->catturato) {
        f->catturato->tipo->puntatore(f->catturato, x, y, pulsanti);
        uikit_sotto(f, widget_contiene(f->catturato, x, y) ? f->catturato : 0);
        if (giu) return;
        f->catturato = 0;
    }
    uikit_sotto(f, uikit_widget_sotto(f, x, y));
    if (!giu && f->sotto && f->sotto->tipo->movimento) f->sotto->tipo->movimento(f->sotto, x, y);
}

/* Apre l'avviso della finestra, al centro, e gli da' la tastiera (Invio, Esc). */
static inline void avviso_apri(struct avviso *v) {
    struct finestra_tk *f = widget_finestra(&v->base);
    v->base.nascosto = 0;
    v->annulla.premuto = v->conferma.premuto = 0;
    avviso_sistema(v, f->base.l, f->base.a);
    if (f->base.tipo == &tipo_finestra_tk) {
        uikit_sotto(f, 0);
        uikit_fuoco(f, &v->base);
        uikit_cambiata_tutta(f);           /* il velo e l'ombra: tutta la finestra */
    }
}

static inline void avviso_chiudi(struct avviso *v) {
    struct finestra_tk *f = widget_finestra(&v->base);
    v->base.nascosto = 1;
    if (f->base.tipo == &tipo_finestra_tk) {
        if (f->con_fuoco == &v->base || f->con_fuoco == &v->annulla.base ||
            f->con_fuoco == &v->conferma.base) uikit_fuoco(f, 0);
        if (f->sotto == &v->annulla.base || f->sotto == &v->conferma.base) uikit_sotto(f, 0);
        uikit_cambiata_tutta(f);
    }
}

/* --- la tastiera fisica: le scorciatoie (tasti.h) --- */

/* Aggiunge un ascoltatore dei tasti fisici della finestra: lo sentira' prima di quelli
 * aggiunti prima di lui. Di nuovo la stessa funzione con gli stessi dati: niente. */
static inline void finestra_tk_ascolta_tasti(struct finestra_tk *f, ascolta_tasti_tk funzione, void *dati) {
    for (int i = 0; i < MAX_ASCOLTATORI; i++)
        if (f->ascoltatori[i].funzione == funzione && f->ascoltatori[i].dati == dati) return;
    for (int i = 0; i < MAX_ASCOLTATORI; i++)
        if (!f->ascoltatori[i].funzione) { f->ascoltatori[i] = (struct ascoltatore_tk){ funzione, dati }; return; }
}

static inline void finestra_tk_smetti_tasti(struct finestra_tk *f, ascolta_tasti_tk funzione, void *dati) {
    for (int i = 0; i < MAX_ASCOLTATORI; i++)
        if (f->ascoltatori[i].funzione == funzione && f->ascoltatori[i].dati == dati) {
            for (int k = i; k < MAX_ASCOLTATORI - 1; k++) f->ascoltatori[k] = f->ascoltatori[k + 1];
            f->ascoltatori[MAX_ASCOLTATORI - 1] = (struct ascoltatore_tk){ 0, 0 };
            return;
        }
}

/* Un tasto fisico premuto o lasciato: i modificatori si contano; un tasto premuto va agli
 * ascoltatori, dall'ultimo al primo, finche' uno lo usa. */
static inline void uikit_tasto_fisico(struct finestra_tk *f, unsigned int codice, int premuto) {
    unsigned int mod = codice == TASTO_CTRL || codice == TASTO_CTRL_DX ? MOD_CTRL
                     : codice == TASTO_MAIUSC || codice == TASTO_MAIUSC_DX ? MOD_MAIUSC
                     : codice == TASTO_ALT ? MOD_ALT : 0;
    if (mod) {
        if (premuto) f->modificatori |= mod; else f->modificatori &= ~mod;
        return;
    }
    if (!premuto || finestra_tk_bloccata(f)) return;
    for (int i = MAX_ASCOLTATORI - 1; i >= 0; i--) {
        struct ascoltatore_tk a = f->ascoltatori[i];
        if (a.funzione && a.funzione(f, codice, f->modificatori, a.dati)) return;
    }
}

/* --- l'altra finestra (una mini finestra per le cose secondarie) --- */

static inline void finestra_tk_chiudi_altra(struct finestra_tk *f);

/* Misura 'altra' (preparata come ogni finestra: finestra_tk_prepara, di solito anche
 * finestra_tk_mini), la apre sullo schermo e la fa servire dal ciclo degli eventi di 'f'. Se
 * c'e' gia' un'altra finestra aperta, prima la si chiude. 0 se fatto, -1 se no. */
static inline int finestra_tk_apri_altra(struct finestra_tk *f, struct finestra_tk *altra) {
    if (f->altra) finestra_tk_chiudi_altra(f);
    int l, a;
    widget_misura(&altra->base, &l, &a);
    altra->base.x = altra->base.y = 0;
    altra->base.l = l;
    altra->base.a = a;
    widget_disponi(&altra->base);
    altra->da_chiudere = 0;
    altra->attiva = 1;
    altra->quante_cambiate = 0;
    uikit_cambiata_tutta(altra);
    if (finestra_apri_come(&altra->finestra, l, a, altra->titolo, RUOLO_FINESTRA, 0, 0, RAGGIO_FINESTRA_NUOVO) < 0)
        return -1;
    finestra_tasti_fisici(&altra->finestra);
    altra->aperta = 1;
    f->altra = altra;
    return uikit_ridisegna(altra) ? 0 : -1;
}

/* Chiude l'altra finestra (se c'e'): sparisce dallo schermo, la memoria torna libera. */
static inline void finestra_tk_chiudi_altra(struct finestra_tk *f) {
    struct finestra_tk *a = f->altra;
    if (!a) return;
    f->altra = 0;
    a->aperta = 0;
    chiudi(a->finestra.collegamento);
    if (a->finestra.immagini[0]) togli(a->finestra.immagini[0]);
    chiudi(a->finestra.foglio);
    a->catturato = a->sotto = a->con_fuoco = 0;
    a->modificatori = a->pulsanti_prima = 0;
    a->chiudi_premuto = a->riduci_premuto = a->comando_sopra = 0;
}

/* Una notizia del compositore per la finestra 'f' (la principale o l'altra). */
static inline void uikit_consegna(struct finestra_tk *f, const struct notizia *n) {
    if (n->tipo == PUNTATORE) {
        uikit_puntatore(f, n->x, n->y, n->pulsanti);
    } else if (n->tipo == USCITO) {
        /* il puntatore non e' piu' nostro (per esempio, il compositore ha cominciato
         * a spostare la finestra): una pressione in corso si annulla, senza scattare */
        if (f->catturato) f->catturato->tipo->puntatore(f->catturato, -1, -1, 0);
        f->catturato = 0;
        f->chiudi_premuto = 0;
        f->riduci_premuto = 0;
        f->pulsanti_prima = 0;
        if (f->comando_sopra) { f->comando_sopra = 0; uikit_cambiata_testata(f); }
        uikit_sotto(f, 0);
    } else if (n->tipo == ROTELLA) {
        uikit_rotella(f, n->x, n->y, n->scatti_x, n->scatti_y);
    } else if (n->tipo == TASTO_FISICO) {
        uikit_tasto_fisico(f, n->tasto, n->premuto);
    } else if (n->tipo == TASTO && (f->modificatori & (MOD_CTRL | MOD_ALT))) {
        /* Ctrl+D, Alt+...: e' una scorciatoia, non un carattere da scrivere */
    } else if (n->tipo == TASTO && finestra_tk_bloccata(f)) {
        avviso_tasto(&f->avviso->base, n->tasto);   /* sotto un avviso bloccante: solo lui */
    } else if (n->tipo == TASTO && f->con_fuoco) {
        f->con_fuoco->tipo->tasto(f->con_fuoco, n->tasto);
    } else if (n->tipo == FUOCO) {
        f->attiva = n->fuoco;
        if (!n->fuoco) f->modificatori = 0;    /* i tasti lasciati altrove non li sentiremo */
        uikit_cambiata_testata(f);             /* il fuoco cambia solo il titolo e i comandi */
    }
}

/* Consegna una notizia, ma i movimenti del puntatore li tiene da parte (in 'sospesa'): se
 * subito dopo ne arriva un altro con gli stessi pulsanti, quello prima non serve piu' (le
 * posizioni di mezzo non le vedrebbe nessuno). Le altre notizie passano dopo il movimento
 * tenuto da parte, nel loro ordine. */
struct sospesa_tk {
    struct finestra_tk *da;            /* 0: nessun movimento da parte */
    struct notizia n;
};

static inline void uikit_consegna_sospesa(struct sospesa_tk *s) {
    if (!s->da) return;
    struct finestra_tk *da = s->da;
    s->da = 0;
    uikit_consegna(da, &s->n);
}

static inline void uikit_consegna_unendo(struct sospesa_tk *s, struct finestra_tk *da, const struct notizia *n) {
    if (s->da && (n->tipo != PUNTATORE || s->da != da || s->n.pulsanti != n->pulsanti))
        uikit_consegna_sospesa(s);
    if (n->tipo == PUNTATORE) {
        s->da = da;
        s->n = *n;
    } else {
        uikit_consegna(da, n);
    }
}

/* Apre la finestra e la fa vivere: torna quando si preme "chiudi" (0) o se il
 * compositore la chiude (1). Serve anche l'altra finestra, se c'e' (finestra_tk_apri_altra):
 * quando si chiude lei (la sua X), si chiude solo lei.
 *
 * IL GIRO. (1) Se c'e' qualcosa da ridisegnare e un'immagine libera, si ridisegna. (2) Si
 * aspetta il compositore (su tutte e due le finestre) fino alla prima scadenza dei "chiamami
 * ogni tanto", e poi si serve TUTTO quello che e' arrivato, senza aspettare piu': i clic, i
 * tasti, la rotella, i movimenti del puntatore (dei movimenti uno di fila all'altro conta
 * l'ultimo). (3) Si chiamano i "chiamami ogni tanto" scaduti: l'input passa davanti alle
 * animazioni, come in GTK, cosi' due scatti veloci restano una raffica anche quando
 * ridisegnare costa. Poi da capo: si ridisegna una volta sola per tutto quello che e'
 * successo. Se le immagini sono tutte e due del compositore non le si aspetta: si continua a
 * servire le notizie finche' lui ne restituisce una (RILASCIATA). */
static inline int uikit_esegui(struct finestra_tk *f) {
    int l, a;
    widget_misura(&f->base, &l, &a);
    f->base.x = f->base.y = 0;
    f->base.l = l;
    f->base.a = a;
    widget_disponi(&f->base);
    if (finestra_tk_avviso_aperto(f)) avviso_sistema(f->avviso, l, a);   /* aperto prima di avere una misura */
    if (finestra_apri_come(&f->finestra, l, a, f->titolo, RUOLO_FINESTRA, 0, 0, RAGGIO_FINESTRA_NUOVO) < 0) return 1;
    finestra_tasti_fisici(&f->finestra);        /* per le scorciatoie (tasti.h) */
    f->aperta = 1;
    f->quante_cambiate = 0;
    uikit_cambiata_tutta(f);

    struct notizia n;
    for (;;) {
        if (f->da_ridisegnare && finestra_libera(&f->finestra) && !uikit_ridisegna(f)) return 1;
        if (f->altra && f->altra->da_ridisegnare && finestra_libera(&f->altra->finestra) &&
            !uikit_ridisegna(f->altra))
            finestra_tk_chiudi_altra(f);

        struct sospesa_tk sospesa = { 0 };
        for (int servite = 0; servite < 64; servite++) {
            /* prima le notizie messe da parte da finestra_tela, poi il compositore */
            struct finestra_tk *da = f->finestra.quante ? f : f->altra && f->altra->finestra.quante ? f->altra : 0;
            if (da) {
                finestra_notizia(&da->finestra, &n);
            } else {
                int resta = 0;                    /* gia' servito qualcosa: non si aspetta piu' */
                if (!servite) {
                    struct chiamata_tk *prima = uikit_prima_chiamata(f);
                    resta = prima ? (int)(prima->scadenza - tempo()) : -1;
                    if (prima && resta < 0) resta = 0;
                }
                struct da_guardare g[2] = { { .descrittore = f->finestra.collegamento },
                                            { .descrittore = f->altra ? f->altra->finestra.collegamento : -1 } };
                int pronti = aspetta(g, f->altra ? 2 : 1, resta);
                if (pronti == 0) break;           /* niente (o e' scaduta una chiamata) */
                if (pronti < 0) {                 /* interrotta: si riprova */
                    if (servite) break;
                    servite--;
                    continue;
                }
                da = g[0].pronto || !f->altra ? f : f->altra;
                if (!finestra_ricevi(&da->finestra, &n)) {
                    if (da == f) return 1;
                    if (sospesa.da == da) sospesa.da = 0;
                    finestra_tk_chiudi_altra(f);       /* il compositore ha chiuso l'altra */
                    continue;
                }
                if (finestra_di_servizio(&n)) continue;   /* un'immagine tornata, un "ci sei?" */
            }
            uikit_consegna_unendo(&sospesa, da, &n);
            if (f->da_chiudere || (f->altra && f->altra->da_chiudere)) break;
        }
        uikit_consegna_sospesa(&sospesa);
        uikit_chiamate_scadute(f);
        if (f->altra && f->altra->da_chiudere) finestra_tk_chiudi_altra(f);
        if (f->da_chiudere) return 0;
    }
}

/* I componenti che stanno ognuno nel suo file (vedi LIBRERIE.md). */
#include "oggetti.h"          /* le icone di file e cartelle (l'"Oggetto" di Figma) */
#include "menu.h"             /* il menu: voce, intestazione, separatore, sottomenu */
#include "barra_laterale.h"   /* la barra laterale e le sue voci */
#include "tabella.h"          /* la tabella: intestazioni di colonna con l'ordine, righe */
#include "griglia.h"          /* la griglia di icone: i tile */
#include "pannello.h"         /* il pannello chiaro incassato del contenuto (Files) */
#include "informazioni.h"     /* il pannello delle informazioni, in una mini finestra */
/* i componenti di Attivita' e dello Store */
#include "paragrafo.h"
#include "pagina.h"
#include "schede.h"
#include "grafico.h"
#include "etichetta_stato.h"
#include "messaggio.h"
#include "icona_app.h"
#include "riga_permesso.h"
#include "riga_app.h"

#endif
