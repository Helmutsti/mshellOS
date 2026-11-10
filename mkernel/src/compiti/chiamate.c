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
#include "eventi.h"
#include "descrittori.h"
#include "socket.h"
#include "fogli.h"
#include "timer.h"
#include "chiamate.h"

#define MAX_SCRITTURA 1024

/* NON SI CREDE AL PROGRAMMA. Un indirizzo che arriva dal ring 3 e' un numero
 * qualsiasi: potrebbe puntare alla memoria del kernel, e il kernel (ring 0) la
 * leggerebbe senza problemi, stampando i suoi segreti. Quindi si controlla, pagina
 * per pagina, che tutto il pezzo stia in memoria che il programma puo' toccare.
 * E se il kernel ci deve SCRIVERE (una risposta per il programma), che il programma
 * stesso ci possa scrivere: se no il kernel scriverebbe nel suo codice, o farebbe page
 * fault in ring 0 (panico) su una pagina di sola lettura. */
#define LEGGERE  PAG_UTENTE
#define SCRIVERE (PAG_UTENTE | PAG_SCRITTURA)
static int del_programma(unsigned int inizio, unsigned int byte, unsigned int serve) {
    if (byte > MAX_SCRITTURA || inizio + byte < inizio) return 0;   /* troppo, o fa il giro */
    for (unsigned int p = inizio & ~(PAGINA - 1); p < inizio + byte; p += PAGINA) {
        unsigned int fisico, permessi;
        if (!pag_traduci(p, &fisico, &permessi) || (permessi & serve) != serve) return 0;
    }
    return 1;
}

/* Una stringa dalla memoria del programma (un percorso, un nome) copiata nel kernel,
 * una lettera alla volta, controllando ogni lettera, fino allo 0 finale. 0 se e' troppo
 * lunga o esce dalla sua memoria. Si copia per non rileggerla dopo: il programma
 * potrebbe cambiarla mentre il kernel la usa. */
static int copia_stringa(char *dove, const char *da, int massimo) {
    for (int i = 0; i < massimo; i++) {
        if (!del_programma((unsigned int)(da + i), 1, LEGGERE)) return 0;
        dove[i] = da[i];
        if (!dove[i]) return 1;
    }
    return 0;
}

unsigned int chiamata_di_sistema(struct registri *r) {
    switch (r->eax) {
    case SIS_SCRIVI: {
        const char *testo = (const char *)r->ebx;
        unsigned int byte = r->ecx;
        if (!del_programma((unsigned int)testo, byte, LEGGERE)) { r->eax = -1; break; }
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
    case SIS_APRI_EVENTI: {
        struct aperto *a = eventi_apri();
        r->eax = -1;
        if (!a) break;                      /* gli eventi sono gia' di un altro */
        int d = descrittore_nuovo(a);
        if (d < 0) { a->op->chiudi(a); break; }
        r->eax = d;
        break;
    }
    case SIS_AVVIA: {
        char percorso[MAX_PERCORSO];
        if (!copia_stringa(percorso, (const char *)r->ebx, MAX_PERCORSO)) { r->eax = -1; break; }

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
        if (!del_programma((unsigned int)info, sizeof *info, SCRIVERE) || !s) break;
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
    case SIS_ASCOLTA:
    case SIS_COLLEGA: {
        char nome[MAX_NOME] = {0};
        if (!copia_stringa(nome, (const char *)r->ebx, MAX_NOME)) { r->eax = -1; break; }
        r->eax = r->eax == SIS_ASCOLTA ? socket_ascolta(nome) : socket_collega(nome);
        break;
    }
    case SIS_ACCETTA:
        /* puo' dormire, come dormi(): si riparte da qui */
        r->eax = socket_accetta(r->ebx);
        break;
    case SIS_MANDA:
    case SIS_RICEVI: {
        /* Si controlla che il pezzo stia nella sua memoria, poi lo si passa alla cosa
         * aperta: se dorme, al risveglio la mappa e' di nuovo la sua, e il pezzo e'
         * ancora li'. */
        struct aperto *a = descrittore(r->ebx);
        unsigned int dati = r->ecx, byte = r->edx;
        if (!a || !del_programma(dati, byte, r->eax == SIS_MANDA ? LEGGERE : SCRIVERE)) {
            r->eax = -1;
            break;
        }
        if (r->eax == SIS_MANDA) {
            /* l'allegato: un descrittore del mittente, o -1 per niente */
            struct aperto *allegato = (int)r->esi == -1 ? 0 : descrittore(r->esi);
            if ((int)r->esi != -1 && !allegato) { r->eax = -1; break; }
            r->eax = a->op->manda(a, (const void *)dati, byte, allegato, r->edi == MANDA_SUBITO);
            break;
        }
        /* dove scrivere il descrittore dell'allegato (0 = non interessa): si controlla
         * PRIMA di ricevere, per non perdere un messaggio gia' preso dalla casella */
        int *dove_allegato = (int *)r->esi;
        if (dove_allegato && !del_programma((unsigned int)dove_allegato, sizeof(int), SCRIVERE)) {
            r->eax = -1;
            break;
        }
        struct aperto *allegato = 0;
        r->eax = a->op->ricevi(a, (void *)dati, byte, &allegato);
        int d = -1;
        if (allegato) {
            /* l'uso del messaggio diventa un descrittore in questo programma. Se non lo
             * vuole, o la sua tabella e' piena, il foglio si lascia. */
            if (dove_allegato) d = descrittore_nuovo(allegato);
            aperto_lascia(allegato);
        }
        if (dove_allegato) *dove_allegato = d;
        break;
    }
    case SIS_FOGLIO:
        r->eax = foglio_nuovo(r->ebx);
        break;
    case SIS_MAPPA: {
        unsigned int *dove_byte = (unsigned int *)r->ecx, byte;
        if (dove_byte && !del_programma((unsigned int)dove_byte, sizeof byte, SCRIVERE)) {
            r->eax = -1;
            break;
        }
        unsigned int indirizzo = foglio_mappa(r->ebx, &byte);
        if (indirizzo && dove_byte) *dove_byte = byte;
        r->eax = indirizzo ? indirizzo : (unsigned int)-1;
        break;
    }
    case SIS_TOGLI:
        r->eax = foglio_togli(r->ebx);
        break;
    case SIS_CHIUDI:
        r->eax = descrittore_chiudi(r->ebx);
        break;
    case SIS_ASPETTA: {
        /* l'elenco si copia nel kernel, si aspetta, e si ricopia con 'pronto' segnato */
        struct da_guardare *elenco = (struct da_guardare *)r->ebx, copia[MAX_GUARDATI];
        int quanti = r->ecx;
        r->eax = -1;
        if (quanti < 1 || quanti > MAX_GUARDATI ||
            !del_programma((unsigned int)elenco, quanti * sizeof *elenco, SCRIVERE))
            break;
        memcpy(copia, elenco, quanti * sizeof *elenco);
        int pronti = descrittori_aspetta(copia, quanti, (int)r->edx);
        if (pronti >= 0) memcpy(elenco, copia, quanti * sizeof *elenco);
        r->eax = pronti;
        break;
    }
    case SIS_TEMPO:
        r->eax = timer_tick() * 10;   /* il timer va a 100 Hz: un tick ogni 10 ms */
        break;
    case SIS_TERMINA: {
        struct compito *c = socket_altro(r->ebx);
        r->eax = -1;
        if (!c || c == compito_attuale()) break;
        compito_termina(c);
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
