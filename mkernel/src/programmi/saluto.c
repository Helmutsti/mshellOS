/* Il primo programma in ring 3.
 *
 * Il codice sta dentro il file del kernel, ma nella zona dei programmi (linker.ld):
 * pagine con il bit utente. Gira in ring 3, sulla sua pila, e per fare qualsiasi
 * cosa fuori dalla sua memoria deve chiedere al kernel dalla porta (libreria.h). */
#include "libreria.h"

void saluto(void) {
    scrivi_testo("[saluto] ciao dal ring 3! Ora dormo un secondo...\n");
    dormi(1000);
    scrivi_testo("[saluto] sveglio. Esco.\n");
    esci();
}
