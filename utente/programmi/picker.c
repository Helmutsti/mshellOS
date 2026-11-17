/* picker: il servizio che fa scegliere alla persona un file da aprire o dove salvare, e lo
 * consegna GIA' APERTO all'app che l'ha chiesto (SISTEMA.md, sezione 1; APPLICAZIONI.md,
 * sezione 9, "Aprire un file scelto dalla persona"). Il contratto con le app (richieste open e
 * save, chiavi, esiti) e' in corekit/picker.h; le funzioni per chi chiede in
 * corekit/interno/picker.c.
 *
 * LA DIVISIONE DEL LAVORO (picker_finestra.h):
 *   - questo file sa DOVE sono i file. Apre la Home della persona e, dentro, ogni cartella e
 *     ogni file, sempre in modo sicuro (qui sotto, "la sicurezza"). Alla finestra passa solo
 *     l'elenco del contenuto di una cartella e la risposta a "c'e' gia' un file con questo
 *     nome?" (la "sorgente": elenca ed esiste).
 *   - picker_finestra.c (l'interfaccia, UIKit) mostra la finestra "Apri" o "Salva" sopra l'app,
 *     fa navigare tra le cartelle, chiede "Sostituire il file?" e torna con la cartella e il
 *     nome scelti. Non apre mai un file da sola.
 *   - picker_nome.c: la regola del nome valido, la stessa per tutti e due.
 * Il programma vero si lega con picker_finestra.c; la prova test/prove/picker.py lo lega invece
 * con utente/prove/picker_finestra_finta.c, una finestra senza grafica che "sceglie" leggendo un
 * file (strumenti/compilazione/programmi.mk).
 *
 * LA SICUREZZA. Il servizio gira come la persona (uid 1000, lo fa partire cosi' init): vede
 * tutti i suoi file, ma deve consegnare all'app SOLO quello che la persona ha scelto, e solo
 * dentro la Home (/data/users/<uid>/home). La finestra dice "cartella Documenti, file
 * nota.txt": il servizio controlla e apre lui. Le regole:
 *   1. il percorso della cartella e il nome devono essere fatti di nomi validi (picker_nome.c):
 *      niente "/" all'inizio, niente "." o "..", niente pezzi vuoti;
 *   2. ogni cosa si apre con openat2 a partire dalla Home, con RESOLVE_BENEATH (il kernel
 *      rifiuta ogni percorso che uscirebbe dalla Home, anche passando da un collegamento),
 *      RESOLVE_NO_SYMLINKS (nessun collegamento simbolico, in nessun pezzo del percorso: la
 *      finestra non li mostra, quindi non ci si arriva mai) e RESOLVE_NO_MAGICLINKS (i
 *      collegamenti speciali di /proc, per scrupolo). Cosi' anche se qualcuno cambia i file
 *      mentre la finestra e' aperta (un collegamento al posto di una cartella), il controllo lo
 *      fa il kernel nel momento in cui si apre, non noi prima;
 *   3. si consegnano solo FILE NORMALI: niente cartelle, dispositivi, tubi o prese. Il file si
 *      apre con O_NONBLOCK (aprire un tubo senza nessuno dall'altra parte non deve bloccare il
 *      servizio), si guarda che sia un file normale (fstat), e poi O_NONBLOCK si toglie;
 *   4. Salva crea (o svuota) il file con O_WRONLY | O_CREAT | O_TRUNC, permessi 0644: e' della
 *      persona, come ogni file della Home.
 * Nell'elenco delle cartelle (elenca) restano solo file normali e cartelle: niente collegamenti
 * (si guarda con fstatat e AT_SYMLINK_NOFOLLOW, che non li segue) e niente altri tipi.
 *
 * UNA RICHIESTA ALLA VOLTA. Mentre la finestra e' aperta il servizio e' dentro picker_finestra
 * e non legge altro: le richieste degli altri restano nelle loro caselle e passano dopo, nel
 * loro turno. (Le app aspettano la risposta fino a PICKER_ATTESA_MS.)
 *
 * I COLLEGAMENTI, come catalog: il nome "picker" glielo presta init al descrittore 0 (chi chiede
 * per nome: chi e' lo dice il kernel), il canale del custode al descrittore 1; le porte delle
 * app arrivano dal launcher con attach (la porta 7 di ogni app, D_PORTA_PICKER: di base, senza
 * permessi). L'app di una porta e' quella SOPRA cui va la finestra. */
#define _GNU_SOURCE   /* O_NOCTTY, AT_SYMLINK_NOFOLLOW, strcasecmp */
#include "corekit/corekit.h"
#include "corekit/messaggi.h"
#include "corekit/picker.h"
#include "corekit/sistema/servizi.h"     /* chi_chiede, app_da_attach, custodisci_porta */
#include "picker_finestra.h"
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#define D_ASCOLTO 0       /* prestato da init */
#define D_CUSTODE 1       /* prestato da init: il canale delle porte delle app custodite (servizi.h) */
#define MAX_CLIENTI 32
#define MAX_PORTE MAX_CLIENTI
#define MAX_PERCORSO_HOME 4096   /* PATH_MAX di Linux: un percorso piu' lungo il kernel non lo apre */

static struct {
    int d;                  /* -1: posto libero */
    struct chi_chiede chi;  /* chi c'e' dall'altra parte */
} clienti[MAX_CLIENTI];

/* LE PORTE DELLE APP (come in catalog.c): il capo che il launcher ci ha dato con attach, e chi e'
 * l'app. Ogni connect sulla porta diventa un cliente con quell'identita'. */
static struct {
    int d;                  /* -1: posto libero */
    int usata;              /* e' gia' arrivato un connect (per scriverlo nel registro una volta) */
    struct chi_chiede chi;
} porte[MAX_PORTE];

static struct messaggio richiesta, risposta;

static void registro(const char *a, const char *b) { scrivi_riga("picker", a, b); }

/* --- la sicurezza: openat2 --- */

/* openat2 (Linux 5.6) non ha ancora una funzione nella libreria C di Alpine: la si chiama con il
 * suo numero, e la struttura e le costanti si scrivono qui (le stesse di linux/openat2.h). */
#ifndef SYS_openat2
#define SYS_openat2 437
#endif
struct come_aprire {
    uint64_t flags;     /* O_RDONLY, O_CREAT... come per open */
    uint64_t mode;      /* i permessi di un file creato (solo con O_CREAT) */
    uint64_t resolve;   /* come si percorre il percorso: le RISOLVI_ qui sotto */
};
#define RISOLVI_NIENTE_MAGICI   0x02   /* RESOLVE_NO_MAGICLINKS: niente collegamenti di /proc */
#define RISOLVI_NIENTE_SIMBOLI  0x04   /* RESOLVE_NO_SYMLINKS: niente collegamenti simbolici */
#define RISOLVI_SOTTO           0x08   /* RESOLVE_BENEATH: mai fuori dalla cartella di partenza */

/* Apre 'percorso' (relativo) dentro la cartella 'casa', con le regole della sicurezza. Il
 * descrittore, o -1. */
static int apri_sotto(int casa, const char *percorso, int flags, unsigned int modo) {
    struct come_aprire come = {
        .flags = (uint64_t)(flags | O_CLOEXEC | O_NOCTTY),
        .mode = (flags & O_CREAT) ? modo : 0,
        .resolve = RISOLVI_SOTTO | RISOLVI_NIENTE_SIMBOLI | RISOLVI_NIENTE_MAGICI,
    };
    long d = syscall(SYS_openat2, casa, percorso, &come, sizeof come);
    return d < 0 ? -1 : (int)d;
}

/* Il percorso di una cartella che arriva dalla finestra va bene? "" (la Home) o nomi validi
 * separati da un "/" solo, senza "/" all'inizio o alla fine (picker_finestra.h). */
static int percorso_valido(const char *percorso) {
    if (!percorso) return 0;
    if (!percorso[0]) return 1;
    char pezzo[256];
    const char *p = percorso;
    for (;;) {
        const char *fine = strchr(p, '/');
        size_t n = fine ? (size_t)(fine - p) : strlen(p);
        if (n == 0 || n >= sizeof pezzo) return 0;   /* "//", "/" all'inizio o alla fine, troppo lungo */
        memcpy(pezzo, p, n);
        pezzo[n] = 0;
        if (!picker_nome_valido(pezzo)) return 0;    /* ".", "..", caratteri di controllo */
        if (!fine) return 1;
        p = fine + 1;
    }
}

/* La cartella 'percorso' della Home, aperta per leggerla: il descrittore, o -1. "" e' la Home. */
static int apri_cartella(int casa, const char *percorso) {
    if (!percorso_valido(percorso)) return -1;
    return apri_sotto(casa, percorso[0] ? percorso : ".", O_RDONLY | O_DIRECTORY, 0);
}

/* "percorso/nome" in 'dove' (o solo "nome" nella Home). 0, o -1 se non va o non ci sta. */
static int unisci(char *dove, size_t massimo, const char *percorso, const char *nome) {
    if (!percorso_valido(percorso) || !picker_nome_valido(nome)) return -1;
    int n = snprintf(dove, massimo, "%s%s%s", percorso, percorso[0] ? "/" : "", nome);
    return n < 0 || (size_t)n >= massimo ? -1 : 0;
}

/* --- la sorgente: quello che la finestra puo' sapere dei file --- */

struct casa { int d; };   /* la Home, aperta (il "ctx" della sorgente) */

/* Le voci della cartella cosi' come le da' il kernel (getdents64: la libreria C non la ha con
 * questo nome; la struttura e' quella di Linux). */
struct voce_kernel {
    uint64_t d_ino;
    int64_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[];
};

/* Cartelle prima, poi i file; dentro, per nome senza guardare maiuscole e minuscole (a parita',
 * per byte: l'ordine e' sempre lo stesso). */
static int confronta(const void *a, const void *b) {
    const struct picker_voce *x = a, *y = b;
    if (x->cartella != y->cartella) return y->cartella - x->cartella;
    int c = strcasecmp(x->nome, y->nome);
    return c ? c : strcmp(x->nome, y->nome);
}

static int elenca(void *ctx, const char *percorso, struct picker_voce *v, int max) {
    struct casa *casa = ctx;
    int d = apri_cartella(casa->d, percorso);
    if (d < 0) return -1;
    int quante = 0;
    static char buffer[8192] __attribute__((aligned(8)));
    for (;;) {
        long letti = syscall(SYS_getdents64, d, buffer, sizeof buffer);
        if (letti < 0) { close(d); return -1; }
        if (letti == 0) break;
        for (long pos = 0; pos < letti;) {
            struct voce_kernel *k = (struct voce_kernel *)(buffer + pos);
            pos += k->d_reclen;
            const char *nome = k->d_name;
            if (!strcmp(nome, ".") || !strcmp(nome, "..") || quante >= max) continue;
            if (!picker_nome_valido(nome)) continue;   /* un nome che la finestra non saprebbe ridare */
            /* il tipo vero, senza seguire i collegamenti (d_type non c'e' su tutti i file system) */
            struct stat s;
            if (fstatat(d, nome, &s, AT_SYMLINK_NOFOLLOW) < 0) continue;
            if (!S_ISDIR(s.st_mode) && !S_ISREG(s.st_mode)) continue;   /* collegamenti, dispositivi... */
            strcpy(v[quante].nome, nome);   /* picker_nome_valido: al massimo 255 byte */
            v[quante].cartella = S_ISDIR(s.st_mode);
            v[quante].byte = S_ISREG(s.st_mode) ? (long long)s.st_size : 0;
            quante++;
        }
    }
    close(d);
    qsort(v, (size_t)quante, sizeof v[0], confronta);
    return quante;
}

static int esiste(void *ctx, const char *percorso, const char *nome) {
    struct casa *casa = ctx;
    if (!picker_nome_valido(nome)) return 0;
    int d = apri_cartella(casa->d, percorso);
    if (d < 0) return 0;
    struct stat s;
    int c_e = fstatat(d, nome, &s, AT_SYMLINK_NOFOLLOW) == 0 && S_ISREG(s.st_mode);
    close(d);
    return c_e;
}

/* --- aprire il file scelto --- */

/* Il file scelto, aperto per l'app: in lettura (Apri) o creato e svuotato in scrittura (Salva).
 * Solo un file normale, sotto la Home. Il descrittore, o -1. */
static int apri_scelto(int casa, int salva, const char *percorso, const char *nome) {
    char dove[MAX_PERCORSO_HOME];
    if (unisci(dove, sizeof dove, percorso, nome) < 0) return -1;
    int flags = salva ? O_WRONLY | O_CREAT | O_TRUNC : O_RDONLY;
    int d = apri_sotto(casa, dove, flags | O_NONBLOCK, 0644);
    if (d < 0) return -1;
    struct stat s;
    int ce_flags = fcntl(d, F_GETFL);
    if (fstat(d, &s) < 0 || !S_ISREG(s.st_mode) || ce_flags < 0 ||
        fcntl(d, F_SETFL, ce_flags & ~O_NONBLOCK) < 0) {
        close(d);
        return -1;
    }
    return d;
}

/* La Home della persona per cui gira il servizio: /data/users/<uid>/home, aperta. -1 se non c'e'. */
static int apri_casa(void) {
    char percorso[64];
    snprintf(percorso, sizeof percorso, "/data/users/%u/home", (unsigned int)getuid());
    return open(percorso, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
}

/* --- le richieste --- */

/* open o save dal cliente k: mostra la finestra e prepara la risposta (con il file allegato).
 * Torna il descrittore del file da chiudere dopo averlo mandato, o -1. */
static int scegli(int k, int salva) {
    const char *proposto = salva ? messaggio_valore(&richiesta, PICKER_CHIAVE_NOME) : 0;
    if (salva && (!proposto || (proposto[0] && !picker_nome_valido(proposto)))) {
        registro("nome proposto non valido da ", clienti[k].chi.programma);
        messaggio_risposta(&risposta, &richiesta, ESITO_NON_VALIDO);
        return -1;
    }
    struct casa casa = { apri_casa() };
    if (casa.d < 0) {
        registro("la Home non si apre: niente finestra per ", clienti[k].chi.programma);
        messaggio_risposta(&risposta, &richiesta, ESITO_ERRORE);
        return -1;
    }
    struct picker_sorgente sorgente = { elenca, esiste, &casa };
    static char percorso[MAX_PERCORSO_HOME], nome[256];
    percorso[0] = nome[0] = 0;
    int esito = picker_finestra(salva ? PICKER_MODO_SALVA : PICKER_MODO_APRI, clienti[k].chi.programma,
                                proposto ? proposto : "", &sorgente,
                                percorso, sizeof percorso, nome, sizeof nome);
    int file = -1;
    if (esito == 1) {
        messaggio_risposta(&risposta, &richiesta, ESITO_ERRORE);
        messaggio_aggiungi(&risposta, PICKER_CHIAVE_MOTIVO, PICKER_MOTIVO_ANNULLATO);
    } else if (esito != 0) {
        registro("la finestra non si e' potuta mostrare sopra ", clienti[k].chi.programma);
        messaggio_risposta(&risposta, &richiesta, ESITO_ERRORE);
    } else if ((file = apri_scelto(casa.d, salva, percorso, nome)) < 0) {
        registro("il file scelto non si apre, per ", clienti[k].chi.programma);
        messaggio_risposta(&risposta, &richiesta, ESITO_ERRORE);
        messaggio_aggiungi(&risposta, PICKER_CHIAVE_MOTIVO, PICKER_MOTIVO_APERTURA);
    } else {
        messaggio_risposta(&risposta, &richiesta, ESITO_OK);
        messaggio_aggiungi(&risposta, PICKER_CHIAVE_NOME, nome);
        risposta.allegato = file;
        registro(salva ? "file da salvare consegnato a " : "file da aprire consegnato a ",
                 clienti[k].chi.programma);
    }
    close(casa.d);
    return file;
}

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

/* Un messaggio dal cliente k. */
static void da_cliente(int k) {
    int n = messaggio_ricevi(clienti[k].d, &richiesta);
    if (n == 0 || n == -1) { scollega(k); return; }   /* ha chiuso */
    int file = -1;
    if (n == MESSAGGIO_NON_VALIDO) {
        messaggio_risposta(&risposta, 0, ESITO_NON_VALIDO);
    } else if (messaggio_vale(&richiesta, CHIAVE_RICHIESTA, RICHIESTA_ATTACCA)) {
        attacca(k);
    } else {
        if (richiesta.allegato >= 0) { chiudi(richiesta.allegato); richiesta.allegato = -1; }
        if (messaggio_vale(&richiesta, CHIAVE_RICHIESTA, PICKER_RICHIESTA_APRI)) {
            file = scegli(k, 0);
        } else if (messaggio_vale(&richiesta, CHIAVE_RICHIESTA, PICKER_RICHIESTA_SALVA)) {
            file = scegli(k, 1);
        } else {
            messaggio_risposta(&risposta, &richiesta, ESITO_NON_VALIDO);
        }
    }
    /* la risposta (con il file, se c'e'): l'app ne riceve una copia, la nostra si chiude */
    if (messaggio_manda(clienti[k].d, &risposta) < 0) scollega(k);
    if (file >= 0) close(file);
    risposta.allegato = -1;
}

/* Un collegamento per nome: chi e' lo dice il kernel. */
static void nuovo_cliente(void) {
    int d = accetta(D_ASCOLTO);
    if (d < 0) return;
    struct chi_chiede c;
    if (chi_chiede(d, &c) < 0) { chiudi(d); return; }   /* sconosciuto */
    nuovo_collegamento(d, &c);
}

/* Una porta che init ci riconsegna dopo una ripartenza: va nella tabella com'era. */
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
            registro("aspettare non va: ", "mi fermo (init mi fara' ripartire)");
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
