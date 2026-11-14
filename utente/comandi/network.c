/* network: chiede al servizio di rete come sta la rete, e per chi ha il permesso la riavvia o
 * la spegne. Un cliente della porta del servizio (SISTEMA.md: "un comando network e' un altro
 * cliente: chiede alla stessa porta e scrive testo"), come "nmcli" per NetworkManager.
 *
 *     network [status]    lo stato della rete, una cosa per riga
 *     network restart     riavvia la rete e il firewall (permesso network.settings)
 *     network stop        spegne la scheda; il firewall resta (permesso network.settings)
 *     network -h          questo aiuto, con i codici di uscita
 *
 * Il contratto della porta e il significato di ogni dato sono in corekit/sistema/network.h;
 * qui si chiama rete_stato / rete_riavvia / rete_ferma e si scrive il risultato in parole.
 * Il servizio e' in sola lettura: la configurazione (indirizzo fisso, regole del firewall)
 * non si cambia da qui.
 *
 * LO STATO si scrive cosi' (le righe che non hanno niente da dire dicono "nessuno"):
 *
 *     scheda      eth0
 *     stato       accesa (collegata)
 *     modo        dhcp, prestito 23 h 12 min
 *     indirizzo   10.0.2.15/24
 *     subnet      10.0.2.0/24 (255.255.255.0)
 *     gateway     10.0.2.2
 *     dns         10.0.2.3
 *     firewall    attivo: entrata blocca, uscita permetti, inoltro blocca, ping permetti
 *
 * Con lo stato "guasta" segue una riga "perche'" con la frase del servizio.
 *
 * I CODICI DI USCITA (li dice anche "network -h"):
 *     0  fatto
 *     1  uso sbagliato
 *     2  il servizio di rete non risponde (non c'e', o la risposta non si capisce)
 *     3  permesso negato
 *     4  non si riesce a scrivere sullo standard output */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "corekit/corekit.h"
#include "corekit/sistema/network.h"

#define USCITA_FATTO     0
#define USCITA_USO       1
#define USCITA_SERVIZIO  2
#define USCITA_PERMESSO  3
#define USCITA_SCRITTURA 4

/* --- scrivere --- */

static void scrivi_tutto(int d, const char *p, unsigned int byte) {
    while (byte > 0) {
        int n = scrivi_in(d, p, byte);
        if (n <= 0) _exit(USCITA_SCRITTURA);
        p += n;
        byte -= (unsigned int)n;
    }
}

static void fuori(const char *s) { scrivi_tutto(1, s, (unsigned int)lunghezza(s)); }

/* Un messaggio d'errore sullo standard error: "network: " e il testo, poi a capo. */
static void __attribute__((noreturn)) fallisci(int uscita, const char *a, const char *b) {
    const char *pezzi[] = { "network: ", a, b, "\n" };
    for (int i = 0; i < 4; i++)
        if (pezzi[i]) scrivi_in(2, pezzi[i], (unsigned int)lunghezza(pezzi[i]));
    _exit(uscita);
}

/* Una riga dello stato: il nome in una colonna larga 12, poi il valore. */
static void riga(const char *nome, const char *valore) {
    char r[512];
    snprintf(r, sizeof r, "%-12s%s\n", nome, valore && valore[0] ? valore : "nessuno");
    fuori(r);
}

static const char aiuto[] =
    "uso: network [status | restart | stop]\n"
    "Chiede al servizio di rete come sta la rete, o la riavvia o la spegne.\n"
    "  status   lo stato della rete (anche senza niente)\n"
    "  restart  riavvia la rete e il firewall (rilegge la configurazione)\n"
    "  stop     spegne la scheda di rete; il firewall resta\n"
    "  -h       questo aiuto\n"
    "La configurazione non si cambia da qui: sta in /system/config/network/.\n"
    "Codici di uscita:\n"
    "  0  fatto\n"
    "  1  uso sbagliato\n"
    "  2  il servizio di rete non risponde\n"
    "  3  permesso negato\n"
    "  4  non si riesce a scrivere l'uscita\n";

/* --- le parole --- */

static const char *parola_stato(const char *s) {
    if (!strcmp(s, RETE_STATO_AVVIO))  return "si sta accendendo";
    if (!strcmp(s, RETE_STATO_ACCESA)) return "accesa";
    if (!strcmp(s, RETE_STATO_FERMA))  return "spenta";
    if (!strcmp(s, RETE_STATO_GUASTA)) return "guasta";
    return s;
}

/* allow / block del riassunto del firewall */
static const char *parola_regola(const char *r) {
    if (!strcmp(r, "allow")) return "permetti";
    if (!strcmp(r, "block")) return "blocca";
    return r[0] ? r : "?";
}

/* I secondi del prestito in parole: "2 g 3 h", "23 h 12 min", "4 min 10 s", "30 s". */
static void scrivi_durata(char *d, unsigned int misura, long s) {
    long g = s / 86400, h = s / 3600 % 24, m = s / 60 % 60, sec = s % 60;
    if (g > 0)      snprintf(d, misura, "%ld g %ld h", g, h);
    else if (h > 0) snprintf(d, misura, "%ld h %ld min", h, m);
    else if (m > 0) snprintf(d, misura, "%ld min %ld s", m, sec);
    else            snprintf(d, misura, "%ld s", sec);
}

/* La subnet di un indirizzo "a.b.c.d/n": la rete e la maschera, "10.0.2.0/24 (255.255.255.0)".
 * Se l'indirizzo non c'e' o non e' IPv4 con il prefisso, d resta vuoto. */
static void scrivi_subnet(char *d, unsigned int misura, const char *indirizzo) {
    char ip[RETE_MAX_TESTO];
    d[0] = 0;
    const char *barra = strchr(indirizzo, '/');
    if (!barra || barra - indirizzo >= (long)sizeof ip) return;
    memcpy(ip, indirizzo, (size_t)(barra - indirizzo));
    ip[barra - indirizzo] = 0;
    char *fine;
    long n = strtol(barra + 1, &fine, 10);
    struct in_addr a;
    if (*fine || n < 0 || n > 32 || inet_pton(AF_INET, ip, &a) != 1) return;
    unsigned int maschera = n ? 0xFFFFFFFFu << (32 - n) : 0;
    unsigned int rete = ntohl(a.s_addr) & maschera;
    snprintf(d, misura, "%u.%u.%u.%u/%ld (%u.%u.%u.%u)", rete >> 24, rete >> 16 & 255, rete >> 8 & 255,
             rete & 255, n, maschera >> 24, maschera >> 16 & 255, maschera >> 8 & 255, maschera & 255);
}

/* --- le richieste --- */

static void controlla(int esito) {
    if (esito == RETE_NON_PERMESSO) fallisci(USCITA_PERMESSO, "permesso negato (serve network.settings)", 0);
    if (esito != RETE_FATTO) fallisci(USCITA_SERVIZIO, "il servizio di rete non risponde", 0);
}

static int stato(void) {
    struct stato_rete s;
    int e = rete_stato(&s);
    if (e == RETE_NON_PERMESSO) fallisci(USCITA_PERMESSO, "permesso negato (serve network.use)", 0);
    controlla(e);

    char v[256];

    riga("scheda", s.scheda);

    snprintf(v, sizeof v, "%s (%s)", parola_stato(s.stato), s.collegata ? "collegata" : "non collegata");
    riga("stato", v);
    if (!strcmp(s.stato, RETE_STATO_GUASTA)) riga("perche'", s.frase);

    if (!strcmp(s.modo, "static")) snprintf(v, sizeof v, "indirizzo fisso");
    else if (s.prestito >= 0) {
        char d[64];
        scrivi_durata(d, sizeof d, s.prestito);
        snprintf(v, sizeof v, "%s, prestito %s", s.modo, d);
    } else if (s.prestito == RETE_PRESTITO_PER_SEMPRE) snprintf(v, sizeof v, "%s, prestito senza scadenza", s.modo);
    else snprintf(v, sizeof v, "%s", s.modo);
    riga("modo", v);

    riga("indirizzo", s.indirizzo);
    scrivi_subnet(v, sizeof v, s.indirizzo);
    riga("subnet", v);
    riga("gateway", s.gateway);

    v[0] = 0;
    for (int i = 0; i < s.quanti_dns; i++) {
        if (i) strncat(v, ", ", sizeof v - strlen(v) - 1);
        strncat(v, s.dns[i], sizeof v - strlen(v) - 1);
    }
    riga("dns", v);

    if (s.firewall_attivo)
        snprintf(v, sizeof v, "attivo: entrata %s, uscita %s, inoltro %s, ping %s", parola_regola(s.f_entrata),
                 parola_regola(s.f_uscita), parola_regola(s.f_inoltro), parola_regola(s.f_ping));
    else snprintf(v, sizeof v, "spento");
    riga("firewall", v);
    return USCITA_FATTO;
}

int main(int argc, char **argv) {
    const char *cosa = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) { fuori(aiuto); return USCITA_FATTO; }
        if (argv[i][0] == '-') fallisci(USCITA_USO, "opzione sconosciuta: ", argv[i]);
        if (cosa) fallisci(USCITA_USO, "una richiesta sola, per favore (network -h per l'aiuto)", 0);
        cosa = argv[i];
    }

    if (!cosa || !strcmp(cosa, "status")) return stato();
    if (!strcmp(cosa, "restart")) {
        controlla(rete_riavvia());
        fuori("la rete si sta riavviando\n");
        return USCITA_FATTO;
    }
    if (!strcmp(cosa, "stop")) {
        controlla(rete_ferma());
        fuori("la rete e' spenta (il firewall resta)\n");
        return USCITA_FATTO;
    }
    fallisci(USCITA_USO, "richiesta sconosciuta: ", cosa);
}
