// svcc: parla con servicenter. Manda la richiesta come una riga e stampa la risposta.
// "help" lo gestisce da se', senza disturbare servicenter.
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "mshelllib.h"

static const char *aiuto =
    "svcc: comandi per i servizi di sistema\n"
    "  svcc help             questo elenco\n"
    "  svcc list             tutti i servizi e il loro stato\n"
    "  svcc <nome> status    stato del servizio, e se e' acceso gli chiede \"ci sei?\"\n"
    "  svcc <nome> down      lo spegne (e non viene piu' riavviato)\n"
    "  svcc <nome> up        lo accende\n"
    "  svcc <nome> restart   lo spegne e lo riaccende\n";

int main(int argc, char *argv[]) {
    if (argc < 2 || strcmp(argv[1], "help") == 0) {
        fputs(aiuto, argc < 2 ? stderr : stdout);
        return argc < 2;
    }
    char riga[128] = "";
    for (int i = 1; i < argc; i++) {
        strncat(riga, argv[i], sizeof riga - strlen(riga) - 2);
        strncat(riga, i + 1 < argc ? " " : "\n", sizeof riga - strlen(riga) - 1);
    }

    int c = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un ind = { .sun_family = AF_UNIX };
    strcpy(ind.sun_path, SOCKET_SERVICENTER);
    if (c < 0 || connect(c, (struct sockaddr *)&ind, sizeof ind) != 0) {
        fprintf(stderr, "svcc: servicenter non raggiungibile: %s\n", strerror(errno));
        return 1;
    }
    scrivi_tutto(c, riga, strlen(riga));

    char buf[512];
    ssize_t n;
    while ((n = read(c, buf, sizeof buf)) > 0)
        fwrite(buf, 1, (size_t)n, stdout);
    close(c);
    return 0;
}
