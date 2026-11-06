// help: comando interno della shell, elenca i comandi che puoi usare.
//   - comandi di sistema: l'elenco e' scritto qui sotto (gli utenti non possono elencare /bin).
//     Quando aggiungi un comando a mshell, aggiungi una riga anche qui.
//     Quelli "solo amministratore" non vengono mostrati agli altri utenti.
//   - comandi interni della shell
//   - i tuoi programmi: quelli eseguibili nella tua cartella /home/bin, se esiste
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "comandi.h"

#define CARTELLA_PROGRAMMI_UTENTE "/home/bin"

struct voce {
    const char *nome;
    const char *descrizione;
    int solo_amministratore;
};

static const struct voce comandi[] = {
    { "cat",      "mostra il contenuto dei file (senza file: legge quello che arriva)", 0 },
    { "cp",       "copia file", 0 },
    { "ls",       "elenca il contenuto di cartelle (senza nome: quella in cui sei)", 0 },
    { "mkdir",    "crea cartelle", 0 },
    { "mv",       "sposta o rinomina file e cartelle", 0 },
    { "rm",       "cancella file e cartelle vuote", 0 },
    { "write",    "scrive e modifica un file di testo (^S salva, ^X esce)", 0 },
    { "shutdown", "spegne il sistema", 1 },
    { "svcc",     "gestisce i servizi di sistema (svcc help)", 1 },
    { NULL, NULL, 0 },
};

static const struct voce interni[] = {
    { "cd",   "cambia cartella", 0 },
    { "exit", "esce e torna all'accesso", 0 },
    { "help", "questo elenco", 0 },
    { NULL, NULL, 0 },
};

static void stampa(const struct voce *v, int amministratore) {
    for (; v->nome; v++)
        if (amministratore || !v->solo_amministratore)
            printf("  %-10s %s\n", v->nome, v->descrizione);
}

// Elenca i file eseguibili nella cartella dei programmi dell'utente, se c'e'.
static void programmi_utente(void) {
    DIR *d = opendir(CARTELLA_PROGRAMMI_UTENTE);
    if (!d)
        return;
    printf("I tuoi programmi (%s):\n", CARTELLA_PROGRAMMI_UTENTE);
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.')
            continue;
        char percorso[512];
        struct stat s;
        snprintf(percorso, sizeof percorso, "%s/%s", CARTELLA_PROGRAMMI_UTENTE, e->d_name);
        if (stat(percorso, &s) == 0 && S_ISREG(s.st_mode) && access(percorso, X_OK) == 0) {
            printf("  %s\n", e->d_name);
            n++;
        }
    }
    closedir(d);
    if (n == 0)
        printf("  (nessuno)\n");
}

int comando_help(char *parole[]) {
    (void)parole;
    int amministratore = getuid() == 0;
    printf("Comandi di sistema:\n");
    stampa(comandi, amministratore);
    printf("Comandi interni della shell:\n");
    stampa(interni, amministratore);
    printf("Simboli:  a | b   a > file   a >> file   a < file   > file\n");
    programmi_utente();
    return CONTINUA;
}
