// mshell: shell rudimentale.
// Ciclo: mostra il prompt, legge una riga, la spezza in parole, esegue, ricomincia.
//   - comandi interni: cd, exit
//   - tutti gli altri: cerca il programma nelle cartelle di PATH (es. /system/bin),
//     crea un processo figlio (fork), lo trasforma nel programma (execve) e aspetta (waitpid).
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_PAROLE 32

extern char **environ;

// Spezza la riga in parole separate da spazi. Modifica la riga.
static int spezza(char *riga, char *parole[]) {
    int n = 0;
    for (char *p = strtok(riga, " \t\n"); p && n < MAX_PAROLE - 1; p = strtok(NULL, " \t\n"))
        parole[n++] = p;
    parole[n] = NULL;
    return n;
}

// Trova il percorso del programma: se il nome contiene '/', e' gia' un percorso;
// altrimenti lo cerca in ogni cartella di PATH.
static int trova(const char *nome, char *percorso, size_t dim) {
    if (strchr(nome, '/')) {
        snprintf(percorso, dim, "%s", nome);
        return access(percorso, X_OK);
    }
    const char *path = getenv("PATH");
    if (!path)
        return -1;
    char copia[512];
    snprintf(copia, sizeof copia, "%s", path);
    for (char *dir = strtok(copia, ":"); dir; dir = strtok(NULL, ":")) {
        snprintf(percorso, dim, "%s/%s", dir, nome);
        if (access(percorso, X_OK) == 0)
            return 0;
    }
    return -1;
}

static void esegui(char *parole[]) {
    char percorso[512];
    if (trova(parole[0], percorso, sizeof percorso) != 0) {
        printf("mshell: comando non trovato: %s\n", parole[0]);
        return;
    }
    pid_t figlio = fork();
    if (figlio < 0) {
        printf("mshell: fork fallito: %s\n", strerror(errno));
        return;
    }
    if (figlio == 0) {
        signal(SIGINT, SIG_DFL);               // il programma si puo' fermare con Ctrl+C
        signal(SIGQUIT, SIG_DFL);
        execve(percorso, parole, environ);
        printf("mshell: impossibile eseguire %s: %s\n", percorso, strerror(errno));
        _exit(127);
    }
    int stato;
    while (waitpid(figlio, &stato, 0) < 0 && errno == EINTR)
        ;
    if (WIFSIGNALED(stato))
        printf("\nmshell: %s interrotto dal segnale %d\n", parole[0], WTERMSIG(stato));
}

int main(void) {
    // La shell stessa ignora Ctrl+C: interrompe solo il programma in corso.
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);

    const char *utente = getenv("USER");
    if (!utente)
        utente = "?";

    // Pulisce lo schermo con le sequenze di escape ANSI, che la console del kernel capisce:
    //   ESC[H  = cursore in alto a sinistra     ESC[2J = cancella tutto lo schermo
    printf("\033[H\033[2J");

    char riga[1024];
    char *parole[MAX_PAROLE];
    for (;;) {
        char cartella[256];
        if (!getcwd(cartella, sizeof cartella))
            strcpy(cartella, "?");
        printf("%s@%s> ", utente, cartella);
        fflush(stdout);

        if (!fgets(riga, sizeof riga, stdin)) {
            if (ferror(stdin) && errno == EINTR) {
                clearerr(stdin);
                printf("\n");
                continue;
            }
            break;                             // fine dell'input
        }
        if (spezza(riga, parole) == 0)
            continue;

        if (strcmp(parole[0], "exit") == 0)
            break;
        if (strcmp(parole[0], "cd") == 0) {
            const char *dove = parole[1] ? parole[1] : getenv("HOME");
            if (!dove || chdir(dove) != 0)
                printf("mshell: cd: %s\n", strerror(errno));
            continue;
        }
        esegui(parole);
    }
    return 0;
}
