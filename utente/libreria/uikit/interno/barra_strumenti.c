/* UIKit, il nucleo: la barra degli strumenti e quello che di solito ci sta dentro, il
 * pulsante della barra, la pillola (il gruppo di pulsanti) e il percorso. Le dichiarazioni
 * sono in uikit.h e in interno/nucleo.h. */
#include "uikit/uikit.h"
#include "uikit/interno/nucleo.h"

/* Alta quanto le sue cose (ALTEZZA_S; le schede 33, con il pixel della loro linea di base). */
static void barra_strumenti_misura(struct widget *w, int *l, int *a) {
    struct barra_strumenti *b = (struct barra_strumenti *)w;
    widget_misura(&b->riga.base, l, a);
    if (*a < ALTEZZA_S) *a = ALTEZZA_S;
}

static void barra_strumenti_disponi(struct widget *w) {
    struct barra_strumenti *b = (struct barra_strumenti *)w;
    b->riga.base.x = w->x;
    b->riga.base.y = w->y;
    b->riga.base.l = w->l;
    b->riga.base.a = w->a;
}

static const struct tipo_widget tipo_barra_strumenti = {
    barra_strumenti_misura, barra_strumenti_disponi, 0, 0, 0, 0, 0, 0, 0
};

void barra_strumenti_prepara(struct barra_strumenti *b) {
    *b = (struct barra_strumenti){ .base = { .tipo = &tipo_barra_strumenti, .largo = 1 } };
    riga_prepara(&b->riga, GAP_M);
    widget_aggiungi(&b->base, &b->riga.base);
}

void barra_strumenti_aggiungi(struct barra_strumenti *b, struct widget *w) {
    widget_aggiungi(&b->riga.base, w);
}

int barra_strumenti_vuota(struct finestra_tk *f, int x, int y) {
    struct widget *w = widget_in(&f->base, x, y);
    if (w && w->tipo == &tipo_spazio) w = w->genitore;          /* uno spazio nella riga */
    if (w && w->genitore && w->genitore->tipo == &tipo_barra_strumenti) w = w->genitore;   /* la riga */
    return w && w->tipo == &tipo_barra_strumenti;
}

static const struct tipo_widget tipo_pillola;   /* piu' sotto */

/* La pillola e' M (alta 40)? */
static int pillola_m(const struct widget *gruppo) {
    return gruppo && ((const struct pillola *)gruppo)->dimensione == MISURA_M;
}

/* Il gruppo (la pillola) in cui sta il pulsante, o 0 se sta da solo nella barra. */
static struct widget *pulsante_barra_gruppo(const struct widget *w) {
    return w->genitore && w->genitore->tipo == &tipo_pillola ? w->genitore : 0;
}

static void pulsante_barra_misura(struct widget *w, int *l, int *a) {
    struct pulsante_barra *b = (struct pulsante_barra *)w;
    struct widget *gruppo = pulsante_barra_gruppo(w);
    int m = pillola_m(gruppo);
    *a = !gruppo ? ALTEZZA_S : m ? GRUPPO_ELEMENTO_M : GRUPPO_ELEMENTO;
    *l = *a;                                                  /* solo icona: tondo */
    if (b->testo[0]) *l = m ? 2 * PADDING_M + larghezza_testo(FONT_PULSANTE_M, b->testo)
                            : 2 * PADDING_S + larghezza_testo(FONT_PULSANTE_S, b->testo);
}

static void pulsante_barra_disegna(struct widget *w, struct pennello *p) {
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
    int m = pillola_m(gruppo), lato = m ? ICONA_M : ICONA_S;
    if (b->testo[0]) {
        testo_centrato(p, m ? FONT_PULSANTE_M : FONT_PULSANTE_S, w->x + (m ? PADDING_M : PADDING_S), w->y,
                       w->a, b->testo);
        return;
    }
    icona(p, b->icona, lato, w->x + (w->l - lato) / 2, w->y + (w->a - lato) / 2);
}

/* La pillola, il gruppo di pulsanti: com'e' fatta e' spiegato in uikit.h ("LA PILLOLA"). */
static void pillola_misura(struct widget *w, int *l, int *a) {
    int visti = 0, largo = 0, m = pillola_m(w);
    for (int i = 0; i < w->quanti_figli; i++) {
        if (w->figli[i]->nascosto) continue;
        int fl, fa;
        widget_misura(w->figli[i], &fl, &fa);
        largo += fl;
        visti++;
    }
    *l = 2 * GRUPPO_PADDING + largo + (visti ? visti - 1 : 0) * (m ? GRUPPO_SPAZIO_M : PILLOLA_SPAZIO);
    *a = m ? ALTEZZA_M : ALTEZZA_S;
}

static void pillola_disponi(struct widget *w) {
    int m = pillola_m(w);
    int x = w->x + GRUPPO_PADDING;
    for (int i = 0; i < w->quanti_figli; i++) {
        struct widget *f = w->figli[i];
        if (f->nascosto) continue;
        int fl, fa;
        widget_misura(f, &fl, &fa);
        f->x = x;
        f->y = w->y + GRUPPO_PADDING;
        f->l = fl;
        f->a = m ? GRUPPO_ELEMENTO_M : GRUPPO_ELEMENTO;
        x += fl + (m ? GRUPPO_SPAZIO_M : PILLOLA_SPAZIO);
    }
}

static void pillola_disegna(struct widget *w, struct pennello *p) {
    struct tondo t = tondo(w->x, w->y, w->l, w->a, w->a / 2);
    ombre_strati_fuori(p, &t, OMBRA_TAB(incassato), OMBRA_N(incassato));
    pennello_colore(p, SUPERFICIE_INCASSATA);
    riempi_forma(p, &t);
    ombre_strati_dentro(p, &t, OMBRA_TAB(incassato), OMBRA_N(incassato));
}

static const struct tipo_widget tipo_pillola = {
    pillola_misura, pillola_disponi, pillola_disegna, 0, 0, 0, 0, 0, 0
};

void pulsante_barra_accendi(struct pulsante_barra *b, int acceso) {
    struct widget *g = b->base.genitore;
    if (acceso && g && g->tipo == &tipo_pillola && ((struct pillola *)g)->a_scelta)
        for (int i = 0; i < g->quanti_figli; i++) {
            struct pulsante_barra *altro = (struct pulsante_barra *)g->figli[i];
            if (altro != b && altro->acceso) { altro->acceso = 0; widget_cambiato(&altro->base); }
        }
    b->acceso = acceso;
    widget_cambiato(&b->base);
}

static void pulsante_barra_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    struct pulsante_barra *b = (struct pulsante_barra *)w;
    if (!pressione(w, &b->premuto, &b->dentro, x, y, pulsanti)) return;
    struct widget *g = w->genitore;
    if (g && g->tipo == &tipo_pillola && ((struct pillola *)g)->a_scelta) pulsante_barra_accendi(b, 1);
    if (b->al_clic) b->al_clic(b, b->dati);
}

static const struct tipo_widget tipo_pulsante_barra = {
    pulsante_barra_misura, 0, pulsante_barra_disegna, pulsante_barra_puntatore, 0, 0, 0, 0, 1
};

void pulsante_barra_prepara(struct pulsante_barra *b, int icona,
                            al_clic_pulsante_barra al_clic, void *dati) {
    *b = (struct pulsante_barra){ .base = { .tipo = &tipo_pulsante_barra }, .icona = icona,
                                  .al_clic = al_clic, .dati = dati };
}

void pulsante_barra_testo(struct pulsante_barra *b, const char *testo,
                          al_clic_pulsante_barra al_clic, void *dati) {
    pulsante_barra_prepara(b, NESSUNA_ICONA, al_clic, dati);
    copia_testo(b->testo, testo, MAX_TESTO_WIDGET);
}

void pillola_prepara(struct pillola *p, int a_scelta) {
    *p = (struct pillola){ .base = { .tipo = &tipo_pillola }, .a_scelta = a_scelta, .dimensione = MISURA_S };
}

void pillola_dimensione(struct pillola *p, int dimensione) {
    p->dimensione = dimensione == MISURA_M ? MISURA_M : MISURA_S;
}

void pillola_aggiungi(struct pillola *p, struct pulsante_barra *b) {
    widget_aggiungi(&p->base, &b->base);
}

static const struct font *percorso_font(const struct percorso *p, int i) {
    return i == p->quanti - 1 ? FONT_CORPO_MEDIO : FONT_CORPO;
}

static int percorso_largo(const struct percorso *p, int i) {
    return larghezza_testo(percorso_font(p, i), p->nomi[i].nome);
}

/* Il separatore tra un nome e l'altro, e quanto e' largo. */
#define PERCORSO_SEPARATORE "/"

static int percorso_separatore(void) { return larghezza_testo(FONT_CORPO, PERCORSO_SEPARATORE); }

/* Al minimo: il nome dell'ultima cartella (ma non piu' di
 * PERCORSO_ULTIMO_MINIMO: un nome lunghissimo si taglia, invece di spingere fuori dalla
 * barra le cose dopo di lui). Il resto viene dall'elastico. */
#define PERCORSO_ULTIMO_MINIMO 120

static void percorso_misura(struct widget *w, int *l, int *a) {
    struct percorso *p = (struct percorso *)w;
    int ultimo = p->quanti ? percorso_largo(p, p->quanti - 1) : 0;
    if (ultimo > PERCORSO_ULTIMO_MINIMO) ultimo = PERCORSO_ULTIMO_MINIMO;
    *l = ultimo;
    *a = ALTEZZA_S;
}

/* Da destra a sinistra: si mettono i nomi finche' ci stanno. (Il primo nome non ha uno
 * spazio davanti: l'inizio lo comprende gia'.) */
static void percorso_disponi(struct widget *w) {
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
static void percorso_disegna(struct widget *w, struct pennello *p) {
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

static void nome_percorso_disegna(struct widget *w, struct pennello *p) {
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

static void nome_percorso_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
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

void percorso_prepara(struct percorso *p, al_clic_percorso al_clic, void *dati) {
    *p = (struct percorso){ .base = { .tipo = &tipo_percorso, .elastico = 1 },
                            .al_clic = al_clic, .dati = dati };
}

void percorso_metti(struct percorso *p, const char *const *nomi, int quanti) {
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
