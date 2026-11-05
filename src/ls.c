// ls: elenca il contenuto di una o piu' cartelle (senza argomenti: quella corrente).
// I nomi sono in ordine alfabetico; le cartelle hanno una "/" in fondo.
// Come su Linux, i nomi che iniziano con "." non vengono mostrati.
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define LARGHEZZA_SCHERMO 80

static int confronta(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static int elenca(const char *cartella) {
    DIR *d = opendir(cartella);
    if (!d) {
        fprintf(stderr, "ls: %s: %s\n", cartella, strerror(errno));
        return 1;
    }

    // raccoglie i nomi (aggiungendo "/" alle cartelle)
    char **nomi = NULL;
    size_t n = 0, spazio = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.')
            continue;
        char percorso[4096];
        snprintf(percorso, sizeof percorso, "%s/%s", cartella, e->d_name);
        struct stat s;
        int e_cartella = lstat(percorso, &s) == 0 && S_ISDIR(s.st_mode);

        if (n == spazio) {
            spazio = spazio ? spazio * 2 : 32;
            nomi = realloc(nomi, spazio * sizeof *nomi);
            if (!nomi) {
                fprintf(stderr, "ls: memoria esaurita\n");
                closedir(d);
                return 1;
            }
        }
        size_t lung = strlen(e->d_name);
        nomi[n] = malloc(lung + 2);
        memcpy(nomi[n], e->d_name, lung);
        if (e_cartella)
            nomi[n][lung++] = '/';
        nomi[n][lung] = '\0';
        n++;
    }
    closedir(d);

    // stampa in ordine, uno dopo l'altro, andando a capo prima del bordo dello schermo
    qsort(nomi, n, sizeof *nomi, confronta);
    int colonna = 0;
    for (size_t i = 0; i < n; i++) {
        int lung = (int)strlen(nomi[i]);
        if (colonna > 0 && colonna + lung + 2 > LARGHEZZA_SCHERMO) {
            printf("\n");
            colonna = 0;
        }
        colonna += printf("%s  ", nomi[i]);
        free(nomi[i]);
    }
    if (colonna > 0)
        printf("\n");
    free(nomi);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2)
        return elenca(".");
    int errori = 0;
    for (int i = 1; i < argc; i++) {
        if (argc > 2)
            printf("%s:\n", argv[i]);
        errori |= elenca(argv[i]);
    }
    return errori;
}
