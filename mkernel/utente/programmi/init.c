/* init: il primo programma, come in Linux.
 *
 * Il kernel, finito di prepararsi, lo legge dal disco (/Sistema/init) e lo fa partire in
 * ring 3, come sistema (uid 0). E' lui a far nascere gli altri programmi: il
 * compositore, che si prende lo schermo, e (finche' non c'e' un lanciatore) tre
 * applicazioni: esempio, che si disegna tutto a mano, contatore e impostazioni, fatte
 * con il toolkit. Non finisce mai: dopo resta in attesa. */
#include "libreria.h"

int main(void) {
    scrivi_testo("ciao come va\n");
    if (avvia("/Sistema/Servizi/compositore") < 0)
        scrivi_testo("init: non riesco ad avviare /Sistema/Servizi/compositore\n");
    /* le applicazioni non sono del sistema: partono come UID_APPLICAZIONI (1000) e
     * aspettano da sole che il compositore sia pronto */
    avvia_come("/esempio", UID_APPLICAZIONI);
    avvia_come("/contatore", UID_APPLICAZIONI);
    avvia_come("/impostazioni", UID_APPLICAZIONI);
    for (;;)
        dormi(60 * 1000);   /* in attesa: dorme, un minuto alla volta, senza usare CPU */
}
