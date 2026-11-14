/* Il suggerimento (Figma "Suggerimento" 113:3004): solo testo e con scorciatoia, come nel
 * disegno; sotto, il gruppo di azioni (indietro / avanti, "Gruppo di azioni" 132:1062) con
 * un suggerimento sotto il pulsante. Uso: TEMA=scuro anteprima.sh suggerimento 2 */
#include "uikit/uikit.h"
#include "uikit/interno/nucleo.h"
#include "piano.h"

static struct finestra_tk finestra;
static struct piano piano;
static struct contenitore colonna, riga_gruppi;
static struct pillola frecce, frecce_m;
static struct pulsante_barra indietro, avanti, indietro_m, avanti_m;

/* due etichette affiancate, disegnate com'e' il suggerimento */
static void mostra_misura(struct widget *w, int *l, int *a) { (void)w; *l = 200; *a = SUGGERIMENTO_ALTEZZA + 48; }
static void mostra_disegna(struct widget *w, struct pennello *p) {
    int s = larghezza_testo(FONT_PICCOLO_NUOVO, "Salva"), c = larghezza_testo(FONT_PICCOLO_NUOVO, "Ctrl+S");
    suggerimento_disegna_forma(p, w->x, w->y + 12, 2 * SUGGERIMENTO_PADDING_X + s, "Salva", 0);
    suggerimento_disegna_forma(p, w->x + 93, w->y + 12, 2 * SUGGERIMENTO_PADDING_X + s + SUGGERIMENTO_GAP + c,
                               "Salva", "Ctrl+S");
}
static const struct tipo_widget tipo_mostra = { mostra_misura, 0, mostra_disegna, 0, 0, 0, 0, 0 };
static struct widget mostra = { .tipo = &tipo_mostra };

int main(void) {
    colonna_prepara(&colonna, 12);
    riga_prepara(&riga_gruppi, 24);
    pillola_prepara(&frecce, 0);
    pulsante_barra_prepara(&indietro, ICONA_CHEVRON_LEFT, 0, 0);
    pulsante_barra_prepara(&avanti, ICONA_CHEVRON_RIGHT, 0, 0);
    pillola_aggiungi(&frecce, &indietro);
    pillola_aggiungi(&frecce, &avanti);
    pillola_prepara(&frecce_m, 0);
    pillola_dimensione(&frecce_m, MISURA_M);
    pulsante_barra_prepara(&indietro_m, ICONA_CHEVRON_LEFT, 0, 0);
    pulsante_barra_prepara(&avanti_m, ICONA_CHEVRON_RIGHT, 0, 0);
    pillola_aggiungi(&frecce_m, &indietro_m);
    pillola_aggiungi(&frecce_m, &avanti_m);
    widget_aggiungi(&riga_gruppi.base, &frecce.base);
    widget_aggiungi(&riga_gruppi.base, &frecce_m.base);
    widget_aggiungi(&colonna.base, &mostra);
    widget_aggiungi(&colonna.base, &riga_gruppi.base);
    piano_prepara(&piano, &colonna.base);
    finestra_tk_prepara(&finestra, "Suggerimento", &piano.base);
    finestra_tk_dimensione(&finestra, 420, 240);
    return uikit_esegui(&finestra);
}
