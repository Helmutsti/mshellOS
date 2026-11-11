/* avvio: il primissimo programma, quello dell'ARCHIVIO DI AVVIO (l'"initramfs").
 *
 * Il kernel Linux di Alpine sa fare poche cose da solo: i driver del disco (virtio_blk),
 * del formato ext4, della scheda video, dell'USB... sono MODULI, pezzi di kernel in file
 * separati (.ko) da caricare quando servono. Ma per leggere i moduli dal disco servirebbe
 * gia' saper leggere il disco. Per questo chi avvia il kernel (il firmware UEFI, o QEMU)
 * gli consegna anche un piccolo archivio (avvio.img, fatto da build.sh), che il kernel
 * apre in memoria come primo file system. Dentro ci sono solo:
 *
 *   /init            questo programma (statico: non ha bisogno di niente)
 *   /moduli/NOME.ko  i moduli che servono, e /moduli/elenco: in che ordine caricarli
 *                    (prima quelli da cui dipendono gli altri)
 *   /dev/console     dove il kernel apre le prime uscite (0, 1, 2) di /init
 *
 * Il kernel fa partire /init come processo numero 1. Lui:
 *   1. monta /dev (devtmpfs: i dispositivi li crea il kernel), /proc e /sys;
 *   2. carica i moduli (finit_module): da qui il kernel vede il disco, lo schermo, l'USB;
 *   3. cerca il DISCO DEL SISTEMA: il primo con un ext4 che si chiama "mshell";
 *   4. lo monta, e ci sposta /dev, /proc e /sys; monta /run (in memoria: tmpfs);
 *   5. lo fa diventare "/" al posto dell'archivio (come switch_root), e
 *   6. diventa /Sistema/init con execve: stesso processo, stesso numero 1, programma
 *      nuovo. Init non sa niente di tutto questo: si trova gia' nel sistema vero.
 *
 * Il processo 1 non deve mai finire (Linux si fermerebbe con un "Kernel panic"): se
 * qualcosa va storto lo si scrive sulla seriale e si aspetta per sempre. */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#define NOME_DISCO "mshell"    /* l'etichetta dell'ext4 del sistema (mkfs.ext4 -L) */
#define REGISTRO 8             /* dove i programmi trovano la console (libreria/interno.h) */
#define ATTESA_DISCO 100       /* volte da 100 ms: 10 secondi al massimo */

static void di(const char *s) {
    ssize_t n = write(1, s, strlen(s));
    (void)n;
}

static void numero(unsigned int n) {
    char c[12];
    int k = 11;
    c[k] = 0;
    do { c[--k] = (char)('0' + n % 10); n /= 10; } while (n);
    di(&c[k]);
}

static void errore(const char *cosa, const char *chi) {
    di("avvio: non riesco a ");
    di(cosa);
    di(" ");
    di(chi);
    di(": ");
    di(strerror(errno));
    di("\n");
}

static void ferma(void) {
    for (;;) pause();
}

/* --- 2. i moduli --- */

static int carica_moduli(void) {
    char elenco[4096];
    int f = open("/moduli/elenco", O_RDONLY | O_CLOEXEC);
    if (f < 0) { errore("aprire", "/moduli/elenco"); return 0; }
    ssize_t n = read(f, elenco, sizeof elenco - 1);
    close(f);
    if (n <= 0) return 0;
    elenco[n] = 0;
    int caricati = 0;
    for (char *nome = strtok(elenco, "\n"); nome; nome = strtok(0, "\n")) {
        char percorso[128] = "/moduli/";
        strncat(percorso, nome, sizeof percorso - 16);
        strcat(percorso, ".ko");
        int m = open(percorso, O_RDONLY | O_CLOEXEC);
        if (m < 0) { errore("aprire", percorso); continue; }
        /* il kernel legge il modulo dal file, controlla e lo aggiunge a se' stesso */
        if (syscall(SYS_finit_module, m, "", 0) == 0 || errno == EEXIST) caricati++;
        else errore("caricare il modulo", nome);
        close(m);
        unlink(percorso);                /* in memoria non serve piu': spazio liberato */
    }
    return caricati;
}

/* --- 3. il disco del sistema --- */

/* Vero se /dev/<nome> contiene un ext4 con l'etichetta NOME_DISCO. Il "superblocco" di
 * ext4 sta a 1024 byte dall'inizio: a +0x38 il numero magico 0xEF53, a +0x78 il nome. */
static int e_il_disco(const char *nome, char *dev) {
    strcpy(dev, "/dev/");
    strncat(dev, nome, 48);
    unsigned char sb[256];
    int f = open(dev, O_RDONLY | O_CLOEXEC);
    if (f < 0) return 0;
    ssize_t n = pread(f, sb, sizeof sb, 1024);
    close(f);
    return n == sizeof sb && sb[0x38] == 0x53 && sb[0x39] == 0xEF &&
           !strncmp((const char *)sb + 0x78, NOME_DISCO, 16);
}

static int trova_disco(char *dev) {
    for (int volta = 0; volta < ATTESA_DISCO; volta++) {
        DIR *c = opendir("/sys/class/block");
        for (struct dirent *v; c && (v = readdir(c));)
            if (v->d_name[0] != '.' && e_il_disco(v->d_name, dev)) { closedir(c); return 1; }
        if (c) closedir(c);
        usleep(100 * 1000);             /* un disco USB puo' arrivare dopo: si riprova */
    }
    return 0;
}

/* --- 4, 5, 6: il sistema vero --- */

static void sposta(const char *da, const char *a) {
    if (mount(da, a, 0, MS_MOVE, 0) < 0) errore("spostare", da);
}

int main(void) {
    /* 1. */
    mount("devtmpfs", "/dev", "devtmpfs", MS_NOSUID, "mode=0755");
    mount("proc", "/proc", "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC, 0);
    mount("sysfs", "/sys", "sysfs", MS_NOSUID | MS_NODEV | MS_NOEXEC, 0);
    if (fcntl(1, F_GETFD) < 0) {        /* senza /dev/console nell'archivio */
        int c = open("/dev/console", O_RDWR);
        if (c >= 0) { dup2(c, 0); dup2(c, 1); dup2(c, 2); if (c > 2) close(c); }
    }
    di("avvio: ciao, sono il primo programma\n");

    /* 2. */
    int caricati = carica_moduli();
    di("avvio: ");
    numero((unsigned int)caricati);
    di(" moduli del kernel caricati\n");

    /* 3. */
    char dev[64];
    if (!trova_disco(dev)) {
        di("avvio: non riesco a trovare il disco del sistema (ext4 \"" NOME_DISCO "\")\n");
        ferma();
    }

    /* 4. */
    mkdir("/nuova", 0755);
    if (mount(dev, "/nuova", "ext4", 0, 0) < 0) { errore("montare", dev); ferma(); }
    sposta("/dev", "/nuova/dev");
    sposta("/proc", "/nuova/proc");
    sposta("/sys", "/nuova/sys");
    if (mount("run", "/nuova/run", "tmpfs", MS_NOSUID | MS_NODEV, "mode=0755") < 0)
        errore("montare", "/run");
    di("avvio: / montato (ext4 su ");
    di(dev);
    di(")\n");

    /* 5. "/" diventa il disco: ci si sposta dentro, lo si mette al posto della radice e
     * da qui in poi "/" e' lui. L'archivio resta in memoria, ma vuoto: /init lo si
     * cancella (il programma che gira resta, finche' non diventa init). */
    unlink("/init");
    if (chdir("/nuova") < 0 || mount(".", "/", 0, MS_MOVE, 0) < 0 || chroot(".") < 0 || chdir("/") < 0) {
        errore("cambiare", "radice");
        ferma();
    }

    /* 6. il registro: la console, per tutti i programmi (libreria/interno.h) */
    dup2(1, REGISTRO);
    char *argomenti[] = { "/Sistema/init", 0 };
    char *ambiente[] = { 0 };
    execve(argomenti[0], argomenti, ambiente);
    errore("far partire", argomenti[0]);
    ferma();
}
