// prova: servizio di prova, non fa niente di utile.
// Lo avvia servicenter con la sua identita' (non root). Sul descrittore 3 trova
// il canale verso servicenter: a ogni "ci sei?" risponde con l'ora in cui e' partito.
// Se servicenter chiude il canale, il servizio finisce.
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "mshelllib.h"

#define CANALE 3

int main(void) {
    time_t partito = time(NULL);
    char ora[16];
    strftime(ora, sizeof ora, "%H:%M:%S", gmtime(&partito));

    char richiesta[64];
    ssize_t n;
    while ((n = read(CANALE, richiesta, sizeof richiesta - 1)) > 0) {
        richiesta[n] = '\0';
        char risposta[128];
        int l;
        if (strncmp(richiesta, "ci sei?", 7) == 0)
            l = snprintf(risposta, sizeof risposta, "si', acceso dalle %s UTC, sono il pid %d (utente %d)\n",
                         ora, getpid(), getuid());
        else
            l = snprintf(risposta, sizeof risposta, "non capisco\n");
        scrivi_tutto(CANALE, risposta, (size_t)l);
    }
    return 0;
}
