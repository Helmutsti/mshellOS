/* rete: prova la rete accesa dal servizio network, e c'e' SOLO per le prove.
 *
 * Sta solo nel disco disco_rete.img (build.sh), in /data/tests: init lo fa partire
 * all'accensione. Serve QEMU con la scheda di rete (-nic user,model=virtio-net-pci, come
 * run.sh): nella sandbox Docker la rete non la gestisce il servizio.
 *
 * Fa tre cose, e scrive sulla seriale come va ognuna:
 *   1. traduce il nome example.com in un indirizzo (getaddrinfo di musl: legge i server
 *      dei nomi da /etc/resolv.conf, cioe' da /run/network/resolv.conf). Il servizio
 *      network ci mette qualche secondo ad avere l'indirizzo: si riprova per un minuto;
 *   2. apre una connessione TCP alla porta 80;
 *   3. manda una richiesta HTTP HEAD e scrive la prima riga della risposta.
 * Alla fine "rete: tutto bene", oppure "rete: fallita" con il passo. */
#include "libreria.h"
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define NOME "example.com"

static void fallita(const char *perche) {
    scrivi_testo("rete: fallita, ");
    scrivi_testo(perche);
    scrivi_testo("\n");
    for (;;) dormi(60 * 60 * 1000);   /* non finisce: init non lo fa ripartire, ma resta li' */
}

int main(void) {
    struct addrinfo chiedo, *trovati = 0;
    memset(&chiedo, 0, sizeof chiedo);
    chiedo.ai_family = AF_INET;
    chiedo.ai_socktype = SOCK_STREAM;
    int e = -1;
    for (int volta = 0; volta < 30 && e != 0; volta++) {   /* un minuto, ogni 2 secondi */
        e = getaddrinfo(NOME, "80", &chiedo, &trovati);
        if (e != 0) dormi(2000);
    }
    if (e != 0) {
        scrivi_testo("rete: " NOME " non si traduce: ");
        scrivi_testo(gai_strerror(e));
        scrivi_testo("\n");
        fallita("nome");
    }
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &((struct sockaddr_in *)trovati->ai_addr)->sin_addr, ip, sizeof ip);
    scrivi_testo("rete: " NOME " e' ");
    scrivi_testo(ip);
    scrivi_testo("\n");

    int s = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (s < 0 || connect(s, trovati->ai_addr, trovati->ai_addrlen) < 0) fallita("connessione");
    scrivi_testo("rete: connesso alla porta 80\n");

    const char richiesta[] = "HEAD / HTTP/1.0\r\nHost: " NOME "\r\nConnection: close\r\n\r\n";
    if (write(s, richiesta, sizeof richiesta - 1) != (ssize_t)(sizeof richiesta - 1)) fallita("invio");
    char risposta[512];
    int n = 0, letti;
    while (n < (int)sizeof risposta - 1 && (letti = (int)read(s, risposta + n, sizeof risposta - 1 - (size_t)n)) > 0)
        n += letti;
    close(s);
    risposta[n] = 0;
    char *fine = strstr(risposta, "\r\n");
    if (fine) *fine = 0;
    if (strncmp(risposta, "HTTP/", 5) != 0) fallita("risposta");
    scrivi_testo("rete: risposta \"");
    scrivi_testo(risposta);
    scrivi_testo("\"\n");
    scrivi_testo("rete: tutto bene\n");
    for (;;) dormi(60 * 60 * 1000);
}
