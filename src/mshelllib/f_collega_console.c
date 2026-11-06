// collega_console: rende la console il terminale del processo.
//   setsid       nuova sessione: il processo non dipende piu' dal terminale di chi l'ha creato
//   TIOCSCTTY    la console diventa il terminale della sessione: Ctrl+C arriva qui
//   dup2         la console diventa entrata (0), uscita (1) ed errori (2)
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "mshelllib.h"

int collega_console(const char *console) {
    setsid();
    int tty = open(console, O_RDWR);
    if (tty < 0)
        return -1;
    if (ioctl(tty, TIOCSCTTY, 0) != 0) {
        close(tty);
        return -1;
    }
    dup2(tty, 0);
    dup2(tty, 1);
    dup2(tty, 2);
    if (tty > 2)
        close(tty);
    return 0;
}
