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
 * Il filtro seccomp e' un elenco di chiamate vietate (vedi metti_seccomp): dentro non si crea
 * piu' un mondo utente nuovo, ne' con unshare ne' con clone. */
#define _GNU_SOURCE
#include <errno.h>
#include <stddef.h>
#include <signal.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
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

#define UID_APP     1000
#define PROGRAMMA   "/data/tests/recinto"
#define RADICE      "/run/recinto"
#define CASA        "/data/users/1000/home"
#define FILE_SISTEMA "/system/config/network/firewall.conf"
#define DISCO       "/dev/vda"
#define LAUNCHER    "/run/mshell/launcher"
#define RISORSE     "/system/resources"
#define PROVA       CASA "/ProvaRecinto"
#define REGISTRO    8   /* il registro del sistema (la seriale), che init passa a tutti: CoreKit, interno.h */

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
    passo(dove, "collegarsi a internet", r, r == 0 || errno == ECONNREFUSED);
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
          dentro ? (r < 0 && errno == ENETUNREACH) : (r == 0 || errno == ECONNREFUSED));
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

/* IL FILTRO SECCOMP (BPF classico, scritto a mano, nessuna libreria).
 * Modello "elenco di chiamate vietate" (come Flatpak): le chiamate qui sotto rispondono EPERM,
 * tutto il resto e' permesso. Si installa DOPO NO_NEW_PRIVS (senza, seccomp chiederebbe poteri) e
 * PRIMA di execve, e si eredita in tutti i figli: l'app non puo' piu' toglierlo.
 * Le chiamate vietate, in gruppi:
 *   - mondi e montaggi (unshare, setns, mount, umount2, pivot_root, chroot, la famiglia nuova
 *     move_mount/open_tree/fsopen/fsmount/fsconfig/fspick/mount_setattr): servono a farsi un
 *     mondo utente nuovo (dove si e' "root" e si ottengono altri poteri) o a smontare i muri;
 *   - ptrace e process_vm_*: spiare o modificare altri programmi;
 *   - kexec_*, *_module, bpf, perf_event_open, userfaultfd: toccare il kernel o sfruttare i suoi
 *     errori (fonte piu' comune di fughe dai recinti);
 *   - keyctl, add_key, request_key, acct, swapon/swapoff, reboot, quotactl, syslog: risorse e
 *     comandi di tutta la macchina;
 *   - personality, open_by_handle_at, name_to_handle_at, lookup_dcookie: cambiare il modo di
 *     eseguire, o aggirare i permessi dei percorsi con le "maniglie" dei file;
 *   - iopl, ioperm (solo x86_64): porte dell'hardware;
 *   - clone3 risponde ENOSYS, non EPERM: musl e glibc, se clone3 non c'e', ripiegano su clone;
 *     clone invece e' permesso (serve per fork e thread) ma non con i flag CLONE_NEW* nel primo
 *     argomento (parte bassa a 32 bit): sarebbe un altro modo di farsi un mondo nuovo.
 * Prima di tutto si controlla l'architettura: se non e' quella per cui si compila, si uccide il
 * programma (altrimenti i numeri delle chiamate cambierebbero significato). Su x86_64 anche le
 * chiamate "x32" (numero con il bit __X32_SYSCALL_BIT) si uccidono: hanno altri numeri. */
#ifdef __aarch64__
#define ARCH_SECCOMP AUDIT_ARCH_AARCH64
#elif defined(__x86_64__)
#define ARCH_SECCOMP AUDIT_ARCH_X86_64
#else
#error "recinto: processore non previsto"
#endif

#ifndef SYS_clone3
#define SYS_clone3 435   /* uguale su tutti i processori */
#endif
#ifndef CLONE_NEWTIME
#define CLONE_NEWTIME 0x00000080
#endif
#define FLAG_MONDI ((unsigned)(CLONE_NEWUSER | CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWNET | \
                               CLONE_NEWIPC | CLONE_NEWUTS | CLONE_NEWCGROUP | CLONE_NEWTIME))

static const int vietate[] = {
    /* mondi e montaggi: tornare a vedere i file o uscire dal muro */
#ifdef SYS_unshare
    SYS_unshare,
#endif
#ifdef SYS_setns
    SYS_setns,
#endif
#ifdef SYS_mount
    SYS_mount,
#endif
#ifdef SYS_umount2
    SYS_umount2,
#endif
#ifdef SYS_pivot_root
    SYS_pivot_root,
#endif
#ifdef SYS_chroot
    SYS_chroot,
#endif
#ifdef SYS_move_mount
    SYS_move_mount,
#endif
#ifdef SYS_open_tree
    SYS_open_tree,
#endif
#ifdef SYS_fsopen
    SYS_fsopen,
#endif
#ifdef SYS_fsmount
    SYS_fsmount,
#endif
#ifdef SYS_fsconfig
    SYS_fsconfig,
#endif
#ifdef SYS_fspick
    SYS_fspick,
#endif
#ifdef SYS_mount_setattr
    SYS_mount_setattr,
#endif
    /* spiare o toccare gli altri programmi */
#ifdef SYS_ptrace
    SYS_ptrace,
#endif
#ifdef SYS_process_vm_readv
    SYS_process_vm_readv,
#endif
#ifdef SYS_process_vm_writev
    SYS_process_vm_writev,
#endif
    /* toccare il kernel: caricare codice, programmi nel kernel, contatori dell'hardware */
#ifdef SYS_kexec_load
    SYS_kexec_load,
#endif
#ifdef SYS_kexec_file_load
    SYS_kexec_file_load,
#endif
#ifdef SYS_init_module
    SYS_init_module,
#endif
#ifdef SYS_finit_module
    SYS_finit_module,
#endif
#ifdef SYS_delete_module
    SYS_delete_module,
#endif
#ifdef SYS_bpf
    SYS_bpf,
#endif
#ifdef SYS_perf_event_open
    SYS_perf_event_open,
#endif
#ifdef SYS_userfaultfd
    SYS_userfaultfd,
#endif
    /* chiavi del kernel, contabilita', swap, spegnimento, quote, messaggi del kernel */
#ifdef SYS_keyctl
    SYS_keyctl,
#endif
#ifdef SYS_add_key
    SYS_add_key,
#endif
#ifdef SYS_request_key
    SYS_request_key,
#endif
#ifdef SYS_acct
    SYS_acct,
#endif
#ifdef SYS_swapon
    SYS_swapon,
#endif
#ifdef SYS_swapoff
    SYS_swapoff,
#endif
#ifdef SYS_reboot
    SYS_reboot,
#endif
#ifdef SYS_quotactl
    SYS_quotactl,
#endif
#ifdef SYS_syslog
    SYS_syslog,
#endif
    /* cambiare modo di eseguire; aggirare i permessi con le maniglie dei file */
#ifdef SYS_personality
    SYS_personality,
#endif
#ifdef SYS_open_by_handle_at
    SYS_open_by_handle_at,
#endif
#ifdef SYS_name_to_handle_at
    SYS_name_to_handle_at,
#endif
#ifdef SYS_lookup_dcookie
    SYS_lookup_dcookie,
#endif
    /* porte dell'hardware (solo x86_64: altrove non esistono) */
#ifdef SYS_iopl
    SYS_iopl,
#endif
#ifdef SYS_ioperm
    SYS_ioperm,
#endif
};
#define N_VIETATE ((int)(sizeof vietate / sizeof vietate[0]))

#define SALTO(codice, k) ((struct sock_filter)BPF_JUMP(codice, k, 0, 0))
#define LEGGI(offset) ((struct sock_filter)BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offset))
#define RISPOSTA(r) ((struct sock_filter)BPF_STMT(BPF_RET | BPF_K, r))

/* Costruisce il programma BPF e lo installa. 0, o -1 con errno. I salti BPF vanno solo avanti:
 * le risposte (permetti, EPERM, ENOSYS, uccidi) e il controllo di clone stanno in fondo, e ogni
 * salto si calcola come "posizione di arrivo - posizione del salto - 1". */
static int metti_seccomp(void) {
    struct sock_filter f[16 + 128];
    int n = 0, salto_arch, salto_x32 = -1, per_clone, per_clone3;
    int salti_eperm[128];
    if (N_VIETATE > 128) { errno = E2BIG; return -1; }

    f[n++] = LEGGI(offsetof(struct seccomp_data, arch));
    salto_arch = n;
    f[n++] = SALTO(BPF_JMP | BPF_JEQ | BPF_K, ARCH_SECCOMP);     /* se diversa: uccidi */
    f[n++] = LEGGI(offsetof(struct seccomp_data, nr));
#ifdef __x86_64__
    salto_x32 = n;
    f[n++] = SALTO(BPF_JMP | BPF_JSET | BPF_K, __X32_SYSCALL_BIT); /* bit x32: uccidi */
#endif
    per_clone = n;
    f[n++] = SALTO(BPF_JMP | BPF_JEQ | BPF_K, SYS_clone);          /* -> controllo dei flag */
    per_clone3 = n;
    f[n++] = SALTO(BPF_JMP | BPF_JEQ | BPF_K, SYS_clone3);         /* -> ENOSYS */
    for (int i = 0; i < N_VIETATE; i++) {
        salti_eperm[i] = n;
        f[n++] = SALTO(BPF_JMP | BPF_JEQ | BPF_K, vietate[i]);     /* -> EPERM */
    }
    /* il kernel vuole che l'ultima istruzione sia una risposta: il controllo di clone sta prima
     * delle risposte, e chi arriva in fondo all'elenco lo salta con un salto fisso */
    int salta_clone = n;
    f[n++] = (struct sock_filter)BPF_STMT(BPF_JMP | BPF_JA, 0);
    int controllo_clone = n;
    f[n++] = LEGGI(offsetof(struct seccomp_data, args[0]));        /* parte bassa (little endian) */
    int salto_flag = n;
    f[n++] = SALTO(BPF_JMP | BPF_JSET | BPF_K, FLAG_MONDI);        /* flag dei mondi: EPERM */
    int permetti = n;
    f[n++] = RISPOSTA(SECCOMP_RET_ALLOW);
    int eperm = n;
    f[n++] = RISPOSTA(SECCOMP_RET_ERRNO | EPERM);
    int enosys = n;
    f[n++] = RISPOSTA(SECCOMP_RET_ERRNO | ENOSYS);
    int uccidi = n;
    f[n++] = RISPOSTA(SECCOMP_RET_KILL_PROCESS);
    f[salta_clone].k = permetti - salta_clone - 1;
    f[salto_flag].jt = eperm - salto_flag - 1;
    f[salto_flag].jf = permetti - salto_flag - 1;

    /* arch: uguale -> prosegue (jt 0), diversa -> uccidi */
    f[salto_arch].jf = uccidi - salto_arch - 1;
    if (salto_x32 >= 0) f[salto_x32].jt = uccidi - salto_x32 - 1;
    f[per_clone].jt = controllo_clone - per_clone - 1;
    f[per_clone3].jt = enosys - per_clone3 - 1;
    for (int i = 0; i < N_VIETATE; i++) f[salti_eperm[i]].jt = eperm - salti_eperm[i] - 1;
    /* i salti BPF sono a 8 bit: il programma deve restare corto */
    if (n > 250) { errno = E2BIG; return -1; }

    struct sock_fprog programma = {.len = n, .filter = f};
    return prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &programma);
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
    CONTROLLA(metti_seccomp());      /* dopo NO_NEW_PRIVS, prima di execve */
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
    /* la rete del sistema ci mette qualche secondo ad accendersi: fino a un minuto */
    for (int volta = 0; volta < 60; volta++) {
        if (internet() == 0 || errno == ECONNREFUSED) break;
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
