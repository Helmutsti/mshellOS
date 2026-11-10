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
#include "tastiera.h"
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
    case SIS_LEGGI: {
        /* dorme finche' non arriva un tasto, come dormi(): si riparte da qui */
        char c = tastiera_aspetta();
        r->eax = c ? (unsigned char)c : (unsigned int)-1;
        break;
    }
    case SIS_AVVIA: {
        /* il percorso sta nella memoria del programma: lo si copia nel kernel una
         * lettera alla volta, controllando ogni lettera, fino allo 0 finale */
        char percorso[MAX_PERCORSO];
        const char *da = (const char *)r->ebx;
        int finito = 0;
        for (int i = 0; i < MAX_PERCORSO && !finito; i++) {
            if (!del_programma((unsigned int)(da + i), 1)) break;   /* fuori dalla sua memoria */
            percorso[i] = da[i];
            finito = percorso[i] == 0;
        }
        if (!finito) { r->eax = -1; break; }   /* troppo lungo, o fuori dalla sua memoria */

        const char *errore;
        struct compito *c = compito_crea_programma(percorso, &errore);
        r->eax = c ? (unsigned int)c->numero : (unsigned int)-1;
        break;
    }
    case SIS_SCHERMO: {
        /* Lo schermo diventa del programma: le pagine della memoria dello schermo (che
         * non sono RAM del magazziniere) vanno nella SUA mappa, a SCHERMO_PROGRAMMA,
         * con il bit utente. Segnate PRESTATE: quando il programma finisce, non si
         * restituiscono al magazziniere. Come mmap di /dev/fb0 in Linux. */
        struct info_schermo *info = (struct info_schermo *)r->ebx;
        const struct schermo *s = console_schermo();
        r->eax = -1;
        if (!del_programma((unsigned int)info, sizeof *info) || !s) break;
        unsigned int byte = s->passo * s->altezza;
        if (s->indirizzo % PAGINA || byte > PILA_PROGRAMMA_CIMA - PAGINA - SCHERMO_PROGRAMMA)
            break;                                 /* non ci sta nella zona dei programmi */
        struct compito *chi = compito_attuale();
        if (!console_presta(chi)) break;           /* e' gia' di qualcun altro */

        unsigned int mappa = chi->mappa;
        for (unsigned int p = 0; p < byte; p += PAGINA)
            if (!pag_mappa_programma(mappa, SCHERMO_PROGRAMMA + p, s->indirizzo + p,
                                     PAG_UTENTE | PAG_SCRITTURA | PAG_PRESTATA)) {
                console_riprendi(chi);             /* memoria finita per le tabelle */
                return (unsigned int)r;
            }
        info->passo = s->passo;
        info->larghezza = s->larghezza;
        info->altezza = s->altezza;
        info->rosso = s->rosso;
        info->verde = s->verde;
        info->blu = s->blu;
        r->eax = 0;
        break;
    }
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
