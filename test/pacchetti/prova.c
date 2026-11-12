/* prova.c: le prove della libreria dei pacchetti (utente/libreria/pacchetto.c), sul
 * computer che compila, dentro il container di Alpine (vedi prova.sh).
 *
 * Le cartelle delle app e delle persone si spostano in /tmp/prova (cartelle_applicazioni e
 * cartella_persone sono variabili apposta); i pacchetti li prepara fai_pacchetti.py. Per
 * ogni caso si controlla il resoconto (installabile? quali voci hanno errori o avvisi?) e,
 * per le installazioni, cosa c'e' sul disco. Alla fine: quante prove riuscite e fallite. */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "pacchetto.h"

#define RADICE "/tmp/prova"
static const char *pacchetti;
static int riuscite, fallite;

static void controlla(int vero, const char *cosa) {
    if (vero) riuscite++;
    else { fallite++; printf("  FALLITA: %s\n", cosa); }
}

static int apri_pacchetto(const char *nome) {
    char p[512];
    snprintf(p, sizeof p, "%s/%s", pacchetti, nome);
    int f = open(p, O_RDONLY);
    if (f < 0) { printf("manca %s\n", p); exit(2); }
    return f;
}

static void mostra(const char *nome, const struct resoconto *r) {
    printf("%s: %s (%d errori, %d avvisi)\n", nome, r->installabile ? "si'" : "no", r->quanti_errori, r->quanti_avvisi);
    for (int i = 0; i < r->quanti; i++)
        printf("    %s [%s] %s\n", r->problemi[i].blocca ? "ERRORE" : "avviso", r->problemi[i].voce, r->problemi[i].testo);
}

/* Un problema con quella voce (e quella gravita') c'e'? */
static int ha(const struct resoconto *r, int blocca, const char *voce) {
    for (int i = 0; i < r->quanti; i++)
        if (r->problemi[i].blocca == blocca && !strcmp(r->problemi[i].voce, voce)) return 1;
    return 0;
}

static struct resoconto r;

static int fai(const char *nome, int cosa) {
    int f = apri_pacchetto(nome);
    int esito = cosa == PACCHETTO_DESCRIVI ? pacchetto_descrivi(f, cosa, &r) : pacchetto_installa(f, cosa, &r);
    close(f);
    mostra(nome, &r);
    return esito;
}

/* Il pacchetto non si deve installare, con un errore su 'voce'. */
static void rifiutato(const char *nome, const char *voce) {
    char cosa[128];
    int esito = fai(nome, PACCHETTO_INSTALLA);
    snprintf(cosa, sizeof cosa, "%s: rifiutato con errore '%s'", nome, voce);
    controlla(esito < 0 && !r.installabile && ha(&r, 1, voce), cosa);
}

static int permessi(const char *percorso, unsigned int attesi, int cartella) {
    struct stat s;
    if (lstat(percorso, &s) < 0) return 0;
    return (s.st_mode & 07777) == attesi && s.st_uid == 0 && s.st_gid == 0 &&
           (cartella ? S_ISDIR(s.st_mode) : S_ISREG(s.st_mode));
}

static int esiste(const char *p) { struct stat s; return lstat(p, &s) == 0; }

int main(int quanti, char **argomenti) {
    if (quanti < 2) { printf("uso: prova <cartella dei pacchetti>\n"); return 2; }
    pacchetti = argomenti[1];
    system("rm -rf " RADICE " && mkdir -p " RADICE "/system/apps/files " RADICE "/system/apps/store "
           RADICE "/data/apps " RADICE "/data/users/1000/appsdata");
    cartelle_applicazioni[0] = RADICE "/system/apps";
    cartelle_applicazioni[1] = RADICE "/data/apps";
    cartella_persone = RADICE "/data/users";
    /* due app del sistema: files, e store che puo' gestire le app */
    FILE *m = fopen(RADICE "/system/apps/files/manifest.toml", "w");
    fprintf(m, "id = \"files\"\nnome = \"Files\"\nversione = \"1.0.0\"\nprocessore = \"" PROCESSORE_QUI "\"\n"
               "sistema_minimo = 1\nprogramma = \"code\"\npermessi = [\"file.scrivere\"]\n");
    fclose(m);
    m = fopen(RADICE "/system/apps/store/manifest.toml", "w");
    fprintf(m, "id = \"store\"\nnome = \"Store\"\nversione = \"1.0.0\"\nprocessore = \"" PROCESSORE_QUI "\"\n"
               "sistema_minimo = 1\nprogramma = \"code\"\npermessi = [\"app.gestire\"]\n");
    fclose(m);

    /* descrivere un pacchetto buono: si', con solo l'avviso delle PNG del tema scuro */
    controlla(fai("buono.app", PACCHETTO_DESCRIVI) == 0 && r.quanti_errori == 0 && r.quanti_avvisi == 1 &&
              ha(&r, 0, "icone") && r.svg_presente[0] && r.svg_presente[1] && !strcmp(r.m.id, "prova") &&
              r.m.icone[0].quante_png == 2 && r.png_presente[0][1],
              "buono.app: descritto, installabile, un avviso sulle PNG");

    /* installarlo */
    controlla(fai("buono.app", PACCHETTO_INSTALLA) == 0, "buono.app: installato");
    controlla(permessi(RADICE "/data/apps/prova", 0755, 1), "cartella dell'app: del sistema, 755");
    controlla(permessi(RADICE "/data/apps/prova/code", 0755, 0), "programma: del sistema, 755");
    controlla(permessi(RADICE "/data/apps/prova/manifest.toml", 0644, 0), "manifesto: del sistema, 644");
    controlla(permessi(RADICE "/data/apps/prova/resources", 0755, 1), "resources: 755");
    controlla(permessi(RADICE "/data/apps/prova/resources/icona-chiara.svg", 0644, 0), "icona: 644");
    controlla(esiste(RADICE "/data/users/1000/appsdata/prova"), "la cartella dei dati della persona c'e'");
    struct descrizione d;
    controlla(leggi_descrizione("prova", &d) == 0 && !d.di_sistema &&
              !strcmp(d.programma, RADICE "/data/apps/prova/code"), "leggi_descrizione trova l'app installata");

    /* i dati della persona non si toccano */
    system("echo record > " RADICE "/data/users/1000/appsdata/prova/punteggio");

    rifiutato("buono.app", "versione");                   /* gia' installata */
    controlla(fai("buono.app", PACCHETTO_AGGIORNA) < 0 && ha(&r, 1, "versione"), "aggiornare alla stessa versione: no");
    controlla(fai("buono-0.9.app", PACCHETTO_AGGIORNA) < 0 && ha(&r, 1, "versione"), "aggiornare a una piu' vecchia: no");
    controlla(fai("buono-1.1.app", PACCHETTO_DESCRIVI) == 0 && r.gia_installata &&
              !strcmp(r.versione_installata, "1.0.0"), "descrivere l'aggiornamento: si', con la versione installata");
    controlla(fai("buono-1.1.app", PACCHETTO_AGGIORNA) == 0, "aggiornare a 1.1.0: si'");
    struct manifesto nuovo;
    controlla(manifesto_dell_app(RADICE "/data/apps/prova", &nuovo) == 0 && !strcmp(nuovo.versione, "1.1.0"),
              "dopo l'aggiornamento c'e' la 1.1.0");
    controlla(esiste(RADICE "/data/users/1000/appsdata/prova/punteggio"), "i dati della persona sono rimasti");
    controlla(system("ls -a " RADICE "/data/apps | grep -q '^\\.nuova\\|^\\.vecchia'") != 0,
              "niente cartelle temporanee rimaste");

    /* gli errori che bloccano */
    rifiutato("slip.app", "sicurezza");
    rifiutato("slip2.app", "sicurezza");
    rifiutato("assoluto.app", "sicurezza");
    rifiutato("collegamento.app", "sicurezza");
    controlla(!esiste(RADICE "/data/apps/fuori.txt") && !esiste(RADICE "/data/fuori.txt") &&
              !esiste("/tmp/assoluto.txt"), "zip slip: nessun file fuori posto");
    rifiutato("arm.app", "processore");
    rifiutato("futuro.app", "sistema_minimo");
    rifiutato("sistema.app", "id");
    rifiutato("permesso-ignoto.app", "permessi");
    rifiutato("deviazione-ignota.app", "deviazioni");
    rifiutato("funzione-ignota.app", "funzioni");
    rifiutato("senza-manifesto.app", "manifesto");
    rifiutato("toml-rotto.app", "manifesto");
    rifiutato("voci-mancanti.app", "processore");
    controlla(ha(&r, 1, "versione") && ha(&r, 1, "sistema_minimo") && ha(&r, 1, "programma"),
              "voci-mancanti.app: un errore per ogni voce che manca");
    rifiutato("id-sbagliato.app", "id");
    rifiutato("tipo-sbagliato.app", "sistema_minimo");
    rifiutato("senza-programma.app", "programma");
    rifiutato("non-zip.app", "pacchetto");
    controlla(fai("buono-1.1.app", 5) < 0, "un'operazione sconosciuta: no");

    /* gli avvisi: si installa lo stesso */
    controlla(fai("permesso-sistema.app", PACCHETTO_INSTALLA) == 0 && ha(&r, 0, "permessi") && ha(&r, 0, "deviazioni"),
              "permesso di sistema e deviazione: avvisi, installata");
    controlla(fai("senza-scura.app", PACCHETTO_INSTALLA) == 0 && ha(&r, 0, "icone"), "senza icona scura: avviso, installata");
    controlla(fai("senza-icone.app", PACCHETTO_INSTALLA) == 0 && r.quanti_avvisi == 1, "senza icone: un avviso, installata");

    /* l'elenco: prima il sistema (files, store), poi le installate in ordine */
    static struct elenco_app e;
    int n = pacchetto_elenco(&e);
    for (int i = 0; i < n; i++) printf("elenco: %s %s %s\n", e.app[i].di_sistema ? "sistema" : "dati   ", e.app[i].m.id, e.app[i].m.versione);
    controlla(n == 6 && e.app[0].di_sistema && !strcmp(e.app[0].m.id, "files") && !strcmp(e.app[1].m.id, "store") &&
              !e.app[2].di_sistema && !strcmp(e.app[2].m.id, "prova") && !strcmp(e.app[5].m.id, "prova-sis"),
              "elenco delle app: sistema prima, poi le installate");

    /* chi puo' chiedere di installare */
    system("touch " RADICE "/system/apps/store/code " RADICE "/system/apps/files/code");
    controlla(pacchetto_puo_gestire(RADICE "/system/apps/store/code"), "store (app.gestire) puo' installare");
    controlla(!pacchetto_puo_gestire(RADICE "/system/apps/files/code"), "files non puo' installare");
    controlla(!pacchetto_puo_gestire(RADICE "/data/apps/prova/code"), "un'app installata non puo' installare");

    /* i pacchetti veri fatti da build.sh (se c'e' la cartella): devono essere tutti
     * installabili, su un sistema senza app (cosi' gli id non sono gia' presi) */
    if (quanti > 2) {
        system("mkdir -p " RADICE "/vuoto/system " RADICE "/vuoto/data");
        cartelle_applicazioni[0] = RADICE "/vuoto/system";
        cartelle_applicazioni[1] = RADICE "/vuoto/data";
        pacchetti = argomenti[2];
        static const char *const veri[] = { "attivita", "contatore", "doom", "esempio", "files", "impostazioni" };
        for (unsigned int i = 0; i < sizeof veri / sizeof veri[0]; i++) {
            char nome[64], cosa[128];
            snprintf(nome, sizeof nome, "%s.app", veri[i]);
            snprintf(cosa, sizeof cosa, "%s (di build.sh): installabile, con l'id giusto", nome);
            controlla(fai(nome, PACCHETTO_DESCRIVI) == 0 && !strcmp(r.m.id, veri[i]) && r.svg_presente[0] &&
                      r.svg_presente[1] && !strcmp(r.m.processore, PROCESSORE_QUI), cosa);
        }
    }

    printf("\nprove dei pacchetti: %d riuscite, %d fallite\n", riuscite, fallite);
    return fallite ? 1 : 0;
}
