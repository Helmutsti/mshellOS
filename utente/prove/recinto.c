/* recinto: prova che un programma chiuso in un recinto (APPLICAZIONI.md, sezione 5, "L'app
 * non vede niente") non arriva al disco, a internet, ai servizi e agli altri programmi, anche
 * se chiede tutto direttamente al kernel, senza CoreKit. C'e' SOLO per le prove.
 *
 * Sta solo nel disco disco_recinto.img (build.sh), in /data/tests: init lo fa partire
 * all'accensione, come sistema. Serve la scheda di rete di QEMU (RETE = True in
 * test/prove/recinto.py), per la controprova di internet.
 *
 * Come e' fatto (lo stesso ordine che seguira' il launcher):
 *   1. CONTROPROVA: una copia di se' diventa uid 1000 (come oggi partono le app) e prova a
 *      leggere la Home, un file del sistema, il disco intero, a parlare al launcher e a
 *      collegarsi a internet. La Home, il file, il launcher e internet ci devono arrivare: se no
 *      la prova dentro il recinto non dimostrerebbe niente.
 *   2. IL RECINTO, costruito da una copia che ha ancora i poteri del sistema:
 *      a. unshare: mondi nuovi per i file montati, la rete, i numeri dei programmi, ecc.;
 *      b. una radice nuova vuota (tmpfs) con dentro solo: /app/code (questo programma, sola
 *         lettura), /data (vuota, dell'app), /system/resources (sola lettura);
 *      c. pivot_root: quella diventa "/" e la radice vera si stacca; la radice si rimonta in
 *         sola lettura;
 *      d. via i poteri: uid e gid 1000, nessun gruppo; poi NO_NEW_PRIVS (mai piu' poteri);
 *      e. execve di /app/code con l'argomento "dentro": da qui e' l'"app".
 *   3. DENTRO, le stesse prove, che devono fallire: "non esiste" (ENOENT) per i file e il
 *      launcher, "rete irraggiungibile" (ENETUNREACH) per internet; e non deve riuscire a
 *      smontare i muri (EPERM). Le risorse del sistema invece si leggono.
 * Ogni passo e' una riga sulla seriale ("recinto: ... -> si'" o "... -> no FALLITA"), scritta in
 * una sola volta; alla fine "recinto: tutto bene" o "recinto: fallita".
 *
 * Volutamente NON c'e' ancora seccomp (l'elenco delle chiamate permesse): la riga
 * "informazione" sui mondi utente dice cosa un'app senza seccomp puo' ancora chiedere. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <poll.h>
#include <sched.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#define UID_APP     1000
#define PROGRAMMA   "/data/tests/recinto"
#define RADICE      "/run/recinto"
#define CASA        "/data/users/1000/home"
#define FILE_SISTEMA "/system/config/network/firewall.conf"
#define DISCO       "/dev/vda"
#define LAUNCHER    "/run/mshell/launcher"
#define RISORSE     "/system/resources"
#define REGISTRO    8   /* il registro del sistema (la seriale), che init passa a tutti: CoreKit, interno.h */

static int fallite;

/* Una riga sulla seriale, in una sola scrittura. */
static void riga(const char *formato, ...) {
    char testo[512];
    int n = snprintf(testo, sizeof testo, "recinto: ");
    va_list a;
    va_start(a, formato);
    n += vsnprintf(testo + n, sizeof testo - n, formato, a);
    va_end(a);
    if (n >= (int)sizeof testo - 1) n = sizeof testo - 2;
    testo[n++] = '\n';
    if (write(REGISTRO, testo, n) < 0) {}
}

static void passo(const char *dove, const char *cosa, int risultato, int giusto) {
    riga("%s: %s: %s -> %s", dove, cosa, risultato < 0 ? strerror(errno) : "riuscito",
         giusto ? "si'" : "no FALLITA");
    if (!giusto) fallite = 1;
}

/* Collegarsi a internet: 10.0.2.2 porta 80 (il PC, dalla rete di QEMU). 0 se collegato, o
 * -1 con errno; "rifiutato" vuol dire che il pacchetto e' arrivato. Al massimo 3 secondi. */
static int internet(void) {
    int s = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (s < 0) return -1;
    struct sockaddr_in a = {.sin_family = AF_INET, .sin_port = htons(80)};
    inet_pton(AF_INET, "10.0.2.2", &a.sin_addr);
    int r = connect(s, (struct sockaddr *)&a, sizeof a);
    if (r < 0 && errno == EINPROGRESS) {
        struct pollfd p = {.fd = s, .events = POLLOUT};
        if (poll(&p, 1, 3000) == 1) {
            int e = 0;
            socklen_t l = sizeof e;
            getsockopt(s, SOL_SOCKET, SO_ERROR, &e, &l);
            r = e ? -1 : 0;
            errno = e;
        } else {
            errno = ETIMEDOUT;
        }
    }
    int e = errno;
    close(s);
    errno = e;
    return r;
}

static int launcher(void) {
    int s = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);   /* come i servizi: comunicazione.c */
    if (s < 0) return -1;
    struct sockaddr_un a = {.sun_family = AF_UNIX};
    strncpy(a.sun_path, LAUNCHER, sizeof a.sun_path - 1);
    int r = connect(s, (struct sockaddr *)&a, sizeof a);
    int e = errno;
    close(s);
    errno = e;
    return r;
}

static int apri(const char *percorso, int modo) {
    int d = open(percorso, modo | O_CLOEXEC);
    if (d >= 0) close(d);
    return d < 0 ? -1 : 0;
}

/* Le prove, fuori (controprova) o dentro il recinto. */
static void prove(int dentro) {
    const char *dove = dentro ? "dentro" : "fuori";
    int r;
    r = apri(CASA, O_RDONLY | O_DIRECTORY);
    passo(dove, "leggere la Home", r, dentro ? (r < 0 && errno == ENOENT) : r == 0);
    r = apri(FILE_SISTEMA, O_RDONLY);
    passo(dove, "leggere un file del sistema", r, dentro ? (r < 0 && errno == ENOENT) : r == 0);
    r = apri(DISCO, O_RDONLY);           /* fuori: riservato al sistema; dentro: non c'e' */
    passo(dove, "leggere il disco intero", r, dentro ? (r < 0 && errno == ENOENT) : r < 0);
    r = launcher();
    passo(dove, "parlare al launcher", r, dentro ? (r < 0 && errno == ENOENT) : r == 0);
    r = internet();
    passo(dove, "collegarsi a internet", r,
          dentro ? (r < 0 && errno == ENETUNREACH) : (r == 0 || errno == ECONNREFUSED));
    if (!dentro) return;
    riga("dentro: uid %d, numero del programma %d (1 = vede solo se stesso)", (int)getuid(), (int)getpid());
    passo(dove, "uid dell'app", 0, getuid() == UID_APP && geteuid() == UID_APP);
    passo(dove, "vede solo se stesso", 0, getpid() == 1);
    r = apri(RISORSE, O_RDONLY | O_DIRECTORY);
    passo(dove, "leggere le risorse del sistema", r, r == 0);
    r = open("/app/code", O_WRONLY | O_CLOEXEC);
    passo(dove, "cambiare il proprio programma", r, r < 0);
    r = umount2("/", MNT_DETACH);
    passo(dove, "smontare la radice", r, r < 0 && errno == EPERM);
    r = unshare(CLONE_NEWNS);
    passo(dove, "farsi un mondo dei file nuovo", r, r < 0 && errno == EPERM);
    r = unshare(CLONE_NEWUSER);
    riga("dentro: informazione: un mondo utente nuovo (lo chiudera' seccomp): %s",
         r < 0 ? strerror(errno) : "riuscito");
}

/* Un file vuoto su cui montare un altro file. */
static int segnaposto(const char *percorso) {
    int d = open(percorso, O_CREAT | O_WRONLY | O_CLOEXEC, 0644);
    if (d < 0) return -1;
    close(d);
    return 0;
}

static int sola_lettura(const char *percorso) {
    return mount(NULL, percorso, NULL, MS_BIND | MS_REMOUNT | MS_RDONLY | MS_NOSUID | MS_NODEV, NULL);
}

/* Costruisce il recinto e ci fa partire questo programma come "app". Gira come sistema,
 * nella copia che e' gia' il primo programma del suo mondo dei numeri. Torna solo se fallisce. */
static void entra_nel_recinto(void) {
#define CONTROLLA(x) do { if ((x) < 0) { riga("%s: %s -> no FALLITA", #x, strerror(errno)); _exit(2); } } while (0)
    CONTROLLA(mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL));
    mkdir(RADICE, 0755);
    CONTROLLA(mount("tmpfs", RADICE, "tmpfs", MS_NOSUID | MS_NODEV, "size=64k,mode=755"));
    CONTROLLA(mkdir(RADICE "/app", 0755));
    CONTROLLA(mkdir(RADICE "/data", 0755));
    CONTROLLA(mkdir(RADICE "/system", 0755));
    CONTROLLA(mkdir(RADICE "/system/resources", 0755));
    CONTROLLA(mkdir(RADICE "/vecchia", 0755));
    CONTROLLA(segnaposto(RADICE "/app/code"));
    CONTROLLA(mount(PROGRAMMA, RADICE "/app/code", NULL, MS_BIND, NULL));
    CONTROLLA(sola_lettura(RADICE "/app/code"));
    CONTROLLA(mount(RISORSE, RADICE "/system/resources", NULL, MS_BIND | MS_REC, NULL));
    CONTROLLA(sola_lettura(RADICE "/system/resources"));
    CONTROLLA(mount("tmpfs", RADICE "/data", "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC,
                    "size=1m,mode=700,uid=1000,gid=1000"));
    CONTROLLA(chdir(RADICE));
    CONTROLLA(syscall(SYS_pivot_root, ".", "vecchia"));
    CONTROLLA(chdir("/"));
    CONTROLLA(umount2("/vecchia", MNT_DETACH));
    CONTROLLA(rmdir("/vecchia"));
    CONTROLLA(sola_lettura("/"));
    CONTROLLA(setgroups(0, NULL));
    CONTROLLA(setgid(UID_APP));
    CONTROLLA(setuid(UID_APP));
    CONTROLLA(prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0));
    execl("/app/code", "code", "dentro", (char *)NULL);
    riga("execve /app/code: %s -> no FALLITA", strerror(errno));
    _exit(2);
#undef CONTROLLA
}

/* Fa girare 'fai' in una copia e aspetta: 0 se la copia e' uscita con 0. */
static int in_una_copia(void (*fai)(void)) {
    pid_t p = fork();
    if (p < 0) return -1;
    if (p == 0) {
        fai();
        _exit(fallite ? 1 : 0);
    }
    int stato;
    if (waitpid(p, &stato, 0) < 0) return -1;
    return WIFEXITED(stato) ? WEXITSTATUS(stato) : -1;
}

static void controprova(void) {
    if (setgroups(0, NULL) < 0 || setgid(UID_APP) < 0 || setuid(UID_APP) < 0) {
        riga("fuori: diventare uid 1000: %s -> no FALLITA", strerror(errno));
        _exit(2);
    }
    prove(0);
}

/* Mondi nuovi, poi una copia che e' il primo programma del mondo dei numeri nuovo. */
static void recinto(void) {
    if (unshare(CLONE_NEWNS | CLONE_NEWNET | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWUTS) < 0) {
        riga("unshare: %s -> no FALLITA", strerror(errno));
        _exit(2);
    }
    _exit(in_una_copia(entra_nel_recinto) == 0 ? 0 : 1);
}

int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "dentro")) {
        prove(1);
        return fallite ? 1 : 0;
    }
    /* la rete del sistema ci mette qualche secondo ad accendersi: fino a un minuto */
    for (int volta = 0; volta < 60; volta++) {
        if (internet() == 0 || errno == ECONNREFUSED) break;
        sleep(1);
    }
    int fuori = in_una_copia(controprova);
    int dentro = in_una_copia(recinto);
    riga("controprova %s, recinto %s", fuori == 0 ? "giusta" : "sbagliata", dentro == 0 ? "giusto" : "sbagliato");
    riga(fuori == 0 && dentro == 0 ? "tutto bene" : "fallita");
    return 0;
}
