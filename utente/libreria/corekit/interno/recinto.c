/* CoreKit, sopra Linux: IL RECINTO DI UN'APP (sistema/recinto.h). Lo usa lancia (corekit.c)
 * per avvia_app_nel_recinto, cioe' il launcher; il filtro delle chiamate lo usa anche la
 * prova `recinto` (utente/prove/recinto.c), che ha dimostrato per prima tutto questo.
 *
 * L'ordine, nel figlio che lancia fa nascere gia' nei mondi nuovi (clone con i flag di
 * recinto_mondi: il figlio e' il numero 1 del suo mondo dei numeri, e il launcher riceve il
 * suo numero vero, quello che vedono tutti gli altri):
 *   1. recinto_costruisci, ancora con i poteri del sistema:
 *      a. i montaggi diventano privati: quello che si monta da qui in poi lo vede solo l'app
 *         (e i suoi figli), e niente di fuori arriva piu' dentro;
 *      b. una radice nuova vuota, in memoria (tmpfs, piccola), preparata in RADICE_NUOVA:
 *         la cartella sta nel /run di tutti, ma il tmpfs montato sopra e' solo di questo
 *         mondo, quindi due app che partono non si toccano;
 *      c. ogni cartella dell'elenco agganciata (bind mount) allo stesso percorso di fuori,
 *         creando le cartelle che servono per arrivarci, con il suo modo (sola lettura, lettura
 *         e scrittura, programma);
 *      d. pivot_root: la radice nuova diventa "/", la vecchia si stacca e sparisce; la radice
 *         si rimonta in sola lettura;
 *   2. chi chiama diventa la persona (uid e gid, nessun gruppo);
 *   3. recinto_chiudi: NO_NEW_PRIVS (mai piu' poteri, neanche con un programma setuid) e il
 *      filtro delle chiamate;
 *   4. execve del programma dell'app, che nel recinto sta allo stesso percorso di fuori.
 *
 * I SEGNI DEGLI AGGANCI. Ogni aggancio e' nosuid (un programma setuid non da' poteri) e nodev
 * (un file di dispositivo non apre niente). Tutti tranne il pacchetto sono anche noexec: un'app
 * esegue solo codice che stava nel suo pacchetto (APPLICAZIONI.md, "La catena della
 * sicurezza"); anche la radice in memoria e' noexec. Gli agganci non sono "ricorsivi": se sotto
 * una cartella concessa ci fosse montato un altro disco, nel recinto non comparirebbe, cosi'
 * dentro arriva solo quello che e' stato chiesto, con i segni chiesti. */
#define _GNU_SOURCE
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#ifdef __x86_64__
#include <asm/unistd.h>   /* __X32_SYSCALL_BIT: sta solo nelle intestazioni del kernel */
#endif
#include <sched.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "../sistema/recinto.h"
#include "interno.h"

/* Dove si prepara la radice dell'app, prima di pivot_root. */
#define RADICE_NUOVA "/run/sandbox"
#define VECCHIA      "old"           /* dove pivot_root mette la radice vera, per staccarla */

/* --- i mondi --- */

int recinto_mondi(const struct recinto *r) {
    /* senza il permesso della rete: una rete vuota (CLONE_NEWNET); con: quella del computer
     * (decisione della persona, 16 novembre 2026, come Android e Flatpak) */
    return CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWUTS | (r->rete ? 0 : CLONE_NEWNET);
}

/* --- la radice nuova --- */

/* Crea 'percorso' e le cartelle sopra, come "mkdir -p". 'percorso' si cambia durante il
 * lavoro e torna com'era. 0, o -1 con errno. */
static int crea_cartelle(char *percorso) {
    for (char *c = percorso + 1;; c++) {
        if (*c != '/' && *c) continue;
        char tenuto = *c;
        *c = 0;
        int r = mkdir(percorso, 0755);
        *c = tenuto;
        if (r < 0 && errno != EEXIST) return -1;
        if (!tenuto) return 0;
    }
}

/* Aggancia la cartella vera 'a->percorso' allo stesso percorso nella radice nuova, con il suo
 * modo. 0, o -1 con errno. */
static int aggancia(const struct aggancio *a) {
    char dentro[sizeof RADICE_NUOVA + 512];
    size_t l = strlen(a->percorso);
    if (a->percorso[0] != '/' || l + sizeof RADICE_NUOVA > sizeof dentro) {
        errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(dentro, RADICE_NUOVA, sizeof RADICE_NUOVA - 1);
    memcpy(dentro + sizeof RADICE_NUOVA - 1, a->percorso, l + 1);
    if (crea_cartelle(dentro) < 0) return -1;
    if (mount(a->percorso, dentro, NULL, MS_BIND, NULL) < 0) return -1;
    /* i segni si mettono rimontando l'aggancio (il kernel non li prende al primo bind) */
    unsigned long segni = MS_BIND | MS_REMOUNT | MS_NOSUID | MS_NODEV;
    if (a->modo == AGGANCIO_LETTURA) segni |= MS_RDONLY | MS_NOEXEC;
    else if (a->modo == AGGANCIO_SCRITTURA) segni |= MS_NOEXEC;
    else segni |= MS_RDONLY;                                /* AGGANCIO_PROGRAMMA */
    return mount(NULL, dentro, NULL, segni, NULL);
}

#define PROVA(x) do { if ((x) < 0) return errno ? errno : EINVAL; } while (0)

int recinto_costruisci(const struct recinto *r) {
    if (r->quanti < 0 || r->quanti > MAX_AGGANCI) return EINVAL;
    PROVA(mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL));
    if (mkdir(RADICE_NUOVA, 0755) < 0 && errno != EEXIST) return errno;
    PROVA(mount("tmpfs", RADICE_NUOVA, "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC, "size=64k,mode=755"));
    for (int i = 0; i < r->quanti; i++) PROVA(aggancia(&r->agganci[i]));
    PROVA(mkdir(RADICE_NUOVA "/" VECCHIA, 0755));
    PROVA(chdir(RADICE_NUOVA));
    PROVA(syscall(SYS_pivot_root, ".", VECCHIA));
    PROVA(chdir("/"));
    PROVA(umount2("/" VECCHIA, MNT_DETACH));
    PROVA(rmdir("/" VECCHIA));
    PROVA(mount(NULL, "/", NULL, MS_BIND | MS_REMOUNT | MS_RDONLY | MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL));
    return 0;
}

int recinto_chiudi(void) {
    PROVA(prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0));
    PROVA(recinto_seccomp());          /* dopo NO_NEW_PRIVS, prima di execve */
    return 0;
}

#undef PROVA

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
int recinto_seccomp(void) {
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
