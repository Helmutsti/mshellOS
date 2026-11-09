/* Il lato del kernel delle chiamate di sistema (vedi chiamate.h).
 *
 * Gira in ring 0, sulla pila del kernel del programma, a interruzioni spente
 * (la porta e' una "porta di interruzione"). I registri del programma sono in 'r':
 * gli argomenti si leggono da li', il risultato si scrive in r->eax e la CPU lo
 * restituira' al programma con iret. */
#include "kernel.h"
#include "idt.h"
#include "pagine.h"
#include "paginazione.h"
#include "compiti.h"
#include "chiamate.h"

#define MAX_SCRITTURA 1024

/* NON SI CREDE AL PROGRAMMA. Un indirizzo che arriva dal ring 3 e' un numero
 * qualsiasi: potrebbe puntare alla memoria del kernel, e il kernel (ring 0) la
 * leggerebbe senza problemi, stampando i suoi segreti. Quindi si controlla, pagina
 * per pagina, che tutto il pezzo stia in memoria che il programma puo' toccare. */
static int del_programma(unsigned int inizio, unsigned int byte) {
    if (byte > MAX_SCRITTURA || inizio + byte < inizio) return 0;   /* troppo, o fa il giro */
    for (unsigned int p = inizio & ~(PAGINA - 1); p < inizio + byte; p += PAGINA) {
        unsigned int fisico, permessi;
        if (!pag_traduci(p, &fisico, &permessi) || !(permessi & PAG_UTENTE)) return 0;
    }
    return 1;
}

unsigned int chiamata_di_sistema(struct registri *r) {
    switch (r->eax) {
    case SIS_SCRIVI: {
        const char *testo = (const char *)r->ebx;
        unsigned int byte = r->ecx;
        if (!del_programma((unsigned int)testo, byte)) { r->eax = -1; break; }
        for (unsigned int i = 0; i < byte; i++) console_putc(testo[i]);
        r->eax = byte;
        break;
    }
    case SIS_DORMI:
        /* dormi() cede il turno: questo compito si ferma QUI, dentro il kernel, sulla
         * sua pila del kernel. Quando si sveglia riprende da qui e torna al programma. */
        dormi(r->ebx);
        r->eax = 0;
        break;
    case SIS_ESCI:
        /* come un compito che sbaglia: si segna finito e si passa a un altro.
         * Lo scheduler liberera' pile e scheda. Al programma non si torna piu'. */
        compito_attuale()->stato = FINITO;
        return scheduler((unsigned int)r);
    default:
        r->eax = -1;   /* servizio che non esiste */
    }
    return (unsigned int)r;   /* si torna al programma */
}
