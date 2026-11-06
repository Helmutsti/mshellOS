// scrivi_tutto: scrive tutti i byte, ripetendo write() finche' non ha finito.
// write() puo' scrivere meno byte di quelli chiesti (per esempio su un tubo pieno)
// oppure fermarsi per un segnale (EINTR): in entrambi i casi si continua da dove era rimasta.
#include <errno.h>
#include <unistd.h>

#include "mshelllib.h"

int scrivi_tutto(int fd, const void *dati, size_t lunghezza) {
    const char *p = dati;
    while (lunghezza > 0) {
        ssize_t scritti = write(fd, p, lunghezza);
        if (scritti < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        p += scritti;
        lunghezza -= (size_t)scritti;
    }
    return 0;
}
