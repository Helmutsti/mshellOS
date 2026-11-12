/* La userbar e il vassoio delle app (userbar.h), su un pezzo di scrivania dentro una finestra
 * di prova: barra a sinistra centrata in altezza, pulsante App premuto, vassoio aperto con il
 * puntatore su Files. Files e' aperta (il puntino a sinistra), Attivita in primo piano (la
 * lineetta). Le icone sono i simboli del sistema: l'anteprima non legge le PNG delle app.
 * Uso: TEMA=scuro anteprima.sh userbar 2 */
#include "uikit/userbar.h"

static struct finestra_tk finestra;
static struct widget scrivania;

static const struct voce_app app[6] = {
    { ICONA_FOLDER, 0, "Files" }, { ICONA_TERMINAL, 0, "Terminale" }, { ICONA_GLOBE, 0, "Browser" },
    { ICONA_MAIL, 0, "Posta" }, { ICONA_SETTINGS, 0, "Impostazioni" }, { ICONA_ACTIVITY, 0, "Attivita" },
};
static const int stati[6] = { APP_APERTA, APP_CHIUSA, APP_CHIUSA, APP_CHIUSA, APP_CHIUSA, APP_ATTIVA };

static void scrivania_misura(struct widget *w, int *l, int *a) { (void)w; *l = 520; *a = 400; }

static void scrivania_disegna(struct widget *w, struct pennello *p) {
    pennello_colore(p, SUPERFICIE_SCENA);
    riempi_rettangolo(p, w->x, w->y, w->l, w->a);
    int bx, by, vx, vy;
    userbar_posto(w->a, 6, &bx, &by);
    bx += w->x; by += w->y;
    userbar_ombra(p, bx, by, 6);                                 /* sulla scrivania: il compositore */
    userbar_disegna(p, bx, by, app, stati, 6, 0, 0);             /* App premuto: il vassoio e' aperto */
    vassoio_posto(bx, by, 6, &vx, &vy);
    vassoio_ombra(p, vx, vy, 6);
    vassoio_disegna(p, vx, vy, app, 6, 0, -1);                   /* il puntatore su Files */
}

static const struct tipo_widget tipo_scrivania = { scrivania_misura, 0, scrivania_disegna, 0, 0, 0, 0, 0 };

int main(void) {
    scrivania = (struct widget){ .tipo = &tipo_scrivania };
    finestra_tk_prepara(&finestra, "Userbar", &scrivania);
    finestra_tk_bordo(&finestra);
    return uikit_esegui(&finestra);
}
