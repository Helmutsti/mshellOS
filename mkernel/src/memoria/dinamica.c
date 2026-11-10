/* La memoria dinamica dei programmi, come brk nello Unix classico.
 *
 * Ogni programma ha, subito dopo il suo codice e le sue variabili, una zona che puo'
 * crescere: comincia a 'dati_inizio' e finisce al CONFINE, la "rottura" (in inglese
 * "program break"). Il programma sposta il confine con la chiamata rottura(): piu'
 * avanti per avere memoria, piu' indietro per restituirla. La malloc della libreria
 * divide poi quella zona in pezzetti.
 *
 *     codice | variabili | zona dinamica ->     ...      | fogli | schermo | pila
 *     1 GB                ^dati_inizio   ^confine          1,5 GB
 *
 * LE PAGINE ARRIVANO AL PRIMO TOCCO, come in Linux. Spostare il confine non prende
 * nessuna pagina: e' solo una PROMESSA ("fin qui, la memoria e' tua"). La prima volta
 * che il programma tocca una pagina promessa, la pagina non c'e', e il processore si
 * ferma con un page fault. Il gestore chiede qui: e' un indirizzo promesso? Se si',
 * prende una pagina libera, la riempie di zeri, la mette nella mappa e fa ripartire il
 * programma dall'istruzione che aveva fallito: il programma non si accorge di niente.
 * Cosi' chi chiede tanto e usa poco consuma poco. */
#include "kernel.h"
#include "pagine.h"
#include "paginazione.h"
#include "compiti.h"
#include "dinamica.h"

static unsigned int pagina_su(unsigned int x) { return (x + PAGINA - 1) & ~(PAGINA - 1); }

unsigned int memoria_rottura(unsigned int nuova) {
    struct compito *c = compito_attuale();
    if (!c->mappa || !nuova) return c->rottura;
    if (nuova < c->dati_inizio || nuova > FOGLI_INIZIO) return c->rottura;   /* fuori zona */

    /* indietro: le pagine oltre il nuovo confine che erano gia' state toccate tornano
     * al magazziniere (quelle mai toccate non ci sono, non c'e' niente da rendere) */
    for (unsigned int v = pagina_su(nuova); v < pagina_su(c->rottura); v += PAGINA) {
        unsigned int fisico, permessi;
        if (!pag_traduci(v, &fisico, &permessi)) continue;
        pag_togli_programma(c->mappa, v);
        pagina_restituisci(fisico & ~(PAGINA - 1));
    }
    c->rottura = nuova;
    return nuova;
}

int memoria_promessa(unsigned int virt) {
    struct compito *c = compito_attuale();
    if (!c || !c->mappa || virt < c->dati_inizio || virt >= c->rottura) return 0;
    unsigned int fisico, permessi;
    if (pag_traduci(virt, &fisico, &permessi)) return 0;   /* c'e' gia': l'errore e' un altro */
    unsigned int pagina = pagina_chiedi();
    if (!pagina) return 0;                                 /* memoria finita: errore vero */
    memset((void *)pagina, 0, PAGINA);   /* mai dare a un programma i resti di un altro */
    if (!pag_mappa_programma(c->mappa, virt & ~(PAGINA - 1), pagina, PAG_UTENTE | PAG_SCRITTURA)) {
        pagina_restituisci(pagina);
        return 0;
    }
    return 1;
}
