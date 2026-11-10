/* init: il primo programma, come in Linux.
 *
 * Il kernel, finito di prepararsi, lo legge dal disco (/Sistema/init) e lo fa partire in
 * ring 3, come sistema (uid 0). E' lui a far nascere gli altri programmi: il compositore
 * e (finche' non c'e' un lanciatore) tre applicazioni: esempio, che si disegna tutto a
 * mano, contatore e impostazioni, fatte con il toolkit.
 *
 * INIT TIENE LE RISORSE DEL COMPOSITORE, e gliele presta. E' lui ad aprire lo schermo,
 * la tastiera e il mouse, e a mettersi in ascolto con il nome "compositore"; poi fa
 * partire il compositore passandogli queste tre cose gia' aperte (avvia_con), come
 * launchd sul Mac e systemd su Linux. Init non le usa: le tiene soltanto in mano.
 *
 * E LO SORVEGLIA. Se il compositore cade, init se ne accorge (il descrittore del
 * processo diventa pronto) e lo fa ripartire, con le stesse risorse. Siccome init non
 * le ha mai lasciate, nel frattempo nessun altro ha potuto prenderle: lo schermo non
 * torna alla console, il nome resta occupato, e le applicazioni che si collegano proprio
 * in quel momento aspettano in fila, invece di trovare la porta chiusa. */
#include "libreria.h"
#include "protocollo.h"

#define COMPOSITORE "/Sistema/Servizi/compositore"

static void numero(int n) {
    char c[12];
    int i = 11;
    c[i] = 0;
    do { c[--i] = '0' + n % 10; n /= 10; } while (n);
    scrivi_testo(&c[i]);
}

int main(void) {
    scrivi_testo("ciao come va\n");

    /* le risorse del compositore, nell'ordine in cui le trovera': 0, 1, 2 */
    int risorse[3] = { apri_schermo(), apri_eventi(), ascolta(NOME_COMPOSITORE) };
    if (risorse[0] < 0 || risorse[1] < 0 || risorse[2] < 0) {
        scrivi_testo("init: non riesco a prendere schermo, tastiera o il nome del compositore\n");
        for (;;) dormi(60 * 1000);
    }

    int volte = 0;
    for (;;) {
        int n = avvia_con(COMPOSITORE, risorse, 3);
        if (n < 0) {
            scrivi_testo("init: non riesco ad avviare " COMPOSITORE "\n");
            dormi(5000);
            continue;
        }
        if (volte++ == 0) {
            /* le applicazioni non sono del sistema: partono come UID_APPLICAZIONI (1000).
             * Il nome "compositore" c'e' gia' (lo tiene init): si collegano subito */
            avvia_come("/Applicazioni/Esempio/Esempio", UID_APPLICAZIONI);
            avvia_come("/Applicazioni/Contatore/Contatore", UID_APPLICAZIONI);
            avvia_come("/Applicazioni/Impostazioni/Impostazioni", UID_APPLICAZIONI);
        }

        /* aspetta che finisca: ricevi sul descrittore del processo torna quando e' finito */
        int p = processo(n);
        char niente;
        if (p >= 0) {
            ricevi(p, &niente, 1);
            chiudi(p);
        }
        scrivi_testo("init: il compositore e' finito, lo faccio ripartire (");
        numero(volte);
        scrivi_testo(")\n");
        dormi(500);   /* se cade subito ogni volta, almeno non si gira a vuoto */
    }
}
