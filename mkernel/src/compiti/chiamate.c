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
#include "dinamica.h"
#include "mfs.h"
#include "file.h"
#include "schermo.h"
#include "processo.h"
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
static int del_programma(unsigned int inizio, unsigned int byte, unsigned int serve) {
    if (byte > MAX_SCRITTURA || inizio + byte < inizio) return 0;   /* troppo, o fa il giro */
    for (unsigned int p = inizio & ~(PAGINA - 1); p < inizio + byte; p += PAGINA) {
        unsigned int fisico, permessi;
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
        if (!del_programma((unsigned int)(da + i), 1, LEGGERE)) return 0;
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
        if (!copia_percorso(percorso, (const char *)r->ebx)) { r->eax = -1; break; }

        /* serve il permesso di eseguirlo; il programma nuovo e' "di" chi lo fa partire */
        const char *errore;
        struct compito *c = file_puo(percorso, PERMESSO_ESEGUIRE)
                          ? compito_crea_programma(percorso, compito_attuale()->uid, &errore) : 0;
        r->eax = c ? (unsigned int)c->numero : (unsigned int)-1;
        break;
    }
    case SIS_AVVIA_COME: {
        /* far partire un programma come un altro: solo il sistema (si puo' solo scendere) */
        char percorso[MAX_PERCORSO];
        unsigned int uid = r->ecx;
        r->eax = -1;
        if (compito_attuale()->uid != UID_SISTEMA || !copia_percorso(percorso, (const char *)r->ebx)) break;
        const char *errore;
        struct compito *c = compito_crea_programma(percorso, uid, &errore);
        if (c) r->eax = c->numero;
        break;
    }
    case SIS_CHI: {
        struct compito *c = socket_altro(r->ebx);
        r->eax = c ? c->uid : (unsigned int)-1;
        break;
    }
    case SIS_APRI_SCHERMO: {
        struct aperto *a = schermo_apri();
        r->eax = -1;
        if (!a) break;                         /* e' gia' aperto */
        int d = descrittore_nuovo(a);
        if (d < 0) { a->op->chiudi(a); break; }
        r->eax = d;
        break;
    }
    case SIS_MAPPA_SCHERMO: {
        /* Le pagine della memoria dello schermo (che non sono RAM del magazziniere) vanno
         * nella mappa di chi tiene il descrittore, a SCHERMO_PROGRAMMA, con il bit utente.
         * Segnate PRESTATE: quando il programma finisce, non si restituiscono al
         * magazziniere. Come mmap di /dev/fb0 in Linux. */
        struct aperto *a = descrittore(r->ebx);
        struct info_schermo *info = (struct info_schermo *)r->ecx;
        const struct schermo *s = console_schermo();
        r->eax = -1;
        if (!a || !e_lo_schermo(a) || !s || !del_programma((unsigned int)info, sizeof *info, SCRIVERE))
            break;
        unsigned int byte = s->passo * s->altezza;
        if (s->indirizzo % PAGINA || byte > PILA_PROGRAMMA_CIMA - PAGINA - SCHERMO_PROGRAMMA)
            break;                                 /* non ci sta nella zona dei programmi */
        unsigned int mappa = compito_attuale()->mappa, p;
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
        r->eax = 0;
        break;
    }
    case SIS_AVVIA_CON: {
        /* Come avvia, ma il figlio nasce con dei descrittori gia' aperti: gli stessi di chi
         * lo fa partire (ognuno e' un uso in piu', vedi descrittori.h), ai posti 0, 1, 2...
         * Come i descrittori ereditati di Unix, e come la "socket activation" di systemd. */
        char percorso[MAX_PERCORSO];
        const int *elenco = (const int *)r->ecx;
        int quanti = r->edx;
        struct aperto *passati[MAX_PASSATI];
        r->eax = -1;
        if (quanti < 0 || quanti > MAX_PASSATI || !copia_percorso(percorso, (const char *)r->ebx) ||
            (quanti && !del_programma((unsigned int)elenco, quanti * sizeof(int), LEGGERE)))
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
        r->eax = c->numero;
        break;
    }
    case SIS_PROCESSO:
        r->eax = processo_apri(r->ebx);
        break;
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
    case SIS_APRI:
    case SIS_INFO:
    case SIS_CREA_CARTELLA:
    case SIS_CANCELLA: {
        char percorso[MAX_PERCORSO] = {0};
        unsigned int servizio = r->eax;
        r->eax = -1;
        if (!copia_percorso(percorso, (const char *)r->ebx)) break;
        if (servizio == SIS_APRI) {
            r->eax = file_apri(percorso, r->ecx);
        } else if (servizio == SIS_INFO) {
            struct info_file *info = (struct info_file *)r->ecx;
            int tipo;
            unsigned int dimensione, proprietario, permessi;
            if (!del_programma((unsigned int)info, sizeof *info, SCRIVERE) ||
                mfs_info(percorso, &tipo, &dimensione, &proprietario, &permessi) < 0) break;
            info->tipo = tipo;
            info->dimensione = dimensione;
            info->proprietario = proprietario;
            info->permessi = permessi;
            r->eax = 0;
        } else {
            /* creare e cancellare: bisogna poter scrivere nella cartella che lo contiene */
            if (!file_puo_nella_cartella(percorso, PERMESSO_SCRIVERE)) break;
            int esito = servizio == SIS_CREA_CARTELLA
                      ? mfs_mkdir(percorso, compito_attuale()->uid, 0755) : mfs_rm(percorso);
            r->eax = esito < 0 ? -1 : 0;
        }
        break;
    }
    case SIS_RINOMINA: {
        char da[MAX_PERCORSO], a[MAX_PERCORSO];
        r->eax = -1;
        if (!copia_percorso(da, (const char *)r->ebx) || !copia_percorso(a, (const char *)r->ecx)) break;
        /* si toglie da una cartella e si mette in un'altra: si deve poter scrivere in tutte e due */
        if (!file_puo_nella_cartella(da, PERMESSO_SCRIVERE) ||
            !file_puo_nella_cartella(a, PERMESSO_SCRIVERE)) break;
        r->eax = mfs_rinomina(da, a) < 0 ? -1 : 0;
        break;
    }
    case SIS_PERMESSI: {
        char percorso[MAX_PERCORSO];
        unsigned int nuovo_proprietario = r->ecx, nuovi_permessi = r->edx;
        r->eax = -1;
        if (!copia_percorso(percorso, (const char *)r->ebx)) break;
        int tipo;
        unsigned int dimensione, proprietario;
        if (mfs_info(percorso, &tipo, &dimensione, &proprietario, 0) < 0) break;
        unsigned int uid = compito_attuale()->uid;
        /* il sistema puo' tutto; il proprietario solo i permessi, e il file resta suo */
        if (uid != UID_SISTEMA && (uid != proprietario || nuovo_proprietario != proprietario)) break;
        r->eax = mfs_permessi(percorso, nuovo_proprietario, nuovi_permessi) < 0 ? -1 : 0;
        break;
    }
    case SIS_POSIZIONA:
        r->eax = file_posiziona(r->ebx, r->ecx);
        break;
    case SIS_ROTTURA:
        r->eax = memoria_rottura(r->ebx);
        break;
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
