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
 *   1. monta /dev (devtmpfs: i dispositivi li crea il kernel), /dev/pts (i terminali
 *      finti), /proc e /sys;
 *   2. carica i moduli (finit_module): da qui il kernel vede il disco, lo schermo, l'USB;
 *   3. cerca le PARTIZIONI del disco per NOME. Il disco ha una tabella delle partizioni
 *      GPT (build.sh), e ogni partizione ha un nome scritto nella tabella (il
 *      "PARTLABEL"): "avvio", "system" e "data". Il kernel legge la tabella da solo e
 *      dice il nome di ogni partizione in /sys/class/block/<partizione>/uevent
 *      (PARTNAME=...): cosi' non conta come si chiama il disco (vda, sda, nvme0n1...);
 *   4. monta system come "/", in SOLA LETTURA: mentre il sistema gira nessun programma,
 *      nemmeno del sistema, lo puo' cambiare. Monta data in /data, in lettura e
 *      scrittura: li' vanno i file della persona e lo stato dei servizi. Ci sposta /dev,
 *      /proc e /sys; monta /run e /tmp (in memoria: tmpfs).
 *      /data lo monta avvio, non init, perche' e' avvio che cerca le partizioni: init
 *      trova tutto gia' al suo posto, come prima;
 *   5. fa diventare system "/" al posto dell'archivio (come switch_root), e
 *   6. diventa /system/init con execve: stesso processo, stesso numero 1, programma
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

#define PARTIZIONE_SISTEMA "system"   /* i nomi delle partizioni nella tabella GPT */
#define PARTIZIONE_DATI "data"
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

/* --- 3. le partizioni --- */

/* Vero se la partizione /sys/class/block/<voce> si chiama 'nome' nella tabella GPT: nel
 * suo file uevent c'e' la riga "PARTNAME=<nome>". Allora in dev mette /dev/<voce>. */
static int si_chiama(const char *voce, const char *nome, char *dev) {
    char percorso[128] = "/sys/class/block/";
    strncat(percorso, voce, 64);
    strcat(percorso, "/uevent");
    char testo[512];
    int f = open(percorso, O_RDONLY | O_CLOEXEC);
    if (f < 0) return 0;
    ssize_t n = read(f, testo, sizeof testo - 1);
    close(f);
    if (n <= 0) return 0;
    testo[n] = 0;
    for (char *riga = strtok(testo, "\n"); riga; riga = strtok(0, "\n"))
        if (!strncmp(riga, "PARTNAME=", 9) && !strcmp(riga + 9, nome)) {
            strcpy(dev, "/dev/");
            strncat(dev, voce, 48);
            return 1;
        }
    return 0;
}

/* Cerca la partizione 'nome' fra quelle di tutti i dischi che il kernel vede: 1 se c'e'
 * (e in dev il suo dispositivo), 0 se dopo ATTESA_DISCO volte non e' ancora arrivata. */
static int trova_partizione(const char *nome, char *dev) {
    for (int volta = 0; volta < ATTESA_DISCO; volta++) {
        DIR *c = opendir("/sys/class/block");
        for (struct dirent *v; c && (v = readdir(c));)
            if (v->d_name[0] != '.' && si_chiama(v->d_name, nome, dev)) { closedir(c); return 1; }
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
    /* i terminali finti (pty: /dev/ptmx e /dev/pts/N), per il terminale della scrivania. Il
     * terminale di una persona e' suo (620: lui legge e scrive) */
    mkdir("/dev/pts", 0755);
    mount("devpts", "/dev/pts", "devpts", MS_NOSUID | MS_NOEXEC, "mode=620,ptmxmode=666");
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
    char sistema[64], dati[64];
    if (!trova_partizione(PARTIZIONE_SISTEMA, sistema)) {
        di("avvio: non riesco a trovare la partizione \"" PARTIZIONE_SISTEMA "\"\n");
        ferma();
    }
    if (!trova_partizione(PARTIZIONE_DATI, dati)) {
        di("avvio: non riesco a trovare la partizione \"" PARTIZIONE_DATI "\"\n");
        ferma();
    }

    /* 4. "/" in sola lettura (MS_RDONLY): il kernel rifiuta ogni scrittura (EROFS) */
    mkdir("/nuova", 0755);
    if (mount(sistema, "/nuova", "ext4", MS_RDONLY, 0) < 0) { errore("montare", sistema); ferma(); }
    di("avvio: / montato (ext4 su ");
    di(sistema);
    di(", sola lettura)\n");
    if (mount(dati, "/nuova/data", "ext4", MS_NOSUID | MS_NODEV, 0) < 0) { errore("montare", dati); ferma(); }
    di("avvio: /data montato (ext4 su ");
    di(dati);
    di(")\n");
    sposta("/dev", "/nuova/dev");
    sposta("/proc", "/nuova/proc");
    sposta("/sys", "/nuova/sys");
    if (mount("run", "/nuova/run", "tmpfs", MS_NOSUID | MS_NODEV, "mode=0755") < 0)
        errore("montare", "/run");
    /* /tmp: tutti ci possono scrivere, ma ognuno cancella solo le sue cose (1777) */
    if (mount("tmp", "/nuova/tmp", "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777") < 0)
        errore("montare", "/tmp");

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
    char *argomenti[] = { "/system/init", 0 };
    char *ambiente[] = { 0 };
    execve(argomenti[0], argomenti, ambiente);
    errore("far partire", argomenti[0]);
    ferma();
}
