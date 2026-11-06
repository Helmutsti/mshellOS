// mshell: shell rudimentale.
// Ciclo: mostra il prompt, legge una riga, la spezza in parole, esegue, ricomincia.
// Simboli come nelle shell Linux:
//   a | b       l'uscita di a entra in b (catena di quanti comandi si vuole)
//   a > file    l'uscita di a va nel file (lo riscrive);   a >> file   la aggiunge in fondo
//   a < file    a legge dal file invece che dalla tastiera
//   > file      da solo: crea il file vuoto (o lo svuota)
//   - comandi interni (cd, exit): scritti in questa cartella, elencati qui sotto
//   - tutti gli altri: il programma con quel nome in /bin (sistema), altrimenti in /home/bin (utente),
//     crea un processo figlio (fork), lo trasforma nel programma (execve) e aspetta (waitpid).
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "comandi.h"

#define MAX_PAROLE 32
#define CARTELLA_PROGRAMMI        "/bin"       // programmi di sistema (sola lettura)
#define CARTELLA_PROGRAMMI_UTENTE "/home/bin"  // programmi dell'utente

extern char **environ;

// Elenco dei comandi interni. Per aggiungerne uno: scrivi nome.c in questa cartella,
// dichiaralo in comandi.h e aggiungi una riga qui.
static const struct comando comandi_interni[] = {
    { "cd",   comando_cd },
    { "exit", comando_exit },
    { "help", comando_help },
    { NULL,   NULL },
};

// Cerca un comando interno per nome. Restituisce NULL se non c'e'.
static const struct comando *comando_interno(const char *nome) {
    for (const struct comando *c = comandi_interni; c->nome; c++)
        if (strcmp(c->nome, nome) == 0)
            return c;
    return NULL;
}

// Spezza la riga in parole. I simboli | > >> < sono parole a se', anche se attaccati
// (es. "ls>f" diventa "ls", ">", "f"). Le parole vengono copiate in "spazio".
static int spezza(const char *riga, char *parole[], char *spazio) {
    int n = 0;
    const char *p = riga;
    while (*p && n < MAX_PAROLE - 1) {
        while (*p == ' ' || *p == '\t' || *p == '\n')
            p++;
        if (!*p)
            break;
        parole[n++] = spazio;
        if (*p == '|' || *p == '<') {
            *spazio++ = *p++;
        } else if (*p == '>') {
            *spazio++ = *p++;
            if (*p == '>')
                *spazio++ = *p++;
        } else {
            while (*p && !strchr(" \t\n|<>", *p))
                *spazio++ = *p++;
        }
        *spazio++ = '\0';
    }
    parole[n] = NULL;
    return n;
}

static int e_simbolo(const char *parola) {
    return strcmp(parola, "|") == 0 || strcmp(parola, "<") == 0
        || strcmp(parola, ">") == 0 || strcmp(parola, ">>") == 0;
}

// Trova il percorso del programma: se il nome contiene '/', e' gia' un percorso;
// altrimenti lo cerca prima tra i programmi di sistema, poi tra quelli dell'utente.
// L'ordine conta: un programma dell'utente non puo' prendere il posto di uno di sistema.
static int trova(const char *nome, char *percorso, size_t dim) {
    if (strchr(nome, '/')) {
        snprintf(percorso, dim, "%s", nome);
        return access(percorso, X_OK);
    }
    snprintf(percorso, dim, "%s/%s", CARTELLA_PROGRAMMI, nome);
    if (access(percorso, X_OK) == 0)
        return 0;
    snprintf(percorso, dim, "%s/%s", CARTELLA_PROGRAMMI_UTENTE, nome);
    return access(percorso, X_OK);
}

// Un comando di una catena "a | b | c", con le sue redirezioni.
struct passo {
    char *argomenti[MAX_PAROLE];
    const char *entrata;        // "< file": legge da qui invece che dalla tastiera
    const char *uscita;         // "> file" o ">> file": scrive qui invece che sullo schermo
    int in_fondo;               // 1 = ">>" (aggiunge in fondo), 0 = ">" (riscrive)
};

// Divide le parole nei passi della catena. Restituisce il numero di passi, -1 se la riga e' sbagliata.
static int componi(char *parole[], struct passo passi[]) {
    int n = 0, k = 0;
    memset(&passi[0], 0, sizeof passi[0]);
    for (int i = 0; parole[i]; i++) {
        char *w = parole[i];
        if (strcmp(w, "|") == 0) {
            if (k == 0 || !parole[i + 1])
                return -1;                     // "| a" oppure "a |": manca un comando
            passi[n].argomenti[k] = NULL;
            n++;
            k = 0;
            memset(&passi[n], 0, sizeof passi[n]);
        } else if (e_simbolo(w)) {
            char *file = parole[i + 1];
            if (!file || e_simbolo(file))
                return -1;                     // "> " senza nome del file
            if (w[0] == '<') {
                passi[n].entrata = file;
            } else {
                passi[n].uscita = file;
                passi[n].in_fondo = w[1] == '>';
            }
            i++;
        } else {
            passi[n].argomenti[k++] = w;
        }
    }
    passi[n].argomenti[k] = NULL;
    return n + 1;
}

// Nel processo figlio: collega entrata e uscita ai file indicati.
static void redirigi(const struct passo *p) {
    if (p->entrata) {
        int fd = open(p->entrata, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "mshell: %s: %s\n", p->entrata, strerror(errno));
            _exit(1);
        }
        dup2(fd, 0);
        close(fd);
    }
    if (p->uscita) {
        int modo = O_WRONLY | O_CREAT | (p->in_fondo ? O_APPEND : O_TRUNC);
        int fd = open(p->uscita, modo, 0644);
        if (fd < 0) {
            fprintf(stderr, "mshell: %s: %s\n", p->uscita, strerror(errno));
            _exit(1);
        }
        dup2(fd, 1);
        close(fd);
    }
}

// Esegue la catena: un processo per passo, collegati da tubi (pipe).
// L'uscita del passo i entra nel passo i+1. La shell aspetta che finiscano tutti.
static void esegui(struct passo passi[], int n) {
    pid_t figli[MAX_PAROLE];
    int precedente = -1;                       // capo di lettura del tubo dal passo prima
    for (int i = 0; i < n; i++) {
        int tubo[2] = { -1, -1 };
        if (i + 1 < n && pipe(tubo) != 0) {
            printf("mshell: pipe fallita: %s\n", strerror(errno));
            n = i;
            break;
        }
        figli[i] = fork();
        if (figli[i] == 0) {
            signal(SIGINT, SIG_DFL);           // il programma si puo' fermare con Ctrl+C
            signal(SIGQUIT, SIG_DFL);
            if (precedente >= 0) {
                dup2(precedente, 0);
                close(precedente);
            }
            if (tubo[1] >= 0) {
                dup2(tubo[1], 1);
                close(tubo[0]);
                close(tubo[1]);
            }
            redirigi(&passi[i]);               // i file vincono sui tubi, come nelle shell Linux
            if (!passi[i].argomenti[0])
                _exit(0);                      // solo "> file": il file e' stato creato (vuoto)
            char percorso[512];
            if (trova(passi[i].argomenti[0], percorso, sizeof percorso) != 0) {
                fprintf(stderr, "mshell: comando non trovato: %s\n", passi[i].argomenti[0]);
                _exit(127);
            }
            execve(percorso, passi[i].argomenti, environ);
            fprintf(stderr, "mshell: impossibile eseguire %s: %s\n", percorso, strerror(errno));
            _exit(127);
        }
        if (precedente >= 0)
            close(precedente);
        if (tubo[1] >= 0)
            close(tubo[1]);
        precedente = tubo[0];
        if (figli[i] < 0) {
            printf("mshell: fork fallito: %s\n", strerror(errno));
            n = i;
            break;
        }
    }
    if (precedente >= 0)
        close(precedente);

    for (int i = 0; i < n; i++) {
        int stato;
        while (waitpid(figli[i], &stato, 0) < 0 && errno == EINTR)
            ;
        if (i == n - 1 && WIFSIGNALED(stato) && passi[i].argomenti[0])
            printf("\nmshell: %s interrotto dal segnale %d\n", passi[i].argomenti[0], WTERMSIG(stato));
    }
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

    char riga[1024], spazio[2048];
    char *parole[MAX_PAROLE];
    struct passo passi[MAX_PAROLE];
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
        if (spezza(riga, parole, spazio) == 0)
            continue;
        int n = componi(parole, passi);
        if (n < 0) {
            printf("mshell: riga sbagliata: manca un comando o il nome di un file\n");
            continue;
        }

        // I comandi interni agiscono sulla shell stessa: niente catene ne' redirezioni.
        const struct comando *interno = passi[0].argomenti[0] ? comando_interno(passi[0].argomenti[0]) : NULL;
        if (interno) {
            if (n > 1 || passi[0].entrata || passi[0].uscita) {
                printf("mshell: %s non si usa con | < > >>\n", passi[0].argomenti[0]);
                continue;
            }
            if (interno->esegui(passi[0].argomenti) == ESCI)
                break;
            continue;
        }
        esegui(passi, n);
    }
    return 0;
}
