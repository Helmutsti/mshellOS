/* custode: prova che init custodisce le porte delle app, e c'e' SOLO per le prove.
 *
 * Sta solo nel disco disco_custode.img (build.sh), in /data/tests: init lo fa partire
 * all'accensione, come sistema (uid 0). Fa da "launcher finto": crea le porte di tre app finte
 * (attacca_porta, come fa il launcher), le prova, poi UCCIDE catalog e network con SIGKILL e
 * riprova le stesse porte, senza rifarle: init ha le copie dei capi "servizio" e le riconsegna ai
 * servizi quando li fa ripartire (servizi.h, "init custodisce le porte"). Le app finte:
 *   /system/apps/finta1/code   senza permessi            (porta verso catalog)
 *   /system/apps/finta2/code   settings.system           (porta verso catalog)
 *   /system/apps/finta3/code   network.use               (porta verso network)
 * Ogni passo si scrive sulla seriale ("custode: ... -> si'" o "... -> no FALLITA"); alla fine
 * "custode: tutto bene" o "custode: fallita".
 *
 * Il capo dell'app lo si mette dove il launcher lo metterebbe (D_PORTA_CATALOG, D_PORTA_NETWORK) con
 * dup2, con l'elenco delle porte nell'ambiente come lo scrive il launcher (VARIABILE_PORTE,
 * messaggi.h), e si usano le funzioni normali di CoreKit (catalog_leggi, catalog_cambia, rete_stato). */
#include "corekit/corekit.h"
#include "corekit/messaggi.h"
#include "corekit/catalog.h"
#include "corekit/sistema/servizi.h"
#include "corekit/sistema/network.h"
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fallite;

static void passo(const char *cosa, int giusto) {
    scrivi_riga("custode", cosa, giusto ? " -> si'" : " -> no FALLITA");
    if (!giusto) fallite = 1;
}

/* Il capo dell'app della porta nuova verso il servizio 'nome', per l'app 'programma' con i
 * permessi dati (0, 1 o piu'); -1 se il servizio non risponde entro 60 secondi (il sistema sta
 * ancora partendo). */
static int fai_porta(const char *nome, const char *programma, const char *permesso) {
    struct chi_chiede app;
    memset(&app, 0, sizeof app);
    app.uid = UID_APPLICAZIONI;
    app.app = 1;
    snprintf(app.programma, sizeof app.programma, "%s", programma);
    if (permesso) snprintf(app.permessi[app.quanti_permessi++], MAX_NOME_PERMESSO, "%s", permesso);
    for (int volta = 0; volta < 600; volta++) {
        int porta[2];
        if (coppia(porta) < 0) return -1;
        int fatto = attacca_porta(nome, &app, porta[1]) == 0;
        chiudi(porta[1]);
        /* il capo dell'app lo si porta in alto: i posti 4 e 6 devono restare liberi per come_app */
        int alto = fatto ? fcntl(porta[0], F_DUPFD, 20) : -1;
        chiudi(porta[0]);
        if (alto >= 0) return alto;
        dormi(100);
    }
    return -1;
}

/* Si "diventa" l'app della porta 'porta': il suo capo va al descrittore che le funzioni di
 * CoreKit guardano per primo. */
static void come_app(int porta, int posto) {
    if (porta >= 0) dup2(porta, posto);
}

/* Cosa puo' fare ognuna: 1 se risponde come deve. 'negato' = 1: cambiare il tema dev'essere negato. */
static int catalog_vale(int porta, int negato) {
    struct catalog_valori v;
    come_app(porta, D_PORTA_CATALOG);
    if (catalog_leggi(&v) != 0 || strcmp(v.tema, CATALOG_TEMA_CHIARO)) return 0;
    if (negato) return catalog_cambia(CATALOG_TEMA_SCURO, 0) == CATALOG_NEGATO;
    int scuro = catalog_cambia(CATALOG_TEMA_SCURO, 0) == 0;
    int chiaro = catalog_cambia(CATALOG_TEMA_CHIARO, 0) == 0;   /* rimesso com'era */
    return scuro && chiaro;
}

static int rete_vale(int porta) {
    static struct stato_rete s;
    come_app(porta, D_PORTA_NETWORK);
    return rete_stato(&s) == RETE_FATTO;
}

/* Riprova finche' non va (al massimo 'secondi' secondi): i servizi, dopo l'uccisione, ci mettono
 * un po' a ripartire. */
static int riprova(int (*prova)(int, int), int porta, int arg, int secondi) {
    for (int i = 0; i < secondi * 2; i++) {
        if (prova(porta, arg)) return 1;
        dormi(500);
    }
    return 0;
}
static int rete_vale2(int porta, int arg) { (void)arg; return rete_vale(porta); }

/* Uccide il processo che si chiama 'nome' (come dice /proc/<n>/comm). 1 se ne ha trovato uno. */
static int uccidi(const char *nome) {
    DIR *p = opendir("/proc");
    if (!p) return 0;
    int trovato = 0;
    struct dirent *e;
    while ((e = readdir(p))) {
        if (!isdigit((unsigned char)e->d_name[0])) continue;
        char percorso[64], comm[64];
        snprintf(percorso, sizeof percorso, "/proc/%s/comm", e->d_name);
        int d = apri(percorso, APRI_LEGGERE);
        if (d < 0) continue;
        int n = leggi(d, comm, sizeof comm - 1);
        chiudi(d);
        if (n <= 0) continue;
        comm[n] = 0;
        comm[strcspn(comm, "\n")] = 0;
        if (!strcmp(comm, nome) && kill(atoi(e->d_name), SIGKILL) == 0) trovato = 1;
    }
    closedir(p);
    return trovato;
}

int main(void) {
    /* l'elenco delle porte, come lo scrive il launcher per un'app: senza, le funzioni di CoreKit
     * chiamerebbero i servizi per nome (questo programma e' del sistema) */
    setenv(VARIABILE_PORTE, NOME_CATALOG "=4 " NOME_NETWORK "=6", 1);
    int finta1 = fai_porta(NOME_CATALOG, "/system/apps/finta1/code", 0);
    int finta2 = fai_porta(NOME_CATALOG, "/system/apps/finta2/code", "settings.system");
    int finta3 = fai_porta(NOME_NETWORK, "/system/apps/finta3/code", "network.use");
    passo("le tre porte fatte (attach a catalog e a network)", finta1 >= 0 && finta2 >= 0 && finta3 >= 0);
    if (finta1 < 0 || finta2 < 0 || finta3 < 0) { scrivi_riga("custode", "fallita", ""); return 0; }

    /* prima dell'uccisione */
    passo("finta1 ha il tema, e cambiarlo e' negato", riprova(catalog_vale, finta1, 1, 10));
    passo("finta2 ha il tema e puo' cambiarlo", riprova(catalog_vale, finta2, 0, 10));
    passo("finta3 ha lo stato della rete", riprova(rete_vale2, finta3, 0, 60));

    /* l'uccisione: init li fa ripartire e riconsegna le porte, che qui NON si rifanno */
    passo("catalog ucciso", uccidi("catalog"));
    passo("network ucciso", uccidi("network"));
    dormi(1500);   /* il tempo che init se ne accorga */

    passo("dopo la caduta finta1 ha ancora il tema, e cambiarlo e' negato", riprova(catalog_vale, finta1, 1, 30));
    passo("dopo la caduta finta2 puo' ancora cambiare il tema", riprova(catalog_vale, finta2, 0, 30));
    passo("dopo la caduta finta3 ha ancora lo stato della rete", riprova(rete_vale2, finta3, 0, 60));

    scrivi_riga("custode", fallite ? "fallita" : "tutto bene", "");
    return 0;
}
