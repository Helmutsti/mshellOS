/* UIKit, il nucleo: lo scorrimento (dove si e' in un contenuto piu' alto della sua vista, e
 * la rotella con l'accelerazione) e la barra di scorrimento, con le sue animazioni. Le
 * dichiarazioni sono in uikit.h e in interno/nucleo.h. */
#include "uikit/uikit.h"
#include "uikit/interno/nucleo.h"

/* Fin dove si puo' scorrere: 0 se il contenuto ci sta tutto. */
static int scorrimento_massimo(const struct scorrimento *s) {
    return s->totale > s->vista ? s->totale - s->vista : 0;
}

/* Va a 'posizione' (dentro i limiti). 1 se si e' spostato. */
static int scorrimento_metti(struct scorrimento *s, int posizione) {
    int massimo = scorrimento_massimo(s);
    if (posizione > massimo) posizione = massimo;
    if (posizione < 0) posizione = 0;
    if (posizione == s->posizione) return 0;
    s->posizione = posizione;
    return 1;
}

void scorrimento_misure(struct scorrimento *s, int totale, int vista) {
    s->totale = totale;
    s->vista = vista;
    scorrimento_metti(s, s->posizione);
}

/* La rotella ha girato di 'scatti' (positivi: in basso, o a destra): si scorre come sul
 * Mac, con l'accelerazione spiegata in stile.h (SCORRIMENTO_RIGA). Basta l'ora di ogni
 * scatto: niente attese, niente chiamate "ogni tanto". 1 se si e' spostato. */
static int scorrimento_rotella(struct scorrimento *s, int scatti) {
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

#define SCORRIMENTO_FOTOGRAMMA 20      /* ms tra un passo e l'altro dell'animazione */

/* Il cursore: dove comincia (y) e quanto e' lungo. */
static void barra_scorrimento_cursore(const struct barra_scorrimento *b, int *y, int *lungo) {
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
static void barra_scorrimento_disegna(struct widget *w, struct pennello *p) {
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
static int avvicina(int valore, int obiettivo, unsigned int passati, int durata) {
    int passo = (int)(255 * passati / (unsigned int)durata);
    if (passo < 1) passo = 1;
    if (valore < obiettivo) return valore + passo > obiettivo ? obiettivo : valore + passo;
    return valore - passo < obiettivo ? obiettivo : valore - passo;
}

static void barra_scorrimento_passo(struct finestra_tk *f, void *dati);

/* La prossima chiamata tra 'ms' millisecondi (0: nessuna). */
static void barra_scorrimento_richiama(struct barra_scorrimento *b, unsigned int ms) {
    if (b->attesa == ms && ms == SCORRIMENTO_FOTOGRAMMA) return;   /* gia' in moto */
    b->attesa = ms;
    finestra_tk_ogni(widget_finestra(&b->base), ms, barra_scorrimento_passo, b);
}

/* Qualcosa e' cambiato (uno scatto, il puntatore sopra): l'animazione riparte. */
static void barra_scorrimento_sveglia(struct barra_scorrimento *b) {
    if (b->attesa != SCORRIMENTO_FOTOGRAMMA) b->passo_prima = tempo();
    barra_scorrimento_richiama(b, SCORRIMENTO_FOTOGRAMMA);
}

/* Un passo dell'animazione: e' la chiamata "ogni tanto". */
static void barra_scorrimento_passo(struct finestra_tk *f, void *dati) {
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
static void barra_scorrimento_usata(struct barra_scorrimento *b, int spostato) {
    b->ultimo = tempo();
    if (spostato) {                    /* si e' mosso il contenuto: tutto il widget che scorre */
        widget_cambiato(&b->base);
        if (b->base.genitore) widget_cambiato(b->base.genitore);
    }
    barra_scorrimento_sveglia(b);
}

int barra_scorrimento_rotella(struct barra_scorrimento *b, int scatti) {
    struct scorrimento *s = &b->scorrimento;
    if (!scorrimento_massimo(s)) return 0;
    barra_scorrimento_usata(b, scorrimento_rotella(s, scatti));
    return 1;
}

/* Il cursore segue il puntatore, tenuto per il punto in cui lo si e' preso. */
static void barra_scorrimento_segui(struct barra_scorrimento *b, int y) {
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
static void barra_scorrimento_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
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

static void barra_scorrimento_su_rotella(struct widget *w, int sx, int sy) {
    (void)sx;
    barra_scorrimento_rotella((struct barra_scorrimento *)w, sy);
}

static void barra_scorrimento_sopra(struct widget *w) {
    barra_scorrimento_sveglia((struct barra_scorrimento *)w);
}

const struct tipo_widget tipo_barra_scorrimento = {
    0, 0, barra_scorrimento_disegna, barra_scorrimento_puntatore, 0,
    barra_scorrimento_su_rotella, barra_scorrimento_sopra, 0, 0
};

void barra_scorrimento_prepara(struct barra_scorrimento *b) {
    *b = (struct barra_scorrimento){ .base = { .tipo = &tipo_barra_scorrimento, .nascosto = 1 } };
}

int barra_scorrimento_riserva(const struct barra_scorrimento *b, int libero) {
    int manca = SCORRIMENTO_ZONA - b->fuori - libero;
    return b->fuori + (scorrimento_massimo(&b->scorrimento) && manca > 0 ? manca : 0);
}

void barra_scorrimento_posto(struct barra_scorrimento *b, int x, int y, int l, int a) {
    b->base.x = x + l - SCORRIMENTO_ZONA;
    b->base.y = y;
    b->base.l = SCORRIMENTO_ZONA;
    b->base.a = a;
}
