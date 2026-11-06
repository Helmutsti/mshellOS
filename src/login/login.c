// login: accesso al sistema. Init lo avvia (come root) su ogni console.
// Chiede nome e password, controlla, e poi diventa il punto d'ingresso della persona
// (la sua shell). Quando la persona esce, init rimette login su quella console.
//
// Possono entrare solo le persone: l'amministratore (numero 0) e la fascia 1000-9999.
// Servizi (sotto 1000) e app (da 10000) hanno un'identita' ma non possono accedere.
//
// File degli utenti, in stile /etc/passwd di Linux. Una riga per utente:
//   nome:password:uid:gid:descrizione:cartella:programma_da_avviare
// Password: "x" = non impostata (si entra senza); altrimenti l'impronta prodotta da crypt().
// La cartella e' /system/users/<uid>: per la persona diventa la radice "/" del suo mondo, con dentro
//   /home  i suoi file (e i suoi programmi in /home/bin)
//   /bin   i programmi di sistema, agganciati in sola lettura
// L'amministratore non ha cartella: vede tutto il sistema e parte da /.
#define _GNU_SOURCE
#include <crypt.h>
#include <errno.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include "mshelllib.h"

#define PERSONE_DA  1000
#define PERSONE_A   9999

static int e_una_persona(const struct utente *u) {
    return u->uid == 0 || (u->uid >= PERSONE_DA && u->uid <= PERSONE_A);
}

// Legge una riga dalla console. Se nascosta, quello che si scrive non compare (per la password).
static int leggi_riga(const char *domanda, char *riga, size_t dim, int nascosta) {
    fputs(domanda, stdout);
    fflush(stdout);
    struct termios prima, senza_eco;
    if (nascosta) {
        tcgetattr(0, &prima);
        senza_eco = prima;
        senza_eco.c_lflag &= ~(tcflag_t)ECHO;
        tcsetattr(0, TCSANOW, &senza_eco);
    }
    char *ok = fgets(riga, (int)dim, stdin);
    if (nascosta) {
        tcsetattr(0, TCSANOW, &prima);
        putchar('\n');
    }
    if (!ok)
        return -1;
    riga[strcspn(riga, "\n")] = '\0';
    return 0;
}

// Controlla la password: confronta l'impronta di quella scritta con quella salvata.
static int password_giusta(const struct utente *u) {
    if (strcmp(u->password, "x") == 0)
        return 1;                              // non impostata: si entra senza
    char scritta[128];
    if (leggi_riga("password: ", scritta, sizeof scritta, 1) != 0)
        return 0;
    const char *impronta = crypt(scritta, u->password);
    memset(scritta, 0, sizeof scritta);
    return impronta && strcmp(impronta, u->password) == 0;
}

// Crea una cartella, se manca, con proprietario e permessi indicati.
static void crea_cartella(const char *percorso, unsigned uid, unsigned gid, mode_t permessi) {
    if (mkdir(percorso, permessi) == 0 && chown(percorso, uid, gid) != 0)
        printf("login: chown %s fallito: %s\n", percorso, strerror(errno));
}

// Fa entrare la persona: la chiude nella sua cartella (se non e' l'amministratore),
// diventa lei e avvia il suo punto d'ingresso. Se va tutto bene non ritorna.
static void entra(const struct utente *u) {
    const char *casa = "";                     // la cartella di partenza, come la vede lei
    if (u->uid != 0) {
        if (!u->cartella[0]) {
            printf("login: %s non ha una cartella\n", u->nome);
            return;
        }
        // La radice e' di root e solo attraversabile (711): la persona non puo' toglierci /bin
        // ne' vederne il contenuto; si muove solo dentro /home. /home e /home/bin sono sue.
        char home[160], bin[170];
        snprintf(home, sizeof home, "%s/home", u->cartella);
        snprintf(bin, sizeof bin, "%s/bin", home);
        crea_cartella(u->cartella, 0, 0, 0711);
        chmod(u->cartella, 0711);
        crea_cartella(home, u->uid, u->gid, 0700);
        crea_cartella(bin, u->uid, u->gid, 0700);

        struct aggancio agganci[] = { { "/system/bin", "/bin", 1 } };
        if (crea_mondo(u->cartella, agganci, 1) != 0) {
            printf("login: impossibile chiudere %s nella sua cartella: %s\n", u->nome, strerror(errno));
            _exit(1);                          // il mondo e' a meta': meglio ricominciare da capo
        }
        casa = "/home";
    }
    // diventa la persona: prima i gruppi, poi l'utente (dopo non si torna indietro)
    if (setgroups(0, NULL) != 0 || setgid(u->gid) != 0 || setuid(u->uid) != 0) {
        printf("login: impossibile diventare %s: %s\n", u->nome, strerror(errno));
        _exit(1);
    }
    if (!casa[0] || chdir(casa) != 0)
        chdir("/");

    char e_home[160], e_user[48];
    snprintf(e_home, sizeof e_home, "HOME=%s", casa);
    snprintf(e_user, sizeof e_user, "USER=%s", u->nome);
    char *ambiente[] = { e_user, "TERM=linux", NULL, NULL };
    if (casa[0])
        ambiente[2] = e_home;                  // HOME solo per chi ha una cartella
    char *argomenti[] = { (char *)u->programma, NULL };
    execve(u->programma, argomenti, ambiente);
    printf("login: impossibile avviare %s: %s\n", u->programma, strerror(errno));
    _exit(127);
}

int main(void) {
    printf("\033[H\033[2J");                  // pulisce lo schermo
    for (;;) {
        printf("\nmshell - accesso\n");
        char nome[32];
        if (leggi_riga("nome: ", nome, sizeof nome, 0) != 0)
            return 1;
        if (!nome[0])
            continue;
        struct utente u;
        // Stessa risposta per nome sconosciuto, non-persona e password sbagliata:
        // chi prova non scopre quali nomi esistono.
        int ok = leggi_utente(nome, &u) == 0 && e_una_persona(&u) && password_giusta(&u);
        if (ok)
            entra(&u);
        printf("accesso negato\n");
        sleep(2);                              // rallenta chi prova le password a raffica
    }
}
