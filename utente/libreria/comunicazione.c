/* La libreria dei programmi, sopra Linux: parlare con gli altri programmi.
 *
 *   ascolta, collega, accetta   socket AF_UNIX di tipo SOCK_SEQPACKET (a pacchetti: ogni
 *                               messaggio arriva intero), con il nome come file in
 *                               /run/mshell/: bind e listen, connect, accept4
 *   coppia                      socketpair: due capi gia' collegati, senza nome
 *   manda, ricevi               sendmsg e recvmsg; un foglio allegato viaggia come
 *                               "SCM_RIGHTS": il kernel lo mette tra i descrittori di
 *                               chi riceve, con un numero nuovo
 *   chi, programma_di, termina  SO_PEERCRED: il kernel dice chi c'e' all'altro capo (il
 *                               suo processo e il suo uid); il suo programma e' in
 *                               /proc/<numero>/exe; termina e' kill
 *   foglio, mappa, togli        memfd_create (memoria senza file), mmap, munmap
 *   aspetta                     poll
 *   chiudi                      close
 *
 * Tutti i descrittori nascono O_CLOEXEC ("da chiudere all'execve"): un programma che ne
 * fa partire un altro non gli lascia niente per sbaglio. Quelli da prestare si passano
 * con avvia_con (libreria.c). */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "libreria.h"
#include "interno.h"

/* --- i nomi --- */

#define CARTELLA_NOMI "/run/mshell"

/* L'indirizzo di un nome: /run/mshell/<nome>. 0, o -1 se il nome non va bene. */
static int indirizzo(const char *nome, struct sockaddr_un *a) {
    int l = (int)strnlen(nome, MAX_NOME);
    if (l == 0 || l >= MAX_NOME || strchr(nome, '/') || !strcmp(nome, ".") || !strcmp(nome, ".."))
        return -1;
    *a = (struct sockaddr_un){ .sun_family = AF_UNIX };
    memcpy(a->sun_path, CARTELLA_NOMI "/", sizeof CARTELLA_NOMI);
    memcpy(a->sun_path + sizeof CARTELLA_NOMI, nome, (size_t)l + 1);
    return 0;
}

static int nuovo_socket(void) { return socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0); }

int collega(const char *nome) {
    struct sockaddr_un a;
    if (indirizzo(nome, &a) < 0) return -1;
    int d = nuovo_socket();
    if (d < 0) return -1;
    int r;
    do r = connect(d, (struct sockaddr *)&a, sizeof a); while (r < 0 && errno == EINTR);
    if (r < 0) { close(d); return -1; }
    return d;
}

/* Il nome e' un file: se c'e' gia', puo' essere di uno che ascolta ancora (allora e'
 * preso: -1) o rimasto da uno che non c'e' piu' (allora lo si toglie). Per saperlo, ci
 * si prova a collegare. Il file si lascia usare a tutti (0666): collegarsi a un nome
 * vuol dire poterci scrivere. */
int ascolta(const char *nome) {
    struct sockaddr_un a;
    if (indirizzo(nome, &a) < 0) return -1;
    mkdir(CARTELLA_NOMI, 0755);
    int prova = collega(nome);
    if (prova >= 0) { close(prova); return -1; }
    unlink(a.sun_path);
    int d = nuovo_socket();
    if (d < 0) return -1;
    if (bind(d, (struct sockaddr *)&a, sizeof a) < 0 || chmod(a.sun_path, 0666) < 0 || listen(d, 16) < 0) {
        close(d);
        return -1;
    }
    return d;
}

int accetta(int d) {
    int nuovo;
    do nuovo = accept4(d, 0, 0, SOCK_CLOEXEC); while (nuovo < 0 && errno == EINTR);
    return nuovo;
}

int coppia(int d[2]) { return socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, d); }

/* --- mandare e ricevere --- */

/* Lo spazio per un descrittore allegato al messaggio (SCM_RIGHTS). */
union allegato {
    struct cmsghdr intestazione;
    char spazio[CMSG_SPACE(sizeof(int))];
};

/* MSG_NOSIGNAL: se l'altro ha chiuso, -1 e basta (senza, Linux manderebbe il segnale
 * SIGPIPE, che chiude il programma). */
static int manda_con(int d, const void *dati, unsigned int byte, int foglio, int subito) {
    struct iovec pezzo = { (void *)dati, byte };
    struct msghdr m = { .msg_iov = &pezzo, .msg_iovlen = 1 };
    union allegato a;
    if (foglio >= 0) {
        memset(&a, 0, sizeof a);
        m.msg_control = a.spazio;
        m.msg_controllen = sizeof a.spazio;
        struct cmsghdr *c = CMSG_FIRSTHDR(&m);
        c->cmsg_level = SOL_SOCKET;
        c->cmsg_type = SCM_RIGHTS;
        c->cmsg_len = CMSG_LEN(sizeof(int));
        memcpy(CMSG_DATA(c), &foglio, sizeof(int));
    }
    if (byte > MAX_MESSAGGIO) return -1;
    ssize_t n;
    do n = sendmsg(d, &m, MSG_NOSIGNAL | (subito ? MSG_DONTWAIT : 0)); while (n < 0 && errno == EINTR);
    if (n < 0 && errno == ENOTSOCK && foglio < 0) return scrivi_in(d, dati, byte);   /* un file */
    return n < 0 ? -1 : (int)n;
}

int manda(int d, const void *dati, unsigned int byte) { return manda_con(d, dati, byte, -1, 0); }

int manda_subito(int d, const void *dati, unsigned int byte) { return manda_con(d, dati, byte, -1, 1); }

int manda_foglio(int d, const void *dati, unsigned int byte, int foglio) {
    return manda_con(d, dati, byte, foglio, 0);
}

int ricevi_foglio(int d, void *dove, unsigned int byte, int *foglio) {
    if (foglio) *foglio = -1;
    struct iovec pezzo = { dove, byte };
    struct msghdr m = { .msg_iov = &pezzo, .msg_iovlen = 1 };
    union allegato a;
    if (foglio) {
        m.msg_control = a.spazio;
        m.msg_controllen = sizeof a.spazio;
    }
    ssize_t n;
    do n = recvmsg(d, &m, MSG_CMSG_CLOEXEC); while (n < 0 && errno == EINTR);
    if (n < 0 && errno == ENOTSOCK) return ricevi_non_socket(d, dove, byte);
    if (n < 0) return -1;
    /* il foglio allegato: il kernel l'ha gia' messo tra i nostri descrittori (c'e' posto
     * per uno solo: se ne mandano di piu', gli altri il kernel li butta) */
    struct cmsghdr *c = foglio ? CMSG_FIRSTHDR(&m) : 0;
    if (c && c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS && c->cmsg_len == CMSG_LEN(sizeof(int)))
        memcpy(foglio, CMSG_DATA(c), sizeof(int));
    return (int)n;
}

/* Senza spazio per gli allegati, un foglio allegato il kernel lo butta via da solo. */
int ricevi(int d, void *dove, unsigned int byte) { return ricevi_foglio(d, dove, byte, 0); }

int chiudi(int d) {
    dimentica_cartella(d);
    return close(d);
}

int aspetta(struct da_guardare *elenco, int quanti, int ms) {
    if (quanti < 0 || quanti > MAX_GUARDATI) return -1;
    struct pollfd p[MAX_GUARDATI];
    for (int i = 0; i < quanti; i++) {
        p[i] = (struct pollfd){ elenco[i].descrittore, POLLIN, 0 };
        elenco[i].pronto = 0;
    }
    int r;
    do r = poll(p, (nfds_t)quanti, ms); while (r < 0 && errno == EINTR);
    if (r < 0) return -1;
    int pronti = 0;
    for (int i = 0; i < quanti; i++) {
        if (p[i].revents & POLLNVAL) return -1;       /* un descrittore che non c'e' */
        if (p[i].revents & (POLLIN | POLLHUP | POLLERR)) {
            elenco[i].pronto = 1;
            pronti++;
        }
    }
    return pronti;
}

/* --- chi c'e' all'altro capo --- */

static int credenziali(int d, struct ucred *c) {
    socklen_t l = sizeof *c;
    return getsockopt(d, SOL_SOCKET, SO_PEERCRED, c, &l) < 0 || c->pid <= 0 ? -1 : 0;
}

int chi(int d) {
    struct ucred c;
    return credenziali(d, &c) < 0 ? -1 : (int)c.uid;
}

int programma_di(int d, char *dove) {
    struct ucred c;
    if (credenziali(d, &c) < 0) return -1;
    char percorso[32] = "/proc/", cifre[12];
    int k = 0, p = 6;
    unsigned int n = (unsigned int)c.pid;
    do { cifre[k++] = (char)('0' + n % 10); n /= 10; } while (n);
    while (k) percorso[p++] = cifre[--k];
    memcpy(percorso + p, "/exe", 5);
    ssize_t l = readlink(percorso, dove, MAX_PERCORSO);
    if (l <= 0 || l >= MAX_PERCORSO) return -1;
    dove[l] = 0;
    return 0;
}

int termina(int d) {
    struct ucred c;
    if (credenziali(d, &c) < 0 || c.pid <= 1) return -1;
    return kill(c.pid, SIGKILL) == 0 ? 0 : -1;
}

/* --- i fogli --- */

/* IL SIGILLO. Chi riceve un foglio lo mette nella sua memoria e ci legge. Se chi l'ha
 * dato potesse accorciarlo dopo, leggere oltre la nuova fine farebbe cadere chi legge
 * (il segnale SIGBUS): un'applicazione potrebbe far cadere il compositore. Per questo un
 * foglio, appena fatto, si sigilla: la sua misura non cambia piu', e mappa accetta solo
 * fogli sigillati. */
#define SIGILLO (F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL)

int foglio(unsigned int byte) {
    if (byte == 0 || byte > FOGLIO_MAX) return -1;
    int d = memfd_create("foglio", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (d < 0) return -1;
    if (ftruncate(d, byte) < 0 || fcntl(d, F_ADD_SEALS, SIGILLO) < 0) {
        close(d);
        return -1;
    }
    return d;
}

/* I fogli messi in memoria, con la loro misura: munmap la vuole. */
static struct { void *dove; size_t byte; } mappati[MAX_MAPPATI];

void *mappa(int d, unsigned int *byte) {
    struct stat s;
    int sigilli = fcntl(d, F_GET_SEALS);
    if (sigilli < 0 || (sigilli & (F_SEAL_SHRINK | F_SEAL_GROW)) != (F_SEAL_SHRINK | F_SEAL_GROW) ||
        fstat(d, &s) < 0 || s.st_size <= 0 || s.st_size > FOGLIO_MAX)
        return 0;
    int i = 0;
    while (i < MAX_MAPPATI && mappati[i].dove) i++;
    if (i == MAX_MAPPATI) return 0;
    void *p = mmap(0, (size_t)s.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, d, 0);
    if (p == MAP_FAILED) return 0;
    mappati[i].dove = p;
    mappati[i].byte = (size_t)s.st_size;
    if (byte) *byte = (unsigned int)s.st_size;
    return p;
}

int togli(void *indirizzo) {
    for (int i = 0; i < MAX_MAPPATI; i++)
        if (indirizzo && mappati[i].dove == indirizzo) {
            munmap(indirizzo, mappati[i].byte);
            mappati[i].dove = 0;
            return 0;
        }
    return -1;
}
