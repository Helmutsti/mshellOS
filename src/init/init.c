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
#include <grp.h>
#include <unistd.h>

#include "mshelllib.h"

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

    // I programmi di sistema compaiono anche in /bin, in sola lettura: e' la stessa
    // cartella del disco (aggancio), non una copia. Cosi' tutti li trovano in /bin.
    if (mkdir("/bin", 0755) != 0 && errno != EEXIST)
        return -1;
    if (mount("/system/bin", "/bin", NULL, MS_BIND, NULL) != 0
        || mount(NULL, "/bin", NULL, MS_REMOUNT | MS_BIND | MS_RDONLY | MS_NOSUID | MS_NODEV, NULL) != 0) {
        printf("mshell: aggancio di /system/bin su /bin fallito: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

// File degli utenti, in stile /etc/passwd di Linux. Una riga per utente:
//   nome:password:uid:gid:descrizione:cartella:programma_da_avviare
// La password "x" vuol dire "non impostata".
// La cartella e' /system/users/<uid>: per l'utente diventa la radice "/" del suo mondo, con dentro
//   /home  i suoi file (e i suoi programmi in /home/bin)
//   /bin   i programmi di sistema, agganciati in sola lettura
// L'amministratore non ha cartella: vede tutto il sistema e parte da /.
// Il programma e' il punto d'ingresso dell'utente, col percorso che vede lui. Per una persona e' la shell.
static const char *utenti_iniziali =
    "admin:x:0:0:Amministratore::/bin/mshell\n"
    "utente:x:1000:1000:Utente di prova:/system/users/1000:/bin/mshell\n";

// Avvia il programma di accesso sulla console indicata (es. /dev/tty1), come root.
// Login chiede nome e password e poi diventa la shell della persona: quando la persona
// esce, il processo finisce e init rimette login su quella console.
#define LOGIN "/system/servizi/login"
static pid_t avvia_login(const char *console) {
    pid_t figlio = fork();
    if (figlio != 0)
        return figlio;                         // init (o errore: -1)

    // --- da qui siamo nel processo figlio ---
    // la console diventa il suo terminale: Ctrl+C arriva qui, non a init
    if (collega_console(console) != 0)
        _exit(1);
    signal(SIGINT, SIG_DFL);

    char *argomenti[] = { LOGIN, NULL };
    char *ambiente[] = { "TERM=linux", NULL };
    execve(LOGIN, argomenti, ambiente);
    printf("mshell: impossibile avviare %s: %s\n", LOGIN, strerror(errno));
    _exit(127);
}

// La persona e' uscita (exit): ferma anche i programmi che ha lasciato accesi.
// Il processo di login apre un gruppo di processi con il suo stesso numero, e tutto quello
// che la shell avvia ci resta dentro: kill(-numero) li raggiunge tutti, anche dopo che la
// shell e' finita. Limite: un programma che si crea un gruppo suo (setsid) sfugge.
static void chiudi_sessione(pid_t gruppo) {
    if (kill(-gruppo, SIGTERM) != 0)
        return;                                // nessuno rimasto
    usleep(300000);                            // un attimo per chiudersi da soli
    kill(-gruppo, SIGKILL);
}

// Avvia servicenter, che tiene accesi i servizi di sistema. Gira come root.
#define SERVICENTER "/system/servizi/servicenter"
static pid_t avvia_servicenter(void) {
    pid_t figlio = fork();
    if (figlio == 0) {
        char *argomenti[] = { SERVICENTER, NULL };
        char *ambiente[] = { NULL };
        execve(SERVICENTER, argomenti, ambiente);
        printf("mshell: impossibile avviare %s: %s\n", SERVICENTER, strerror(errno));
        _exit(127);
    }
    return figlio;
}

// Al primo avvio crea il file degli utenti, leggibile e scrivibile solo da root (0600),
// con la scrittura sicura: dopo un'interruzione il file c'e' intero oppure non c'e'.
// Se esiste gia' non lo tocca. In entrambi i casi ne stampa il contenuto.
static void prepara_utenti(void) {
    if (mkdir(CARTELLA_UTENTI, 0700) != 0 && errno != EEXIST)
        printf("mshell: mkdir %s fallito: %s\n", CARTELLA_UTENTI, strerror(errno));
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
// (per esempio la shell, se ha come cartella corrente una cartella dentro /system).
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
    umount("/bin");                            // l'aggancio dei programmi tiene occupato il disco
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

    // Il programma di accesso su ogni console: Alt+F1, Alt+F2, ...
    const char *console[] = { "/dev/tty1", "/dev/tty2" };
    enum { N_CONSOLE = sizeof console / sizeof console[0] };
    pid_t accesso[N_CONSOLE];
    pid_t servicenter = -1;
    for (int i = 0; i < N_CONSOLE; i++)
        accesso[i] = -1;
    if (monta_system() == 0) {
        prepara_utenti();
        servicenter = avvia_servicenter();
        usleep(200000);                        // lascia stampare a servicenter il suo messaggio
        // Zittisce il kernel sulla console: da qui in poi stampa solo i messaggi
        // critici (livello < 3), cosi' non si mescolano con la shell. Gli altri
        // non si perdono: restano nel registro del kernel, leggibile da /dev/kmsg.
        klogctl(LOG_CONSOLE_IMPOSTA_LIVELLO, NULL, 3);
        for (int i = 0; i < N_CONSOLE; i++)
            accesso[i] = avvia_login(console[i]);
    }
    // Ciclo del PID 1: aspetta che un figlio termini (wait). Raccoglie cosi' anche
    // i processi orfani che il kernel gli affida. Se una persona esce (o login termina),
    // rimette login su quella console.
    // Un segnale (spegnimento) interrompe l'attesa.
    while (!richiesta_spegnimento) {
        pid_t finito = wait(NULL);
        if (finito < 0) {
            if (errno == ECHILD)
                pause();                       // nessun figlio: dorme fino a un segnale
            continue;
        }
        for (int i = 0; i < N_CONSOLE; i++) {
            if (finito == accesso[i] && !richiesta_spegnimento) {
                chiudi_sessione(finito);
                sleep(1);
                accesso[i] = avvia_login(console[i]);
            }
        }
        if (finito == servicenter && !richiesta_spegnimento) {
            sleep(1);
            servicenter = avvia_servicenter();
        }
    }
    spegni();

    for (;;)                                   // non si arriva qui: il PID 1 non deve uscire
        pause();
}
