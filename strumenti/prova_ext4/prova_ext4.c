/* prova_ext4: prova il driver ext4 del kernel (src/fs/ext4/) sul computer che compila.
 *
 *     prova_ext4 [-s BYTE_SETTORE] IMMAGINE COMANDO [ARGOMENTI]
 *
 *   elenca [PERCORSO]        tutto l'albero da PERCORSO (di solito /), una riga per nodo:
 *                            percorso, tipo (f d l a), permessi, proprietario, gruppo,
 *                            collegamenti, dimensione, data; per i collegamenti anche
 *                            " -> destinazione"
 *   leggi PERCORSO           il contenuto del file sull'uscita (segue i collegamenti)
 *   destinazione PERCORSO    dove punta il collegamento PERCORSO (senza seguirlo)
 *   estrai CARTELLA          copia tutti i file normali in CARTELLA (sul computer), con
 *                            i buchi dei file sparsi lasciati buchi
 *   monta                    solo il montaggio (per vedere gli avvisi)
 *
 * IL TRUCCO (come crea_disco): non c'e' un ext4 riscritto apposta per la prova. Si
 * compilano INSIEME a questo file gli stessi file .c di src/fs/ext4 del kernel, senza toccarli;
 * le poche cose del kernel che usano (kmalloc, kprintf, memcpy...) le danno
 * strumenti/prova_ext4/finto/ e le funzioni qui sotto. Il disco e' un file IMMAGINE,
 * letto a settori come farebbe il driver di un disco vero.
 *
 * Controlli in piu': il driver non deve mai scrivere sul disco, deve restituire tutta la
 * memoria che chiede (si contano i kmalloc senza kfree), e il tipo che una voce di
 * cartella dichiara deve essere quello dell'inode. Un errore -> uscita 1.
 *
 * Lo usa prova.sh (stessa cartella), che prepara i dischi con mkfs.ext4 e confronta. */
#define _GNU_SOURCE
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include "vfs.h"
#include "ext4.h"

/* --- quello che il driver si aspetta dal kernel --- */

void vkprintf(const char *fmt, va_list ap) { vfprintf(stderr, fmt, ap); }
void kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vkprintf(fmt, ap);
    va_end(ap);
}

static long pezzi_vivi;
void *kmalloc(unsigned int byte) {
    void *p = malloc(byte ? byte : 1);
    if (p) pezzi_vivi++;
    return p;
}
void kfree(void *p) {
    if (p) {
        pezzi_vivi--;
        free(p);
    }
}

/* --- il disco: un file --- */

static int fd_disco;
static unsigned long long letture;

static int disco_leggi(struct dispositivo_blocchi *d, unsigned long long primo,
                       unsigned int quanti, void *dove) {
    size_t byte = (size_t)quanti * d->byte_settore;
    off_t pos = (off_t)(primo * d->byte_settore);
    if (primo + quanti > d->settori) {
        fprintf(stderr, "prova_ext4: lettura oltre la fine del disco\n");
        return -1;
    }
    letture++;
    while (byte) {
        ssize_t n = pread(fd_disco, dove, byte, pos);
        if (n <= 0) return -1;
        dove = (char *)dove + n;
        byte -= (size_t)n;
        pos += n;
    }
    return 0;
}

static int disco_scrivi(struct dispositivo_blocchi *d, unsigned long long primo,
                        unsigned int quanti, const void *da) {
    (void)d; (void)primo; (void)quanti; (void)da;
    fprintf(stderr, "prova_ext4: ERRORE: il driver ha provato a scrivere sul disco\n");
    exit(1);
}

static struct dispositivo_blocchi disco = {
    .nome = "immagine", .byte_settore = 512, .leggi = disco_leggi, .scrivi = disco_scrivi,
};
static struct montato m = { .tipo = &tipo_ext4, .disco = &disco };
static int errori;

static void sbaglio(const char *fmt, ...) {
    va_list ap;
    fprintf(stderr, "prova_ext4: ERRORE: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    errori++;
}

/* --- i percorsi, come li percorrera' il file system virtuale --- */

/* Il contenuto di un collegamento in buf (con lo 0). <0 se non va. */
static long long leggi_collegamento(unsigned long long nodo, char *buf, unsigned int max) {
    struct info_vfs i;
    long long n;
    if (tipo_ext4.info(&m, nodo, &i) < 0) return -1;
    if (i.dimensione >= max) return -1;
    n = tipo_ext4.leggi(&m, nodo, 0, buf, (unsigned int)i.dimensione);
    if (n != (long long)i.dimensione) return -1;
    buf[n] = 0;
    return n;
}

/* Trova il nodo di 'percorso'. 'segui': se l'ultimo pezzo e' un collegamento, si segue. */
static int trova(const char *percorso, int segui, unsigned long long *nodo) {
    char resto[8192];
    unsigned long long qui = m.radice;
    int salti = 0;
    snprintf(resto, sizeof resto, "%s", percorso);

    char *p = resto;
    for (;;) {
        while (*p == '/') p++;
        if (!*p) break;
        char *fine = strchr(p, '/');
        unsigned int lung = fine ? (unsigned int)(fine - p) : (unsigned int)strlen(p);
        unsigned long long dopo;
        int r = tipo_ext4.cerca(&m, qui, p, lung, &dopo);
        if (r) return r;
        p += lung;
        struct info_vfs i;
        r = tipo_ext4.info(&m, dopo, &i);
        if (r) return r;
        int ultimo = 1;
        for (char *c = p; *c; c++) if (*c != '/') { ultimo = 0; break; }
        if (i.tipo == VFS_COLLEGAMENTO && (!ultimo || segui)) {
            char dest[4200], nuovo[8192];
            if (++salti > 40) return VFS_ERR_TROPPI_LINK;
            if (leggi_collegamento(dopo, dest, sizeof dest) < 0) return VFS_ERR_FORMATO;
            snprintf(nuovo, sizeof nuovo, "%s/%s", dest, p);
            if (dest[0] == '/') qui = m.radice;   /* assoluto: dalla radice dell'immagine */
            strcpy(resto, nuovo);
            p = resto;
            continue;
        }
        qui = dopo;
    }
    *nodo = qui;
    return 0;
}

static const char *nome_tipo(unsigned int t) {
    switch (t) {
    case VFS_FILE: return "f";
    case VFS_CARTELLA: return "d";
    case VFS_COLLEGAMENTO: return "l";
    default: return "a";
    }
}

static void stampa_nodo(const char *percorso, unsigned long long nodo, struct info_vfs *i) {
    printf("%s %s %o %u %u %u %llu %lld", percorso, nome_tipo(i->tipo), i->permessi,
           i->proprietario, i->gruppo, i->collegamenti, i->dimensione, i->modificato);
    if (i->tipo == VFS_COLLEGAMENTO) {
        char dest[4200];
        if (leggi_collegamento(nodo, dest, sizeof dest) < 0) sbaglio("%s: collegamento illeggibile", percorso);
        else printf(" -> %s", dest);
    }
    printf("\n");
}

/* Elenca senza ricorsione del C: una pila esplicita di cartelle da visitare. */
struct da_fare { char *percorso; unsigned long long nodo; };

static void elenca(const char *inizio) {
    unsigned long long nodo;
    struct info_vfs i;
    int r = trova(inizio, 1, &nodo);
    if (r) { sbaglio("%s: errore %d", inizio, r); return; }
    if (tipo_ext4.info(&m, nodo, &i)) { sbaglio("%s: info", inizio); return; }
    stampa_nodo(inizio, nodo, &i);
    if (i.tipo != VFS_CARTELLA) return;

    size_t n = 0, max = 64;
    struct da_fare *pila = malloc(max * sizeof *pila);
    pila[n++] = (struct da_fare){ strdup(strcmp(inizio, "/") ? inizio : ""), nodo };
    while (n) {
        struct da_fare c = pila[--n];
        unsigned long long cursore = 0;
        struct voce_vfs v;
        while ((r = tipo_ext4.voce(&m, c.nodo, &cursore, &v)) == 1) {
            char *p;
            if (strlen(v.nome) != v.lunghezza) sbaglio("nome con uno 0 dentro");
            if (asprintf(&p, "%s/%s", c.percorso, v.nome) < 0) exit(2);
            if (tipo_ext4.info(&m, v.nodo, &i)) { sbaglio("%s: info", p); free(p); continue; }
            if (v.tipo && v.tipo != i.tipo)
                sbaglio("%s: la voce dice tipo %u, l'inode %u", p, v.tipo, i.tipo);
            /* cerca deve trovare lo stesso nodo */
            unsigned long long ritrovato;
            if (tipo_ext4.cerca(&m, c.nodo, v.nome, v.lunghezza, &ritrovato) ||
                ritrovato != v.nodo)
                sbaglio("%s: cerca non trova il nodo della voce", p);
            stampa_nodo(p, v.nodo, &i);
            if (i.tipo == VFS_CARTELLA) {
                if (n == max) pila = realloc(pila, (max *= 2) * sizeof *pila);
                pila[n++] = (struct da_fare){ p, v.nodo };
            } else {
                free(p);
            }
        }
        if (r < 0) sbaglio("%s: voce: errore %d", c.percorso, r);
        free(c.percorso);
    }
    free(pila);
}

/* Le letture si fanno a pezzi di misure diverse (anche 1 byte, anche a cavallo dei
 * blocchi), per provare tutte le strade di leggi(). */
static const unsigned int pezzi[] = { 1 << 20, 1, 4095, 7, 65537, 3 << 20, 512, 4097 };

/* Copia il nodo su 'fd'. 'sparso': i pezzi a zero non si scrivono (si salta avanti). */
static int copia(unsigned long long nodo, int fd, int sparso) {
    struct info_vfs i;
    static char buf[3 << 20], zero[3 << 20];
    unsigned long long pos = 0;
    unsigned int k = 0;
    if (tipo_ext4.info(&m, nodo, &i)) return -1;
    for (;;) {
        unsigned int chiesti = pezzi[k++ % (sizeof pezzi / sizeof pezzi[0])];
        long long n = tipo_ext4.leggi(&m, nodo, pos, buf, chiesti);
        if (n < 0) { sbaglio("leggi: errore %lld al byte %llu", n, pos); return -1; }
        if (n == 0) break;
        if (n > chiesti) { sbaglio("leggi: piu' byte di quelli chiesti"); return -1; }
        if (sparso && !memcmp(buf, zero, (size_t)n)) {
            if (lseek(fd, n, SEEK_CUR) < 0) return -1;
        } else if (write(fd, buf, (size_t)n) != n) {
            return -1;
        }
        pos += (unsigned long long)n;
    }
    if (pos != i.dimensione) sbaglio("letti %llu byte, la dimensione e' %llu", pos, i.dimensione);
    if (sparso && ftruncate(fd, (off_t)pos) < 0) return -1;
    /* oltre la fine: 0 byte */
    if (tipo_ext4.leggi(&m, nodo, pos + 5, buf, 10) != 0) sbaglio("leggi oltre la fine non da' 0");
    return 0;
}

static void estrai(const char *dove) {
    size_t n = 0, max = 64;
    struct da_fare *pila = malloc(max * sizeof *pila);
    mkdir(dove, 0755);
    pila[n++] = (struct da_fare){ strdup(dove), m.radice };
    while (n) {
        struct da_fare c = pila[--n];
        unsigned long long cursore = 0;
        struct voce_vfs v;
        int r;
        while ((r = tipo_ext4.voce(&m, c.nodo, &cursore, &v)) == 1) {
            struct info_vfs i;
            char *p;
            if (asprintf(&p, "%s/%s", c.percorso, v.nome) < 0) exit(2);
            if (tipo_ext4.info(&m, v.nodo, &i)) { sbaglio("%s: info", p); free(p); continue; }
            if (i.tipo == VFS_CARTELLA) {
                mkdir(p, 0755);
                if (n == max) pila = realloc(pila, (max *= 2) * sizeof *pila);
                pila[n++] = (struct da_fare){ p, v.nodo };
                continue;
            }
            if (i.tipo == VFS_FILE) {
                int fd = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0 || copia(v.nodo, fd, 1)) sbaglio("%s: copia", p);
                if (fd >= 0) close(fd);
            }
            free(p);
        }
        if (r < 0) sbaglio("%s: voce: errore %d", c.percorso, r);
        free(c.percorso);
    }
    free(pila);
}

int main(int argc, char **argv) {
    int a = 1;
    if (a + 1 < argc && !strcmp(argv[a], "-s")) {
        disco.byte_settore = (unsigned int)atoi(argv[a + 1]);
        a += 2;
    }
    if (argc - a < 2) {
        fprintf(stderr, "uso: prova_ext4 [-s BYTE_SETTORE] IMMAGINE "
                        "elenca|leggi|destinazione|estrai|monta [ARGOMENTO]\n");
        return 2;
    }
    const char *immagine = argv[a], *comando = argv[a + 1], *arg = argc - a > 2 ? argv[a + 2] : 0;

    fd_disco = open(immagine, O_RDONLY);
    if (fd_disco < 0) { perror(immagine); return 2; }
    struct stat st;
    fstat(fd_disco, &st);
    disco.settori = (unsigned long long)st.st_size / disco.byte_settore;

    int r = tipo_ext4.monta(&m, &disco, 1);
    if (r) {
        fprintf(stderr, "prova_ext4: montaggio rifiutato: %d\n", r);
        return 3;
    }
    if (!strcmp(comando, "monta")) {
        /* niente */
    } else if (!strcmp(comando, "elenca")) {
        elenca(arg ? arg : "/");
    } else if (!strcmp(comando, "leggi") || !strcmp(comando, "destinazione")) {
        unsigned long long nodo;
        int leggi = comando[0] == 'l';
        if (!arg) return 2;
        r = trova(arg, leggi, &nodo);
        if (r) {
            sbaglio("%s: errore %d", arg, r);
        } else if (leggi) {
            if (copia(nodo, 1, 0)) sbaglio("%s: copia", arg);
        } else {
            char dest[4200];
            if (leggi_collegamento(nodo, dest, sizeof dest) < 0) sbaglio("%s: non e' un collegamento", arg);
            else printf("%s\n", dest);
        }
    } else if (!strcmp(comando, "estrai")) {
        if (!arg) return 2;
        estrai(arg);
    } else {
        fprintf(stderr, "prova_ext4: comando sconosciuto %s\n", comando);
        return 2;
    }
    tipo_ext4.smonta(&m);
    if (pezzi_vivi) sbaglio("%ld pezzi di memoria mai restituiti", pezzi_vivi);
    if (getenv("PROVA_LETTURE")) fprintf(stderr, "letture dal disco: %llu\n", letture);
    return errori ? 1 : 0;
}
