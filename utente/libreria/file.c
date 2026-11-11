/* La libreria dei programmi, sopra Linux: i file e le cartelle.
 *
 * Le partizioni del disco sono ext4 (system, "/", in sola lettura; data, /data, che si
 * scrive: vedi chiamate.h), che Linux legge e scrive: le funzioni qui sono quasi
 * sempre una chiamata di Linux sola (open, read, write, stat, mkdir, unlink, rename,
 * chmod, chown), con i percorsi controllati da percorso_vero e gli errori tradotti da
 * errore_file (libreria.c).
 *
 * LE CARTELLE. Linux da' le voci di una cartella in un ordine suo (per ext4, quello di una
 * tabella "hash": in pratica a caso). I programmi invece le vogliono sempre nello stesso
 * ordine (la scrivania mostra le applicazioni come le trova): quando si apre una
 * cartella, la libreria legge tutte le voci, salta "." e "..", le mette in ordine
 * alfabetico e poi le consegna una alla volta, a ogni leggi. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "libreria.h"
#include "interno.h"

/* --- le cartelle aperte --- */

#define MAX_CARTELLE 16

struct cartella {
    int d;                          /* il descrittore (-1: posto libero) */
    struct voce_cartella *voci;
    int quante, prossima;
};

static struct cartella cartelle[MAX_CARTELLE];
static int cartelle_pronte;

static struct cartella *cartella_di(int d) {
    if (d < 0) return 0;
    for (int i = 0; i < cartelle_pronte; i++)
        if (cartelle[i].d == d && cartelle[i].voci) return &cartelle[i];
    return 0;
}

void dimentica_cartella(int d) {
    struct cartella *c = cartella_di(d);
    if (!c) return;
    free(c->voci);
    *c = (struct cartella){ .d = -1 };
}

static unsigned int tipo_di(mode_t m) {
    return S_ISREG(m) ? TIPO_FILE : S_ISDIR(m) ? TIPO_CARTELLA : S_ISLNK(m) ? TIPO_COLLEGAMENTO
                                                                            : TIPO_ALTRO;
}

static int per_nome(const void *a, const void *b) {
    return strcmp(((const struct voce_cartella *)a)->nome, ((const struct voce_cartella *)b)->nome);
}

/* Legge tutte le voci della cartella aperta in d (getdents64) e le mette in ordine. */
static int prepara_cartella(int d) {
    struct cartella *c = 0;
    for (int i = 0; i < MAX_CARTELLE && !c; i++)
        if (i >= cartelle_pronte || !cartelle[i].voci) c = &cartelle[i];
    if (!c) return ERRORE_MEMORIA;
    if (c == &cartelle[cartelle_pronte]) cartelle_pronte++;
    *c = (struct cartella){ .d = d };

    int capienza = 0;
    char blocco[4096];
    for (;;) {
        long n = syscall(SYS_getdents64, d, blocco, sizeof blocco);
        if (n < 0) { dimentica_cartella(d); return errore_file(errno); }
        if (n == 0) break;
        for (long k = 0; k < n;) {
            struct { unsigned long long ino; long long dopo; unsigned short lung; unsigned char tipo; char nome[]; }
                *voce = (void *)(blocco + k);
            k += voce->lung;
            const char *nome = voce->nome;
            if (!strcmp(nome, ".") || !strcmp(nome, "..") || strlen(nome) >= MAX_NOME_FILE) continue;
            if (c->quante == capienza) {
                capienza = capienza ? 2 * capienza : 16;
                struct voce_cartella *piu = realloc(c->voci, (size_t)capienza * sizeof *piu);
                if (!piu) { dimentica_cartella(d); return ERRORE_MEMORIA; }
                c->voci = piu;
            }
            struct voce_cartella *v = &c->voci[c->quante++];
            struct stat s;
            *v = (struct voce_cartella){ .tipo = TIPO_ALTRO };
            /* com'e' fatta: il collegamento simbolico non si segue */
            if (fstatat(d, nome, &s, AT_SYMLINK_NOFOLLOW) == 0) {
                v->tipo = tipo_di(s.st_mode);
                v->permessi = s.st_mode & 07777;
                v->dimensione = (unsigned long long)s.st_size;
                v->modificato = s.st_mtime;
            }
            strcpy(v->nome, nome);
        }
    }
    if (!c->voci && !(c->voci = malloc(sizeof *c->voci))) {   /* vuota: ma e' aperta */
        *c = (struct cartella){ .d = -1 };
        return ERRORE_MEMORIA;
    }
    qsort(c->voci, (size_t)c->quante, sizeof *c->voci, per_nome);
    return 0;
}

/* --- aprire, leggere, scrivere --- */

int apri(const char *percorso, unsigned int modo) {
    char vero[MAX_VERO];
    int e = percorso_vero(percorso, vero);
    if (e < 0) return e;
    int flag = O_CLOEXEC;
    if ((modo & APRI_LEGGERE) && (modo & APRI_SCRIVERE)) flag |= O_RDWR;
    else if (modo & APRI_SCRIVERE) flag |= O_WRONLY;
    else flag |= O_RDONLY;
    if (modo & APRI_CREARE) flag |= O_CREAT;
    if ((modo & APRI_SVUOTARE) && (modo & APRI_SCRIVERE)) flag |= O_TRUNC;
    int d = open(vero, flag, 0644);
    if (d < 0) return errore_file(errno);
    struct stat s;
    if (fstat(d, &s) < 0) { e = errore_file(errno); close(d); return e; }
    if (S_ISDIR(s.st_mode)) {
        if ((e = prepara_cartella(d)) < 0) { close(d); return e; }
    } else if (!S_ISREG(s.st_mode)) {     /* dispositivi, code...: non si aprono */
        close(d);
        return ERRORE_NON_SUPPORTATO;
    }
    return d;
}

#ifndef SYS_pidfd_send_signal
#define SYS_pidfd_send_signal 424
#endif

/* ricevi su qualcosa che non e' un socket: la prossima voce di una cartella, i byte di
 * un file, o (per il descrittore di un processo) aspettare che finisca. */
int ricevi_non_socket(int d, void *dove, unsigned int byte) {
    struct cartella *c = cartella_di(d);
    if (c) {
        if (byte < sizeof(struct voce_cartella)) return -1;   /* una voce non si spezza */
        if (c->prossima >= c->quante) return 0;
        memcpy(dove, &c->voci[c->prossima++], sizeof(struct voce_cartella));
        return (int)sizeof(struct voce_cartella);
    }
    /* un processo (pidfd): il segnale 0 non fa niente, ma dice se d e' un pidfd */
    if (syscall(SYS_pidfd_send_signal, d, 0, 0, 0) == 0 || errno == ESRCH) {
        struct pollfd p = { d, POLLIN, 0 };
        while (poll(&p, 1, -1) < 0 && errno == EINTR) ;
        return 0;
    }
    ssize_t n;
    do n = read(d, dove, byte); while (n < 0 && errno == EINTR);
    return n < 0 ? errore_file(errno) : (int)n;
}

int leggi(int d, void *dove, unsigned int byte) { return ricevi_non_socket(d, dove, byte); }

int scrivi_in(int d, const void *dati, unsigned int byte) {
    ssize_t n;
    do n = write(d, dati, byte); while (n < 0 && errno == EINTR);
    return n < 0 ? errore_file(errno) : (int)n;
}

long long posiziona(int d, unsigned long long byte) {
    struct cartella *c = cartella_di(d);
    if (c) {
        c->prossima = byte < (unsigned long long)c->quante ? (int)byte : c->quante;
        return c->prossima;
    }
    struct stat s;
    if (fstat(d, &s) < 0) return errore_file(errno);
    if (byte > (unsigned long long)s.st_size) byte = (unsigned long long)s.st_size;   /* non oltre la fine */
    off_t p = lseek(d, (off_t)byte, SEEK_SET);
    return p < 0 ? errore_file(errno) : (long long)p;
}

/* --- com'e' fatto, e cambiarlo --- */

int info(const char *percorso, struct info_file *i) {
    char vero[MAX_VERO];
    int e = percorso_vero(percorso, vero);
    if (e < 0) return e;
    struct stat s;
    if (stat(vero, &s) < 0) return errore_file(errno);
    *i = (struct info_file){ .tipo = tipo_di(s.st_mode), .permessi = s.st_mode & 07777,
                             .proprietario = s.st_uid, .collegamenti = (unsigned int)s.st_nlink,
                             .dimensione = (unsigned long long)s.st_size, .modificato = s.st_mtime };
    return 0;
}

int crea_cartella(const char *percorso) {
    char vero[MAX_VERO];
    int e = percorso_vero(percorso, vero);
    if (e < 0) return e;
    return mkdir(vero, 0755) < 0 ? errore_file(errno) : 0;
}

/* Un file o una cartella vuota: per Linux sono due chiamate (unlink e rmdir). */
int cancella(const char *percorso) {
    char vero[MAX_VERO];
    int e = percorso_vero(percorso, vero);
    if (e < 0) return e;
    if (unlink(vero) == 0) return 0;
    if (errno != EISDIR) return errore_file(errno);
    return rmdir(vero) < 0 ? errore_file(errno) : 0;
}

int rinomina(const char *da, const char *a) {
    char vero_da[MAX_VERO], vero_a[MAX_VERO];
    int e = percorso_vero(da, vero_da);
    if (e < 0 || (e = percorso_vero(a, vero_a)) < 0) return e;
    return rename(vero_da, vero_a) < 0 ? errore_file(errno) : 0;
}

/* Il proprietario si cambia (chown) solo se e' diverso: lo puo' fare solo il sistema, e
 * a chi chiede solo i permessi dei suoi file non serve. I permessi (chmod) li puo'
 * cambiare il proprietario. Lo controlla il kernel. */
int cambia_permessi(const char *percorso, unsigned int proprietario, unsigned int permessi) {
    char vero[MAX_VERO];
    int e = percorso_vero(percorso, vero);
    if (e < 0) return e;
    struct stat s;
    if (stat(vero, &s) < 0) return errore_file(errno);
    if (s.st_uid != proprietario && chown(vero, proprietario, (gid_t)-1) < 0) return errore_file(errno);
    return chmod(vero, permessi & 07777) < 0 ? errore_file(errno) : 0;
}
