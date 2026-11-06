// cat: stampa il contenuto di uno o piu' file, uno dopo l'altro.
// Senza argomenti legge dalla tastiera e ripete quello che scrivi (Ctrl+D per finire).
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mshelllib.h"

// Copia tutto da un descrittore all'uscita standard.
static int copia(int da, const char *nome) {
    char blocco[4096];
    for (;;) {
        ssize_t letti = read(da, blocco, sizeof blocco);
        if (letti == 0)
            return 0;                          // fine del file
        if (letti < 0) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "cat: %s: %s\n", nome, strerror(errno));
            return 1;
        }
        if (scrivi_tutto(1, blocco, (size_t)letti) != 0)
            return 1;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2)
        return copia(0, "tastiera");
    int errori = 0;
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "cat: %s: %s\n", argv[i], strerror(errno));
            errori = 1;
            continue;
        }
        errori |= copia(fd, argv[i]);
        close(fd);
    }
    return errori;
}
