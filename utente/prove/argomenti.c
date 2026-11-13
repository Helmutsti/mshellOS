/* argomenti: un programma che c'e' SOLO per provare la shell quando fa partire i comandi.
 *
 * Scrive sull'uscita (descrittore 1, il terminale che gli da' la shell) i suoi argomenti,
 * uno per riga e numerati, poi la cartella in cui gira (quella del kernel: getcwd) e le
 * variabili PWD e HOME che gli ha dato la shell. Poi:
 *   - con un argomento "esci3" finisce con esito 3 (la shell deve dire "uscito con 3");
 *   - con un argomento "aspetta" resta fermo finche' non riceve un segnale: Ctrl-C nel
 *     terminale deve arrivare a lui (e chiuderlo), non alla shell.
 *
 * Nel disco normale non c'e'. build.sh prepara disco_shell.img, con questo programma in
 * /data/users/0/bin/argomenti (non in /data/tests: init non deve farlo partire da solo).
 * La prova si fa a mano sulla seriale, in QEMU: si scrive nella shell, per esempio,
 *     argomenti uno "due tre"
 *     cd /system
 *     argomenti esci3
 *     argomenti aspetta      (poi Ctrl-C, e la shell deve rispondere ancora) */
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void dici(const char *s) {
    ssize_t n = write(1, s, strlen(s));
    (void)n;
}

static void dici_numero(int n) {
    char c[16];
    int k = 0;
    do { c[k++] = (char)('0' + n % 10); n /= 10; } while (n);
    char fuori[16];
    int m = 0;
    while (k) fuori[m++] = c[--k];
    fuori[m] = 0;
    dici(fuori);
}

int main(int quanti, char **argomenti) {
    int esci3 = 0, aspetta = 0;
    for (int i = 0; i < quanti; i++) {
        dici("argomento "); dici_numero(i); dici(": ["); dici(argomenti[i]); dici("]\n");
        if (i > 0 && !strcmp(argomenti[i], "esci3")) esci3 = 1;
        if (i > 0 && !strcmp(argomenti[i], "aspetta")) aspetta = 1;
    }
    char qui[PATH_MAX];
    dici("cartella: "); dici(getcwd(qui, sizeof qui) ? qui : "(sconosciuta)"); dici("\n");
    const char *pwd = getenv("PWD"), *casa = getenv("HOME");
    dici("PWD="); dici(pwd ? pwd : "(manca)"); dici("  HOME="); dici(casa ? casa : "(manca)"); dici("\n");
    if (aspetta) {
        dici("aspetto Ctrl-C\n");
        for (;;) pause();
    }
    return esci3 ? 3 : 0;
}
