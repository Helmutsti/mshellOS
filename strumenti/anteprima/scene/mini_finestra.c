/* La mini finestra delle informazioni di un file (informazioni.h), con tre righe etichetta /
 * valore, come nel disegno ("Finestra mini - Informazioni").
 * Uso: TEMA=scuro anteprima.sh mini_finestra 2 */
#include "uikit/uikit.h"

static struct finestra_tk finestra;
static struct informazioni_tk informazioni;

int main(void) {
    informazioni_prepara(&finestra, &informazioni);
    informazioni_riga(&informazioni, "Tipo immagine", "PNG");
    informazioni_riga(&informazioni, "Dimensione", "412 KB");
    informazioni_riga(&informazioni, "Posizione", "Home");
    copia_testo(finestra.titolo, "Informazioni - foto-mare.png", MAX_TITOLO);
    return uikit_esegui(&finestra);
}
