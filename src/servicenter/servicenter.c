// servicenter: tiene accesi i servizi di sistema e risponde alle richieste del comando svcc.
// Lo avvia init all'accensione (e lo riavvia se cade). Gira come root perche' deve
// avviare i servizi, ognuno con la sua identita' (mai root).
//
// Ogni servizio riceve, al posto del descrittore 3, un capo di un canale a due sensi
// (socketpair): l'altro capo lo tiene servicenter. E' cosi' che servicenter gli parla.
//
// Le richieste arrivano su un socket in /system (cartella che apre solo root).
// Il kernel dice chi c'e' dall'altra parte (SO_PEERCRED): si accetta solo root.
// Una richiesta = una riga di testo:
//   list | <servizio> status | <servizio> down | <servizio> up | <servizio> restart
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "mshelllib.h"

#define CANALE_SERVIZIO  3                     // descrittore su cui il servizio trova il suo canale

struct servizio {
    const char *nome;
    const char *programma;
    uid_t uid;
    gid_t gid;
    // stato, gestito da servicenter
    int voluto;                                // 1 = deve restare acceso
    pid_t pid;                                 // 0 = spento
    int canale;                                // capo di servicenter del canale, -1 se spento
    time_t partito, prossimo_avvio;
    int riavvii;                               // quante volte e' caduto ed e' stato riavviato
};

// I servizi di sistema. Per ora c'e' solo quello di prova.
static struct servizio servizi[] = {
    { "prova", "/system/servizi/prova", 100, 100, 1, 0, -1, 0, 0, 0 },
};
#define N_SERVIZI (int)(sizeof servizi / sizeof servizi[0])

static struct servizio *trova(const char *nome) {
    for (int i = 0; i < N_SERVIZI; i++)
        if (nome && strcmp(servizi[i].nome, nome) == 0)
            return &servizi[i];
    return NULL;
}

// Avvia un servizio: canale a due sensi, copia del processo, identita' del servizio, programma.
static void avvia(struct servizio *s) {
    int capi[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, capi) != 0)
        return;
    pid_t pid = fork();
    if (pid < 0) {
        close(capi[0]);
        close(capi[1]);
        return;
    }
    if (pid == 0) {
        // Copia: mette il suo capo del canale al descrittore 3 (dup2 toglie CLOEXEC),
        // tutto il resto si chiude da solo all'execve. Poi smette di essere root.
        dup2(capi[1], CANALE_SERVIZIO);
        signal(SIGCHLD, SIG_DFL);
        signal(SIGPIPE, SIG_DFL);
        // Il servizio e' un'identita' come le altre: la sua cartella /system/users/<uid>
        // diventa la sua radice, e ci compare solo il suo programma, in /servizio.
        char radice[64];
        snprintf(radice, sizeof radice, CARTELLA_UTENTI "/%u", (unsigned)s->uid);
        if (mkdir(radice, 0755) != 0 && errno != EEXIST)
            _exit(125);
        struct aggancio agganci[] = { { s->programma, "/servizio", 1 } };
        if (crea_mondo(radice, agganci, 1) != 0)
            _exit(125);
        if (setgroups(0, NULL) != 0 || setgid(s->gid) != 0 || setuid(s->uid) != 0)
            _exit(126);
        // se servicenter muore, il servizio riceve SIGTERM: niente servizi orfani doppi
        // (va impostato dopo il cambio di utente, che lo azzererebbe)
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        chdir("/");
        char *argomenti[] = { (char *)s->nome, NULL };
        char *ambiente[] = { NULL };
        execve("/servizio", argomenti, ambiente);
        _exit(127);
    }
    close(capi[1]);
    s->pid = pid;
    s->canale = capi[0];
    s->partito = time(NULL);
}

// Raccoglie i servizi terminati. Se dovevano restare accesi, li riavvia fra un secondo.
static void raccogli(void) {
    pid_t pid;
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
        for (int i = 0; i < N_SERVIZI; i++) {
            struct servizio *s = &servizi[i];
            if (s->pid != pid)
                continue;
            close(s->canale);
            s->canale = -1;
            s->pid = 0;
            if (s->voluto && s->prossimo_avvio == 0) {   // caduto da solo
                s->riavvii++;
                s->prossimo_avvio = time(NULL) + 1;
            }
        }
    }
}

// Avvia i servizi che devono essere accesi e non lo sono, quando e' il momento.
static void controlla(void) {
    time_t ora = time(NULL);
    for (int i = 0; i < N_SERVIZI; i++) {
        struct servizio *s = &servizi[i];
        if (s->voluto && s->pid == 0 && ora >= s->prossimo_avvio) {
            s->prossimo_avvio = 0;
            avvia(s);
        }
    }
}

// Ferma un servizio: prima SIGTERM, dopo un secondo SIGKILL.
static void ferma(struct servizio *s) {
    if (s->pid == 0)
        return;
    pid_t pid = s->pid;
    kill(pid, SIGTERM);
    for (int i = 0; i < 10 && waitpid(pid, NULL, WNOHANG) == 0; i++)
        usleep(100000);
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
    close(s->canale);
    s->canale = -1;
    s->pid = 0;
}

// Aspetta dati su un descrittore per al massimo ms millisecondi.
// Ripete l'attesa se la interrompe un segnale (per esempio un servizio che termina).
static int aspetta_dati(int fd, int ms) {
    struct pollfd p = { .fd = fd, .events = POLLIN };
    int r;
    while ((r = poll(&p, 1, ms)) < 0 && errno == EINTR)
        ;
    return r == 1;
}

static void rispondi(int c, const char *formato, ...) {
    char testo[512];
    va_list a;
    va_start(a, formato);
    int n = vsnprintf(testo, sizeof testo, formato, a);
    va_end(a);
    if (n > 0)
        scrivi_tutto(c, testo, n < (int)sizeof testo ? (size_t)n : sizeof testo - 1);
}

// Inoltra "ci sei?" al servizio sul suo canale e gira la risposta a chi l'ha chiesta.
static void chiedi(int c, struct servizio *s) {
    if (s->pid == 0) {
        rispondi(c, "%s e' spento\n", s->nome);
        return;
    }
    scrivi_tutto(s->canale, "ci sei?\n", 8);
    char risposta[256];
    ssize_t n;
    if (aspetta_dati(s->canale, 1000) && (n = read(s->canale, risposta, sizeof risposta - 1)) > 0) {
        risposta[n] = '\0';
        rispondi(c, "%s risponde: %s", s->nome, risposta);
    } else {
        rispondi(c, "%s non risponde\n", s->nome);
    }
}

static void intestazione(int c) {
    rispondi(c, "%-12s %-8s %-7s %-10s %s\n", "SERVIZIO", "STATO", "PID", "DA (s)", "RIAVVII");
}

static void riga_servizio(int c, struct servizio *s) {
    if (s->pid)
        rispondi(c, "%-12s %-8s %-7d %-10ld %d\n", s->nome, "acceso", s->pid,
                 (long)(time(NULL) - s->partito), s->riavvii);
    else
        rispondi(c, "%-12s %-8s %-7s %-10s %d\n", s->nome, "spento", "-", "-", s->riavvii);
}

// list: tutti i servizi, una riga ciascuno.
static void elenco(int c) {
    intestazione(c);
    for (int i = 0; i < N_SERVIZI; i++)
        riga_servizio(c, &servizi[i]);
}

// <nome> status: la riga del servizio e, se e' acceso, la sua risposta a "ci sei?".
static void stato(int c, struct servizio *s) {
    intestazione(c);
    riga_servizio(c, s);
    chiedi(c, s);
}

// Legge una richiesta, controlla chi la manda, la esegue e risponde.
static void servi(int c) {
    struct ucred chi;
    socklen_t l = sizeof chi;
    if (getsockopt(c, SOL_SOCKET, SO_PEERCRED, &chi, &l) != 0 || chi.uid != 0) {
        rispondi(c, "permesso negato: solo l'amministratore\n");
        return;
    }
    char riga[128];
    ssize_t n;
    if (!aspetta_dati(c, 1000) || (n = read(c, riga, sizeof riga - 1)) <= 0)
        return;
    riga[n] = '\0';
    char *nome = strtok(riga, " \t\n");
    char *azione = strtok(NULL, " \t\n");
    if (!nome)
        return;

    if (strcmp(nome, "list") == 0) {
        elenco(c);
        return;
    }
    struct servizio *s = trova(nome);
    if (!s) {
        rispondi(c, "servizio sconosciuto: %s\n", nome);
        return;
    }
    if (!azione) {
        rispondi(c, "manca il comando: %s status | down | up | restart\n", nome);
    } else if (strcmp(azione, "status") == 0) {
        stato(c, s);
    } else if (strcmp(azione, "up") == 0) {
        s->voluto = 1;
        if (s->pid == 0) {
            s->prossimo_avvio = 0;
            avvia(s);
        }
        rispondi(c, s->pid ? "%s acceso (pid %d)\n" : "%s: avvio fallito\n", s->nome, s->pid);
    } else if (strcmp(azione, "down") == 0) {
        s->voluto = 0;
        ferma(s);
        rispondi(c, "%s fermato\n", s->nome);
    } else if (strcmp(azione, "restart") == 0) {
        s->voluto = 1;
        ferma(s);
        s->prossimo_avvio = 0;
        avvia(s);
        rispondi(c, s->pid ? "%s riavviato (pid %d)\n" : "%s: avvio fallito\n", s->nome, s->pid);
    } else {
        rispondi(c, "comando sconosciuto: %s (prova: svcc help)\n", azione);
    }
}

static void al_figlio(int segnale) {
    (void)segnale;                             // serve solo a interrompere poll()
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);                  // un cliente che chiude non deve ucciderci
    struct sigaction sa = { .sa_handler = al_figlio };   // senza SA_RESTART: poll si interrompe
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, NULL);

    int ascolto = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    struct sockaddr_un ind = { .sun_family = AF_UNIX };
    strcpy(ind.sun_path, SOCKET_SERVICENTER);
    unlink(SOCKET_SERVICENTER);                  // quello rimasto dall'avvio precedente
    if (ascolto < 0 || bind(ascolto, (struct sockaddr *)&ind, sizeof ind) != 0
        || chmod(SOCKET_SERVICENTER, 0600) != 0 || listen(ascolto, 8) != 0) {
        printf("servicenter: impossibile aprire %s: %s\n", SOCKET_SERVICENTER, strerror(errno));
        return 1;
    }
    printf("servicenter: pronto, %d servizi\n", N_SERVIZI);

    for (;;) {
        raccogli();
        controlla();
        struct pollfd p = { .fd = ascolto, .events = POLLIN };
        if (poll(&p, 1, 1000) == 1) {
            int c = accept4(ascolto, NULL, NULL, SOCK_CLOEXEC);
            if (c >= 0) {
                servi(c);
                close(c);
            }
        }
    }
}
