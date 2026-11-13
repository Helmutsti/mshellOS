/* shell: una shell stupida, SOLO PER LE PROVE (provvisoria, STATO.md). Gira in due posti:
 *   - sulla SERIALE (la console del kernel): init la fa partire come sistema (uid 0) e la fa
 *     ripartire quando finisce;
 *   - nel TERMINALE (utente/programmi/terminale.c, l'icona "Shell" della scrivania), come
 *     SISTEMA (uid 0): la fa partire il launcher (APRI_SHELL), con il terminale finto (pty)
 *     del terminale come 0, 1 e 2.
 * Quale dei due lo dice il descrittore 0: se e' un terminale si usa lui, se no la seriale.
 *
 * Sa fare solo muoversi tra le cartelle e guardarle:
 *     pwd           la cartella in cui siamo
 *     cd [cartella] ci si sposta ("cd" da solo: la radice, "cd .." sale)
 *     ls [-l] [percorso]   cosa c'e' dentro; -l: tipo, permessi, proprietario, misura
 *     help          l'elenco dei comandi
 *     exit          finisce (init la rifa' partire)
 *
 * LA RIGA la legge e la corregge il kernel: la seriale e' un terminale in modo normale,
 * che fa da se' l'eco di quello che si scrive e il tasto di cancellazione (backspace); a
 * noi arriva la riga intera, quando si preme Invio. Ctrl-D, a riga vuota, e' come exit.
 * Lo stesso nel terminale della finestra (un pty, che il kernel tratta allo stesso modo).
 * La cartella in cui siamo la tiene la shell (non il kernel): i percorsi si rendono
 * assoluti qui, e poi si danno alle funzioni della libreria, che vogliono solo percorsi
 * assoluti. "/home" e' la casa di chi chiede: sulla seriale (uid 0) /data/users/0/home,
 * anche nel terminale (la shell e' sistema in tutti e due i posti). */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "corekit/corekit.h"

#ifdef __aarch64__
#define TERMINALE "/dev/ttyAMA0"
#else
#define TERMINALE "/dev/ttyS0"
#endif

#define MAX_RIGA 4200
#define MAX_PAROLE 8

static int tty = -1;                    /* il terminale: si legge e si scrive qui */
static int a_colori;                    /* nel terminale della finestra: colori e grassetto */
static char cartella[MAX_PERCORSO] = "/";

/* --- scrivere --- */

static void dici(const char *s) { scrivi_in(tty, s, (unsigned int)lunghezza(s)); }

/* I colori e il grassetto, come li dicono tutti i terminali (sequenze ANSI SGR): si mandano
 * solo al terminale della finestra, mai alla seriale. Quali colori siano davvero lo decide il
 * terminale secondo il tema: noi diciamo solo rosso, verde... ("0" ripristina). */
static void sgr(const char *codici) {
    if (!a_colori) return;
    dici("\033["); dici(codici); dici("m");
}
#define ROSSO "31"
#define VERDE "32"
#define GIALLO "33"
#define BLU "34"
#define VIOLA "35"
#define MENTA "36"
#define GRIGIO "90"
#define ARANCIO "91"
#define GRASSETTO(c) "1;" c
#define NORMALE "0"
#define SOLO(c) "0;" c                  /* il colore, senza il grassetto di prima */

static void dici_numero(unsigned long long n, int larghezza) {
    char c[24];
    int k = 0;
    do { c[k++] = (char)('0' + n % 10); n /= 10; } while (n);
    char fuori[24];
    int m = 0;
    for (int i = k; i < larghezza; i++) fuori[m++] = ' ';
    while (k) fuori[m++] = c[--k];
    fuori[m] = 0;
    dici(fuori);
}

static const char *errore_in_parole(int e, const char *non_trovato) {
    switch (e) {
    case ERRORE_NON_TROVATO:  return non_trovato;
    case ERRORE_PERMESSO:     return "permesso negato";
    case ERRORE_NON_CARTELLA: return "non e' una cartella";
    case ERRORE_NOME:         return "percorso scritto male o troppo lungo";
    case ERRORE_TROPPI_LINK:  return "troppi collegamenti simbolici";
    default:                  return "errore del disco";
    }
}

/* Un messaggio d'errore, in rosso, fino a capo. */
static void dici_errore(const char *a, const char *b, const char *c) {
    sgr(ROSSO);
    dici(a); dici(b); dici(c);
    sgr(NORMALE);
    dici("\n");
}

static void errore(const char *comando, const char *percorso, int e) {
    sgr(ROSSO);
    dici(comando); dici(": ");
    if (percorso) { dici(percorso); dici(": "); }
    dici(errore_in_parole(e, "cartella inesistente"));
    sgr(NORMALE);
    dici("\n");
}

/* --- i percorsi --- */

/* Il percorso 'p' (assoluto, o relativo alla cartella in cui siamo) in forma pulita in
 * 'fuori': niente "//", "." e "..". 0, o ERRORE_NOME se e' troppo lungo. */
static int rendi_assoluto(const char *p, char *fuori) {
    static char grezzo[2 * MAX_PERCORSO + 2];
    int n = 0;
    if (p[0] != '/') {
        for (const char *s = cartella; *s; s++) grezzo[n++] = *s;
        grezzo[n++] = '/';
    }
    for (const char *s = p; *s; s++) {
        if (n >= (int)sizeof grezzo - 1) return ERRORE_NOME;
        grezzo[n++] = *s;
    }
    grezzo[n] = 0;
    int u = 0;                                   /* quanti byte di fuori sono scritti */
    fuori[0] = 0;
    for (const char *s = grezzo; *s;) {
        while (*s == '/') s++;
        const char *inizio = s;
        while (*s && *s != '/') s++;
        int l = (int)(s - inizio);
        if (l == 0 || (l == 1 && inizio[0] == '.')) continue;
        if (l == 2 && inizio[0] == '.' && inizio[1] == '.') {
            while (u > 0 && fuori[u - 1] != '/') u--;
            if (u > 0) u--;                      /* anche la barra davanti */
            fuori[u] = 0;
            continue;
        }
        if (u + 1 + l >= MAX_PERCORSO) return ERRORE_NOME;
        fuori[u++] = '/';
        for (int i = 0; i < l; i++) fuori[u++] = inizio[i];
        fuori[u] = 0;
    }
    if (u == 0) { fuori[0] = '/'; fuori[1] = 0; }
    return 0;
}

/* --- i comandi --- */

static void comando_pwd(void) { dici(cartella); dici("\n"); }

static void comando_cd(const char *dove) {
    char nuova[MAX_PERCORSO];
    int e = rendi_assoluto(dove, nuova);
    struct info_file i;
    if (e == 0) e = info(nuova, &i);
    if (e < 0) { errore("cd", dove, e); return; }
    if (i.tipo != TIPO_CARTELLA) { errore("cd", dove, ERRORE_NON_CARTELLA); return; }
    /* si puo' entrare solo se si puo' leggere: lo dice aprirla */
    int d = apri(nuova, APRI_LEGGERE);
    if (d < 0) { errore("cd", dove, d); return; }
    chiudi(d);
    int n = 0;
    while ((cartella[n] = nuova[n]) != 0) n++;
}

static void dici_permessi(unsigned int tipo, unsigned int p) {
    char s[11];
    s[0] = tipo == TIPO_CARTELLA ? 'd' : tipo == TIPO_COLLEGAMENTO ? 'l' :
           tipo == TIPO_FILE ? '-' : '?';
    for (int i = 0; i < 3; i++) {
        unsigned int c = (p >> (6 - 3 * i)) & 7;
        s[1 + 3 * i] = c & 4 ? 'r' : '-';
        s[2 + 3 * i] = c & 2 ? 'w' : '-';
        s[3 + 3 * i] = c & 1 ? 'x' : '-';
    }
    s[10] = 0;
    dici(s);
}

/* Il proprietario (e il gruppo, che per ora e' lo stesso): "root" per l'uid 0, per gli altri
 * il numero. Largo almeno 4 caselle, a sinistra. */
static void dici_utente(unsigned int uid) {
    char c[16];
    int n = 0;
    if (uid == 0) { c[n++] = 'r'; c[n++] = 'o'; c[n++] = 'o'; c[n++] = 't'; }
    else {
        char r[12];
        int k = 0;
        do { r[k++] = (char)('0' + uid % 10); uid /= 10; } while (uid);
        while (k) c[n++] = r[--k];
    }
    while (n < 4) c[n++] = ' ';
    c[n] = 0;
    dici(c);
}

/* La parte di "ls -l" prima del nome: permessi, proprietario, gruppo, misura. */
static void riga_lunga(const struct voce_cartella *v, const char *percorso) {
    struct stat st;
    unsigned int uid = lstat(percorso, &st) == 0 ? st.st_uid : 0;
    sgr(GRIGIO);
    dici_permessi(v->tipo, v->permessi & 0777);
    sgr(NORMALE);
    dici("  ");
    sgr(ARANCIO);
    dici_utente(uid); dici("  "); dici_utente(uid);
    sgr(NORMALE);
    dici("  ");
    sgr(VIOLA);
    dici_numero(v->dimensione, 5);
    sgr(NORMALE);
    dici("  ");
}

/* Per un collegamento, in "ls -l": " -> destinazione", con la freccia grigia. */
static void dici_destinazione(const char *percorso) {
    char dove[MAX_PERCORSO];
    ssize_t n = readlink(percorso, dove, sizeof dove - 1);
    if (n <= 0) return;
    dove[n] = 0;
    dici(" ");
    sgr(GRIGIO); dici("->"); sgr(NORMALE);
    dici(" "); dici(dove);
}

/* Il nome in un elenco: cartelle blu e grassetto (e '/' sulla seriale, dove i colori non ci
 * sono), programmi (file che si possono eseguire) verdi e grassetto, collegamenti menta e
 * grassetto, gli altri file nel colore del testo. */
static void dici_nome(const char *nome, unsigned int tipo, unsigned int permessi) {
    if (tipo == TIPO_CARTELLA) sgr(GRASSETTO(BLU));
    else if (tipo == TIPO_COLLEGAMENTO) sgr(GRASSETTO(MENTA));
    else if (tipo == TIPO_FILE && (permessi & 0111)) sgr(GRASSETTO(VERDE));
    dici(nome);
    sgr(NORMALE);
    if (tipo == TIPO_CARTELLA && !a_colori) dici("/");
}

#define MAX_VOCI 512
static struct voce_cartella voci[MAX_VOCI];

static int caselle_di(const char *nome) {      /* le lettere, non i byte (UTF-8) */
    int n = 0;
    for (; *nome; nome++) if (((unsigned char)*nome & 0xC0) != 0x80) n++;
    return n;
}

/* Le larghezze delle colonne se l'elenco sta in 'righe' righe (una colonna alla volta);
 * restituisce la larghezza totale, con 2 caselle tra una colonna e l'altra. */
static int larghezze_colonne(int quante, int righe, int *largo) {
    int colonne = (quante + righe - 1) / righe, totale = 2 * (colonne - 1);
    for (int c = 0; c < colonne; c++) {
        largo[c] = 0;
        for (int r = 0; r < righe && c * righe + r < quante; r++) {
            int l = caselle_di(voci[c * righe + r].nome);
            if (l > largo[c]) largo[c] = l;
        }
        totale += largo[c];
    }
    return totale;
}

/* L'elenco nel terminale della finestra, in colonne come "ls": meno righe possibile, nella
 * larghezza del terminale. */
static void elenco_in_colonne(int quante) {
    struct winsize w = { 0, 0, 0, 0 };
    int larghezza = ioctl(tty, TIOCGWINSZ, &w) == 0 && w.ws_col > 0 ? w.ws_col : 80;
    static int largo[MAX_VOCI];
    int righe = 1;
    while (righe < quante && larghezze_colonne(quante, righe, largo) > larghezza) righe++;
    int colonne = (quante + righe - 1) / righe;
    larghezze_colonne(quante, righe, largo);
    for (int r = 0; r < righe; r++) {
        for (int c = 0; c < colonne && c * righe + r < quante; c++) {
            const struct voce_cartella *v = &voci[c * righe + r];
            dici_nome(v->nome, v->tipo, v->permessi);
            if ((c + 1) * righe + r < quante)
                for (int n = caselle_di(v->nome); n < largo[c] + 2; n++) dici(" ");
        }
        dici("\n");
    }
}

static void comando_ls(const char *dove, int lungo) {
    char percorso[MAX_PERCORSO];
    int e = rendi_assoluto(dove, percorso);
    struct info_file i;
    if (e == 0) e = info(percorso, &i);
    if (e < 0) { errore("ls", dove, e); return; }
    if (i.tipo != TIPO_CARTELLA) {       /* un file solo: si dice il suo nome */
        struct voce_cartella v = { .tipo = i.tipo, .permessi = i.permessi,
                                   .dimensione = i.dimensione, .modificato = i.modificato };
        if (lungo) riga_lunga(&v, percorso);
        dici_nome(dove, i.tipo, i.permessi); dici("\n");
        return;
    }
    int d = apri(percorso, APRI_LEGGERE);
    if (d < 0) { errore("ls", dove, d); return; }
    int quante = 0;
    while (quante < MAX_VOCI && leggi(d, &voci[quante], sizeof voci[0]) == (int)sizeof voci[0]) {
        voci[quante].nome[MAX_NOME_FILE - 1] = 0;
        quante++;
    }
    chiudi(d);
    if (a_colori && !lungo) {            /* nel terminale: in colonne */
        if (quante) elenco_in_colonne(quante);
        return;
    }
    for (int k = 0; k < quante; k++) {   /* sulla seriale, e con -l: uno per riga */
        const struct voce_cartella *v = &voci[k];
        char figlio[MAX_PERCORSO];
        int n = 0;
        for (const char *s = percorso; *s; s++) figlio[n++] = *s;
        if (n > 1) figlio[n++] = '/';
        for (const char *s = v->nome; *s && n < MAX_PERCORSO - 1; s++) figlio[n++] = *s;
        figlio[n] = 0;
        if (lungo) riga_lunga(v, figlio);
        dici_nome(v->nome, v->tipo, v->permessi);
        if (lungo && v->tipo == TIPO_COLLEGAMENTO) dici_destinazione(figlio);
        dici("\n");
    }
}

/* Una riga dell'elenco dei comandi nel terminale: il comando verde e in grassetto, largo 8
 * caselle, poi la descrizione in grigio. */
static void riga_help(const char *comando, const char *descrizione) {
    sgr(GRASSETTO(VERDE));
    dici(comando);
    sgr(NORMALE);
    for (int n = lunghezza(comando); n < 8; n++) dici(" ");
    sgr(GRIGIO);
    dici(descrizione);
    sgr(NORMALE);
    dici("\n");
}

static void comando_help(void) {
    if (a_colori) {
        riga_help("pwd", "mostra la cartella corrente");
        riga_help("cd", "entra in una cartella");
        riga_help("ls", "elenca i file della cartella");
        riga_help("ls -l", "elenca con permessi e dimensioni");
        riga_help("help", "mostra questo elenco");
        riga_help("exit", "chiude il terminale");
        return;
    }
    dici("comandi:\n"
         "  pwd                  la cartella in cui siamo\n"
         "  cd [cartella]        si sposta (da solo: la radice; cd .. sale)\n"
         "  ls [-l] [percorso]   cosa c'e' dentro (-l: tipo, permessi, proprietario, misura)\n"
         "  help                 questo elenco\n"
         "  exit                 finisce (sulla seriale init la rifa' partire)\n");
}

/* --- la riga --- */

/* Ctrl-C nel terminale: il kernel manda SIGINT a chi lo usa, cioe' a noi. Non deve farci
 * finire: interrompe solo la lettura della riga (read torna con EINTR) e si ricomincia
 * con un prompt nuovo, come fanno le shell vere. */
static void al_ctrl_c(int segnale) { (void)segnale; }


static int uguali(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

/* Un comando. Restituisce 1 per exit. */
static int esegui(char *riga) {
    char *parola[MAX_PAROLE];
    int quante = 0;
    for (char *s = riga; *s;) {
        while (*s == ' ' || *s == '\t') *s++ = 0;
        if (!*s) break;
        if (quante == MAX_PAROLE) { dici_errore("troppe parole", "", ""); return 0; }
        parola[quante++] = s;
        while (*s && *s != ' ' && *s != '\t') s++;
    }
    if (quante == 0) return 0;
    const char *c = parola[0];
    if (uguali(c, "pwd") || uguali(c, "help") || uguali(c, "exit")) {
        if (quante > 1) { dici_errore(c, ": non vuole argomenti", ""); return 0; }
        if (c[0] == 'p') comando_pwd();
        else if (c[0] == 'h') comando_help();
        else return 1;
    } else if (uguali(c, "cd")) {
        if (quante > 2) { dici_errore("cd: troppi argomenti", "", ""); return 0; }
        comando_cd(quante == 2 ? parola[1] : "/");
    } else if (uguali(c, "ls")) {
        int lungo = 0;
        const char *dove = 0;
        for (int i = 1; i < quante; i++) {
            if (uguali(parola[i], "-l")) lungo = 1;
            else if (parola[i][0] == '-' && parola[i][1]) {
                dici_errore("ls: opzione sconosciuta: ", parola[i], "");
                return 0;
            } else if (dove) { dici_errore("ls: un percorso solo", "", ""); return 0; }
            else dove = parola[i];
        }
        comando_ls(dove ? dove : ".", lungo);
    } else {
        dici_errore(c, ": comando sconosciuto (scrivi help)", "");
    }
    return 0;
}

/* I messaggi del kernel vanno sulla stessa seriale e si mescolerebbero con quello che si
 * scrive: si abbassa il livello della console, cosi' resta solo il peggio (i panic). */
static void silenzia_kernel(void) {
    int d = apri("/proc/sys/kernel/printk", APRI_SCRIVERE);
    if (d < 0) return;
    scrivi_in(d, "1\n", 2);
    chiudi(d);
}

int main(void) {
    if (isatty(0)) {
        /* il terminale della finestra: diventa il NOSTRO terminale (una sessione nuova, di
         * cui lui e' il terminale di controllo), cosi' Ctrl-C arriva a noi come SIGINT */
        tty = 0;
        a_colori = 1;
        setsid();
        ioctl(0, TIOCSCTTY, 0);
        struct sigaction sa = { .sa_handler = al_ctrl_c };   /* senza SA_RESTART: read si ferma */
        sigaction(SIGINT, &sa, 0);
    } else {
        tty = open(TERMINALE, O_RDWR | O_NOCTTY | O_CLOEXEC);
        if (tty < 0) {
            scrivi_testo("shell: non riesco ad aprire " TERMINALE "\n");
            dormi(5000);                       /* init ci fa ripartire: non a raffica */
            return 1;
        }
        silenzia_kernel();
        dici("\n");                            /* dopo i messaggi dell'avvio */
    }
    if (a_colori && getuid() == 0) {            /* il terminale, come sistema: l'avviso del disegno */
        sgr(GRASSETTO(GIALLO)); dici("Attenzione:");
        sgr(SOLO(GIALLO)); dici(" questa shell ha i permessi di super amministratore.\n");
        sgr(SOLO(GRIGIO)); dici("Scrivi help per l'elenco dei comandi.\n");
        sgr(NORMALE);
    } else {
        dici("mshell: shell di sviluppo (scrivi help)\n");
    }
    /* il prompt: '#' per il sistema, '$' per una persona, come nelle shell Unix */
    const char *prompt = getuid() == 0 ? "#" : "$";
    static char riga[MAX_RIGA];
    for (;;) {
        if (a_colori) {                         /* "root@mshell <cartella> # ", a colori */
            sgr(GRASSETTO(ROSSO)); dici(getuid() == 0 ? "root" : "utente");
            sgr(SOLO(GRIGIO)); dici("@mshell");
            sgr(NORMALE); dici(" ");
            sgr(GRASSETTO(BLU)); dici(cartella);
            sgr(NORMALE); dici(" ");
            sgr(getuid() == 0 ? GRASSETTO(ROSSO) : GRASSETTO(VERDE)); dici(prompt);
            sgr(NORMALE); dici(" ");
        } else {
            dici(cartella); dici(" "); dici(prompt); dici(" ");
        }
        int n = (int)read(tty, riga, MAX_RIGA - 1);
        if (n < 0 && errno == EINTR) { dici("\n"); continue; }   /* Ctrl-C */
        if (n <= 0) { dici("\n"); break; }      /* Ctrl-D */
        if (riga[n - 1] == '\n') n--;
        riga[n] = 0;
        if (n == MAX_RIGA - 1) { dici_errore("riga troppo lunga", "", ""); continue; }
        if (esegui(riga)) break;
    }
    return 0;
}
