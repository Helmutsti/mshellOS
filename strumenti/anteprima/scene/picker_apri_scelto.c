/* La finestra Apri con appunti.txt scelto (disegno: schermata "Apri con un file scelto").
 * Uso: TEMA=scuro anteprima.sh picker_apri_scelto 1 */
#include "picker_finta.h"

int main(void) {
    costruisci(PICKER_MODO_APRI, 0, &sorgente_finta);
    scelto = 3;
    tabella_tk_scegli(&tabella, 3);
    aggiorna_pulsanti();
    return uikit_esegui(&finestra);
}
