/* La userbar e il vassoio delle app (userbar.h), su un pezzo di scrivania dentro una finestra
 * di prova: barra a sinistra centrata in altezza, pulsante App premuto, vassoio aperto con il
 * puntatore su Files. Uso: TEMA=scuro anteprima.sh userbar 2 */
#include "userbar.h"

static struct finestra_tk finestra;
static struct widget scrivania;

static const int fissate[6] = { ICONA_FOLDER, ICONA_TERMINAL, ICONA_GLOBE, ICONA_MAIL, ICONA_SETTINGS,
                                ICONA_ACTIVITY };
static const struct voce_vassoio app[6] = {
    { ICONA_FOLDER, "Files" }, { ICONA_TERMINAL, "Terminale" }, { ICONA_GLOBE, "Browser" },
    { ICONA_MAIL, "Posta" }, { ICONA_SETTINGS, "Impostazioni" }, { ICONA_ACTIVITY, "Attivita" },
};

static void scrivania_misura(struct widget *w, int *l, int *a) { (void)w; *l = 520; *a = 400; }

static void scrivania_disegna(struct widget *w, struct pennello *p) {
    pennello_colore(p, SUPERFICIE_SCENA);
    riempi_rettangolo(p, w->x, w->y, w->l, w->a);
    int bx, by, vx, vy;
    userbar_posto(w->a, 6, &bx, &by);
    bx += w->x; by += w->y;
    userbar_disegna(p, bx, by, fissate, 6, 0, 0);          /* App premuto: il vassoio e' aperto */
    vassoio_posto(bx, by, 6, &vx, &vy);
    vassoio_disegna(p, vx, vy, app, 6, 0, -1);             /* il puntatore su Files */
}

static const struct tipo_widget tipo_scrivania = { scrivania_misura, 0, scrivania_disegna, 0, 0, 0, 0, 0 };

int main(void) {
    scrivania = (struct widget){ .tipo = &tipo_scrivania };
    finestra_tk_prepara(&finestra, "Userbar", &scrivania);
    finestra_tk_bordo(&finestra);
    return toolkit_esegui(&finestra);
}
