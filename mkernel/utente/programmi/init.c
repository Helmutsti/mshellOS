/* init: il primo programma, come in Linux.
 *
 * Il kernel, finito di prepararsi, lo legge dal disco (/init) e lo fa partire in
 * ring 3. E' lui a far nascere gli altri programmi: per ora la shell. Non finisce mai:
 * dopo resta in attesa. */
#include "libreria.h"

int main(void) {
    scrivi_testo("ciao come va\n");
    if (avvia("/shell") < 0)
        scrivi_testo("init: non riesco ad avviare /shell\n");
    for (;;)
        dormi(60 * 1000);   /* in attesa: dorme, un minuto alla volta, senza usare CPU */
}
