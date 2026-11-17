/* La finestra Apri (disegno: disegni/ApriSalva/, schermata "Apri", nessun file scelto).
 * Uso: TEMA=scuro anteprima.sh picker_apri 1 */
#include "picker_finta.h"

int main(void) {
    costruisci(PICKER_MODO_APRI, 0, &sorgente_finta);
    return uikit_esegui(&finestra);
}
