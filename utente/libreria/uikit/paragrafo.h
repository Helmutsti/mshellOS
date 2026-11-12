/* IL PARAGRAFO: un testo che va a capo da solo, con l'interlinea del design (UIKit, incluso da
 * uikit.h). E' il "testo" dei frame del file Figma: un font, un'altezza di riga (20 per il corpo,
 * 16 per il piccolo, 32 per il titolo) e un colore. Diverso dall'etichetta, che e' alta quanto il
 * font e non va a capo.
 *
 * LA LARGHEZZA. Il paragrafo e' 'largo': prende la larghezza che gli da' il suo contenitore e va
 * a capo li'. Per questo, misurato, dice larghezza 0 (sta in quella che riceve) e l'altezza delle
 * righe che gli servono nella larghezza che ha gia' (w->l: la decide la pagina, vedi pagina.h,
 * prima di misurarlo). Con 'largo' a 0 invece e' largo quanto il suo testo, su una riga sola.
 *
 *     paragrafo_prepara(&nome, "Note", FONT_TITOLO_NUOVO, INTERLINEA_TITOLO, TESTO_PRIMARIO);
 *
 * Il testo e' gia' nella code page 437 del font (come per l'etichetta). */
#ifndef UIKIT_PARAGRAFO_H
#define UIKIT_PARAGRAFO_H

#define PARAGRAFO_TESTO 400                /* lettere al massimo, compreso lo 0 */
#define PARAGRAFO_RIGHE 6                  /* righe al massimo: oltre, l'ultima finisce con "..." */
#define PARAGRAFO_RIGA  200                /* lettere di una riga, compreso lo 0 */

enum { PARAGRAFO_SINISTRA, PARAGRAFO_CENTRO, PARAGRAFO_DESTRA };

/* Come testo_a_capo (elemento_file.h), ma con righe lunghe fino a PARAGRAFO_RIGA lettere (quella
 * e' fatta per i nomi dei file, e spezza dopo 92): spezza 'testo' in righe larghe al massimo
 * 'largo' pixel, al massimo 'massimo' righe, andando a capo dopo uno spazio o un trattino; se non
 * ci sta tutto, l'ultima riga finisce con "...". Quante righe. */
static inline int paragrafo_a_capo(const struct font *f, const char *testo, int largo, char righe[][PARAGRAFO_RIGA], int massimo) {
    int pos = 0, n = 0, puntini = larghezza_testo(f, "...");
    while (testo[pos] == ' ') pos++;
    while (testo[pos] && n < massimo) {
        int fine = pos, l = 0;
        while (testo[fine] && fine - pos < PARAGRAFO_RIGA - 4) {
            int c = glifo_di(f, (unsigned char)testo[fine])->avanzo;
            if (l + c > largo) break;
            l += c;
            fine++;
        }
        if (fine == pos) fine++;
        int taglio = fine;
        if (testo[fine] && n == massimo - 1) {             /* l'ultima riga: si taglia con i puntini */
            while (fine > pos && larghezza_pezzo(f, testo, pos, fine) + puntini > largo) fine--;
            int k = 0;
            for (int i = pos; i < fine; i++) righe[n][k++] = testo[i];
            righe[n][k++] = '.'; righe[n][k++] = '.'; righe[n][k++] = '.';
            righe[n][k] = 0;
            return n + 1;
        }
        if (testo[fine])
            for (int k = fine; k > pos; k--)
                if (testo[k - 1] == ' ' || testo[k - 1] == '-') { taglio = k; break; }
        int k = 0, ultimo = taglio;
        while (ultimo > pos && testo[ultimo - 1] == ' ') ultimo--;
        for (int i = pos; i < ultimo; i++) righe[n][k++] = testo[i];
        righe[n][k] = 0;
        n++;
        pos = taglio;
        while (testo[pos] == ' ') pos++;
    }
    return n;
}

struct paragrafo {
    struct widget base;
    char testo[PARAGRAFO_TESTO];
    const struct font *font;
    int interlinea;                        /* l'altezza di una riga (INTERLINEA_...) */
    unsigned int colore;
    int allineamento;                      /* PARAGRAFO_SINISTRA, _CENTRO, _DESTRA */
};

/* Le righe nella larghezza 'largo' (0: senza limite, una riga). */
static inline int paragrafo_righe(const struct paragrafo *g, int largo, char righe[][PARAGRAFO_RIGA]) {
    if (!g->testo[0]) return 0;
    if (largo <= 0) largo = 1 << 20;
    return paragrafo_a_capo(g->font, g->testo, largo, righe, PARAGRAFO_RIGHE);
}

static inline void paragrafo_misura(struct widget *w, int *l, int *a) {
    struct paragrafo *g = (struct paragrafo *)w;
    char righe[PARAGRAFO_RIGHE][PARAGRAFO_RIGA];
    int n = paragrafo_righe(g, w->largo ? w->l : 0, righe);
    *l = w->largo ? 0 : larghezza_testo(g->font, g->testo);
    *a = n * g->interlinea;
}

static inline void paragrafo_disegna(struct widget *w, struct pennello *p) {
    struct paragrafo *g = (struct paragrafo *)w;
    char righe[PARAGRAFO_RIGHE][PARAGRAFO_RIGA];
    int n = paragrafo_righe(g, w->largo ? w->l : 0, righe);
    pennello_colore(p, g->colore);
    int y0 = w->y + (w->a > n * g->interlinea ? (w->a - n * g->interlinea) / 2 : 0);   /* in una riga piu' alta: al centro */
    for (int i = 0; i < n; i++) {
        int x = w->x, lr = larghezza_testo(g->font, righe[i]);
        if (g->allineamento == PARAGRAFO_CENTRO) x += (w->l - lr) / 2;
        else if (g->allineamento == PARAGRAFO_DESTRA) x += w->l - lr;
        testo_centrato(p, g->font, x, y0 + i * g->interlinea, g->interlinea, righe[i]);
    }
}

static const struct tipo_widget tipo_paragrafo = { paragrafo_misura, 0, paragrafo_disegna, 0, 0, 0, 0, 0, 0 };

static inline void paragrafo_prepara(struct paragrafo *g, const char *testo, const struct font *font,
                                     int interlinea, unsigned int colore) {
    *g = (struct paragrafo){ .base = { .tipo = &tipo_paragrafo, .largo = 1 }, .font = font,
                             .interlinea = interlinea, .colore = colore };
    copia_testo(g->testo, testo ? testo : "", PARAGRAFO_TESTO);
}

/* Cambia il testo. Se cambia il numero delle righe, chi lo contiene rifa' l'impaginazione
 * (uikit_ridisponi). */
static inline void paragrafo_testo(struct paragrafo *g, const char *testo) {
    char nuovo[PARAGRAFO_TESTO];
    copia_testo(nuovo, testo ? testo : "", PARAGRAFO_TESTO);
    int i = 0;
    while (nuovo[i] && nuovo[i] == g->testo[i]) i++;
    if (nuovo[i] == g->testo[i]) return;      /* lo stesso testo: niente da ridisegnare */
    copia_testo(g->testo, nuovo, PARAGRAFO_TESTO);
    widget_cambiato(&g->base);
}

#endif
