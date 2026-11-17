/* recinto: prova che un programma chiuso in un recinto (APPLICAZIONI.md, sezione 5, "L'app
 * non vede niente") non arriva al disco, a internet, ai servizi e agli altri programmi, anche
 * se chiede tutto direttamente al kernel, senza CoreKit. C'e' SOLO per le prove.
 *
 * Sta solo nel disco disco_recinto.img (build.sh), in /data/tests: init lo fa partire
 * all'accensione, come sistema. Serve la scheda di rete di QEMU con l'ascoltatore della prova
 * (INTERNET = True in test/prove/recinto.py, vedi internet() sotto), per la controprova di
 * internet.
 *
 * Come e' fatto (lo stesso ordine del launcher, che dal 17 novembre 2026 chiude ogni app in un
 * recinto: corekit/sistema/recinto.h. Il launcher aggancia le cartelle allo stesso percorso di
 * fuori; questa prova, nata prima, usa ancora /app/code, /data e /home: per i muri non cambia):
 *   1. CONTROPROVA: una copia di se' diventa uid 1000 (come un programma senza recinto) e prova a
 *      leggere la Home, un file del sistema, il disco intero, a parlare al launcher e a
 *      collegarsi a internet. La Home, il file, il launcher e internet ci devono arrivare: se no
 *      la prova dentro il recinto non dimostrerebbe niente.
 *   2. IL RECINTO, costruito da una copia che ha ancora i poteri del sistema:
 *      a. unshare: mondi nuovi per i file montati, la rete, i numeri dei programmi, ecc.;
 *      b. una radice nuova vuota (tmpfs) con dentro solo: /app/code (questo programma, sola
 *         lettura), /data (vuota, dell'app), /system/resources (sola lettura) e le cartelle
 *         concesse (APPLICAZIONI.md, "I permessi sulle cartelle"), agganciate come farebbe il
 *         launcher: la Home in /home in sola lettura e, sopra, la sua sottocartella
 *         ProvaRecinto/Scrivibile in lettura e scrittura (la piu' interna vince);
 *      c. pivot_root: quella diventa "/" e la radice vera si stacca; la radice si rimonta in
 *         sola lettura;
 *      d. via i poteri: uid e gid 1000, nessun gruppo; poi NO_NEW_PRIVS (mai piu' poteri);
 *      e. seccomp: le chiamate vietate (unshare, mount, ptrace, bpf, ...) rispondono EPERM;
 *      f. execve di /app/code con l'argomento "dentro": da qui e' l'"app".
 *   2bis. IL RECINTO CON LA RETE (permesso network.use, "come Android e Flatpak"): uguale, ma
 *      senza CLONE_NEWNET: l'app vede la rete del computer. Si rilancia con "dentro-rete" e le
 *      righe dicono "dentro con la rete: ...": internet arriva, Home, launcher e disco restano
 *      invisibili (ENOENT), il seccomp c'e' ancora. Si conta anche quanti socket Unix astratti
 *      (che vivono nel mondo della rete) ci sarebbero da raggiungere: i nostri servizi usano
 *      file in /run/mshell, quindi 0.
 *   3. DENTRO, le stesse prove, che devono fallire: "non esiste" (ENOENT) per i file e il
 *      launcher, "rete irraggiungibile" (ENETUNREACH) per internet; e non deve riuscire a
 *      smontare i muri, farsi un mondo utente nuovo, spiare (ptrace) o montare (EPERM, per
 *      seccomp); un fork normale invece riesce. Le risorse del sistema invece si leggono. Nelle cartelle
 *      concesse: la Home si legge ma non si scrive (EROFS, "file system in sola lettura"),
 *      Scrivibile si scrive; con ".." da li' non si esce (si resta sulla radice dell'app).
 * Prima di tutto, come sistema, prepara nella Home vera ProvaRecinto/leggimi.txt e
 * ProvaRecinto/Scrivibile/, della persona (uid 1000): fuori la persona ci scrive (controprova:
 * a bloccare dentro e' l'aggancio, non i permessi del file).
 * Ogni passo e' una riga sulla seriale ("recinto: ... -> si'" o "... -> no FALLITA"), scritta in
 * una sola volta; alla fine "recinto: tutto bene" o "recinto: fallita".
 *
 * Il filtro seccomp e' un elenco di chiamate vietate, quello di CoreKit (recinto_seccomp): dentro
 * non si crea piu' un mondo utente nuovo, ne' con unshare ne' con clone. */
#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
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
#include <sys/ptrace.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include "corekit/sistema/recinto.h"   /* recinto_seccomp: il filtro delle chiamate, uguale al launcher */

#define UID_APP     1000
#define PROGRAMMA   "/data/tests/recinto"
#define RADICE      "/run/recinto"
#define CASA        "/data/users/1000/home"
#define FILE_SISTEMA "/system/config/network/firewall.conf"
#define DISCO       "/dev/vda"
#define LAUNCHER    "/run/mshell/launcher"
#define RISORSE     "/system/resources"
#define PROVA       CASA "/ProvaRecinto"
#define REGISTRO    16  /* il registro del sistema (la seriale), che init passa a tutti: CoreKit, interno.h */

static int fallite;
static int con_rete;   /* il recinto che si costruisce (o in cui si e') ha la rete del computer */

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

/* Collegarsi a internet: 10.0.2.100 porta 80, un indirizzo fisso della rete di QEMU che QEMU
 * inoltra a un ascoltatore aperto sul PC dalla prova (test/qemu.py, Macchina con
 * internet=True: "guestfwd"). 0 se collegato, o -1 con errno. Al massimo 3 secondi.
 * Vale solo un collegamento RIUSCITO: prima si chiedeva la porta 80 del PC e bastava il
 * rifiuto ("il pacchetto e' arrivato"), ma su Windows la rete di QEMU verso una porta chiusa
 * del PC non risponde mai (ogni tentativo scade dopo 3 s), e la prova dipendeva da come il PC
 * risponde. Cosi' risponde sempre qualcuno che la prova stessa ha acceso, sul Mac come sul PC. */
#define INTERNET_INDIRIZZO "10.0.2.100"
#define INTERNET_PORTA     80
static int internet(void) {
    int s = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (s < 0) return -1;
    struct sockaddr_in a = {.sin_family = AF_INET, .sin_port = htons(INTERNET_PORTA)};
    inet_pton(AF_INET, INTERNET_INDIRIZZO, &a.sin_addr);
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

/* Crea (o riscrive) un file con una riga. 0, o -1 con errno. */
static int scrivi_file(const char *percorso) {
    int d = open(percorso, O_CREAT | O_WRONLY | O_TRUNC | O_CLOEXEC, 0644);
    if (d < 0) return -1;
    int r = write(d, "ciao\n", 5) == 5 ? 0 : -1;
    close(d);
    return r;
}

/* Vero se 'percorso' e' la stessa cartella di "/" (stesso disco e stesso numero). */
static int e_la_radice(const char *percorso) {
    struct stat a, b;
    return stat(percorso, &a) == 0 && stat("/", &b) == 0 && a.st_dev == b.st_dev && a.st_ino == b.st_ino;
}

/* Le cartelle concesse, viste dall'app: la Home in sola lettura, Scrivibile anche in scrittura. */
static void prove_cartelle(void) {
    const char *dove = "dentro";
    int r;
    r = apri("/home/ProvaRecinto/leggimi.txt", O_RDONLY);
    passo(dove, "leggere un file della Home", r, r == 0);
    r = apri("/home/ProvaRecinto/leggimi.txt", O_WRONLY);
    passo(dove, "cambiare un file della Home", r, r < 0 && errno == EROFS);
    r = scrivi_file("/home/ProvaRecinto/nuovo.txt");
    passo(dove, "creare un file nella Home", r, r < 0 && errno == EROFS);
    r = unlink("/home/ProvaRecinto/leggimi.txt");
    passo(dove, "cancellare un file della Home", r, r < 0 && errno == EROFS);
    r = rename("/home/ProvaRecinto/leggimi.txt", "/home/ProvaRecinto/altro.txt");
    passo(dove, "cambiare nome a un file della Home", r, r < 0 && errno == EROFS);
    r = scrivi_file("/home/ProvaRecinto/Scrivibile/nuovo.txt");
    passo(dove, "creare un file in Scrivibile", r, r == 0);
    r = unlink("/home/ProvaRecinto/Scrivibile/nuovo.txt");
    passo(dove, "cancellarlo", r, r == 0);
    r = scrivi_file("/home/ProvaRecinto/Scrivibile/../nuovo.txt");
    passo(dove, "scrivere da Scrivibile con .. nella Home", r, r < 0 && errno == EROFS);
    passo(dove, "\"../../../..\" da Scrivibile resta sulla radice", 0,
          e_la_radice("/home/ProvaRecinto/Scrivibile/../../../../../.."));
    r = apri("/home/ProvaRecinto/Scrivibile/../../../../data/users", O_RDONLY | O_DIRECTORY);
    passo(dove, "arrivare al disco vero con ..", r, r < 0 && errno == ENOENT);
    /* da una cartella aperta, come se qualcuno gliel'avesse passata: e' dentro l'aggancio */
    int c = open("/home/ProvaRecinto/Scrivibile", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    int su = c < 0 ? -1 : openat(c, "../../../../../..", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    struct stat a, b;
    int giusto = su >= 0 && fstat(su, &a) == 0 && stat("/", &b) == 0 && a.st_ino == b.st_ino && a.st_dev == b.st_dev;
    passo(dove, "\"..\" da una cartella aperta resta sulla radice", su, giusto);
    if (su >= 0) close(su);
    if (c >= 0) close(c);
}

/* Le prove nel recinto con la rete del computer (permesso network.use): la rete non apre il
 * resto. Internet deve arrivare; Home, launcher e disco restano invisibili; il seccomp c'e'. */
static void prove_con_rete(void) {
    const char *dove = "dentro con la rete";
    int r;
    r = internet();
    passo(dove, "collegarsi a internet", r, r == 0);
    r = apri(CASA, O_RDONLY | O_DIRECTORY);
    passo(dove, "leggere la Home", r, r < 0 && errno == ENOENT);
    r = launcher();
    passo(dove, "parlare al launcher", r, r < 0 && errno == ENOENT);
    r = apri(DISCO, O_RDONLY);
    passo(dove, "leggere il disco intero", r, r < 0 && errno == ENOENT);
    r = unshare(CLONE_NEWUSER);
    passo(dove, "mondo utente nuovo con unshare", r, r < 0 && errno == EPERM);
}

/* Le prove, fuori (controprova) o dentro il recinto. */
static void prove(int dentro) {
    const char *dove = dentro ? "dentro" : "fuori";
    int r;
    if (dentro == 2) {
        prove_con_rete();
        return;
    }
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
          dentro ? (r < 0 && errno == ENETUNREACH) : r == 0);
    if (!dentro) {
        r = scrivi_file(PROVA "/nuovo.txt");
        passo(dove, "creare un file in ProvaRecinto (permessi del file)", r, r == 0);
        unlink(PROVA "/nuovo.txt");
        return;
    }
    prove_cartelle();
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
    passo(dove, "mondo utente nuovo con unshare", r, r < 0 && errno == EPERM);
    r = syscall(SYS_clone, CLONE_NEWUSER | SIGCHLD, 0, 0, 0, 0);
    if (r == 0) _exit(0);        /* se fosse riuscito, il figlio esce subito */
    passo(dove, "mondo utente nuovo con clone", r, r < 0 && errno == EPERM);
    r = ptrace(PTRACE_TRACEME, 0, 0, 0);
    passo(dove, "ptrace", r, r < 0 && errno == EPERM);
    r = mount("tmpfs", "/data", "tmpfs", 0, NULL);
    passo(dove, "montare un tmpfs su /data", r, r < 0 && errno == EPERM);
    /* controprova: un fork normale funziona ancora */
    pid_t figlio = fork();
    if (figlio == 0) _exit(0);
    int stato = -1;
    r = figlio < 0 ? -1 : waitpid(figlio, &stato, 0);
    passo(dove, "fork normale", r, r >= 0 && WIFEXITED(stato) && WEXITSTATUS(stato) == 0);
}

/* IL FILTRO SECCOMP e' quello di CoreKit (recinto_seccomp, corekit/interno/recinto.c: l'elenco
 * delle chiamate vietate e il perche' sono li'): lo stesso che il launcher mette a ogni app,
 * cosi' la prova controlla proprio quello. */

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
    /* le cartelle concesse: prima la piu' grande, poi la piu' piccola sopra */
    CONTROLLA(mkdir(RADICE "/home", 0755));
    CONTROLLA(mount(CASA, RADICE "/home", NULL, MS_BIND | MS_REC, NULL));
    CONTROLLA(sola_lettura(RADICE "/home"));
    CONTROLLA(mount(PROVA "/Scrivibile", RADICE "/home/ProvaRecinto/Scrivibile", NULL, MS_BIND | MS_REC, NULL));
    CONTROLLA(mount(NULL, RADICE "/home/ProvaRecinto/Scrivibile", NULL, MS_BIND | MS_REMOUNT | MS_NOSUID | MS_NODEV, NULL));
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
    CONTROLLA(recinto_seccomp());    /* dopo NO_NEW_PRIVS, prima di execve */
    execl("/app/code", "code", con_rete ? "dentro-rete" : "dentro", (char *)NULL);
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
    /* senza il permesso della rete: una rete vuota (CLONE_NEWNET); con: quella del computer */
    if (unshare(CLONE_NEWNS | (con_rete ? 0 : CLONE_NEWNET) | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWUTS) < 0) {
        riga("unshare: %s -> no FALLITA", strerror(errno));
        _exit(2);
    }
    _exit(in_una_copia(entra_nel_recinto) == 0 ? 0 : 1);
}

int main(int argc, char **argv) {
    if (argc > 1 && (!strcmp(argv[1], "dentro") || !strcmp(argv[1], "dentro-rete"))) {
        prove(argv[1][6] ? 2 : 1);
        return fallite ? 1 : 0;
    }
    /* le cartelle della persona per la prova, come se le avesse fatte lei */
    mkdir(PROVA, 0755);
    mkdir(PROVA "/Scrivibile", 0755);
    if (scrivi_file(PROVA "/leggimi.txt") < 0 || chown(PROVA, UID_APP, UID_APP) < 0 ||
        chown(PROVA "/Scrivibile", UID_APP, UID_APP) < 0 || chown(PROVA "/leggimi.txt", UID_APP, UID_APP) < 0) {
        riga("preparare %s: %s -> no FALLITA", PROVA, strerror(errno));
        riga("fallita");
        return 0;
    }
    /* la rete del sistema ci mette qualche secondo ad accendersi: fino a un minuto. Finche'
     * non ha l'indirizzo il tentativo risponde subito "rete irraggiungibile"; appena c'e', il
     * collegamento all'ascoltatore della prova riesce */
    for (int volta = 0; volta < 60; volta++) {
        if (internet() == 0) break;
        sleep(1);
    }
    int fuori = in_una_copia(controprova);
    int dentro = in_una_copia(recinto);
    con_rete = 1;
    int dentro_rete = in_una_copia(recinto);
    /* con la rete del computer l'app vedrebbe anche i socket Unix "astratti" (vivono nel mondo
     * della rete): i nostri servizi usano file in /run/mshell, quindi qui non deve essercene
     * nessuno; si conta dall'elenco del kernel (le righe il cui nome comincia con '@') */
    int astratti = 0;
    FILE *u = fopen("/proc/net/unix", "re");
    if (u) {
        char l[512];
        while (fgets(l, sizeof l, u)) {
            char *nome = strrchr(l, ' ');
            if (nome && nome[1] == '@') astratti++;
        }
        fclose(u);
    }
    riga("socket astratti visibili con la rete del computer: %d", astratti);
    riga("controprova %s, recinto %s, recinto con la rete %s", fuori == 0 ? "giusta" : "sbagliata",
         dentro == 0 ? "giusto" : "sbagliato", dentro_rete == 0 ? "giusto" : "sbagliato");
    riga(fuori == 0 && dentro == 0 && dentro_rete == 0 ? "tutto bene" : "fallita");
    return 0;
}
