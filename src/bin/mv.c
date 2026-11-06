// mv: sposta o rinomina file e cartelle.
//   mv vecchio nuovo                 rinomina (se nuovo esiste gia' come file, viene sostituito)
//   mv cosa1 cosa2 ... cartella      sposta tutto dentro la cartella, con lo stesso nome
// Lo spostamento e' una sola chiamata al kernel (rename): il contenuto non viene copiato,
// cambia solo la voce nella cartella. Per questo funziona solo dentro lo stesso disco.
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int e_cartella(const char *percorso) {
    struct stat info;
    return stat(percorso, &info) == 0 && S_ISDIR(info.st_mode);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "uso: mv vecchio nuovo\n     mv cosa... cartella\n");
        return 1;
    }
    const char *ultimo = argv[argc - 1];
    int in_cartella = e_cartella(ultimo);
    if (argc > 3 && !in_cartella) {
        fprintf(stderr, "mv: %s: con piu' nomi la destinazione deve essere una cartella\n", ultimo);
        return 1;
    }
    int errori = 0;
    for (int i = 1; i < argc - 1; i++) {
        char percorso[4096];
        if (in_cartella) {
            char nome[4096];
            snprintf(nome, sizeof nome, "%s", argv[i]);
            snprintf(percorso, sizeof percorso, "%s/%s", ultimo, basename(nome));
        } else {
            snprintf(percorso, sizeof percorso, "%s", ultimo);
        }
        if (rename(argv[i], percorso) != 0) {
            if (errno == EXDEV)
                fprintf(stderr, "mv: %s: non posso spostare su un altro disco\n", argv[i]);
            else
                fprintf(stderr, "mv: %s: %s\n", argv[i], strerror(errno));
            errori = 1;
        }
    }
    return errori;
}
