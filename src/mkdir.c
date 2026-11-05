// mkdir: crea una o piu' cartelle.
// Il proprietario e' l'utente che lancia il comando; i permessi sono 0755
// (il kernel toglie quelli indicati dalla "umask" del processo).
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "uso: mkdir cartella [altre cartelle...]\n");
        return 1;
    }
    int errori = 0;
    for (int i = 1; i < argc; i++) {
        if (mkdir(argv[i], 0755) != 0) {
            fprintf(stderr, "mkdir: %s: %s\n", argv[i], strerror(errno));
            errori = 1;
        }
    }
    return errori;
}
