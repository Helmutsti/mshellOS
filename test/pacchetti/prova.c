/* prova.c: le prove dei pacchetti di CoreKit (utente/libreria/corekit/interno/pacchetto.c), sul
 * computer che compila, dentro il container di Alpine (vedi prova.sh).
 *
 * Le cartelle delle app e delle persone si spostano in /tmp/prova (cartelle_applicazioni e
 * cartella_persone sono variabili apposta); i pacchetti li prepara fai_pacchetti.py. Per
 * ogni caso si controlla il resoconto (installabile? quali voci hanno errori o avvisi?) e,
 * per le installazioni, cosa c'e' sul disco. Alla fine: quante prove riuscite e fallite. */
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "corekit/sistema/pacchetto.h"

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

/* Quante voci nella cartella cominciano con 'inizio' ("" per tutte, tranne "." e "..")?
 * (Senza system: in prova_togli SIGCHLD e' ignorato, e system non saprebbe l'esito.) */
static int voci(const char *p, const char *inizio) {
    DIR *d = opendir(p);
    if (!d) return -1;
    int n = 0;
    struct dirent *v;
    while ((v = readdir(d)))
        if (strcmp(v->d_name, ".") && strcmp(v->d_name, "..") && !strncmp(v->d_name, inizio, strlen(inizio))) n++;
    closedir(d);
    return n;
}

static int vuota(const char *p) { return voci(p, "") == 0; }

/* La cartella dei dati: della persona (1000), 700. */
static int cartella_dati_giusta(const char *p) {
    struct stat s;
    return lstat(p, &s) == 0 && S_ISDIR(s.st_mode) && (s.st_mode & 07777) == 0700 && s.st_uid == 1000;
}

#define DATI RADICE "/data/users/1000/appsdata"

/* Togliere un'app e svuotarne i dati (pacchetto_togli, pacchetto_svuota_dati). Prima c'e'
 * "prova" 1.1.0 installata, con il file "punteggio" nei suoi dati. */
static void prova_togli(void) {
    char msg[MAX_MESSAGGIO_TOGLI];
    /* i dati si cancellano con l'identita' della persona: le sue cartelle devono essere sue */
    system("chown 1000:1000 " RADICE "/data/users/1000 " DATI);
    /* come nel launcher (corekit.c): i figli finiti spariscono da soli */
    signal(SIGCHLD, SIG_IGN);

    /* percorsi pericolosi: rifiutati, niente cancellato */
    controlla(pacchetto_togli("../apps/prova", 0, msg, sizeof msg) == TOGLI_ID_SBAGLIATO, "togliere '../apps/prova': no");
    controlla(pacchetto_togli("..", 0, msg, sizeof msg) == TOGLI_ID_SBAGLIATO, "togliere '..': no");
    controlla(pacchetto_togli("prova/code", 0, msg, sizeof msg) == TOGLI_ID_SBAGLIATO, "togliere 'prova/code': no");
    controlla(pacchetto_togli("", 0, msg, sizeof msg) == TOGLI_ID_SBAGLIATO, "togliere '': no");
    controlla(pacchetto_svuota_dati("../1000", msg, sizeof msg) == TOGLI_ID_SBAGLIATO, "svuotare '../1000': no");
    controlla(pacchetto_svuota_dati("/data", msg, sizeof msg) == TOGLI_ID_SBAGLIATO, "svuotare '/data': no");
    controlla(esiste(RADICE "/data/apps/prova/code") && esiste(DATI "/prova/punteggio"), "dopo i rifiuti non manca niente");

    /* un'app del sistema non si toglie; una che non c'e' neanche */
    controlla(pacchetto_togli("files", 0, msg, sizeof msg) == TOGLI_DEL_SISTEMA && esiste(RADICE "/system/apps/files"),
              "togliere un'app del sistema: no");
    printf("    %s\n", msg);
    controlla(pacchetto_togli("nessuna", 0, msg, sizeof msg) == TOGLI_NON_INSTALLATA, "togliere un'app che non c'e': no");
    controlla(pacchetto_svuota_dati("nessuna", msg, sizeof msg) == TOGLI_NON_INSTALLATA, "svuotare un'app che non c'e': no");

    /* accesa: no. Il suo programma e' una copia di questa prova, che dorme */
    char questa[512], comando[1200];
    ssize_t k = readlink("/proc/self/exe", questa, sizeof questa - 1);   /* questa prova (non la shell di system) */
    questa[k > 0 ? k : 0] = 0;
    snprintf(comando, sizeof comando, "cp '%s' " RADICE "/data/apps/prova/code && chmod 755 " RADICE "/data/apps/prova/code",
             questa);
    system(comando);
    pid_t accesa = fork();
    if (accesa == 0) {
        execl(RADICE "/data/apps/prova/code", "code", "dormi", (char *)0);
        _exit(1);
    }
    usleep(300000);
    controlla(pacchetto_in_uso(RADICE "/data/apps/prova/code"), "l'app accesa si vede");
    controlla(pacchetto_togli("prova", 0, msg, sizeof msg) == TOGLI_IN_USO && esiste(RADICE "/data/apps/prova/code"),
              "togliere un'app accesa: no");
    printf("    %s\n", msg);
    controlla(pacchetto_svuota_dati("prova", msg, sizeof msg) == TOGLI_IN_USO && esiste(DATI "/prova/punteggio"),
              "svuotare i dati di un'app accesa: no");
    kill(accesa, SIGKILL);
    waitpid(accesa, 0, 0);
    controlla(!pacchetto_in_uso(RADICE "/data/apps/prova/code"), "spenta, non si vede piu'");

    /* svuotare: dentro non resta niente, la cartella si'; un collegamento che punta fuori si
     * toglie, ma quello a cui punta resta */
    system("mkdir -p " RADICE "/fuori && echo tieni > " RADICE "/fuori/importante && "
           "mkdir -p " DATI "/prova/sotto && echo x > " DATI "/prova/sotto/file && "
           "ln -s " RADICE "/fuori " DATI "/prova/collegamento && chmod 755 " DATI "/prova && "
           "chown -R -h 1000:1000 " DATI "/prova");
    controlla(pacchetto_svuota_dati("prova", msg, sizeof msg) == TOGLI_FATTO, "svuotare i dati di prova: si'");
    controlla(cartella_dati_giusta(DATI "/prova") && vuota(DATI "/prova"), "la cartella dei dati resta, vuota, sua, 700");
    controlla(esiste(RADICE "/fuori/importante"), "svuotare non segue i collegamenti");
    controlla(esiste(RADICE "/data/apps/prova/code"), "svuotare non toglie l'app");

    /* svuotare i dati di un'app del sistema: si' */
    system("mkdir -p " DATI "/files && echo x > " DATI "/files/preferiti && chmod 700 " DATI "/files && "
           "chown -R 1000:1000 " DATI "/files");
    controlla(pacchetto_svuota_dati("files", msg, sizeof msg) == TOGLI_FATTO && cartella_dati_giusta(DATI "/files") &&
              vuota(DATI "/files"), "svuotare i dati di un'app del sistema: si'");

    /* togliere tenendo i dati: l'app sparisce, i dati restano, e reinstallandola si ritrovano */
    system("echo record > " DATI "/prova/punteggio && chown 1000:1000 " DATI "/prova/punteggio");
    controlla(pacchetto_togli("prova", 1, msg, sizeof msg) == TOGLI_FATTO, "togliere prova tenendo i dati: si'");
    printf("    %s\n", msg);
    controlla(!esiste(RADICE "/data/apps/prova") && esiste(DATI "/prova/punteggio"), "l'app non c'e' piu', i dati si'");
    controlla(fai("buono.app", PACCHETTO_INSTALLA) == 0 && esiste(DATI "/prova/punteggio"),
              "reinstallata, ritrova i suoi dati");

    /* togliere con i dati: non resta niente, neanche le cartelle nascoste */
    system("ln -s " RADICE "/fuori " DATI "/prova/collegamento && chown -h 1000:1000 " DATI "/prova/collegamento");
    controlla(pacchetto_togli("prova", 0, msg, sizeof msg) == TOGLI_FATTO, "togliere prova con i dati: si'");
    controlla(!esiste(RADICE "/data/apps/prova") && !esiste(DATI "/prova"), "non c'e' piu' ne' l'app ne' i dati");
    controlla(esiste(RADICE "/fuori/importante"), "togliere non segue i collegamenti");
    controlla(voci(RADICE "/data/apps", ".togliere") == 0, "niente cartelle nascoste rimaste");
    signal(SIGCHLD, SIG_DFL);
    controlla(pacchetto_togli("prova", 0, msg, sizeof msg) == TOGLI_NON_INSTALLATA, "togliere di nuovo: non c'e' piu'");
}

int main(int quanti, char **argomenti) {
    if (quanti == 2 && !strcmp(argomenti[1], "dormi")) { sleep(30); return 0; }   /* l'app "accesa" */
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

    prova_togli();

    /* i pacchetti veri fatti da build.sh (se c'e' la cartella): devono essere tutti
     * installabili, su un sistema senza app (cosi' gli id non sono gia' presi) */
    if (quanti > 2) {
        system("mkdir -p " RADICE "/vuoto/system " RADICE "/vuoto/data");
        cartelle_applicazioni[0] = RADICE "/vuoto/system";
        cartelle_applicazioni[1] = RADICE "/vuoto/data";
        pacchetti = argomenti[2];
        static const char *const veri[] = { "attivita", "contatore", "doom", "esempio", "files", "impostazioni", "store" };
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
