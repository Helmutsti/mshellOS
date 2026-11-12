/* Le primitive nuove di gkit_disegno.h: ombre a strati, ombra interna, pillola, cerchio, arco.
 * Valori presi da strumenti/grafica/token-figma.json (chiave "ombre"). Va guardata su
 * fondo chiaro:  SFONDO=bianco anteprima.sh primitive 2
 * Nessun widget di UIKit: un solo widget fatto qui, che si disegna da se'. */
#include "uikit/uikit.h"
#include <stdio.h>
#include <time.h>

#define A(op) ((unsigned int)((op) * 255 + 0.5))
#define INK(op) VELATO(0x14141A, A(op))

/* ombra/sollevato: [DS 0.06 0 1 1 0] [DS 0.07 0 2 4 -1] [DS 0.06 0 6 12 -4] */
static const struct strato_ombra sollevato[] = {
    { INK(0.06), OMBRA_ESTERNA, 0, 1, 1, 0 },
    { INK(0.07), OMBRA_ESTERNA, 0, 2, 4, -1 },
    { INK(0.06), OMBRA_ESTERNA, 0, 6, 12, -4 },
};
/* ombra/incassato: [IS 0.07 0 1 2 0] [DS 0.04 0 1 0 0] */
static const struct strato_ombra incassato[] = {
    { INK(0.07), OMBRA_INTERNA, 0, 1, 2, 0 },
    { INK(0.04), OMBRA_ESTERNA, 0, 1, 0, 0 },
};
/* ombra/sovrapposto: [DS 0.05 0 1 2 0] [DS 0.08 0 8 16 -4] [DS 0.14 0 20 40 -12] */
static const struct strato_ombra sovrapposto[] = {
    { INK(0.05), OMBRA_ESTERNA, 0, 1, 2, 0 },
    { INK(0.08), OMBRA_ESTERNA, 0, 8, 16, -4 },
    { INK(0.14), OMBRA_ESTERNA, 0, 20, 40, -12 },
};

/* La scheda: ombre di fuori, fondo, ombre di dentro (l'ordine spiegato in gkit_disegno.h). */
static void scheda(struct pennello *p, int x, int y, int l, int a, int r,
                   const struct strato_ombra *s, int n, unsigned int fondo) {
    struct tondo t = tondo(x, y, l, a, r);
    ombre_strati_fuori(p, &t, s, n);
    pennello_colore(p, fondo);
    riempi_forma(p, &t);
    ombre_strati_dentro(p, &t, s, n);
}

static void misura_v(struct widget *w, int *l, int *a) { (void)w; *l = 760; *a = 400; }

static void disegna_v(struct widget *w, struct pennello *pp) {
    (void)w;
    struct pennello *p = pp;
    p->s.rx = 0; p->s.ry = 0; p->s.rx2 = p->tela->larghezza; p->s.ry2 = p->tela->altezza;
    pennello_colore(p, OPACO(0xF7F7F9));
    riempi_rettangolo(p, 0, 0, p->tela->larghezza, p->tela->altezza);

    scheda(p, 40, 30, 200, 120, 14, sollevato, 3, OPACO(0xFFFFFF));
    scheda(p, 290, 30, 200, 120, 14, incassato, 2, OPACO(0xEDEDF0));
    scheda(p, 540, 30, 180, 120, 14, sovrapposto, 3, OPACO(0xFFFFFF));

    /* una pillola e un cerchio, con l'ombra sollevata; la pillola con l'ombra interna */
    struct tondo pil = tondo(40, 220, 140, 40, 20);
    ombre_strati_fuori(p, &pil, sollevato, 3);
    pennello_colore(p, OPACO(0x3B82F6));
    riempi_pillola(p, 40, 220, 140, 40);
    struct strato_ombra luce = { VELATO(0xFFFFFF, 90), OMBRA_INTERNA, 0, 1, 1, 0 };
    ombre_strati_dentro(p, &pil, &luce, 1);
    struct tondo cer = tondo(220, 220, 40, 40, 20);
    ombre_strati_fuori(p, &cer, sollevato, 3);
    pennello_colore(p, OPACO(0xFFFFFF));
    riempi_cerchio(p, 240, 240, 20);

    /* barra circolare con tappi dritti, e l'anello in 4 rotazioni (arco di 270, 3 px) */
    pennello_colore(p, VELATO(0x14141A, 25));
    riempi_arco(p, 330, 240, 22, 19, 0, 360);
    pennello_colore(p, OPACO(0x3B82F6));
    riempi_arco_dritto(p, 330, 240, 22, 19, 0, 200);
    for (int i = 0; i < 4; i++) {
        int cx = 430 + i * 60;
        pennello_colore(p, OPACO(0x3B82F6));
        riempi_arco(p, cx, 240, 12, 9, i * 90, i * 90 + 270);
    }
    /* un ingrandimento a occhio dell'anello: raggio grande, spessore 3 */
    pennello_colore(p, OPACO(0x14141A));
    riempi_arco(p, 700, 240, 30, 27, 30, 300);

    /* costo: 20 ombre a 3 strati di 200x120 su una tela a parte */
    static unsigned int prova[400 * 300];
    struct tela tp = { prova, 400, 300 };
    struct pennello q;
    pennello_prepara(&q, &tp);
    struct tondo t = tondo(100, 80, 200, 120, 14);
    for (int k = 0; k < 2; k++) {
        const struct strato_ombra *s3 = k ? sollevato : sovrapposto;
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < 20; i++) ombre_strati_fuori(&q, &t, s3, 3);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        fprintf(stderr, "%s 200x120, 3 strati: %.2f ms\n", k ? "sollevato" : "sovrapposto",
                ((t1.tv_sec - t0.tv_sec) * 1e3 + (t1.tv_nsec - t0.tv_nsec) / 1e6) / 20);
    }
}

static const struct tipo_widget tipo_v = { .misura = misura_v, .disegna = disegna_v };
static struct widget vista = { .tipo = &tipo_v };
static struct finestra_tk finestra;

int main(void) {
    finestra_tk_prepara(&finestra, "Primitive", &vista);
    return uikit_esegui(&finestra);
}
