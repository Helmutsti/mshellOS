// Primo programma dell'area utente: il kernel lo esegue come PID 1.
// Il PID 1 non deve mai terminare: se esce, il kernel va in panic.
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/klog.h>

// Azione 8 della chiamata syslog del kernel: "imposta il livello dei messaggi
// mostrati in console" (vedi man 2 syslog). musl non le da' un nome.
#define LOG_CONSOLE_IMPOSTA_LIVELLO 8
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include "file_sicuro.h"

// Moduli del kernel da caricare, nell'ordine giusto: prima le dipendenze.
// I file li mette run.ps1 nell'archivio di avvio, in /lib/modules.
static const char *moduli[] = { "crc16", "mbcache", "jbd2", "ext4", "virtio_blk" };

// Carica un modulo nel kernel: apre il file .ko e lo consegna al kernel
// con la chiamata di sistema finit_module. Da quel momento e' codice kernel.
static int carica_modulo(const char *nome) {
    char percorso[128];
    snprintf(percorso, sizeof percorso, "/lib/modules/%s.ko", nome);
    int fd = open(percorso, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        printf("mshell: modulo %s non trovato: %s\n", nome, strerror(errno));
        return -1;
    }
    int r = (int)syscall(SYS_finit_module, fd, "", 0);
    int errore = errno;
    close(fd);
    if (r != 0 && errore != EEXIST) {
        printf("mshell: modulo %s rifiutato: %s\n", nome, strerror(errore));
        return -1;
    }
    return 0;
}

// Monta il disco su /system e lo rende accessibile solo a root (permessi 0700).
// I permessi vengono scritti sul disco, quindi restano anche ai prossimi avvii.
static int monta_system(void) {
    if (mkdir("/system", 0700) != 0 && errno != EEXIST) {
        printf("mshell: mkdir /system fallito: %s\n", strerror(errno));
        return -1;
    }
    if (mount("/dev/vda", "/system", "ext4", 0, NULL) != 0) {
        printf("mshell: mount del disco su /system fallito: %s\n", strerror(errno));
        return -1;
    }
    if (chown("/system", 0, 0) != 0 || chmod("/system", 0700) != 0) {
        printf("mshell: impossibile proteggere /system: %s\n", strerror(errno));
        return -1;
    }
    struct stat s;
    stat("/system", &s);
    printf("mshell: disco montato su /system (proprietario %u, permessi %o)\n",
           s.st_uid, s.st_mode & 0777);
    return 0;
}

// File degli utenti, in stile /etc/passwd di Linux. Una riga per utente:
//   nome:password:uid:gid:descrizione:cartella_personale:programma_da_avviare
// La password "x" vuol dire "non impostata".
#define FILE_UTENTI "/system/users"
static const char *utenti_iniziali =
    "admin:x:0:0:Amministratore:/system/admin:/system/bin/mshell\n";

// Un utente letto dal file: solo i campi che servono per avviare la sua shell.
struct utente {
    char nome[32];
    unsigned uid, gid;
    char casa[128];     // cartella personale
    char programma[128];
};

// Cerca un utente per nome nel file degli utenti. Restituisce 0 se lo trova.
static int leggi_utente(const char *nome, struct utente *u) {
    FILE *f = fopen(FILE_UTENTI, "r");
    if (!f)
        return -1;
    char riga[512];
    int trovato = -1;
    while (trovato != 0 && fgets(riga, sizeof riga, f)) {
        riga[strcspn(riga, "\n")] = '\0';
        char *campi[7];
        int n = 0;
        char *p = riga;
        while (n < 7) {                        // 7 campi separati da ':' (anche vuoti)
            campi[n++] = p;
            p = strchr(p, ':');
            if (!p)
                break;
            *p++ = '\0';
        }
        if (n != 7 || strcmp(campi[0], nome) != 0)
            continue;
        snprintf(u->nome, sizeof u->nome, "%s", campi[0]);
        u->uid = (unsigned)strtoul(campi[2], NULL, 10);
        u->gid = (unsigned)strtoul(campi[3], NULL, 10);
        snprintf(u->casa, sizeof u->casa, "%s", campi[5]);
        snprintf(u->programma, sizeof u->programma, "%s", campi[6]);
        trovato = 0;
    }
    fclose(f);
    return trovato;
}

// Avvia il programma dell'utente (la sua shell) sulla prima console dello schermo.
// Restituisce il numero del processo figlio, o -1.
static pid_t avvia_shell(const char *nome) {
    struct utente u;
    if (leggi_utente(nome, &u) != 0) {
        printf("mshell: utente %s non trovato in %s\n", nome, FILE_UTENTI);
        return -1;
    }
    // cartella personale: se manca la crea, di proprieta' dell'utente e solo sua
    if (mkdir(u.casa, 0700) == 0 && chown(u.casa, u.uid, u.gid) != 0)
        printf("mshell: chown %s fallito: %s\n", u.casa, strerror(errno));

    pid_t figlio = fork();
    if (figlio != 0)
        return figlio;                         // init (o errore: -1)

    // --- da qui siamo nel processo figlio ---
    // nuova sessione con tty1 come terminale: Ctrl+C arriva qui, non a init
    setsid();
    int tty = open("/dev/tty1", O_RDWR);
    if (tty < 0)
        _exit(1);
    ioctl(tty, TIOCSCTTY, 0);
    dup2(tty, 0);
    dup2(tty, 1);
    dup2(tty, 2);
    if (tty > 2)
        close(tty);

    signal(SIGINT, SIG_DFL);
    // diventa l'utente: prima il gruppo, poi l'utente (dopo non si torna indietro)
    if (setgid(u.gid) != 0 || setuid(u.uid) != 0) {
        printf("mshell: impossibile diventare %s: %s\n", u.nome, strerror(errno));
        _exit(1);
    }
    if (chdir(u.casa) != 0)
        chdir("/");

    char e_home[160], e_user[48];
    snprintf(e_home, sizeof e_home, "HOME=%s", u.casa);
    snprintf(e_user, sizeof e_user, "USER=%s", u.nome);
    char *ambiente[] = { e_home, e_user, "PATH=/system/bin", "TERM=linux", NULL };
    char *argomenti[] = { u.programma, NULL };
    execve(u.programma, argomenti, ambiente);
    printf("mshell: impossibile avviare %s: %s\n", u.programma, strerror(errno));
    _exit(127);
}

// Al primo avvio crea il file degli utenti, leggibile e scrivibile solo da root (0600),
// con la scrittura sicura: dopo un'interruzione il file c'e' intero oppure non c'e'.
// Se esiste gia' non lo tocca. In entrambi i casi ne stampa il contenuto.
static void prepara_utenti(void) {
    if (access(FILE_UTENTI, F_OK) != 0) {
        if (scrivi_file_sicuro(FILE_UTENTI, utenti_iniziali, strlen(utenti_iniziali), 0600) != 0) {
            printf("mshell: impossibile creare %s: %s\n", FILE_UTENTI, strerror(errno));
            return;
        }
        printf("mshell: primo avvio, creato %s\n", FILE_UTENTI);
    }

    FILE *f = fopen(FILE_UTENTI, "r");
    if (!f) {
        printf("mshell: impossibile leggere %s: %s\n", FILE_UTENTI, strerror(errno));
        return;
    }
    struct stat s;
    fstat(fileno(f), &s);
    printf("mshell: %s (proprietario %u, permessi %o):\n", FILE_UTENTI, s.st_uid, s.st_mode & 0777);
    char riga[256];
    while (fgets(riga, sizeof riga, f))
        printf("  %s", riga);
    fclose(f);
}

// Crea la cartella (punto di aggancio) e ci monta un file system del kernel.
static void monta(const char *tipo, const char *cartella) {
    if (mkdir(cartella, 0755) != 0 && errno != EEXIST) {
        printf("mshell: mkdir %s fallito: %s\n", cartella, strerror(errno));
        return;
    }
    if (mount(tipo, cartella, tipo, 0, NULL) != 0)
        printf("mshell: mount %s su %s fallito: %s\n", tipo, cartella, strerror(errno));
    else
        printf("mshell: montato %-8s su %s\n", tipo, cartella);
}

// Stampa i nomi contenuti in una cartella, uno dopo l'altro sulla stessa riga.
static void elenca(const char *cartella) {
    DIR *d = opendir(cartella);
    if (!d) {
        printf("mshell: impossibile aprire %s: %s\n", cartella, strerror(errno));
        return;
    }
    printf("mshell: contenuto di %s\n", cartella);
    struct dirent *e;
    int n = 0, colonna = 0;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.')
            continue;
        int lung = (int)strlen(e->d_name) + 1;
        if (colonna + lung > 79) {
            printf("\n");
            colonna = 0;
        }
        colonna += printf("%s ", e->d_name);
        n++;
    }
    closedir(d);
    printf("\nmshell: %d voci\n", n);
}

// --- Spegnimento pulito ---------------------------------------------------
// Con reboot(RB_DISABLE_CAD) il PID 1 dice al kernel: "a Ctrl+Alt+Canc non riavviare
// di colpo, mandami il segnale SIGINT". Il segnale imposta solo questo flag;
// lo spegnimento vero lo fa il ciclo principale.
static volatile sig_atomic_t richiesta_spegnimento = 0;

static void al_segnale_spegnimento(int segnale) {
    (void)segnale;
    richiesta_spegnimento = 1;
}

// Chiude tutti i processi tranne init: prima con gentilezza (SIGTERM), poi d'autorita' (SIGKILL).
// Serve perche' un disco non si puo' smontare mentre qualcuno lo sta usando
// (per esempio la shell, che ha come cartella corrente /system/admin).
static void chiudi_processi(void) {
    kill(-1, SIGTERM);                         // -1 = tutti i processi tranne il PID 1
    for (int i = 0; i < 20 && waitpid(-1, NULL, WNOHANG) >= 0; i++)
        usleep(100000);                        // fino a 2 secondi per chiudersi da soli
    kill(-1, SIGKILL);
    while (waitpid(-1, NULL, 0) > 0)           // raccoglie tutti i figli terminati
        ;
}

// Salva tutto sul disco, stacca /system e spegne la macchina.
static void spegni(void) {
    printf("mshell: spegnimento in corso\n");
    chiudi_processi();
    sync();                                    // tutto quello che e' in memoria va sul disco
    if (umount("/system") == 0) {
        printf("mshell: /system smontato\n");
    } else {
        // se qualcosa lo sta usando, almeno lo blocca in sola lettura
        printf("mshell: umount /system fallito (%s), passo a sola lettura\n", strerror(errno));
        mount(NULL, "/system", NULL, MS_REMOUNT | MS_RDONLY, NULL);
    }
    sync();
    printf("mshell: arrivederci\n");
    reboot(RB_POWER_OFF);                      // il kernel spegne la macchina
}

int main(void) {
    printf("mshell: ciao dal PID %d\n", getpid());

    struct sigaction sa = { .sa_handler = al_segnale_spegnimento };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    reboot(RB_DISABLE_CAD);

    monta("proc", "/proc");
    monta("sysfs", "/sys");
    monta("devtmpfs", "/dev");

    elenca("/dev");

    int caricati = 0;
    for (size_t i = 0; i < sizeof moduli / sizeof moduli[0]; i++)
        if (carica_modulo(moduli[i]) == 0)
            caricati++;
    printf("mshell: moduli caricati %d su %d\n", caricati,
           (int)(sizeof moduli / sizeof moduli[0]));

    printf("mshell: pronto. Per spegnere: shutdown, oppure Ctrl+Alt+Canc\n");

    pid_t shell = -1;
    if (monta_system() == 0) {
        prepara_utenti();
        // Zittisce il kernel sulla console: da qui in poi stampa solo i messaggi
        // critici (livello < 3), cosi' non si mescolano con la shell. Gli altri
        // non si perdono: restano nel registro del kernel, leggibile da /dev/kmsg.
        klogctl(LOG_CONSOLE_IMPOSTA_LIVELLO, NULL, 3);
        shell = avvia_shell("admin");
    }
    // Ciclo del PID 1: aspetta che un figlio termini (wait). Raccoglie cosi' anche
    // i processi orfani che il kernel gli affida. Se termina la shell, la riavvia.
    // Un segnale (spegnimento) interrompe l'attesa.
    while (!richiesta_spegnimento) {
        pid_t finito = wait(NULL);
        if (finito < 0) {
            if (errno == ECHILD)
                pause();                       // nessun figlio: dorme fino a un segnale
            continue;
        }
        if (finito == shell && !richiesta_spegnimento) {
            printf("mshell: la shell e' terminata, la riavvio\n");
            sleep(1);
            shell = avvia_shell("admin");
        }
    }
    spegni();

    for (;;)                                   // non si arriva qui: il PID 1 non deve uscire
        pause();
}
