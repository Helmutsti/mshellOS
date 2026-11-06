// cd: cambia la cartella corrente della shell.
// Senza argomenti va nella cartella personale (HOME), se l'utente ne ha una.
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "comandi.h"

int comando_cd(char *parole[]) {
    const char *dove = parole[1] ? parole[1] : getenv("HOME");
    if (!dove)
        printf("mshell: cd: indica una cartella\n");
    else if (chdir(dove) != 0)
        printf("mshell: cd: %s: %s\n", dove, strerror(errno));
    return CONTINUA;
}
