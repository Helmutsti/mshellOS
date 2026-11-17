/* Salva con il nome di un file che esiste gia': l'avviso "Sostituire il file?" (disegno:
 * schermata "Salva - sostituire il file?"). Si preme Salva come farebbe la persona.
 * Uso: TEMA=scuro anteprima.sh picker_sostituisci 1 */
#include "picker_finta.h"

int main(void) {
    costruisci(PICKER_MODO_SALVA, "appunti.txt", &sorgente_finta);
    premi_conferma(0, 0);
    return uikit_esegui(&finestra);
}
