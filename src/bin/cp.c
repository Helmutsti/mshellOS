// cp: copia file.
//   cp origine destinazione          copia un file (se destinazione esiste, viene sovrascritta)
//   cp file1 file2 ... cartella      copia i file dentro la cartella, con lo stesso nome
// Copia solo file, non cartelle. La copia ha gli stessi permessi dell'originale
// e appartiene a chi lancia il comando.
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mshelllib.h"

static int e_cartella(const char *percorso) {
    struct stat info;
    return stat(percorso, &info) == 0 && S_ISDIR(info.st_mode);
}

static int copia(const char *origine, const char *destinazione) {
    struct stat info;
    int da = open(origine, O_RDONLY);
    if (da < 0 || fstat(da, &info) != 0) {
        fprintf(stderr, "cp: %s: %s\n", origine, strerror(errno));
        if (da >= 0) close(da);
        return 1;
    }
    if (S_ISDIR(info.st_mode)) {
        fprintf(stderr, "cp: %s: e' una cartella, copio solo file\n", origine);
        close(da);
        return 1;
    }
    int a = open(destinazione, O_WRONLY | O_CREAT | O_TRUNC, info.st_mode & 07777);
    if (a < 0) {
        fprintf(stderr, "cp: %s: %s\n", destinazione, strerror(errno));
        close(da);
        return 1;
    }
    char blocco[4096];
    int errore = 0;
    for (;;) {
        ssize_t letti = read(da, blocco, sizeof blocco);
        if (letti == 0) break;
        if (letti < 0) {
            if (errno == EINTR) continue;
            fprintf(stderr, "cp: %s: %s\n", origine, strerror(errno));
            errore = 1;
            break;
        }
        if (scrivi_tutto(a, blocco, (size_t)letti) != 0) {
            fprintf(stderr, "cp: %s: %s\n", destinazione, strerror(errno));
            errore = 1;
            break;
        }
    }
    close(da);
    if (close(a) != 0 && !errore) {
        fprintf(stderr, "cp: %s: %s\n", destinazione, strerror(errno));
        errore = 1;
    }
    return errore;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "uso: cp origine destinazione\n     cp file... cartella\n");
        return 1;
    }
    const char *ultimo = argv[argc - 1];
    int in_cartella = e_cartella(ultimo);
    if (argc > 3 && !in_cartella) {
        fprintf(stderr, "cp: %s: con piu' file la destinazione deve essere una cartella\n", ultimo);
        return 1;
    }
    int errori = 0;
    for (int i = 1; i < argc - 1; i++) {
        char percorso[4096];
        if (in_cartella) {
            char nome[4096];
            snprintf(nome, sizeof nome, "%s", argv[i]);   // basename puo' modificare la stringa
            snprintf(percorso, sizeof percorso, "%s/%s", ultimo, basename(nome));
        } else {
            snprintf(percorso, sizeof percorso, "%s", ultimo);
        }
        errori |= copia(argv[i], percorso);
    }
    return errori;
}
