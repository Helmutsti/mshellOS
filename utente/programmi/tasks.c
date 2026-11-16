/* tasks: il servizio dei processi, della memoria e del disco (SISTEMA.md, sezione 1). Il contratto
 * (richieste, chiavi, permessi) e' in corekit/tasks.h; le funzioni per chi chiede stanno in
 * corekit/interno/tasks.c (elenco_processi, stato_memoria, stato_disco, dimensione_cartella,
 * termina_processo, in corekit.h).
 *
 * Fa quello che prima faceva CoreKit dentro ogni programma: legge /proc (i processi, la memoria),
 * chiede a Linux lo spazio di una partizione (statvfs), cammina nelle cartelle per pesarle e ferma
 * un processo. Ora lo fa solo lui, perche' un'app nel recinto non vede niente di tutto questo; e lo
 * da' solo a chi ha il permesso da "task manager" (processes.view, o processes.stop che lo
 * comprende).
 *
 * Gira come SISTEMA (uid 0, lo fa partire cosi' init): deve vedere tutti i processi. Per questo la
 * regola di stop non e' lasciata al kernel (il sistema potrebbe fermare qualsiasi cosa): tasks
 * guarda l'uid del processo e ferma SOLO quelli della persona (uid 1000), mai quelli del sistema ne'
 * init, a chiunque lo chieda.
 *
 * Il nome "tasks" glielo presta init al descrittore 0, e il canale del custode al 1 (le porte
 * delle app, come catalog e launcher). Tiene aperti fino a MAX_CLIENTI collegamenti insieme e
 * serve una richiesta alla volta: una dura al piu' qualche decina di millisecondi (folder_size
 * cammina nelle cartelle, un po' di piu'). */
#define _GNU_SOURCE
#include "corekit/corekit.h"
#include "corekit/messaggi.h"
#include "corekit/tasks.h"
#include "corekit/sistema/servizi.h"     /* chi_chiede, ha_permesso */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <unistd.h>

#define D_ASCOLTO 0       /* prestato da init */
#define D_CUSTODE 1       /* prestato da init: il canale del custode delle porte (servizi.h) */
#define MAX_CLIENTI 32

static struct {
    int d;                  /* -1: posto libero */
    struct chi_chiede chi;  /* chi c'e' dall'altra parte (lo dice il kernel all'arrivo, o il launcher) */
} clienti[MAX_CLIENTI];

/* LE PORTE DELLE APP (messaggi.h e sistema/servizi.h): il capo che il launcher ci ha dato con
 * attach, e chi e' l'app. Ogni connect sulla porta diventa un cliente con quell'identita'. */
#define MAX_PORTE MAX_CLIENTI
static struct {
    int d;                  /* -1: posto libero */
    int usata;              /* e' gia' arrivato un connect (per scriverlo nel registro una volta) */
    struct chi_chiede chi;
} porte[MAX_PORTE];

static struct messaggio richiesta, risposta;

static void registro(const char *a, const char *b) { scrivi_riga("tasks", a, b); }

/* --- i permessi --- */

/* Chi chiede puo' vedere i programmi, la memoria e il disco? processes.view, o processes.stop, che
 * lo comprende (per un'app: tra quelli che il launcher le ha concesso c'e' l'uno o l'altro). Il
 * rifiuto lo scrive ha_permesso nel registro. */
static int puo_vedere(const struct chi_chiede *c) {
    if (c->app)
        for (int i = 0; i < c->quanti_permessi; i++)
            if (!strcmp(c->permessi[i], PERMESSO_FERMARE_PROCESSI)) return 1;
    return ha_permesso(c, PERMESSO_VEDERE_PROCESSI);
}

/* --- /proc: i processi e la memoria --- */

/* /proc/<numero><file> in 'dove' */
static void percorso_proc(char *dove, unsigned int byte, int numero, const char *file) {
    snprintf(dove, byte, "/proc/%d%s", numero, file);
}

/* Legge un file piccolo di /proc in 'dove' (con lo 0 in fondo). I byte, o -1. */
static int leggi_proc(const char *percorso, char *dove, int massimo) {
    int f = open(percorso, O_RDONLY | O_CLOEXEC);
    if (f < 0) return -1;
    int n = 0;
    while (n < massimo - 1) {
        ssize_t r = read(f, dove + n, (size_t)(massimo - 1 - n));
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) break;
        n += (int)r;
    }
    close(f);
    dove[n] = 0;
    return n;
}

/* Il numero dopo "chiave:" in un testo come /proc/meminfo o status, o 0. */
static unsigned long long valore_di(const char *testo, const char *chiave) {
    size_t l = strlen(chiave);
    for (const char *riga = testo; riga && *riga; riga = strchr(riga, '\n') ? strchr(riga, '\n') + 1 : 0)
        if (!strncmp(riga, chiave, l) && riga[l] == ':') return strtoull(riga + l + 1, 0, 10);
    return 0;
}

/* IL NOME DI UN'APPLICAZIONE. Linux chiama un processo come il file del suo programma; il
 * programma di ogni applicazione pero' si chiama "code" (/system/apps/<app>/code, o in
 * /data/apps). Allora, per un processo che si chiama "code", il nome e' quello della
 * cartella dell'applicazione, preso dal percorso del programma: e' il primo argomento
 * (avvia lo passa sempre), in /proc/<numero>/cmdline, che tutti possono leggere. */
static void nome_applicazione(int numero, char *nome, int massimo) {
    if (strcmp(nome, "code")) return;
    char percorso[48], testo[MAX_PERCORSO];
    percorso_proc(percorso, sizeof percorso, numero, "/cmdline");
    if (leggi_proc(percorso, testo, sizeof testo) <= 0) return;   /* fino al primo 0 */
    char *fine = strrchr(testo, '/');
    if (!fine || strcmp(fine, "/code")) return;
    *fine = 0;
    char *inizio = strrchr(testo, '/');
    if (!inizio || !inizio[1]) return;
    int l = (int)strlen(inizio + 1);
    if (l > massimo - 1) l = massimo - 1;
    memcpy(nome, inizio + 1, (size_t)l);
    nome[l] = 0;
}

#define PF_KTHREAD 0x00200000   /* in /proc/<n>/stat: e' un lavoro del kernel, non un programma */

/* Un processo, da /proc/<numero>: 1 se e' un programma (e lo scrive in p), 0 se no. Lo stato
 * "in esecuzione" non lo dice mai: sarebbe tasks stesso (e' chi chiede che sa chi e'). */
static int leggi_processo(int numero, struct info_processo *p) {
    char percorso[48], testo[1024];
    /* stat: "numero (nome) stato padre ..." e, al nono campo, i "flag" */
    percorso_proc(percorso, sizeof percorso, numero, "/stat");
    if (leggi_proc(percorso, testo, sizeof testo) <= 0) return 0;
    char *apre = strchr(testo, '('), *chiude = strrchr(testo, ')');
    if (!apre || !chiude || chiude < apre) return 0;
    if (chiude[1] != ' ' || !chiude[2]) return 0;
    char stato = chiude[2], *dopo = chiude + 3;
    int padre = (int)strtol(dopo, &dopo, 10);                          /* il padre (PPid) */
    for (int campo = 0; campo < 4; campo++) strtol(dopo, &dopo, 10);   /* gruppo, sessione... */
    unsigned long flag = strtoul(dopo, 0, 10);
    if (flag & PF_KTHREAD) return 0;

    *p = (struct info_processo){ .numero = numero, .padre = padre };
    int l = (int)(chiude - apre - 1);
    if (l > (int)sizeof p->nome - 1) l = (int)sizeof p->nome - 1;
    memcpy(p->nome, apre + 1, (size_t)l);
    p->nome[l] = 0;
    nome_applicazione(numero, p->nome, (int)sizeof p->nome);
    /* R: gira o aspetta il suo turno; S e D: aspetta qualcosa; Z e X: e' finito */
    p->stato = stato == 'R' ? STATO_PRONTO : stato == 'Z' || stato == 'X' ? STATO_FINITO : STATO_IN_ATTESA;

    /* status: l'uid ("Uid: reale effettivo ...") e la memoria sua, non condivisa (RssAnon) */
    percorso_proc(percorso, sizeof percorso, numero, "/status");
    if (leggi_proc(percorso, testo, sizeof testo) > 0) {
        p->uid = (unsigned int)valore_di(testo, "Uid");
        p->memoria = (unsigned int)(valore_di(testo, "RssAnon") * 1024);
    }
    return 1;
}

static int per_numero(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

/* I programmi che girano, in ordine di numero, in 'v' (al massimo 'max'): quanti. */
static int elenco_processi_proc(struct info_processo *v, int max) {
    /* i numeri: le cartelle di /proc che sono numeri */
    int numeri[1024], quanti = 0;
    int d = open("/proc", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (d < 0) return -1;
    char blocco[4096];
    for (;;) {
        long n = syscall(SYS_getdents64, d, blocco, sizeof blocco);
        if (n <= 0) break;
        for (long k = 0; k < n;) {
            struct { unsigned long long ino; long long dopo; unsigned short lung; unsigned char tipo; char nome[]; }
                *voce = (void *)(blocco + k);
            char *fine;
            long numero = strtol(voce->nome, &fine, 10);
            if (!*fine && numero > 0 && quanti < (int)(sizeof numeri / sizeof numeri[0]))
                numeri[quanti++] = (int)numero;
            k += voce->lung;
        }
    }
    close(d);
    qsort(numeri, (size_t)quanti, sizeof numeri[0], per_numero);
    int n = 0;
    for (int i = 0; i < quanti && n < max; i++)
        if (leggi_processo(numeri[i], &v[n])) n++;
    return n;
}

/* processes: i programmi nel foglio allegato alla risposta (un struct elenco_processi). */
static void processi(void) {
    unsigned int byte = sizeof(struct elenco_processi);
    int f = foglio(byte);
    struct elenco_processi *e = f >= 0 ? mappa(f, 0) : 0;
    if (!e) {
        if (f >= 0) chiudi(f);
        messaggio_risposta(&risposta, &richiesta, ESITO_ERRORE);
        return;
    }
    int n = elenco_processi_proc(e->p, MAX_PROCESSI_TASKS);
    if (n < 0) {
        togli(e);
        chiudi(f);
        messaggio_risposta(&risposta, &richiesta, ESITO_ERRORE);
        return;
    }
    e->quanti = n;
    togli(e);
    messaggio_risposta(&risposta, &richiesta, ESITO_OK);
    messaggio_aggiungi_numero(&risposta, TASKS_CHIAVE_QUANTI, n);
    risposta.allegato = f;     /* lo chiude chi risponde, dopo averlo mandato */
}

/* memory: da /proc/meminfo. */
static void memoria(void) {
    char testo[4096];
    if (leggi_proc("/proc/meminfo", testo, sizeof testo) <= 0) {
        messaggio_risposta(&risposta, &richiesta, ESITO_ERRORE);
        return;
    }
    messaggio_risposta(&risposta, &richiesta, ESITO_OK);
    messaggio_aggiungi_numero(&risposta, TASKS_CHIAVE_TOTALE, (long)(valore_di(testo, "MemTotal") * 1024));
    messaggio_aggiungi_numero(&risposta, TASKS_CHIAVE_LIBERA, (long)(valore_di(testo, "MemFree") * 1024));
    /* i fogli (memfd) stanno in Shmem */
    messaggio_aggiungi_numero(&risposta, TASKS_CHIAVE_CONDIVISA, (long)(valore_di(testo, "Shmem") * 1024));
}

/* --- il disco --- */

static int tra(const char *percorso, const char *const *ammessi, int quanti) {
    for (int i = 0; i < quanti; i++)
        if (!strcmp(percorso, ammessi[i])) return 1;
    return 0;
}

/* Quali percorsi si possono chiedere: le due partizioni, e le cartelle che Attivita' pesa. Un
 * altro percorso e' invalid: tasks e' il sistema e non deve dire com'e' fatto il resto. */
static const char *const partizioni[] = { "/system", "/data" };
static const char *const cartelle[] = { "/system", "/system/apps", "/data/apps", "/data/users" };

/* disk: LO SPAZIO DI UNA PARTIZIONE. statvfs chiede al file system in cui sta il percorso quanti
 * blocchi ha in tutto e quanti liberi (come "df"). I liberi sono f_bfree, tutti: anche quelli che
 * ext4 tiene da parte per il sistema (f_bavail sono solo quelli che puo' usare chiunque). */
static void disco(const char *percorso) {
    struct statvfs s;
    if (statvfs(percorso, &s) < 0) {
        messaggio_risposta(&risposta, &richiesta, ESITO_ERRORE);
        return;
    }
    messaggio_risposta(&risposta, &richiesta, ESITO_OK);
    messaggio_aggiungi_numero(&risposta, TASKS_CHIAVE_TOTALE, (long)((unsigned long long)s.f_blocks * s.f_frsize));
    messaggio_aggiungi_numero(&risposta, TASKS_CHIAVE_LIBERA, (long)((unsigned long long)s.f_bfree * s.f_frsize));
}

/* Quanto pesano i file di una cartella, contando anche quelli delle cartelle dentro.
 * 'percorso' e' lungo 'lungo' lettere, in un posto di MAX_PERCORSO. Una cartella che non si
 * puo' aprire conta 0. Oltre PROFONDITA_TASKS cartelle una dentro l'altra non si scende; i
 * collegamenti simbolici non si seguono (CoreKit li da' di un altro tipo). */
static unsigned long long peso_cartella(char *percorso, int lungo, int profondita) {
    int d = apri(percorso, APRI_LEGGERE);
    if (d < 0) return 0;
    unsigned long long somma = 0;
    struct voce_cartella v;
    while (leggi(d, &v, sizeof v) == sizeof v) {
        v.nome[MAX_NOME_FILE - 1] = 0;
        if (v.tipo == TIPO_FILE) {
            somma += v.dimensione;
            continue;
        }
        if (v.tipo != TIPO_CARTELLA || profondita >= PROFONDITA_TASKS) continue;
        if (v.nome[0] == '.' && (!v.nome[1] || (v.nome[1] == '.' && !v.nome[2]))) continue;
        int n = (int)strlen(v.nome);
        if (lungo + 1 + n >= MAX_PERCORSO) continue;
        percorso[lungo] = '/';
        memcpy(percorso + lungo + 1, v.nome, (size_t)n + 1);
        somma += peso_cartella(percorso, lungo + 1 + n, profondita + 1);
        percorso[lungo] = 0;
    }
    chiudi(d);
    return somma;
}

/* folder_size */
static void peso(const char *percorso) {
    char copia[MAX_PERCORSO];
    strcpy(copia, percorso);    /* e' uno dei percorsi ammessi: sta di sicuro */
    unsigned long long byte = peso_cartella(copia, (int)strlen(copia), 0);
    messaggio_risposta(&risposta, &richiesta, ESITO_OK);
    messaggio_aggiungi_numero(&risposta, TASKS_CHIAVE_PESO, (long)byte);
}

/* --- fermare un processo --- */

#ifndef SYS_pidfd_open
#define SYS_pidfd_open 434
#endif
#ifndef SYS_pidfd_send_signal
#define SYS_pidfd_send_signal 424
#endif

/* stop: la regola (tasks.h). Si apre il processo come descrittore (pidfd_open): da li' in poi
 * l'uid che si legge e il segnale che si manda sono di QUEL processo, anche se il suo numero
 * venisse riusato da un altro nel frattempo. */
static void ferma(const struct chi_chiede *c) {
    long pid;
    if (messaggio_numero(&richiesta, TASKS_CHIAVE_PID, &pid) < 0 || pid <= 1 || pid > 0x7fffffff) {
        messaggio_risposta(&risposta, &richiesta, ESITO_NON_VALIDO);   /* 1 e' init, e 0 e meno non sono processi */
        return;
    }
    if (!ha_permesso(c, PERMESSO_FERMARE_PROCESSI)) {
        messaggio_risposta(&risposta, &richiesta, ESITO_NEGATO);
        return;
    }
    int pd = (int)syscall(SYS_pidfd_open, (int)pid, 0);
    char percorso[48], testo[1024];
    percorso_proc(percorso, sizeof percorso, (int)pid, "/status");
    if (pd < 0 || leggi_proc(percorso, testo, sizeof testo) <= 0) {
        if (pd >= 0) close(pd);
        messaggio_risposta(&risposta, &richiesta, ESITO_ERRORE);
        messaggio_aggiungi(&risposta, TASKS_CHIAVE_MOTIVO, TASKS_MOTIVO_NON_TROVATO);
        return;
    }
    if (valore_di(testo, "Uid") != UID_APPLICAZIONI) {
        messaggio_risposta(&risposta, &richiesta, ESITO_NEGATO);
        messaggio_aggiungi(&risposta, TASKS_CHIAVE_MOTIVO, TASKS_MOTIVO_NON_SUO);
        registro("non fermo un processo che non e' della persona, chiesto da ",
                 c->programma[0] ? c->programma : "un programma sconosciuto");
        close(pd);
        return;
    }
    int fatto = syscall(SYS_pidfd_send_signal, pd, SIGKILL, 0, 0) == 0;
    close(pd);
    if (fatto) {
        messaggio_risposta(&risposta, &richiesta, ESITO_OK);
    } else {
        messaggio_risposta(&risposta, &richiesta, ESITO_ERRORE);
        messaggio_aggiungi(&risposta, TASKS_CHIAVE_MOTIVO, TASKS_MOTIVO_NON_TROVATO);
    }
}

/* --- i collegamenti --- */

static void scollega(int k) {
    chiudi(clienti[k].d);
    clienti[k].d = -1;
}

/* attach dal cliente k: solo dal sistema (il launcher), con il capo e il programma dell'app. */
static void attacca(int k) {
    int capo = richiesta.allegato;
    richiesta.allegato = -1;
    int p = 0;
    while (p < MAX_PORTE && porte[p].d >= 0) p++;
    if (clienti[k].chi.uid != UID_SISTEMA) {
        messaggio_risposta(&risposta, &richiesta, ESITO_NEGATO);
    } else if (capo < 0 || p == MAX_PORTE || app_da_attach(&richiesta, &porte[p].chi) < 0) {
        messaggio_risposta(&risposta, &richiesta, ESITO_NON_VALIDO);
    } else {
        porte[p].d = capo;
        porte[p].usata = 0;
        custodisci_porta(D_CUSTODE, &porte[p].chi, capo);   /* init ne tiene una copia (prima di dire ok) */
        capo = -1;
        messaggio_risposta(&risposta, &richiesta, ESITO_OK);
    }
    if (capo >= 0) chiudi(capo);
}

/* Un collegamento nuovo 'd', di chi e' 'c'. */
static void nuovo_collegamento(int d, const struct chi_chiede *c) {
    int k = 0;
    while (k < MAX_CLIENTI && clienti[k].d >= 0) k++;
    if (k == MAX_CLIENTI) { chiudi(d); return; }   /* troppi */
    clienti[k].d = d;
    clienti[k].chi = *c;
}

/* Un messaggio sulla porta p: connect con il capo allegato diventa un cliente dell'app della
 * porta. L'app chiusa (o un messaggio sbagliato): la porta si chiude. */
static void da_porta(int p) {
    static struct messaggio m;
    int n = messaggio_ricevi(porte[p].d, &m);
    if (n == 1 && m.allegato >= 0 && messaggio_vale(&m, CHIAVE_RICHIESTA, RICHIESTA_COLLEGA)) {
        if (!porte[p].usata) registro("collegamento dalla porta di ", porte[p].chi.programma);
        porte[p].usata = 1;
        nuovo_collegamento(m.allegato, &porte[p].chi);
        return;
    }
    if (n == 1 && m.allegato >= 0) chiudi(m.allegato);
    chiudi(porte[p].d);
    porte[p].d = -1;
}

/* Una richiesta con un percorso (disk, folder_size): permesso, e il percorso tra quelli ammessi. */
static void con_percorso(const struct chi_chiede *c, const char *const *ammessi, int quanti, int disco_o_peso) {
    const char *percorso = messaggio_valore(&richiesta, TASKS_CHIAVE_PERCORSO);
    if (!percorso || !tra(percorso, ammessi, quanti)) {
        messaggio_risposta(&risposta, &richiesta, ESITO_NON_VALIDO);
    } else if (!puo_vedere(c)) {
        messaggio_risposta(&risposta, &richiesta, ESITO_NEGATO);
    } else if (disco_o_peso) {
        disco(percorso);
    } else {
        peso(percorso);
    }
}

/* Un messaggio dal cliente k. */
static void da_cliente(int k) {
    int n = messaggio_ricevi(clienti[k].d, &richiesta);
    if (n == 0 || n == -1) { scollega(k); return; }   /* ha chiuso */
    const struct chi_chiede *c = &clienti[k].chi;
    if (n == MESSAGGIO_NON_VALIDO) {
        messaggio_risposta(&risposta, 0, ESITO_NON_VALIDO);
    } else if (messaggio_vale(&richiesta, CHIAVE_RICHIESTA, RICHIESTA_ATTACCA)) {
        attacca(k);
    } else {
        if (richiesta.allegato >= 0) { chiudi(richiesta.allegato); richiesta.allegato = -1; }
        if (messaggio_vale(&richiesta, CHIAVE_RICHIESTA, TASKS_RICHIESTA_PROCESSI)) {
            if (puo_vedere(c)) processi(); else messaggio_risposta(&risposta, &richiesta, ESITO_NEGATO);
        } else if (messaggio_vale(&richiesta, CHIAVE_RICHIESTA, TASKS_RICHIESTA_MEMORIA)) {
            if (puo_vedere(c)) memoria(); else messaggio_risposta(&risposta, &richiesta, ESITO_NEGATO);
        } else if (messaggio_vale(&richiesta, CHIAVE_RICHIESTA, TASKS_RICHIESTA_DISCO)) {
            con_percorso(c, partizioni, (int)(sizeof partizioni / sizeof partizioni[0]), 1);
        } else if (messaggio_vale(&richiesta, CHIAVE_RICHIESTA, TASKS_RICHIESTA_CARTELLA)) {
            con_percorso(c, cartelle, (int)(sizeof cartelle / sizeof cartelle[0]), 0);
        } else if (messaggio_vale(&richiesta, CHIAVE_RICHIESTA, TASKS_RICHIESTA_FERMA)) {
            ferma(c);
        } else {
            messaggio_risposta(&risposta, &richiesta, ESITO_NON_VALIDO);
        }
    }
    if (clienti[k].d >= 0 && messaggio_manda(clienti[k].d, &risposta) < 0) scollega(k);
    if (risposta.allegato >= 0) { chiudi(risposta.allegato); risposta.allegato = -1; }
}

/* Un collegamento per nome: chi e' lo dice il kernel. */
static void nuovo_cliente(void) {
    int d = accetta(D_ASCOLTO);
    if (d < 0) return;
    struct chi_chiede c;
    if (chi_chiede(d, &c) < 0) { chiudi(d); return; }   /* sconosciuto */
    nuovo_collegamento(d, &c);
}

/* Una porta che init ci riconsegna dopo una ripartenza: va nella tabella com'era, e il collegamento
 * e' gia' stato scritto nel registro da chi c'era prima. */
static void riprendi(const struct chi_chiede *app, int capo, void *dato) {
    (void)dato;
    int p = 0;
    while (p < MAX_PORTE && porte[p].d >= 0) p++;
    if (p == MAX_PORTE) { chiudi(capo); return; }
    porte[p].d = capo;
    porte[p].usata = 1;
    porte[p].chi = *app;
}

int main(void) {
    for (int k = 0; k < MAX_CLIENTI; k++) clienti[k].d = -1;
    for (int p = 0; p < MAX_PORTE; p++) porte[p].d = -1;
    riprendi_porte(D_CUSTODE, riprendi, 0);   /* le porte delle app che teneva init, se sono ripartito */
    for (;;) {
        struct da_guardare g[1 + MAX_CLIENTI + MAX_PORTE];
        int chi[1 + MAX_CLIENTI + MAX_PORTE];   /* un cliente (>= 0), una porta (-2 - p), o -1 */
        int n = 0;
        g[n].descrittore = D_ASCOLTO;
        chi[n++] = -1;
        for (int k = 0; k < MAX_CLIENTI; k++)
            if (clienti[k].d >= 0) { g[n].descrittore = clienti[k].d; chi[n++] = k; }
        for (int p = 0; p < MAX_PORTE; p++)
            if (porte[p].d >= 0) { g[n].descrittore = porte[p].d; chi[n++] = -2 - p; }
        if (aspetta(g, n, -1) < 0) {
            registro("aspettare non e' piu' possibile: ", "mi fermo (init mi fara' ripartire)");
            return 1;
        }
        /* prima i messaggi, poi i collegamenti nuovi (un nuovo potrebbe riusare un numero) */
        for (int i = 1; i < n; i++)
            if (g[i].pronto && chi[i] >= 0 && clienti[chi[i]].d == g[i].descrittore) da_cliente(chi[i]);
        for (int i = 1; i < n; i++)
            if (g[i].pronto && chi[i] <= -2 && porte[-2 - chi[i]].d == g[i].descrittore) da_porta(-2 - chi[i]);
        if (g[0].pronto) nuovo_cliente();
    }
}
