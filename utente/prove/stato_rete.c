/* stato_rete: prova la porta di controllo del servizio network (corekit/sistema/network.h),
 * e c'e' SOLO per le prove.
 *
 * Sta solo nel disco disco_stato_rete.img (build.sh), in /data/tests: init lo fa partire
 * all'accensione, come sistema. Serve QEMU con la scheda di rete (-nic user,model=virtio-net-pci:
 * la prova test/prove/stato_rete.py la accende con RETE = True).
 *
 * Fa, e scrive sulla seriale come va ogni passo ("stato_rete: ... -> si'" o "... -> no FALLITA"):
 *   1. chiede lo stato finche' la rete non e' accesa (il servizio ci mette qualche secondo
 *      ad avere l'indirizzo: si riprova per un minuto), lo scrive e controlla che sia
 *      quello della configurazione del sistema (eth0, dhcp, firewall con incoming=block...);
 *   2. chiede stop: lo stato diventa stopped, senza indirizzo, ma il firewall resta attivo;
 *   3. chiede restart: la rete riparte da sola, e si aspetta che sia di nuovo accesa;
 *   4. una richiesta sconosciuta riceve invalid (messaggi.h, a mano: e' il contratto);
 *   5. il comando network (utente/comandi/network.c, /system/bin/network) scrive lo stato in
 *      parole, con la subnet ricavata dall'indirizzo; con la rete spenta dice "spenta"; una
 *      richiesta sbagliata esce con 1. Si lancia come farebbe la shell e si legge quello che
 *      scrive (la prova controlla le righe, non la porta, gia' provata sopra).
 * Alla fine "stato_rete: tutto bene" o "stato_rete: fallita". */
#include "corekit/corekit.h"
#include "corekit/messaggi.h"
#include "corekit/sistema/network.h"
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int fallite;

static void passo(const char *cosa, int giusto) {
    scrivi_testo("stato_rete: ");
    scrivi_testo(cosa);
    scrivi_testo(giusto ? " -> si'\n" : " -> no FALLITA\n");
    if (!giusto) fallite = 1;
}

/* Scrive lo stato, una riga per dato. */
static void scrivi_stato(const struct stato_rete *s) {
    char riga[256];
    snprintf(riga, sizeof riga, "stato_rete: stato=%s scheda=%s collegata=%s modo=%s\n", s->stato, s->scheda,
             s->collegata ? "si" : "no", s->modo);
    scrivi_testo(riga);
    snprintf(riga, sizeof riga, "stato_rete: indirizzo=%s gateway=%s dns=%s%s%s prestito=%ld\n", s->indirizzo,
             s->gateway, s->quanti_dns > 0 ? s->dns[0] : "", s->quanti_dns > 1 ? " " : "",
             s->quanti_dns > 1 ? s->dns[1] : "", s->prestito);
    scrivi_testo(riga);
    snprintf(riga, sizeof riga, "stato_rete: firewall=%s entrata=%s inoltro=%s uscita=%s ping=%s\n",
             s->firewall_attivo ? "attivo" : "spento", s->f_entrata, s->f_inoltro, s->f_uscita, s->f_ping);
    scrivi_testo(riga);
}

/* Chiede lo stato ogni secondo, fino a un minuto, finche' ha lo stato 'voluto'. */
static int aspetta_stato(struct stato_rete *s, const char *voluto) {
    for (int volta = 0; volta < 60; volta++) {
        if (rete_stato(s) == RETE_FATTO && !strcmp(s->stato, voluto)) return 1;
        dormi(1000);
    }
    return 0;
}

/* Dopo che la scheda e' stata riaccesa il collegamento (il "carrier" del kernel) arriva con un
 * piccolo ritardo, anche se l'indirizzo c'e' gia': lo stato dice la verita', e qui si aspetta. */
static int aspetta_collegata(struct stato_rete *s) {
    for (int volta = 0; volta < 20; volta++) {
        if (rete_stato(s) == RETE_FATTO && s->collegata) return 1;
        dormi(500);
    }
    return 0;
}

/* Lancia /system/bin/network con l'argomento 'arg' (0: nessuno), mette in 'uscita' quello che
 * scrive sullo standard output e torna il suo codice di uscita (-1 se non parte). */
static int comando_network(const char *arg, char *uscita, int misura) {
    int tubo[2];
    if (pipe(tubo) < 0) return -1;
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        /* init fa partire le prove con 0, 1 e 2 chiusi: il tubo puo' avere proprio quei
         * numeri. Si mette la scrittura su 1 e 2 (anche gli errori: si vedono nel registro
         * della prova) e si chiude solo quello che non e' 1 o 2. */
        int r = tubo[0], w = tubo[1];
        if (w != 1) dup2(w, 1);
        if (w != 2) dup2(w, 2);
        if (r != 1 && r != 2) close(r);
        if (w > 2) close(w);
        char *argv[] = { "network", (char *)arg, 0 };
        execv("/system/bin/network", argv);
        _exit(127);
    }
    close(tubo[1]);
    int n = 0;
    ssize_t k;
    while (n < misura - 1 && (k = read(tubo[0], uscita + n, (size_t)(misura - 1 - n))) > 0) n += (int)k;
    uscita[n] = 0;
    close(tubo[0]);
    int st;
    if (waitpid(pid, &st, 0) < 0 || !WIFEXITED(st)) return -1;
    return WEXITSTATUS(st);
}

/* Prova il comando network con la rete accesa ('accesa' 1) o spenta (0). */
static void prova_comando(int accesa) {
    char u[2048];
    int e = comando_network(accesa ? 0 : "status", u, sizeof u);
    char numero[48];
    snprintf(numero, sizeof numero, "stato_rete: network uscito con %d\n", e);
    scrivi_testo(numero);
    /* lo scrive anche sulla seriale, riga per riga, per chi legge il registro della prova */
    for (char *r = u, *a; *r; r = a ? a + 1 : r + strlen(r)) {
        a = strchr(r, '\n');
        if (a) *a = 0;
        scrivi_testo("stato_rete: network| ");
        scrivi_testo(r);
        scrivi_testo("\n");
        if (a) *a = '\n';
        if (!a) break;
    }
    if (accesa) {
        passo("il comando network esce con 0", e == 0);
        passo("network: scheda, stato e modo",
              strstr(u, "scheda      eth0\n") && strstr(u, "stato       accesa (collegata)\n") &&
                  strstr(u, "modo        dhcp, prestito "));
        passo("network: indirizzo e subnet",
              strstr(u, "indirizzo   10.0.2.15/24\n") && strstr(u, "subnet      10.0.2.0/24 (255.255.255.0)\n"));
        passo("network: gateway, dns, firewall",
              strstr(u, "gateway     10.0.2.2\n") && strstr(u, "dns         10.0.2.3\n") &&
                  strstr(u, "firewall    attivo: entrata blocca, uscita permetti, inoltro blocca, ping permetti\n"));
        char niente[64];
        passo("network con una richiesta sbagliata esce con 1", comando_network("foo", niente, sizeof niente) == 1);
    } else {
        passo("network (status) con la rete spenta esce con 0", e == 0);
        passo("network: spenta, senza indirizzo ne' subnet, firewall attivo",
              strstr(u, "stato       spenta (non collegata)\n") && strstr(u, "indirizzo   nessuno\n") &&
                  strstr(u, "subnet      nessuno\n") && strstr(u, "firewall    attivo"));
    }
}

int main(void) {
    struct stato_rete s;

    passo("la rete si accende", aspetta_stato(&s, RETE_STATO_ACCESA));
    scrivi_stato(&s);
    passo("scheda eth0, dhcp", !strcmp(s.scheda, "eth0") && !strcmp(s.modo, "dhcp"));
    passo("collegata", aspetta_collegata(&s));
    passo("ha indirizzo, gateway e DNS", s.indirizzo[0] && s.gateway[0] && s.quanti_dns > 0);
    passo("il prestito del DHCP e' contato", s.prestito > 0 || s.prestito == RETE_PRESTITO_PER_SEMPRE);
    passo("firewall attivo, ingresso bloccato, uscita libera",
          s.firewall_attivo && !strcmp(s.f_entrata, "block") && !strcmp(s.f_uscita, "allow") &&
              !strcmp(s.f_inoltro, "block") && !strcmp(s.f_ping, "allow"));

    /* una richiesta sconosciuta: invalid, con lo stesso id */
    {
        struct messaggio q, r;
        messaggio_inizia(&q);
        messaggio_aggiungi(&q, CHIAVE_RICHIESTA, "set_address");
        int d = collega(NOME_NETWORK);
        int e = d >= 0 ? chiedi(d, &q, &r) : -1;
        if (d >= 0) chiudi(d);
        passo("una richiesta sconosciuta (la configurazione non si cambia) e' invalid",
              e == 0 && messaggio_vale(&r, CHIAVE_ESITO, ESITO_NON_VALIDO));
    }

    prova_comando(1);

    passo("stop accettato", rete_ferma() == RETE_FATTO);
    passo("lo stato diventa stopped", aspetta_stato(&s, RETE_STATO_FERMA));
    scrivi_stato(&s);
    passo("spenta: niente indirizzo, niente collegamento", !s.indirizzo[0] && !s.collegata);
    passo("il firewall e' rimasto", s.firewall_attivo && !strcmp(s.f_entrata, "block"));
    prova_comando(0);

    passo("restart accettato", rete_riavvia() == RETE_FATTO);
    passo("la rete si riaccende", aspetta_stato(&s, RETE_STATO_ACCESA));
    scrivi_stato(&s);
    passo("di nuovo collegata, con indirizzo e firewall",
          aspetta_collegata(&s) && s.indirizzo[0] && s.firewall_attivo);

    scrivi_testo(fallite ? "stato_rete: fallita\n" : "stato_rete: tutto bene\n");
    for (;;) dormi(60 * 60 * 1000);
}
