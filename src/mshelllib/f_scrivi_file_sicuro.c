// Scrittura sicura di un file, il metodo usato dai sistemi reali:
//   1. scrive tutto in un file temporaneo accanto a quello definitivo
//   2. fsync: chiede al kernel di salvarlo sul disco subito, e aspetta la conferma
//   3. rename: gli da' il nome definitivo con un'unica operazione indivisibile
//   4. fsync della cartella: salva su disco anche il cambio di nome
#include "mshelllib.h"

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int scrivi_file_sicuro(const char *percorso, const void *dati, size_t lunghezza, mode_t permessi) {
    char temporaneo[512];
    if (snprintf(temporaneo, sizeof temporaneo, "%s.tmp", percorso) >= (int)sizeof temporaneo) {
        errno = ENAMETOOLONG;
        return -1;
    }

    // 1. file temporaneo, creato gia' con i permessi definitivi
    int fd = open(temporaneo, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, permessi);
    if (fd < 0)
        return -1;
    if (fchmod(fd, permessi) != 0)
        goto errore;

    if (scrivi_tutto(fd, dati, lunghezza) != 0)
        goto errore;

    // 2. sul disco, adesso
    if (fsync(fd) != 0)
        goto errore;
    if (close(fd) != 0) {
        fd = -1;
        goto errore;
    }
    fd = -1;

    // 3. nome definitivo
    if (rename(temporaneo, percorso) != 0)
        goto errore;

    // 4. salva il cambio di nome: la voce del file sta nella cartella
    char copia[512];
    snprintf(copia, sizeof copia, "%s", percorso);
    int cartella = open(dirname(copia), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (cartella < 0)
        return -1;
    int r = fsync(cartella);
    int salvato = errno;
    close(cartella);
    errno = salvato;
    return r;

errore:;
    int motivo = errno;
    if (fd >= 0)
        close(fd);
    unlink(temporaneo);
    errno = motivo;
    return -1;
}
