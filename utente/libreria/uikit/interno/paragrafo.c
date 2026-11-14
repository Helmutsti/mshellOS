/* UIKit, i componenti: il paragrafo, il testo che va a capo da solo. Le dichiarazioni sono
 * in paragrafo.h. */
#include "uikit/uikit.h"
#include "uikit/interno/nucleo.h"

int paragrafo_a_capo(const struct font *f, const char *testo, int largo, char righe[][PARAGRAFO_RIGA], int massimo) {
    int sp = spaziatura_del_font(f);   /* centesimi di pixel dopo ogni lettera */
    int pos = 0, n = 0, puntini = larghezza_testo_spaziato(f, "...", sp);
    while (testo[pos] == ' ') pos++;
    while (testo[pos] && n < massimo) {
        int fine = pos;
        while (testo[fine] && fine - pos < PARAGRAFO_RIGA - 4) {
            if (larghezza_pezzo(f, testo, pos, fine + 1) > largo) break;
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

/* Le righe nella larghezza 'largo' (0: senza limite, una riga). */
static int paragrafo_righe(const struct paragrafo *g, int largo, char righe[][PARAGRAFO_RIGA]) {
    if (!g->testo[0]) return 0;
    if (largo <= 0) largo = 1 << 20;
    return paragrafo_a_capo(g->font, g->testo, largo, righe, PARAGRAFO_RIGHE);
}

static void paragrafo_misura(struct widget *w, int *l, int *a) {
    struct paragrafo *g = (struct paragrafo *)w;
    char righe[PARAGRAFO_RIGHE][PARAGRAFO_RIGA];
    int n = paragrafo_righe(g, w->largo ? w->l : 0, righe);
    *l = w->largo ? 0 : larghezza_testo_stile(g->font, g->testo);
    *a = n * g->interlinea;
}

static void paragrafo_disegna(struct widget *w, struct pennello *p) {
    struct paragrafo *g = (struct paragrafo *)w;
    char righe[PARAGRAFO_RIGHE][PARAGRAFO_RIGA];
    int n = paragrafo_righe(g, w->largo ? w->l : 0, righe);
    pennello_colore(p, COLORE_ICONA(g->colore));
    int y0 = w->y + (w->a > n * g->interlinea ? (w->a - n * g->interlinea) / 2 : 0);   /* in una riga piu' alta: al centro */
    for (int i = 0; i < n; i++) {
        int x = w->x, lr = larghezza_testo_stile(g->font, righe[i]);
        if (g->allineamento == PARAGRAFO_CENTRO) x += (w->l - lr) / 2;
        else if (g->allineamento == PARAGRAFO_DESTRA) x += w->l - lr;
        testo_centrato(p, g->font, x, y0 + i * g->interlinea, g->interlinea, righe[i]);
    }
}

static const struct tipo_widget tipo_paragrafo = { paragrafo_misura, 0, paragrafo_disegna, 0, 0, 0, 0, 0, 0 };

void paragrafo_prepara(struct paragrafo *g, const char *testo, const struct font *font,
                       int interlinea, int colore) {
    *g = (struct paragrafo){ .base = { .tipo = &tipo_paragrafo, .largo = 1 }, .font = font,
                             .interlinea = interlinea, .colore = colore };
    copia_testo(g->testo, testo ? testo : "", PARAGRAFO_TESTO);
}

void paragrafo_testo(struct paragrafo *g, const char *testo) {
    char nuovo[PARAGRAFO_TESTO];
    copia_testo(nuovo, testo ? testo : "", PARAGRAFO_TESTO);
    int i = 0;
    while (nuovo[i] && nuovo[i] == g->testo[i]) i++;
    if (nuovo[i] == g->testo[i]) return;      /* lo stesso testo: niente da ridisegnare */
    copia_testo(g->testo, nuovo, PARAGRAFO_TESTO);
    widget_cambiato(&g->base);
}
