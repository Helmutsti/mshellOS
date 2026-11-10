/* Il lato del kernel delle chiamate di sistema (vedi chiamate.h).
 *
 * Gira in ring 0, sulla pila del kernel del programma, a interruzioni spente
 * (la porta e' una "porta di interruzione"). I registri del programma sono in 'r':
 * il numero del servizio, gli argomenti e il risultato passano dallo sportello della
 * CPU (arch.h: chiamata_numero, chiamata_argomento, chiamata_risultato), che sa in quali
 * registri stanno. Al ritorno, la CPU restituira' il risultato al programma. */
#include "kernel.h"
#include "pagine.h"
#include "paginazione.h"
#include "compiti.h"
#include "eventi.h"
#include "descrittori.h"
#include "socket.h"
#include "fogli.h"
#include "timer.h"
#include "dinamica.h"
#include "mfs.h"
#include "file.h"
#include "schermo.h"
#include "processo.h"
#include "orologio.h"
#include "chiamate.h"

#define MAX_SCRITTURA (1024 * 1024)   /* il pezzo piu' grande in una chiamata: 1 MB */

/* NON SI CREDE AL PROGRAMMA. Un indirizzo che arriva dal ring 3 e' un numero
 * qualsiasi: potrebbe puntare alla memoria del kernel, e il kernel (ring 0) la
 * leggerebbe senza problemi, stampando i suoi segreti. Quindi si controlla, pagina
 * per pagina, che tutto il pezzo stia in memoria che il programma puo' toccare.
 * E se il kernel ci deve SCRIVERE (una risposta per il programma), che il programma
 * stesso ci possa scrivere: se no il kernel scriverebbe nel suo codice, o farebbe page
 * fault in ring 0 (panico) su una pagina di sola lettura. */
#define LEGGERE  PAG_UTENTE
#define SCRIVERE (PAG_UTENTE | PAG_SCRITTURA)
static int del_programma(indirizzo_t inizio, unsigned int byte, unsigned int serve) {
    if (byte > MAX_SCRITTURA || inizio + byte < inizio) return 0;   /* troppo, o fa il giro */
    for (indirizzo_t p = inizio & ~(indirizzo_t)(PAGINA - 1); p < inizio + byte; p += PAGINA) {
        indirizzo_t fisico;
        unsigned int permessi;
        if (!pag_traduci(p, &fisico, &permessi)) {
            /* non c'e': forse e' una pagina promessa della memoria dinamica, mai toccata */
            if (!memoria_promessa(p) || !pag_traduci(p, &fisico, &permessi)) return 0;
        }
        if ((permessi & serve) != serve) return 0;
    }
    return 1;
}

/* Una stringa dalla memoria del programma (un percorso, un nome) copiata nel kernel,
 * una lettera alla volta, controllando ogni lettera, fino allo 0 finale. 0 se e' troppo
 * lunga o esce dalla sua memoria. Si copia per non rileggerla dopo: il programma
 * potrebbe cambiarla mentre il kernel la usa. */
static int lunghezza_c(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static int copia_stringa(char *dove, const char *da, int massimo) {
    for (int i = 0; i < massimo; i++) {
        if (!del_programma((indirizzo_t)(da + i), 1, LEGGERE)) return 0;
        dove[i] = da[i];
        if (!dove[i]) return 1;
    }
    return 0;
}

/* /home E' LA CASA DI CHI CHIEDE. Come i mount namespace di Linux: ogni programma vede
 * il file system a modo suo. Per un programma con uid U, "/home" e' "/Utenti/U": per
 * un'applicazione (1000) "/home/Scrivania" e' "/Utenti/1000/Scrivania". Il kernel lo
 * traduce qui, appena riceve un percorso, e poi tutto il resto vede il percorso vero.
 * 0 se il percorso tradotto non ci sta. */
static int traduci_home(char *percorso) {
    static const char home[] = "/home";
    int i = 0;
    while (home[i] && percorso[i] == home[i]) i++;
    if (home[i] || (percorso[i] && percorso[i] != '/')) return 1;   /* non comincia con /home */

    char casa[24] = "/Utenti/", cifre[12];
    unsigned int uid = compito_attuale()->uid;
    int n = 0, l = 8;
    do { cifre[n++] = '0' + uid % 10; uid /= 10; } while (uid);
    while (n) casa[l++] = cifre[--n];
    int resto = lunghezza_c(percorso + i);
    if (l + resto + 1 > MAX_PERCORSO) return 0;
    memmove(percorso + l, percorso + i, resto + 1);   /* il resto, con lo 0 finale */
    memcpy(percorso, casa, l);
    return 1;
}

/* Un percorso dalla memoria del programma, copiato e tradotto (/home). */
static int copia_percorso(char *dove, const char *da) {
    return copia_stringa(dove, da, MAX_PERCORSO) && traduci_home(dove);
}

indirizzo_t chiamata_di_sistema(struct registri *r) {
    const unsigned int numero = chiamata_numero(r);   /* quale servizio */
    switch (numero) {
    case SIS_SCRIVI: {
        const char *testo = (const char *)chiamata_argomento(r, 1);
        unsigned int byte = chiamata_argomento(r, 2);
        if (!del_programma((indirizzo_t)testo, byte, LEGGERE)) { chiamata_risultato(r, -1); break; }
        for (unsigned int i = 0; i < byte; i++) console_putc(testo[i]);
        chiamata_risultato(r, byte);
        break;
    }
    case SIS_DORMI:
        /* dormi() cede il turno: questo compito si ferma QUI, dentro il kernel, sulla
         * sua pila del kernel. Quando si sveglia riprende da qui e torna al programma. */
        dormi(chiamata_argomento(r, 1));
        chiamata_risultato(r, 0);
        break;
    case SIS_APRI_EVENTI: {
        struct aperto *a = eventi_apri();
        chiamata_risultato(r, -1);
        if (!a) break;                      /* gli eventi sono gia' di un altro */
        int d = descrittore_nuovo(a);
        if (d < 0) { a->op->chiudi(a); break; }
        chiamata_risultato(r, d);
        break;
    }
    case SIS_AVVIA: {
        char percorso[MAX_PERCORSO];
        if (!copia_percorso(percorso, (const char *)chiamata_argomento(r, 1))) { chiamata_risultato(r, -1); break; }

        /* serve il permesso di eseguirlo; il programma nuovo e' "di" chi lo fa partire */
        const char *errore;
        struct compito *c = file_puo(percorso, PERMESSO_ESEGUIRE)
                          ? compito_crea_programma(percorso, compito_attuale()->uid, &errore) : 0;
        chiamata_risultato(r, c ? (indirizzo_t)c->numero : (indirizzo_t)-1);
        break;
    }
    case SIS_AVVIA_COME: {
        /* far partire un programma come un altro: solo il sistema (si puo' solo scendere) */
        char percorso[MAX_PERCORSO];
        unsigned int uid = chiamata_argomento(r, 2);
        chiamata_risultato(r, -1);
        if (compito_attuale()->uid != UID_SISTEMA || !copia_percorso(percorso, (const char *)chiamata_argomento(r, 1))) break;
        const char *errore;
        struct compito *c = compito_crea_programma(percorso, uid, &errore);
        if (c) chiamata_risultato(r, c->numero);
        break;
    }
    case SIS_CHI: {
        struct compito *c = socket_altro(chiamata_argomento(r, 1));
        chiamata_risultato(r, c ? c->uid : (indirizzo_t)-1);
        break;
    }
    case SIS_APRI_SCHERMO: {
        struct aperto *a = schermo_apri();
        chiamata_risultato(r, -1);
        if (!a) break;                         /* e' gia' aperto */
        int d = descrittore_nuovo(a);
        if (d < 0) { a->op->chiudi(a); break; }
        chiamata_risultato(r, d);
        break;
    }
    case SIS_MAPPA_SCHERMO: {
        /* Le pagine della memoria dello schermo (che non sono RAM del magazziniere) vanno
         * nella mappa di chi tiene il descrittore, a SCHERMO_PROGRAMMA, con il bit utente.
         * Segnate PRESTATE: quando il programma finisce, non si restituiscono al
         * magazziniere. Come mmap di /dev/fb0 in Linux. */
        struct aperto *a = descrittore(chiamata_argomento(r, 1));
        struct info_schermo *info = (struct info_schermo *)chiamata_argomento(r, 2);
        const struct schermo *s = console_schermo();
        chiamata_risultato(r, -1);
        if (!a || !e_lo_schermo(a) || !s || !del_programma((indirizzo_t)info, sizeof *info, SCRIVERE))
            break;
        unsigned int byte = s->passo * s->altezza;
        if (s->indirizzo % PAGINA || byte > PILA_PROGRAMMA_CIMA - PAGINA - SCHERMO_PROGRAMMA)
            break;                                 /* non ci sta nella zona dei programmi */
        indirizzo_t mappa = compito_attuale()->mappa;
        unsigned int p;
        for (p = 0; p < byte; p += PAGINA)
            if (!pag_mappa_programma(mappa, SCHERMO_PROGRAMMA + p, s->indirizzo + p,
                                     PAG_UTENTE | PAG_SCRITTURA | PAG_PRESTATA)) break;
        if (p < byte) break;                       /* memoria finita per le tabelle */
        info->passo = s->passo;
        info->larghezza = s->larghezza;
        info->altezza = s->altezza;
        info->rosso = s->rosso;
        info->verde = s->verde;
        info->blu = s->blu;
        chiamata_risultato(r, 0);
        break;
    }
    case SIS_AVVIA_CON: {
        /* Come avvia, ma il figlio nasce con dei descrittori gia' aperti: gli stessi di chi
         * lo fa partire (ognuno e' un uso in piu', vedi descrittori.h), ai posti 0, 1, 2...
         * Come i descrittori ereditati di Unix, e come la "socket activation" di systemd. */
        char percorso[MAX_PERCORSO];
        const int *elenco = (const int *)chiamata_argomento(r, 2);
        int quanti = chiamata_argomento(r, 3);
        struct aperto *passati[MAX_PASSATI];
        chiamata_risultato(r, -1);
        if (quanti < 0 || quanti > MAX_PASSATI || !copia_percorso(percorso, (const char *)chiamata_argomento(r, 1)) ||
            (quanti && !del_programma((indirizzo_t)elenco, quanti * sizeof(int), LEGGERE)))
            break;
        int ok = 1;
        for (int i = 0; i < quanti && ok; i++) {
            passati[i] = descrittore(elenco[i]);
            ok = passati[i] && !e_un_capo(passati[i]);
        }
        if (!ok || !file_puo(percorso, PERMESSO_ESEGUIRE)) break;
        const char *errore;
        struct compito *c = compito_crea_programma(percorso, compito_attuale()->uid, &errore);
        if (!c) break;
        /* il figlio non gira finche' non si torna dalla chiamata (le interruzioni sono
         * spente): i descrittori sono al loro posto prima della sua prima istruzione */
        for (int i = 0; i < quanti; i++) {
            c->aperti[i] = passati[i];
            aperto_tieni(passati[i]);
        }
        chiamata_risultato(r, c->numero);
        break;
    }
    case SIS_DATA_ORA: {
        struct data_ora *d = (struct data_ora *)chiamata_argomento(r, 1);
        chiamata_risultato(r, -1);
        if (!del_programma((indirizzo_t)d, sizeof *d, SCRIVERE)) break;
        orologio_leggi(d);
        chiamata_risultato(r, 0);
        break;
    }
    case SIS_PROGRAMMA_DI: {
        struct compito *c = socket_altro(chiamata_argomento(r, 1));
        char *dove = (char *)chiamata_argomento(r, 2);
        chiamata_risultato(r, -1);
        if (!c || !del_programma((indirizzo_t)dove, MAX_PERCORSO, SCRIVERE)) break;
        memcpy(dove, c->percorso, MAX_PERCORSO);
        chiamata_risultato(r, 0);
        break;
    }
    case SIS_ELENCO_PROCESSI: {
        struct info_processo *v = (struct info_processo *)chiamata_argomento(r, 1);
        int max = chiamata_argomento(r, 2);
        chiamata_risultato(r, -1);
        if (max < 0 || max > 256 || !del_programma((indirizzo_t)v, max * sizeof *v, SCRIVERE)) break;
        chiamata_risultato(r, compiti_elenco(v, max));
        break;
    }
    case SIS_TERMINA_PROCESSO: {
        /* la regola di Unix: il sistema termina chiunque, gli altri solo i loro processi.
         * L'inattivo (0) e init (1) mai */
        struct compito *c = compito_numero(chiamata_argomento(r, 1)), *io = compito_attuale();
        chiamata_risultato(r, -1);
        if (!c || c->numero <= 1 || c->stato == FINITO) break;
        if (io->uid != UID_SISTEMA && c->uid != io->uid) break;
        compito_termina(c);
        chiamata_risultato(r, 0);
        break;
    }
    case SIS_MEMORIA: {
        struct info_memoria *m = (struct info_memoria *)chiamata_argomento(r, 1);
        chiamata_risultato(r, -1);
        if (!del_programma((indirizzo_t)m, sizeof *m, SCRIVERE)) break;
        m->totale = memoria_fine();
        m->libera = pagine_libere() * PAGINA;
        m->condivisa = fogli_pagine() * PAGINA;
        chiamata_risultato(r, 0);
        break;
    }
    case SIS_PROCESSO:
        chiamata_risultato(r, processo_apri(chiamata_argomento(r, 1)));
        break;
    case SIS_ASCOLTA:
    case SIS_COLLEGA: {
        char nome[MAX_NOME] = {0};
        if (!copia_stringa(nome, (const char *)chiamata_argomento(r, 1), MAX_NOME)) { chiamata_risultato(r, -1); break; }
        chiamata_risultato(r, numero == SIS_ASCOLTA ? socket_ascolta(nome) : socket_collega(nome));
        break;
    }
    case SIS_ACCETTA:
        /* puo' dormire, come dormi(): si riparte da qui */
        chiamata_risultato(r, socket_accetta(chiamata_argomento(r, 1)));
        break;
    case SIS_MANDA:
    case SIS_RICEVI: {
        /* Si controlla che il pezzo stia nella sua memoria, poi lo si passa alla cosa
         * aperta: se dorme, al risveglio la mappa e' di nuovo la sua, e il pezzo e'
         * ancora li'. */
        struct aperto *a = descrittore(chiamata_argomento(r, 1));
        indirizzo_t dati = chiamata_argomento(r, 2);
        unsigned int byte = chiamata_argomento(r, 3);
        if (!a || !del_programma(dati, byte, numero == SIS_MANDA ? LEGGERE : SCRIVERE)) {
            chiamata_risultato(r, -1);
            break;
        }
        if (numero == SIS_MANDA) {
            /* l'allegato: un descrittore del mittente, o -1 per niente */
            struct aperto *allegato = (int)chiamata_argomento(r, 4) == -1 ? 0 : descrittore(chiamata_argomento(r, 4));
            if ((int)chiamata_argomento(r, 4) != -1 && !allegato) { chiamata_risultato(r, -1); break; }
            chiamata_risultato(r, a->op->manda(a, (const void *)dati, byte, allegato, chiamata_argomento(r, 5) == MANDA_SUBITO));
            break;
        }
        /* dove scrivere il descrittore dell'allegato (0 = non interessa): si controlla
         * PRIMA di ricevere, per non perdere un messaggio gia' preso dalla casella */
        int *dove_allegato = (int *)chiamata_argomento(r, 4);
        if (dove_allegato && !del_programma((indirizzo_t)dove_allegato, sizeof(int), SCRIVERE)) {
            chiamata_risultato(r, -1);
            break;
        }
        struct aperto *allegato = 0;
        chiamata_risultato(r, a->op->ricevi(a, (void *)dati, byte, &allegato));
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
        chiamata_risultato(r, foglio_nuovo(chiamata_argomento(r, 1)));
        break;
    case SIS_MAPPA: {
        unsigned int *dove_byte = (unsigned int *)chiamata_argomento(r, 2), byte;
        if (dove_byte && !del_programma((indirizzo_t)dove_byte, sizeof byte, SCRIVERE)) {
            chiamata_risultato(r, -1);
            break;
        }
        indirizzo_t indirizzo = foglio_mappa(chiamata_argomento(r, 1), &byte);
        if (indirizzo && dove_byte) *dove_byte = byte;
        chiamata_risultato(r, indirizzo ? indirizzo : (indirizzo_t)-1);
        break;
    }
    case SIS_TOGLI:
        chiamata_risultato(r, foglio_togli(chiamata_argomento(r, 1)));
        break;
    case SIS_CHIUDI:
        chiamata_risultato(r, descrittore_chiudi(chiamata_argomento(r, 1)));
        break;
    case SIS_ASPETTA: {
        /* l'elenco si copia nel kernel, si aspetta, e si ricopia con 'pronto' segnato */
        struct da_guardare *elenco = (struct da_guardare *)chiamata_argomento(r, 1), copia[MAX_GUARDATI];
        int quanti = chiamata_argomento(r, 2);
        chiamata_risultato(r, -1);
        if (quanti < 1 || quanti > MAX_GUARDATI ||
            !del_programma((indirizzo_t)elenco, quanti * sizeof *elenco, SCRIVERE))
            break;
        memcpy(copia, elenco, quanti * sizeof *elenco);
        int pronti = descrittori_aspetta(copia, quanti, (int)chiamata_argomento(r, 3));
        if (pronti >= 0) memcpy(elenco, copia, quanti * sizeof *elenco);
        chiamata_risultato(r, pronti);
        break;
    }
    case SIS_APRI:
    case SIS_INFO:
    case SIS_CREA_CARTELLA:
    case SIS_CANCELLA: {
        char percorso[MAX_PERCORSO] = {0};
        unsigned int servizio = numero;
        chiamata_risultato(r, -1);
        if (!copia_percorso(percorso, (const char *)chiamata_argomento(r, 1))) break;
        if (servizio == SIS_APRI) {
            chiamata_risultato(r, file_apri(percorso, chiamata_argomento(r, 2)));
        } else if (servizio == SIS_INFO) {
            struct info_file *info = (struct info_file *)chiamata_argomento(r, 2);
            int tipo;
            unsigned int dimensione, proprietario, permessi;
            if (!del_programma((indirizzo_t)info, sizeof *info, SCRIVERE) ||
                mfs_info(percorso, &tipo, &dimensione, &proprietario, &permessi) < 0) break;
            info->tipo = tipo;
            info->dimensione = dimensione;
            info->proprietario = proprietario;
            info->permessi = permessi;
            chiamata_risultato(r, 0);
        } else {
            /* creare e cancellare: bisogna poter scrivere nella cartella che lo contiene */
            if (!file_puo_nella_cartella(percorso, PERMESSO_SCRIVERE)) break;
            int esito = servizio == SIS_CREA_CARTELLA
                      ? mfs_mkdir(percorso, compito_attuale()->uid, 0755) : mfs_rm(percorso);
            chiamata_risultato(r, esito < 0 ? -1 : 0);
        }
        break;
    }
    case SIS_RINOMINA: {
        char da[MAX_PERCORSO], a[MAX_PERCORSO];
        chiamata_risultato(r, -1);
        if (!copia_percorso(da, (const char *)chiamata_argomento(r, 1)) || !copia_percorso(a, (const char *)chiamata_argomento(r, 2))) break;
        /* si toglie da una cartella e si mette in un'altra: si deve poter scrivere in tutte e due */
        if (!file_puo_nella_cartella(da, PERMESSO_SCRIVERE) ||
            !file_puo_nella_cartella(a, PERMESSO_SCRIVERE)) break;
        chiamata_risultato(r, mfs_rinomina(da, a) < 0 ? -1 : 0);
        break;
    }
    case SIS_PERMESSI: {
        char percorso[MAX_PERCORSO];
        unsigned int nuovo_proprietario = chiamata_argomento(r, 2), nuovi_permessi = chiamata_argomento(r, 3);
        chiamata_risultato(r, -1);
        if (!copia_percorso(percorso, (const char *)chiamata_argomento(r, 1))) break;
        int tipo;
        unsigned int dimensione, proprietario;
        if (mfs_info(percorso, &tipo, &dimensione, &proprietario, 0) < 0) break;
        unsigned int uid = compito_attuale()->uid;
        /* il sistema puo' tutto; il proprietario solo i permessi, e il file resta suo */
        if (uid != UID_SISTEMA && (uid != proprietario || nuovo_proprietario != proprietario)) break;
        chiamata_risultato(r, mfs_permessi(percorso, nuovo_proprietario, nuovi_permessi) < 0 ? -1 : 0);
        break;
    }
    case SIS_POSIZIONA:
        chiamata_risultato(r, file_posiziona(chiamata_argomento(r, 1), chiamata_argomento(r, 2)));
        break;
    case SIS_ROTTURA:
        chiamata_risultato(r, memoria_rottura(chiamata_argomento(r, 1)));
        break;
    case SIS_TEMPO:
        chiamata_risultato(r, timer_tick() * 10);   /* il timer va a 100 Hz: un tick ogni 10 ms */
        break;
    case SIS_TERMINA: {
        struct compito *c = socket_altro(chiamata_argomento(r, 1));
        chiamata_risultato(r, -1);
        if (!c || c == compito_attuale()) break;
        compito_termina(c);
        chiamata_risultato(r, 0);
        break;
    }
    case SIS_ESCI:
        /* come un compito che sbaglia: si segna finito e si passa a un altro.
         * Lo scheduler liberera' pile e scheda. Al programma non si torna piu'. */
        compito_attuale()->stato = FINITO;
        return scheduler((indirizzo_t)r);
    default:
        chiamata_risultato(r, -1);   /* servizio che non esiste */
    }
    return (indirizzo_t)r;   /* si torna al programma */
}
