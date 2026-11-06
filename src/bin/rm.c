// rm: cancella uno o piu' file.
// Una cartella si cancella solo se e' vuota (prima vanno cancellati i file dentro).
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "uso: rm file [altri file...]\n");
        return 1;
    }
    int errori = 0;
    for (int i = 1; i < argc; i++) {
        struct stat info;
        int esito;
        if (lstat(argv[i], &info) == 0 && S_ISDIR(info.st_mode))
            esito = rmdir(argv[i]);            // cartella: il kernel la toglie solo se vuota
        else
            esito = unlink(argv[i]);           // file: toglie il nome dalla cartella
        if (esito != 0) {
            fprintf(stderr, "rm: %s: %s\n", argv[i], strerror(errno));
            errori = 1;
        }
    }
    return errori;
}
