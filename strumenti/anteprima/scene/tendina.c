/* Il menu a tendina nelle tre misure (S, M, L), nei tre stati del disegno (normale, aperto,
 * disabilitato) e con segnaposto o scelta: come il frame "Menu a tendina" di Figma. L'aperto si
 * forza a mano (il menu e' "aperto" ma nascosto: l'anteprima non ha eventi e qui non si vedono
 * gli elenchi). Uso: TEMA=scuro anteprima.sh tendina 2 */
#include "uikit/uikit.h"
#include "piano.h"

static struct finestra_tk finestra;
static struct piano piano;
static struct contenitore colonna, righe[6];
static struct tendina_tk tendine[6][3];
static const struct voce_menu_tk voci[] = { TENDINA_VOCE(0, "Nome"), TENDINA_VOCE(1, "Tipo"),
                                          TENDINA_VOCE(2, "Dimensione"), TENDINA_VOCE(3, "Data") };

#define HA_RITOCCO
static void ritocco(struct finestra_tk *f) {
    (void)f;
    for (int r = 0; r < 6; r++) tendine[r][1].menu.quanti = 1;   /* "aperto": la freccia in su e l'anello */
}

int main(void) {
    colonna_prepara(&colonna, 16);
    for (int r = 0; r < 6; r++) {
        riga_prepara(&righe[r], 16);
        for (int c = 0; c < 3; c++) {
            struct tendina_tk *t = &tendine[r][c];
            tendina_prepara(t, voci, 4, r % 2 ? 0 : TENDINA_SENZA_SCELTA, "Seleziona...", 0, 0);
            tendina_stile(t, r / 2);
            t->base.disabilitato = c == 2;
            widget_aggiungi(&righe[r].base, &t->base);
        }
        widget_aggiungi(&colonna.base, &righe[r].base);
    }
    piano_prepara(&piano, &colonna.base);
    finestra_tk_prepara(&finestra, "Menu a tendina", &piano.base);
    return uikit_esegui(&finestra);
}
