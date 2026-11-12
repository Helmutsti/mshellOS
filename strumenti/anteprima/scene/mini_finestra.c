/* La mini finestra: "Informazioni" su un file, con tre righe etichetta / valore.
 * Uso: TEMA=scuro anteprima.sh mini_finestra 2 */
#include "toolkit.h"

static struct finestra_tk finestra;
static struct contenitore colonna, righe[3];
static struct etichetta nomi[3], valori[3];
static struct spazio vuoti[3];

static const char *etichette[3] = { "Tipo", "Dimensione", "Posizione" };
static const char *dati[3] = { "immagine PNG", "412 KB", "Home" };

int main(void) {
    colonna_prepara(&colonna, SPAZIO_8);
    for (int i = 0; i < 3; i++) {
        riga_prepara(&righe[i], SPAZIO_8);
        etichetta_prepara(&nomi[i], etichette[i], FONT_PICCOLO_NUOVO);
        nomi[i].colore = TESTO_SECONDARIO;
        spazio_prepara(&vuoti[i]);
        etichetta_prepara(&valori[i], dati[i], FONT_PICCOLO_NUOVO);
        widget_aggiungi(&righe[i].base, &nomi[i].base);
        widget_aggiungi(&righe[i].base, &vuoti[i].base);
        widget_aggiungi(&righe[i].base, &valori[i].base);
        widget_aggiungi(&colonna.base, &righe[i].base);
    }
    finestra_tk_prepara(&finestra, "Informazioni - foto-mare.png", &colonna.base);
    finestra_tk_mini(&finestra);
    finestra_tk_dimensione(&finestra, 320, 133);
    return toolkit_esegui(&finestra);
}
