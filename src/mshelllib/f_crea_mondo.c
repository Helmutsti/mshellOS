// crea_mondo: chiude il processo dentro una cartella, che per lui diventa la radice "/".
//
// Va chiamata da root, nel processo figlio, PRIMA di cambiare utente e di avviare il programma.
// Passi:
//   1. unshare(CLONE_NEWNS): il processo riceve una copia privata dell'elenco dei montaggi;
//      da qui in poi quello che monta o smonta non lo vede nessun altro.
//   2. Aggancia nella cartella le cose del sistema che deve vedere (mount --bind):
//      non sono copie, e' la stessa cartella o lo stesso file del disco che compare anche li'.
//   3. Sposta la cartella al posto di / (MS_MOVE) e ci si chiude dentro (chroot).
//      E' il metodo di switch_root: pivot_root non funziona quando la radice e' l'initramfs.
//      Dopo il cambio di utente il processo non puo' piu' uscire: chroot richiede i poteri di root.
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mshelllib.h"

int crea_mondo(const char *radice, const struct aggancio *agganci, int n) {
    if (unshare(CLONE_NEWNS) != 0)
        return -1;
    // i montaggi di questo mondo non si propagano fuori (e viceversa)
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) != 0)
        return -1;
    // per poterla spostare al posto di /, la cartella deve essere essa stessa un aggancio
    if (mount(radice, radice, NULL, MS_BIND | MS_REC, NULL) != 0)
        return -1;

    for (int i = 0; i < n; i++) {
        struct stat s;
        if (stat(agganci[i].sorgente, &s) != 0)
            return -1;
        char punto[256];
        snprintf(punto, sizeof punto, "%s%s", radice, agganci[i].destinazione);
        // il punto di aggancio deve esistere ed essere dello stesso tipo: cartella o file
        if (S_ISDIR(s.st_mode)) {
            if (mkdir(punto, 0755) != 0 && errno != EEXIST)
                return -1;
        } else {
            int fd = open(punto, O_CREAT | O_WRONLY | O_CLOEXEC, 0644);
            if (fd < 0)
                return -1;
            close(fd);
        }
        if (mount(agganci[i].sorgente, punto, NULL, MS_BIND | MS_REC, NULL) != 0)
            return -1;
        unsigned long opzioni = MS_REMOUNT | MS_BIND | MS_NOSUID | MS_NODEV;
        if (agganci[i].sola_lettura)
            opzioni |= MS_RDONLY;
        if (mount(NULL, punto, NULL, opzioni, NULL) != 0)
            return -1;
    }

    if (chdir(radice) != 0 || mount(radice, "/", NULL, MS_MOVE, NULL) != 0)
        return -1;
    if (chroot(".") != 0 || chdir("/") != 0)
        return -1;
    return 0;
}
