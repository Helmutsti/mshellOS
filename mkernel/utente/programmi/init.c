/* init: il primo programma, come in Linux.
 *
 * Il kernel, finito di prepararsi, lo legge dal disco (/init) e lo fa partire in
 * ring 3. E' lui a far nascere gli altri programmi: per ora il compositore, che si
 * prende lo schermo. Non finisce mai: dopo resta in attesa. */
#include "libreria.h"

int main(void) {
    scrivi_testo("ciao come va\n");
    if (avvia("/compositore") < 0)
        scrivi_testo("init: non riesco ad avviare /compositore\n");
    for (;;)
        dormi(60 * 1000);   /* in attesa: dorme, un minuto alla volta, senza usare CPU */
}
