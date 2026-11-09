/* init: il primo programma, come in Linux.
 *
 * Il kernel, finito di prepararsi, lo legge dal disco (/init) e lo fa partire in
 * ring 3. Non finisce mai: saluta e poi resta in attesa. */
#include "libreria.h"

int main(void) {
    scrivi_testo("ciao come va\n");
    for (;;)
        dormi(60 * 1000);   /* in attesa: dorme, un minuto alla volta, senza usare CPU */
}
