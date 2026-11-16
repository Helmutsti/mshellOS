/* CoreKit, sopra Linux: i programmi, il tempo, i processi.
 *
 * Ogni funzione di corekit.h fa le chiamate di sistema di Linux che servono, attraverso
 * la libreria C musl. Ecco dove sta ogni servizio che una volta dava mkernel:
 *
 *   scrivi                     write sul REGISTRO, la console del kernel (interno.h)
 *   dormi, tempo               nanosleep, clock_gettime(CLOCK_MONOTONIC)
 *   data_ora                   clock_gettime(CLOCK_REALTIME): l'orologio del PC, letto
 *                              da Linux all'avvio
 *   avvia, avvia_come,         fork, poi nel figlio dup2 (i descrittori prestati),
 *   avvia_con                  setuid/setgid (l'uid nuovo) ed execve
 *   processo                   pidfd_open: un descrittore che diventa pronto quando il
 *                              processo finisce
 *   elenco_processi,           una richiesta al servizio tasks (tasks.c, qui accanto): lui
 *   termina_processo,          legge /proc e ferma i processi, e decide chi puo'
 *   stato_memoria, stato_disco
 *   apri, leggi, info...       open, read, stat, mkdir, rename... (file.c)
 *   socket e fogli, aspetta    socket AF_UNIX, memfd, mmap, poll (comunicazione.c)
 *   lo schermo                 il DRM di Linux, /dev/dri/card0 (gkit/interno/schermo.c)
 *   gli eventi                 /dev/input, letti dal servizio eventi
 *                              (utente/programmi/eventi.c)
 *   malloc e free              quelle di musl (prima: memoria.c, sopra "rottura") */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include "../corekit.h"
#include "../sistema/privilegi.h"
#include "interno.h"

/* --- il registro --- */

int scrivi(const char *testo, int byte) {
    return byte < 0 ? -1 : (int)write(REGISTRO, testo, (size_t)byte);
}

void scrivi_riga(const char *chi, const char *a, const char *b) {
    char riga[512];
    size_t n = 0;
    const char *pezzi[] = { chi, ": ", a, b };
    for (int i = 0; i < 4; i++)
        for (const char *s = pezzi[i]; s && *s && n < sizeof riga - 1; s++) riga[n++] = *s;
    riga[n++] = '\n';
    scrivi(riga, (int)n);
}

/* --- il tempo --- */

void dormi(unsigned int millisecondi) {
    struct timespec t = { millisecondi / 1000, (long)(millisecondi % 1000) * 1000000 };
    while (nanosleep(&t, &t) < 0 && errno == EINTR) ;   /* interrotto: si dorme il resto */
}

unsigned int tempo(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (unsigned int)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}

/* L'OROLOGIO DEL PC (l'RTC) segna l'ora di casa, non quella di Greenwich (QEMU: -rtc
 * base=localtime, come fa Windows). Linux lo legge all'avvio come se fosse l'ora di
 * Greenwich: quindi qui la si legge "di Greenwich" (gmtime), senza fusi, ed esce l'ora
 * dell'orologio, quella di casa. */
int data_ora(struct data_ora *d) {
    struct timespec t;
    struct tm tm;
    if (clock_gettime(CLOCK_REALTIME, &t) < 0 || !gmtime_r(&t.tv_sec, &tm)) return -1;
    d->anno = (unsigned int)tm.tm_year + 1900;
    d->mese = (unsigned int)tm.tm_mon + 1;
    d->giorno = (unsigned int)tm.tm_mday;
    d->giorno_settimana = (unsigned int)tm.tm_wday;
    d->ore = (unsigned int)tm.tm_hour;
    d->minuti = (unsigned int)tm.tm_min;
    d->secondi = (unsigned int)tm.tm_sec;
    return 0;
}

/* --- i programmi --- */

void esci(void) { exit(0); }

/* I FIGLI FINITI. In Unix un processo finito resta nell'elenco ("zombie") finche' chi
 * l'ha fatto partire non chiede com'e' andata (wait). Noi non lo chiediamo mai: chi vuole
 * sapere quando un programma finisce usa processo(). Allora si dice al kernel di
 * ignorare il segnale SIGCHLD ("un figlio e' finito"): cosi' i figli finiti spariscono da
 * soli. Vale anche per init, che si ritrova "figli" tutti i programmi rimasti orfani. */
static void figli_spariscono(void) {
    static int fatto;
    if (fatto) return;
    struct sigaction sa = { .sa_handler = SIG_IGN };
    sigaction(SIGCHLD, &sa, 0);
    fatto = 1;
}

/* Nel figlio, prima di execve: che non si porti dietro niente di chi l'ha fatto partire,
 * tranne i descrittori ai posti 0..quanti-1 e il registro. Quelli da tenere sono gia'
 * stati messi al loro posto; gli altri si segnano "da chiudere all'execve" (close_range
 * con CLOSE_RANGE_CLOEXEC: il tubo che dice com'e' andata deve restare aperto fino ad
 * allora). */
#ifndef SYS_close_range
#define SYS_close_range 436
#endif
#ifndef CLOSE_RANGE_CLOEXEC
#define CLOSE_RANGE_CLOEXEC (1U << 2)
#endif
static void solo_quelli_passati(int quanti) {
    if (quanti < REGISTRO) syscall(SYS_close_range, quanti, REGISTRO - 1, CLOSE_RANGE_CLOEXEC);
    syscall(SYS_close_range, REGISTRO + 1, ~0U, CLOSE_RANGE_CLOEXEC);
}

/* FAR PARTIRE UN PROGRAMMA. fork fa una copia di questo processo; la copia (il figlio)
 * mette a posto i descrittori e l'uid, poi con execve diventa il programma nuovo. Se
 * execve non riesce (il file non c'e', non si puo' eseguire...), il figlio lo dice al
 * padre attraverso un tubo e finisce: cosi' avvia sa subito se e' andata, e restituisce
 * -1. Se execve riesce, il tubo si chiude da solo (O_CLOEXEC) e il padre legge 0 byte.
 * uid -1: lo stesso di chi lo fa partire. 'argomento' (0: nessuno) arriva al programma
 * nuovo come secondo argomento di main (argv[1]): per esempio la cartella da aprire.
 * 'variabile' (0: nessuna) e' l'unica variabile d'ambiente del programma nuovo, "NOME=valore":
 * il launcher ci scrive l'elenco delle porte dell'app (messaggi.h, VARIABILE_PORTE). */
static int lancia(const char *percorso, const int *passati, int quanti, int uid,
                  const char *argomento, const char *variabile) {
    char vero[MAX_VERO];
    if (percorso_vero(percorso, vero) < 0 || quanti < 0 || quanti > MAX_PASSATI) return -1;
    figli_spariscono();
    int tubo[2];
    if (pipe2(tubo, O_CLOEXEC) < 0) return -1;
    pid_t figlio = fork();
    if (figlio == 0) {
        /* --- il figlio --- */
        int errore = 0;
        struct sigaction sa = { .sa_handler = SIG_DFL };
        sigaction(SIGCHLD, &sa, 0);              /* i segnali come li trova un programma nuovo */
        int alti[MAX_PASSATI];
        /* prima tutti in alto, poi ognuno al suo posto: un descrittore da passare
         * potrebbe stare proprio al posto di un altro */
        for (int i = 0; i < quanti && !errore; i++)
            if ((alti[i] = fcntl(passati[i], F_DUPFD_CLOEXEC, 64)) < 0) errore = errno;
        for (int i = 0; i < quanti && !errore; i++)
            if (dup2(alti[i], i) < 0) errore = errno;   /* dup2 toglie O_CLOEXEC */
        solo_quelli_passati(quanti);
        if (!errore && uid >= 0 && (setgroups(0, 0) < 0 || setgid((gid_t)uid) < 0 || setuid((uid_t)uid) < 0))
            errore = errno;
        if (!errore) {
            /* il primo argomento e' sempre il programma; poi, se c'e', quello per lui */
            char *argomenti[] = { vero, (char *)argomento, 0 };
            char *ambiente[] = { (char *)variabile, 0 };
            execve(vero, argomenti, ambiente);
            errore = errno;
        }
        ssize_t scritti = write(tubo[1], &errore, sizeof errore);
        (void)scritti;
        _exit(127);
    }
    /* --- il padre --- */
    close(tubo[1]);
    int errore = 0;
    ssize_t letti = -1;
    if (figlio > 0)
        do letti = read(tubo[0], &errore, sizeof errore); while (letti < 0 && errno == EINTR);
    close(tubo[0]);
    return figlio > 0 && letti == 0 ? (int)figlio : -1;
}

int avvia(const char *percorso) { return lancia(percorso, 0, 0, -1, 0, 0); }

int avvia_come(const char *percorso, unsigned int uid) { return lancia(percorso, 0, 0, (int)uid, 0, 0); }

int avvia_come_con(const char *percorso, unsigned int uid, const char *argomento) {
    return lancia(percorso, 0, 0, (int)uid, argomento && argomento[0] ? argomento : 0, 0);
}

int avvia_con(const char *percorso, const int *descrittori, int quanti) {
    return lancia(percorso, descrittori, quanti, -1, 0, 0);
}

int avvia_come_con_descrittori(const char *percorso, unsigned int uid, const int *descrittori, int quanti) {
    return lancia(percorso, descrittori, quanti, (int)uid, 0, 0);
}

unsigned int uid_mio(void) { return (unsigned int)getuid(); }

int apri_nulla(void) { return open("/dev/null", O_RDWR | O_CLOEXEC); }

int avvia_app(const char *percorso, unsigned int uid, const char *argomento, const int *descrittori, int quanti,
              const char *variabile) {
    return lancia(percorso, descrittori, quanti, (int)uid, argomento && argomento[0] ? argomento : 0, variabile);
}

#ifndef SYS_pidfd_open
#define SYS_pidfd_open 434
#endif
int processo(int numero) {
    if (numero <= 0) return -1;
    return (int)syscall(SYS_pidfd_open, numero, 0);   /* gia' O_CLOEXEC */
}

/* Due aiuti per scrivere i percorsi senza printf: la printf di musl si porta
 * dietro i numeri con la virgola, che su ARM64 chiedono funzioni in piu' al compilatore. */
static char *metti_testo(char *d, const char *s) {
    while (*s) *d++ = *s++;
    *d = 0;
    return d;
}

static char *metti_numero(char *d, unsigned long n) {
    char cifre[24];
    int k = 0;
    do { cifre[k++] = (char)('0' + n % 10); n /= 10; } while (n);
    while (k) *d++ = cifre[--k];
    *d = 0;
    return d;
}


/* --- percorsi ed errori --- */

int percorso_vero(const char *percorso, char *vero) {
    int l = (int)strnlen(percorso, MAX_PERCORSO);
    if (l == 0 || l >= MAX_PERCORSO || percorso[0] != '/') return ERRORE_NOME;
    if (!strncmp(percorso, "/home", 5) && (percorso[5] == 0 || percorso[5] == '/')) {
        char *d = metti_numero(metti_testo(vero, "/data/users/"), getuid());
        metti_testo(metti_testo(d, "/home"), percorso + 5);
        return 0;
    }
    memcpy(vero, percorso, (size_t)l + 1);
    return 0;
}

int errore_file(int numero) {
    switch (numero) {
    case ENOENT:       return ERRORE_NON_TROVATO;
    case EEXIST:       return ERRORE_ESISTE;
    case ENOTDIR:      return ERRORE_NON_CARTELLA;
    case EISDIR:       return ERRORE_E_CARTELLA;
    case ENOTEMPTY:    return ERRORE_NON_VUOTA;
    case ENOSPC:
    case EDQUOT:       return ERRORE_PIENO;
    case ENAMETOOLONG:
    case EINVAL:       return ERRORE_NOME;
    case EACCES:
    case EPERM:        return ERRORE_PERMESSO;
    case EROFS:        return ERRORE_SOLA_LETTURA;
    case ENOMEM:       return ERRORE_MEMORIA;
    case ELOOP:        return ERRORE_TROPPI_LINK;
    case EUCLEAN:      return ERRORE_FORMATO;     /* per ext4: il disco e' rovinato */
    case EOPNOTSUPP:   return ERRORE_NON_SUPPORTATO;
    default:           return ERRORE_DISCO;
    }
}
