/* La finestra Salva con il nome "Senza titolo.txt" (disegno: schermata "Salva").
 * Uso: TEMA=scuro anteprima.sh picker_salva 1 */
#include "picker_finta.h"

int main(void) {
    costruisci(PICKER_MODO_SALVA, "Senza titolo.txt", &sorgente_finta);
    return uikit_esegui(&finestra);
}
