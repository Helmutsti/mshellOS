/* La barra di scorrimento in tre stati, affiancati: a riposo mentre si scorre (solo il
 * cursore), con il puntatore sopra (binario incassato + cursore in SCORRIMENTO_CURSORE_SOPRA)
 * e trascinata (cursore in SCORRIMENTO_CURSORE_TRASCINATO_NUOVO). L'anteprima non ha eventi
 * ne' animazioni: gli stati si forzano a mano, come nella scena campo.
 * Uso: TEMA=scuro anteprima.sh scorrimento 3 */
#include "toolkit.h"

#define ALTEZZA_PROVA 200

/* un widget che tiene tre barre e le mette una accanto all'altra (ognuna in una "vista") */
struct prova { struct widget base; struct barra_scorrimento barre[3]; };
static struct finestra_tk finestra;
static struct prova prova;

static void prova_misura(struct widget *w, int *l, int *a) { (void)w; *l = 3 * 90; *a = ALTEZZA_PROVA; }
static void prova_disponi(struct widget *w) {
    struct prova *pr = (struct prova *)w;
    for (int i = 0; i < 3; i++) barra_scorrimento_posto(&pr->barre[i], w->x + i * 90, w->y, 60, w->a);
}
static void prova_disegna(struct widget *w, struct pennello *p) {
    pennello_colore(p, SUPERFICIE_ELEVATA);       /* la "vista": una scheda dietro ogni barra */
    for (int i = 0; i < 3; i++) riempi_tondo(p, w->x + i * 90, w->y, 60, w->a, RAGGIO_CONTROLLO_NUOVO);
}
static const struct tipo_widget tipo_prova = { prova_misura, prova_disponi, prova_disegna, 0, 0, 0, 0, 0 };

int main(void) {
    prova.base.tipo = &tipo_prova;
    for (int i = 0; i < 3; i++) {
        struct barra_scorrimento *b = &prova.barre[i];
        barra_scorrimento_prepara(b);
        scorrimento_misure(&b->scorrimento, 1000, 300);
        b->scorrimento.posizione = 250;
        b->opacita = 255;
        b->base.nascosto = 0;
        b->spessore = i ? 255 : 0;           /* solo il cursore, o con il binario */
        b->base.sopra = i == 1;
        b->trascinata = i == 2;
        widget_aggiungi(&prova.base, &b->base);
    }
    finestra_tk_prepara(&finestra, "Scorrimento", &prova.base);
    return toolkit_esegui(&finestra);
}
