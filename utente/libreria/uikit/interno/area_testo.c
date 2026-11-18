/* UIKit, i componenti: l'area di testo. Le dichiarazioni, e come funziona vista da fuori, sono in
 * area_testo.h; qui c'e' il buffer con il buco, l'a capo automatico, il cursore, la selezione, il
 * disegno e il mouse.
 *
 * COSA TIENE IL COMPONENTE. Il testo (UTF-8, in un buffer con il buco), il cursore e il capo fermo
 * della selezione (due numeri di byte), e l'elenco delle righe visive (da dove a dove va ogni riga),
 * che si rifa' tutto a ogni cambiamento di testo o di larghezza: costa quanto il testo, ma e' semplice
 * e basta ai testi di un blocco note. Se un giorno servissero testi enormi, si rifa' solo da dove
 * cambia. */
#include <stdlib.h>
#include <string.h>

#include "uikit/uikit.h"
#include "uikit/interno/nucleo.h"

#define DENTRO AREA_TESTO_DENTRO
#define RIGA   AREA_TESTO_RIGA
#define CASELLA_X AREA_TESTO_CASELLA_X
#define MISURA_LARGHEZZA 480                /* la misura del disegno, se nessuno ne da' un'altra */
#define MISURA_ALTEZZA   240

/* --- il buffer con il buco --- */

static int lung(const struct area_testo *a) { return a->cap - (a->buco_fin - a->buco_ini); }

/* Il byte 'i' del testo (0 <= i < lung), saltando il buco. */
static unsigned char byte_in(const struct area_testo *a, int i) {
    return (unsigned char)a->testo[i < a->buco_ini ? i : i + (a->buco_fin - a->buco_ini)];
}

/* Il buco va a 'pos': i byte tra la vecchia e la nuova posizione passano dall'altra parte. */
static void sposta_buco(struct area_testo *a, int pos) {
    if (pos < a->buco_ini) {
        int d = a->buco_ini - pos;
        memmove(a->testo + a->buco_fin - d, a->testo + pos, (size_t)d);
        a->buco_ini -= d;
        a->buco_fin -= d;
    } else if (pos > a->buco_ini) {
        int d = pos - a->buco_ini;
        memmove(a->testo + a->buco_ini, a->testo + a->buco_fin, (size_t)d);
        a->buco_ini += d;
        a->buco_fin += d;
    }
}

/* Il buco ha posto per almeno 'n' byte; se no il blocco cresce (raddoppia). 0 se la memoria non c'e'. */
static int fai_posto(struct area_testo *a, int n) {
    if (a->buco_fin - a->buco_ini >= n) return 1;
    int dopo = a->cap - a->buco_fin, servono = lung(a) + n + 256;
    int nuova = a->cap ? a->cap : 1024;
    while (nuova < servono) nuova *= 2;
    char *b = malloc((size_t)nuova);
    if (!b) return 0;
    if (a->buco_ini) memcpy(b, a->testo, (size_t)a->buco_ini);
    if (dopo) memcpy(b + nuova - dopo, a->testo + a->buco_fin, (size_t)dopo);
    free(a->testo);
    a->testo = b;
    a->buco_fin = nuova - dopo;
    a->cap = nuova;
    return 1;
}

/* Sostituisce i byte [da, fino) con 'n' byte di 'src'. Non tocca il cursore: lo decide chi chiama.
 * 0 se la memoria non c'e' (allora il testo resta com'era). */
static int modifica(struct area_testo *a, int da, int fino, const char *src, int n) {
    if (!fai_posto(a, n > fino - da ? n - (fino - da) : 0)) return 0;
    sposta_buco(a, da);
    a->buco_fin += fino - da;              /* i byte cancellati finiscono nel buco */
    if (n) { memcpy(a->testo + a->buco_ini, src, (size_t)n); a->buco_ini += n; }
    return 1;
}

/* Il carattere che comincia al byte 'i': ne da' il numero Unicode e quanti byte e'. 0 alla fine. */
static int carattere_in(const struct area_testo *a, int i, unsigned int *cp) {
    int n = lung(a);
    if (i >= n) { *cp = 0; return 0; }
    unsigned char c = byte_in(a, i);
    if (c < 0x80) { *cp = c; return 1; }
    char t[5];
    int m = n - i < 4 ? n - i : 4;
    for (int k = 0; k < m; k++) t[k] = (char)byte_in(a, i + k);
    t[m] = 0;
    return utf8_leggi(t, cp);
}

/* Dove comincia il carattere che finisce al byte 'i'. */
static int prima_di(const struct area_testo *a, int i) {
    if (i <= 0) return 0;
    char t[5];
    int m = i < 4 ? i : 4;
    for (int k = 0; k < m; k++) t[k] = (char)byte_in(a, i - m + k);
    t[m] = 0;
    return i - m + utf8_prima(t, m);
}

/* --- le righe visive: l'a capo automatico --- */

/* Quante caselle occupa un carattere che comincia alla colonna 'col': la tabulazione arriva alla
 * prossima colonna multipla di AREA_TESTO_TAB. */
static int larghezza_di(unsigned int cp, int col) {
    return cp == '\t' ? AREA_TESTO_TAB - col % AREA_TESTO_TAB : 1;
}

/* Quante colonne ci stanno in 'larghezza' pixel. */
static int colonne_in(int larghezza) {
    int k = 0;
    while (CASELLA_X(k + 1) <= larghezza && k < 1000) k++;
    return k < 1 ? 1 : k;
}

static int aggiungi_riga(struct area_testo *a, int ini, int fine, int segno) {
    if (a->n_righe == a->cap_righe) {
        int nuove = a->cap_righe ? 2 * a->cap_righe : 64;
        struct riga_area_testo *r = realloc(a->righe, (size_t)nuove * sizeof *r);
        if (!r) return 0;
        a->righe = r;
        a->cap_righe = nuove;
    }
    a->righe[a->n_righe++] = (struct riga_area_testo){ ini, fine, segno };
    return 1;
}

/* L'altezza di tutto il testo, con il padding sopra e sotto: quanto scorre la barra. */
static void aggiorna_scorrimento(struct area_testo *a) {
    scorrimento_misure(&a->barra.scorrimento, a->n_righe * RIGA + 2 * DENTRO, a->base.a);
}

/* Rifa' l'elenco delle righe. Dentro ogni riga del testo (tra due '\n') si conta la colonna, e quando
 * un carattere che non e' uno spazio non ci sta piu' si va a capo A CARATTERE: la riga si riempie fino
 * all'ultima colonna. Se l'a capo cade in mezzo a una parola (il carattere prima e quello dopo non sono
 * spazio ne' tabulazione) l'ultima casella della riga e' il segno "_": la riga ha una lettera in meno
 * (quella che non ci stava col segno passa alla riga dopo) e 'segno' e' 1. Il segno si disegna
 * soltanto: non e' nel testo. Gli spazi non vanno mai a capo: restano in fondo alla riga.
 * Ci sono sempre almeno una riga. */
static void rifai_righe(struct area_testo *a) {
    int n = lung(a), cols = a->colonne < 1 ? 1 : a->colonne;
    a->n_righe = 0;
    int ini = 0;
    for (;;) {
        int i = ini, col = 0, prima = -1;          /* 'prima': dove comincia il carattere prima di i */
        while (i < n) {
            unsigned int cp;
            int len = carattere_in(a, i, &cp);
            if (cp == '\n') break;
            int spazio = cp == ' ' || cp == '\t';
            int w = larghezza_di(cp, col);
            if (!spazio && col > 0 && col + w > cols) {
                unsigned int cq = 0;
                if (prima >= 0) carattere_in(a, prima, &cq);
                int segno = prima > ini && cq != ' ' && cq != '\t';
                int fine = segno ? prima : i;
                if (!aggiungi_riga(a, ini, fine, segno)) return;
                ini = i = fine;
                col = 0;
                prima = -1;
                continue;
            }
            col += w;
            prima = i;
            i += len;
        }
        if (!aggiungi_riga(a, ini, i, 0)) return;
        if (i >= n) break;
        ini = i + 1;                       /* salta il '\n' */
    }
    aggiorna_scorrimento(a);
}

/* La riga in cui sta il cursore: l'ultima che comincia non dopo di lui; ma se il cursore e' a "fine
 * riga" (a capo automatico, End, un clic dopo l'ultima lettera) sta in fondo alla riga di sopra. */
static int riga_del_cursore(const struct area_testo *a) {
    int lo = 0, hi = a->n_righe - 1;
    while (lo < hi) {
        int m = (lo + hi + 1) / 2;
        if (a->righe[m].ini <= a->cursore) lo = m; else hi = m - 1;
    }
    if (a->a_fine_riga && lo > 0 && a->righe[lo].ini == a->cursore && a->righe[lo - 1].fine == a->cursore) lo--;
    return lo;
}

/* La colonna del byte 'off' nella riga 'li'. */
static int colonna_in(const struct area_testo *a, int li, int off) {
    int col = 0;
    for (int i = a->righe[li].ini; i < off;) {
        unsigned int cp;
        int len = carattere_in(a, i, &cp);
        if (len <= 0) break;
        col += larghezza_di(cp, col);
        i += len;
    }
    return col;
}

/* Il byte della riga 'li' piu' vicino alla colonna 'col' (in fondo alla riga se la riga e' piu' corta;
 * dentro una tabulazione, dal lato piu' vicino). */
static int offset_a_colonna(const struct area_testo *a, int li, int col) {
    int c = 0;
    for (int i = a->righe[li].ini; i < a->righe[li].fine;) {
        unsigned int cp;
        int len = carattere_in(a, i, &cp);
        if (len <= 0) break;
        int w = larghezza_di(cp, c);
        if (c + w > col) return (col - c) * 2 >= w ? i + len : i;
        c += w;
        i += len;
    }
    /* in fondo a una riga con il segno il cursore non va oltre l'ultima lettera (oltre sarebbe la riga dopo) */
    return a->righe[li].segno && a->righe[li].fine > a->righe[li].ini ? prima_di(a, a->righe[li].fine) : a->righe[li].fine;
}

/* --- il cursore e la selezione --- */

static int minimo(int x, int y) { return x < y ? x : y; }
static int massimo(int x, int y) { return x > y ? x : y; }

/* La vista segue il cursore: la sua riga sta tutta dentro, a DENTRO dal bordo se si puo'. */
static void assicura_visibile(struct area_testo *a) {
    if (a->base.a <= 0) return;
    int li = riga_del_cursore(a);
    int sopra = li * RIGA, sotto = DENTRO + (li + 1) * RIGA + DENTRO - a->base.a;
    int *pos = &a->barra.scorrimento.posizione, prima = *pos;
    if (*pos > sopra) *pos = sopra;
    if (*pos < sotto) *pos = sotto;
    scorrimento_misure(&a->barra.scorrimento, a->n_righe * RIGA + 2 * DENTRO, a->base.a);   /* tiene la posizione nei limiti */
    if (*pos != prima) barra_scorrimento_mostra(&a->barra);   /* scorre da solo: la barra si fa vedere */
}

/* Il testo, il cursore o la selezione sono cambiati: righe, vista e ridisegno. */
static void cambiato(struct area_testo *a) {
    assicura_visibile(a);
    widget_cambiato(&a->base);
}

/* Il testo e' cambiato: le righe si rifanno. */
static void testo_cambiato(struct area_testo *a) {
    rifai_righe(a);
    cambiato(a);
}

/* La persona ha cambiato il testo scrivendo o cancellando: lo dice all'app (area_testo_al_cambiato).
 * area_testo_imposta non passa di qui: il testo messo dall'app non e' un cambiamento "della persona". */
static void avvisa_cambiato(struct area_testo *a) {
    if (a->al_cambiato) a->al_cambiato(a, a->dati_cambiato);
}

/* Il cursore va a 'off'. Senza 'estendi' anche il capo fermo (nessuna selezione). 'riga' e' la riga in
 * cui lo si e' portato (-1 se non conta): se 'off' e' la fine di una riga a capo automatico, il
 * cursore resta in fondo a quella e non salta all'inizio della prossima. */
static void metti_cursore(struct area_testo *a, int off, int estendi, int riga) {
    a->cursore = off;
    if (!estendi) a->ancora = off;
    a->a_fine_riga = riga >= 0 && riga + 1 < a->n_righe && !a->righe[riga].segno &&
                     a->righe[riga].fine == off && a->righe[riga + 1].ini == off;   /* sul segno "_" il cursore non si ferma */
}

/* Su o giu' di 'quante' righe, alla colonna voluta. Oltre la prima riga si va all'inizio del testo,
 * oltre l'ultima alla fine. */
static void muovi_verticale(struct area_testo *a, int quante, int estendi) {
    int li = riga_del_cursore(a);
    if (a->voluta < 0) a->voluta = colonna_in(a, li, a->cursore);
    int nuova = li + quante;
    if (nuova < 0) { metti_cursore(a, 0, estendi, 0); return; }
    if (nuova >= a->n_righe) { metti_cursore(a, lung(a), estendi, a->n_righe - 1); return; }
    metti_cursore(a, offset_a_colonna(a, nuova, a->voluta), estendi, nuova);
}

/* Le tre classi per andare di parola in parola: spazi, lettere e cifre (anche accentate), il resto. */
static int classe(unsigned int cp) {
    if (cp == ' ' || cp == '\t' || cp == '\n' || cp == 0xA0) return 0;
    if ((cp >= '0' && cp <= '9') || (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || cp == '_' ||
        (cp >= 0xC0 && cp != 0xD7 && cp != 0xF7 && !(cp >= 0x2000 && cp <= 0x2BFF)))
        return 1;
    return 2;
}

/* Ctrl+destra: la fine della parola (o dei segni) in cui si e', poi gli spazi: si arriva all'inizio
 * della prossima. Un a capo si salta da solo. */
static int parola_dopo(const struct area_testo *a, int off) {
    int n = lung(a);
    unsigned int cp;
    int len = carattere_in(a, off, &cp);
    if (!len) return n;
    if (cp == '\n') return off + len;
    int c = classe(cp);
    while (c && off < n) {
        len = carattere_in(a, off, &cp);
        if (cp == '\n' || classe(cp) != c) break;
        off += len;
    }
    while (off < n) {
        len = carattere_in(a, off, &cp);
        if (cp != ' ' && cp != '\t' && cp != 0xA0) break;
        off += len;
    }
    return off;
}

/* Ctrl+sinistra: salta gli spazi prima di 'off', poi la parola (o i segni) che trova. */
static int parola_prima(const struct area_testo *a, int off) {
    unsigned int cp;
    if (off <= 0) return 0;
    int q = prima_di(a, off);
    carattere_in(a, q, &cp);
    if (cp == '\n') return q;
    while (off > 0) {
        q = prima_di(a, off);
        carattere_in(a, q, &cp);
        if (cp != ' ' && cp != '\t' && cp != 0xA0) break;
        off = q;
    }
    if (off == 0) return 0;
    q = prima_di(a, off);
    carattere_in(a, q, &cp);
    if (cp == '\n') return off;
    int c = classe(cp);
    while (off > 0) {
        q = prima_di(a, off);
        carattere_in(a, q, &cp);
        if (cp == '\n' || classe(cp) != c) break;
        off = q;
    }
    return off;
}

/* Il doppio clic: la parola sotto 'off' (o gli spazi, o i segni). */
static void seleziona_parola(struct area_testo *a, int off) {
    int n = lung(a);
    unsigned int cp;
    int pos = off, len = carattere_in(a, off, &cp);
    if (!len || cp == '\n') {              /* in fondo alla riga: la parola prima, se c'e' */
        if (off <= 0) return;
        pos = prima_di(a, off);
        carattere_in(a, pos, &cp);
        if (cp == '\n') return;
    }
    int c = classe(cp), s = pos, e = pos;
    unsigned int c2;
    while (s > 0) {
        int q = prima_di(a, s);
        carattere_in(a, q, &c2);
        if (c2 == '\n' || classe(c2) != c) break;
        s = q;
    }
    while (e < n) {
        len = carattere_in(a, e, &c2);
        if (c2 == '\n' || classe(c2) != c) break;
        e += len;
    }
    a->ancora = s;
    a->cursore = e;
    a->a_fine_riga = 0;
}

/* --- scrivere e cancellare --- */

/* Mette 'src' al posto della selezione (o nel cursore, se non c'e'). */
static void sostituisci(struct area_testo *a, const char *src, int n) {
    int s = minimo(a->cursore, a->ancora), e = massimo(a->cursore, a->ancora);
    if (!modifica(a, s, e, src, n)) return;
    a->cursore = a->ancora = s + n;
    a->voluta = -1;
    a->a_fine_riga = 0;
    testo_cambiato(a);
    avvisa_cambiato(a);
}

static void cancella_indietro(struct area_testo *a) {
    if (a->cursore == a->ancora) {
        if (a->cursore <= 0) return;
        a->ancora = prima_di(a, a->cursore);
    }
    sostituisci(a, 0, 0);
}

static void cancella_avanti(struct area_testo *a) {
    if (a->cursore == a->ancora) {
        unsigned int cp;
        int len = carattere_in(a, a->cursore, &cp);
        if (!len) return;
        a->ancora = a->cursore + len;
    }
    sostituisci(a, 0, 0);
}

static int inizio_riga_testo(const struct area_testo *a, int off) {
    while (off > 0 && byte_in(a, off - 1) != '\n') off--;
    return off;
}

/* Il rientro delle righe del testo toccate dalla selezione (o dal cursore): con 'indietro' si toglie
 * (un '\t', o fino a AREA_TESTO_TAB spazi), se no si aggiunge un '\t'. Se la selezione finisce
 * proprio all'inizio di una riga, quella riga non conta. Si lavora dall'ultima riga alla prima, cosi'
 * i byte che restano da fare non si spostano; cursore e capo fermo seguono. */
static void rientra(struct area_testo *a, int indietro) {
    int s = minimo(a->cursore, a->ancora), e = massimo(a->cursore, a->ancora);
    int ultimo = e > s && byte_in(a, e - 1) == '\n' ? e - 1 : e;
    int primo = inizio_riga_testo(a, s), p = inizio_riga_testo(a, ultimo);
    for (;;) {
        int n = lung(a), tolti = 0;
        if (indietro) {
            if (p < n && byte_in(a, p) == '\t') tolti = 1;
            else while (tolti < AREA_TESTO_TAB && p + tolti < n && byte_in(a, p + tolti) == ' ') tolti++;
            if (tolti && !modifica(a, p, p + tolti, 0, 0)) return;
        } else {
            if (!modifica(a, p, p, "\t", 1)) return;
        }
        int *capi[2] = { &a->cursore, &a->ancora };
        for (int k = 0; k < 2; k++) {
            if (indietro) { if (*capi[k] > p) *capi[k] -= minimo(*capi[k] - p, tolti); }
            else if (*capi[k] > p) *capi[k] += 1;     /* chi sta proprio all'inizio resta prima del rientro */
        }
        if (p <= primo) break;
        p = inizio_riga_testo(a, p - 1);
    }
    a->voluta = -1;
    a->a_fine_riga = 0;
    testo_cambiato(a);
    avvisa_cambiato(a);
}

/* --- il widget --- */

static void area_misura(struct widget *w, int *l, int *a) {
    (void)w;
    *l = MISURA_LARGHEZZA;
    *a = MISURA_ALTEZZA;
}

static void area_disponi(struct widget *w) {
    struct area_testo *a = (struct area_testo *)w;
    int colonne = colonne_in(w->l - 2 * DENTRO);
    /* la barra sta lungo il bordo destro del pannello (a filo), ma tra il padding in alto e in basso, come nel Terminale */
    barra_scorrimento_posto(&a->barra, w->x, w->y + DENTRO, w->l, w->a - 2 * DENTRO);
    if (colonne != a->colonne || !a->n_righe) {
        a->colonne = colonne;
        rifai_righe(a);
    }
    aggiorna_scorrimento(a);
    assicura_visibile(a);
}

/* Disegna una lettera nella casella 'col' della riga che comincia a 'y'. */
static void disegna_lettera(struct pennello *p, unsigned int cp, int x, int y) {
    char b[5];
    b[utf8_scrivi(b, cp)] = 0;
    testo(p, AREA_TESTO_FONT, x, y + (RIGA - AREA_TESTO_FONT->altezza) / 2, b);
}

static void area_disegna(struct widget *w, struct pennello *p) {
    struct area_testo *a = (struct area_testo *)w;
    int x0 = w->x + DENTRO, y0 = w->y + DENTRO - a->barra.scorrimento.posizione;
    int s = minimo(a->cursore, a->ancora), e = massimo(a->cursore, a->ancora);
    pennello_salva(p);
    pennello_ritaglia(p, w->x, w->y, w->l, w->a);        /* il testo che scorre si ritaglia sul bordo del pannello */
    pennello_colore(p, SUPERFICIE_ELEVATA);
    riempi_tondo(p, w->x, w->y, w->l, w->a, RAGGIO_PANNELLO);

    if (!lung(a) && a->segnaposto) {
        pennello_colore(p, TESTO_SECONDARIO);
        int col = 0;
        for (const char *t = a->segnaposto; *t;) {
            unsigned int cp;
            t += utf8_leggi(t, &cp);
            disegna_lettera(p, cp, x0 + CASELLA_X(col++), y0);
        }
    }

    int prima = a->barra.scorrimento.posizione < DENTRO ? 0 : (a->barra.scorrimento.posizione - DENTRO) / RIGA;
    int ultima = (a->barra.scorrimento.posizione + w->a - DENTRO) / RIGA;
    if (ultima > a->n_righe - 1) ultima = a->n_righe - 1;
    for (int li = prima; li <= ultima; li++) {
        const struct riga_area_testo *r = &a->righe[li];
        int ry = y0 + li * RIGA;
        int da = massimo(s, r->ini), fino = minimo(e, r->fine);
        if (da < fino) {                                  /* lo sfondo della selezione, caselle intere */
            int c1 = minimo(colonna_in(a, li, da), a->colonne), c2 = minimo(colonna_in(a, li, fino), a->colonne);
            pennello_colore(p, w->fuoco ? TESTO_SELEZIONE : TESTO_SELEZIONE_INATTIVA);
            riempi_rettangolo(p, x0 + CASELLA_X(c1), ry, CASELLA_X(c2) - CASELLA_X(c1), RIGA);
        }
        if (r->segno) {                                   /* il segno dell'a capo in mezzo a una parola: solo disegnato */
            pennello_colore(p, TESTO_SECONDARIO);
            disegna_lettera(p, '_', x0 + CASELLA_X(a->colonne - 1), ry);
        }
        int col = 0, selezionato = 0;
        pennello_colore(p, TESTO_PRIMARIO);
        for (int i = r->ini; i < r->fine && col < a->colonne;) {
            unsigned int cp;
            int len = carattere_in(a, i, &cp);
            if (len <= 0) break;
            if (cp != ' ' && cp != '\t') {
                int dentro = w->fuoco && i >= s && i < e;
                if (dentro != selezionato) {
                    pennello_colore(p, dentro ? INFORMAZIONE_TESTO_TENUE : TESTO_PRIMARIO);
                    selezionato = dentro;
                }
                disegna_lettera(p, cp, x0 + CASELLA_X(col), ry);
            }
            col += larghezza_di(cp, col);
            i += len;
        }
    }

    if (w->fuoco) {                                       /* il cursore: una barra, al centro della riga */
        int li = riga_del_cursore(a);
        int col = minimo(colonna_in(a, li, a->cursore), a->colonne);
        pennello_colore(p, TESTO_CURSORE);
        riempi_rettangolo(p, x0 + CASELLA_X(col), y0 + li * RIGA + (RIGA - AREA_TESTO_CURSORE_A) / 2,
                          AREA_TESTO_CURSORE_L, AREA_TESTO_CURSORE_A);
    }
    pennello_ripristina(p);
}

/* Il byte sotto il punto (x, y) della finestra; in *riga la riga. Fuori dal testo, la riga o la
 * colonna piu' vicina. */
static int offset_da_punto(const struct area_testo *a, int x, int y, int *riga) {
    const struct widget *w = &a->base;
    int ry = y - (w->y + DENTRO) + a->barra.scorrimento.posizione;
    int li = ry < 0 ? 0 : ry / RIGA;
    if (li >= a->n_righe) li = a->n_righe - 1;
    int xr = x - (w->x + DENTRO), col = 0;
    if (xr > 0) {
        while (CASELLA_X(col + 1) <= xr && col < 1000) col++;
        if (xr - CASELLA_X(col) >= (CASELLA_X(col + 1) - CASELLA_X(col) + 1) / 2) col++;   /* il bordo piu' vicino */
    }
    *riga = li;
    return offset_a_colonna(a, li, col);
}

static void area_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    struct area_testo *a = (struct area_testo *)w;
    if (x < 0 && y < 0) { a->premuto = 0; return; }       /* il puntatore e' uscito dalla finestra */
    int giu = pulsanti & PULSANTE_SINISTRO;
    if (!giu) { a->premuto = 0; return; }
    int riga, off = offset_da_punto(a, x, y, &riga);
    a->voluta = -1;
    if (!a->premuto) {                                    /* appena premuto: cursore, o la parola */
        a->premuto = 1;
        unsigned int ora = tempo();
        if (a->clic_su == off && (int)(ora - a->clic_prima) < DOPPIO_CLIC) {
            seleziona_parola(a, off);
            a->clic_su = -1;
        } else {
            metti_cursore(a, off, 0, riga);
            a->clic_su = off;
        }
        a->clic_prima = ora;
    } else {                                              /* trascinando: si muove solo un capo */
        metti_cursore(a, off, 1, riga);
    }
    cambiato(a);
}

/* Un carattere. Invio, Backspace e Tab sono caratteri; Canc, le frecce e il resto sono tasti fisici
 * (area_tasto_fisico). Con Maiusc giu' Tab toglie il rientro: UIKit tiene il conto dei modificatori. */
static void area_tasto(struct widget *w, unsigned int tasto) {
    struct area_testo *a = (struct area_testo *)w;
    if (tasto == '\b') {
        cancella_indietro(a);
    } else if (tasto == '\t') {
        int s = minimo(a->cursore, a->ancora), e = massimo(a->cursore, a->ancora), righe = 0;
        for (int i = s; i < e; i++) if (byte_in(a, i) == '\n') righe = 1;
        if (widget_finestra(w)->modificatori & MOD_MAIUSC) rientra(a, 1);
        else if (righe) rientra(a, 0);
        else sostituisci(a, "\t", 1);
    } else if (tasto == '\n' || (tasto >= ' ' && tasto < 0x7F) || tasto >= 0xA0) {
        char b[4];
        sostituisci(a, b, utf8_scrivi(b, tasto));
    }
}

/* I tasti fisici: il cursore e la selezione. Maiusc muove un capo e lascia fermo l'altro; Ctrl va di
 * parola in parola (sinistra, destra) o a inizio e fine del testo (Home, Fine). Alt no: e' dell'app. */
static int area_tasto_fisico(struct widget *w, unsigned int codice, unsigned int modificatori) {
    struct area_testo *a = (struct area_testo *)w;
    int estendi = modificatori & MOD_MAIUSC ? 1 : 0, ctrl = modificatori & MOD_CTRL ? 1 : 0;
    if (modificatori & MOD_ALT) return 0;
    int n = lung(a), selezione = a->cursore != a->ancora, off;
    int pagina = (w->a - 2 * DENTRO) / RIGA - 1;
    if (pagina < 1) pagina = 1;
    unsigned int cp;
    switch (codice) {
    case TASTO_SINISTRA:
        if (ctrl) off = parola_prima(a, a->cursore);
        else if (selezione && !estendi) off = minimo(a->cursore, a->ancora);
        else off = prima_di(a, a->cursore);
        metti_cursore(a, off, estendi, -1);
        a->voluta = -1;
        break;
    case TASTO_DESTRA:
        if (ctrl) off = parola_dopo(a, a->cursore);
        else if (selezione && !estendi) off = massimo(a->cursore, a->ancora);
        else off = a->cursore + carattere_in(a, a->cursore, &cp);
        metti_cursore(a, off, estendi, -1);
        a->voluta = -1;
        break;
    case TASTO_SU:
        muovi_verticale(a, -1, estendi);
        break;
    case TASTO_GIU:
        muovi_verticale(a, 1, estendi);
        break;
    case TASTO_SU_PAGINA:
    case TASTO_GIU_PAGINA: {
        int verso = codice == TASTO_SU_PAGINA ? -1 : 1;
        muovi_verticale(a, verso * pagina, estendi);
        a->barra.scorrimento.posizione += verso * pagina * RIGA;   /* la vista scorre di una pagina, poi segue il cursore */
        break;
    }
    case TASTO_HOME:
        if (ctrl) metti_cursore(a, 0, estendi, -1);
        else metti_cursore(a, a->righe[riga_del_cursore(a)].ini, estendi, -1);
        a->voluta = -1;
        break;
    case TASTO_FINE:
        if (ctrl) metti_cursore(a, n, estendi, -1);
        else { int li = riga_del_cursore(a); metti_cursore(a, a->righe[li].fine, estendi, li); }
        a->voluta = -1;
        break;
    case TASTO_CANC:
        if (ctrl) return 0;
        cancella_avanti(a);
        return 1;
    case TASTO_A:
        if (!ctrl) return 0;
        a->ancora = 0;
        metti_cursore(a, n, 1, -1);
        a->voluta = -1;
        break;
    default:
        return 0;
    }
    cambiato(a);
    return 1;
}

static void area_rotella(struct widget *w, int sx, int sy) {
    (void)sx;
    if (barra_scorrimento_rotella(&((struct area_testo *)w)->barra, sy)) widget_cambiato(w);
}

static const struct tipo_widget tipo_area_testo = {
    area_misura, area_disponi, area_disegna, area_puntatore, area_tasto, area_rotella, 0, 0, 0
};

void area_testo_prepara(struct area_testo *a) {
    *a = (struct area_testo){ .base = { .tipo = &tipo_area_testo, .elastico = 1, .largo = 1,
                                        .forma_puntatore = FORMA_TESTO, .tasto_fisico = area_tasto_fisico },
                              .voluta = -1, .colonne = colonne_in(MISURA_LARGHEZZA - 2 * DENTRO), .clic_su = -1 };
    barra_scorrimento_prepara(&a->barra);
    widget_aggiungi(&a->base, &a->barra.base);
    rifai_righe(a);                        /* una riga vuota */
}

void area_testo_segnaposto(struct area_testo *a, const char *testo) {
    a->segnaposto = testo;
    widget_cambiato(&a->base);
}

void area_testo_imposta(struct area_testo *a, const char *utf8) {
    a->buco_ini = 0;
    a->buco_fin = a->cap;                  /* tutto il blocco e' buco: il testo e' vuoto */
    for (const char *t = utf8; t && *t; t++) {
        unsigned char c = (unsigned char)*t;
        if ((c < ' ' && c != '\n' && c != '\t') || c == 0x7F) continue;   /* anche '\r': il file di Windows va a capo con \r\n */
        if (!fai_posto(a, 1)) break;
        a->testo[a->buco_ini++] = (char)c;
    }
    a->cursore = a->ancora = 0;
    a->voluta = -1;
    a->a_fine_riga = 0;
    a->barra.scorrimento.posizione = 0;
    testo_cambiato(a);
}

void area_testo_al_cambiato(struct area_testo *a, al_cambiato_area_testo f, void *dati) {
    a->al_cambiato = f;
    a->dati_cambiato = dati;
}

int area_testo_lunghezza(const struct area_testo *a) { return lung(a); }

int area_testo_copia(const struct area_testo *a, char *dove, int massimo_byte) {
    int n = lung(a);
    if (massimo_byte <= 0) return n;
    int m = n < massimo_byte - 1 ? n : massimo_byte - 1;
    for (int i = 0; i < m; i++) dove[i] = (char)byte_in(a, i);
    dove[m] = 0;
    dove[utf8_entra(dove, m)] = 0;         /* senza tagliare un carattere a meta' */
    return n;
}
