/* firewall: prova il firewall messo dal servizio network, e c'e' SOLO per le prove.
 *
 * Sta nei dischi disco_firewall.img e disco_firewall_aperto.img (build.sh), in
 * /data/tests, accanto al programma rete (utente/prove/rete.c), che prova l'uscita: nome
 * tradotto e richiesta HTTP. Questo prova l'ingresso: ascolta sulla porta TCP 8080 e,
 * per ogni connessione che arriva, lo scrive sulla seriale e risponde con una riga.
 *
 * Serve QEMU con la scheda di rete e un inoltro dal PC alla porta 8080 del sistema:
 *   -nic user,model=virtio-net-pci,hostfwd=tcp:127.0.0.1:45580-:8080
 * Poi, dal PC, ci si collega a 127.0.0.1:45580. QEMU accetta sempre la connessione dal
 * lato del PC, e poi prova a raggiungere il sistema:
 *   - con il firewall (incoming=block, disco_firewall.img) il pacchetto viene scartato:
 *     qui non arriva niente, e il PC non riceve la riga;
 *   - con incoming=allow (disco_firewall_aperto.img) arriva: qui si scrive
 *     "firewall: connessione arrivata da ..." e il PC riceve la riga. */
#include "libreria.h"
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define PORTA 8080

int main(void) {
    int s = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    int si = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &si, sizeof si);
    struct sockaddr_in qui = { .sin_family = AF_INET, .sin_port = htons(PORTA) };
    if (s < 0 || bind(s, (struct sockaddr *)&qui, sizeof qui) < 0 || listen(s, 4) < 0) {
        scrivi_testo("firewall: non riesco ad ascoltare sulla porta 8080\n");
        for (;;) dormi(60 * 60 * 1000);   /* non finisce: init non lo fa ripartire */
    }
    scrivi_testo("firewall: ascolto sulla porta 8080\n");
    for (;;) {
        struct sockaddr_in da;
        socklen_t lungo = sizeof da;
        int c = accept(s, (struct sockaddr *)&da, &lungo);
        if (c < 0) continue;
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &da.sin_addr, ip, sizeof ip);
        scrivi_testo("firewall: connessione arrivata da ");
        scrivi_testo(ip);
        scrivi_testo("\n");
        const char riga[] = "mshell: connessione arrivata\n";
        if (write(c, riga, sizeof riga - 1) < 0) { /* il PC se n'e' gia' andato: niente */ }
        close(c);
    }
}
