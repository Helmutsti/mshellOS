/* buchi: un file con un blocco di dati ogni due (gli altri sono buchi), per prova.sh.
 *
 *     buchi FILE BLOCCO N
 *
 * Scrive N blocchi da BLOCCO byte ai posti 0, 2, 4... (ognuno pieno di un disegno
 * diverso), piu' 123 byte dopo l'ultimo buco. mkfs.ext4 -d copia i buchi come buchi:
 * ogni blocco di dati diventa un extent a se', e con migliaia di extent l'albero
 * diventa profondo (e con la mappa all'antica servono gli indiretti). */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "uso: buchi FILE BLOCCO N\n");
        return 2;
    }
    long blocco = atol(argv[2]), n = atol(argv[3]);
    unsigned char *buf = malloc(blocco);
    int fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0 || !buf) return 1;
    for (long k = 0; k <= n; k++) {
        long quanti = k < n ? blocco : 123;
        for (long i = 0; i < quanti; i++) buf[i] = (unsigned char)(k * 7 + i * 13 + 1) | 1;
        if (pwrite(fd, buf, quanti, (off_t)(2 * k) * blocco) != quanti) return 1;
    }
    close(fd);
    return 0;
}
