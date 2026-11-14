/* L'esempio del menu a tendina aperto (Figma "Esempio - menu a tendina aperto"): il campo M con
 * "Nome" e sotto l'elenco con la spunta su Nome. Uso: TEMA=scuro anteprima.sh tendina_aperta 2 */
#include "uikit/uikit.h"
#include "piano.h"

static struct finestra_tk finestra;
static struct piano piano;
static struct contenitore colonna;
static struct tendina_tk tendina;
static const struct voce_menu_tk voci[] = { TENDINA_VOCE(0, "Nome"), TENDINA_VOCE(1, "Tipo"),
                                          TENDINA_VOCE(2, "Dimensione"), TENDINA_VOCE(3, "Data") };

#define HA_RITOCCO
static void ritocco(struct finestra_tk *f) {
    (void)f;
    tendina_apri(&tendina);
}

int main(void) {
    colonna_prepara(&colonna, 0);
    tendina_prepara(&tendina, voci, 4, 0, "Seleziona...", 0, 0);
    widget_aggiungi(&colonna.base, &tendina.base);
    piano_prepara(&piano, &colonna.base);
    finestra_tk_prepara(&finestra, "Menu a tendina", &piano.base);
    finestra_tk_dimensione(&finestra, 320, 360);
    tendina_aggiungi_menu(&finestra, &tendina);
    return uikit_esegui(&finestra);
}
