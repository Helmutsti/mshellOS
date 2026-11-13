/* avvio, nella SANDBOX: il primo programma del container, al posto di utente/avvio/avvio.c.
 *
 * Nel container non c'e' niente da montare: "/" e' gia' il sistema (l'immagine Docker, in
 * sola lettura: docker run --read-only), /data e' un volume scrivibile con dentro i dati
 * dell'immagine, /run e /tmp sono in memoria (--tmpfs), /dev /proc /sys li da' Docker.
 * Qui si fa solo quello che nel disco delle prove e' gia' scritto, ma cambia da prova a
 * prova, e poi si passa la mano a /system/init, con lo stesso numero (1) e il registro al
 * posto 8, come fa avvio:
 *
 *   avvio TEMA PROVE
 *     TEMA   "chiaro" (come il disco delle prove: il file del tema non c'e') o "scuro"
 *            (si scrive /data/users/1000/preferences/tema)
 *     PROVE  "nessuna" (disco.img), "riempitivi" (disco_prove.img: 6 riempitivi in
 *            /data/tests), "pacchetti" (disco_pacchetti.img: il programma pacchetti) o
 *            "messaggi" (disco_messaggi.img: il programma messaggi)
 *
 * LE DATE DELLA CASA. In QEMU l'orologio e' fermo al 1/1/2026 e i file della casa sono del
 * 31/12/2025 alle 18:02: Files scrive "ieri, 18:02". Il container ha l'orologio vero (non
 * si puo' cambiare: e' quello del computer), quindi qui le date si spostano a IERI alle
 * 18:02, e Files scrive lo stesso. */
#define _GNU_SOURCE
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define REGISTRO 8
#define CASA "/data/users/1000/home"
#define PROVE_ORIGINE "/.sandbox/prove/"

static void di(const char *s) {
    ssize_t n = write(1, s, strlen(s));
    (void)n;
}

/* La data di ogni file e cartella dentro 'percorso' (lui compreso), senza seguire i
 * collegamenti. */
static void data_a(const char *percorso, const struct timespec quando[2]) {
    utimensat(AT_FDCWD, percorso, quando, AT_SYMLINK_NOFOLLOW);
    DIR *c = opendir(percorso);
    if (!c) return;
    for (struct dirent *v; (v = readdir(c));) {
        if (!strcmp(v->d_name, ".") || !strcmp(v->d_name, "..")) continue;
        char figlio[4096];
        if ((size_t)snprintf(figlio, sizeof figlio, "%s/%s", percorso, v->d_name) >= sizeof figlio) continue;
        if (v->d_type == DT_DIR) data_a(figlio, quando);
        else utimensat(AT_FDCWD, figlio, quando, AT_SYMLINK_NOFOLLOW);
    }
    closedir(c);
}

/* Copia un programma delle prove in /data/tests (del sistema, 755), come build.sh. */
static void metti_prova(const char *programma, const char *nome) {
    char da[256], a[256];
    snprintf(da, sizeof da, PROVE_ORIGINE "%s", programma);
    snprintf(a, sizeof a, "/data/tests/%s", nome);
    int i = open(da, O_RDONLY | O_CLOEXEC);
    int o = open(a, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0755);
    char b[65536];
    ssize_t n;
    while (i >= 0 && o >= 0 && (n = read(i, b, sizeof b)) > 0)
        if (write(o, b, (size_t)n) != n) break;
    if (i >= 0) close(i);
    if (o >= 0) {
        fchmod(o, 0755);
        close(o);
    }
}

int main(int quanti, char **argomenti) {
    const char *tema = quanti > 1 ? argomenti[1] : "chiaro";
    const char *prove = quanti > 2 ? argomenti[2] : "nessuna";
    di("avvio: sandbox (Docker): niente kernel ne' dischi nostri\n");
    di("avvio: /data montato (sandbox)\n");

    if (!strcmp(tema, "scuro")) {
        int f = open("/data/users/1000/preferences/tema", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
        if (f >= 0) {
            di("avvio: tema scuro\n");
            ssize_t n = write(f, "scuro", 5);
            (void)n;
            fchown(f, 1000, 1000);
            close(f);
        }
    }

    if (!strcmp(prove, "riempitivi") || !strcmp(prove, "pacchetti") || !strcmp(prove, "messaggi")) {
        mkdir("/data/tests", 0755);
        chmod("/data/tests", 0755);
        if (!strcmp(prove, "riempitivi"))
            for (char nome[] = "riempitivo-1"; nome[11] <= '6'; nome[11]++) metti_prova("riempitivo", nome);
        else
            metti_prova(prove, prove);      /* un programma solo, con lo stesso nome */
    }

    time_t ora = time(0);
    time_t ieri = ora - ora % 86400 - 86400 + 18 * 3600 + 2 * 60;
    struct timespec quando[2] = { { ieri, 0 }, { ieri, 0 } };
    data_a(CASA, quando);

    dup2(1, REGISTRO);
    char *init[] = { "/system/init", 0 };
    char *ambiente[] = { 0 };
    execve(init[0], init, ambiente);
    di("avvio: non riesco a far partire /system/init\n");
    return 1;
}
