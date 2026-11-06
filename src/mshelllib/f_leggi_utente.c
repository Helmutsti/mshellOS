// leggi_utente: cerca un utente per nome nel file degli utenti (FILE_UTENTI).
// Ogni riga ha 7 campi separati da ':' (anche vuoti).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mshelllib.h"

int leggi_utente(const char *nome, struct utente *u) {
    FILE *f = fopen(FILE_UTENTI, "r");
    if (!f)
        return -1;
    char riga[512];
    int trovato = -1;
    while (trovato != 0 && fgets(riga, sizeof riga, f)) {
        riga[strcspn(riga, "\n")] = '\0';
        char *campi[7];
        int n = 0;
        char *p = riga;
        while (n < 7) {
            campi[n++] = p;
            p = strchr(p, ':');
            if (!p)
                break;
            *p++ = '\0';
        }
        if (n != 7 || strcmp(campi[0], nome) != 0)
            continue;
        snprintf(u->nome, sizeof u->nome, "%s", campi[0]);
        snprintf(u->password, sizeof u->password, "%s", campi[1]);
        u->uid = (unsigned)strtoul(campi[2], NULL, 10);
        u->gid = (unsigned)strtoul(campi[3], NULL, 10);
        snprintf(u->descrizione, sizeof u->descrizione, "%s", campi[4]);
        snprintf(u->cartella, sizeof u->cartella, "%s", campi[5]);
        snprintf(u->programma, sizeof u->programma, "%s", campi[6]);
        trovato = 0;
    }
    fclose(f);
    return trovato;
}
