/* La shell del kernel.
 *
 *   1. i tasti arrivano uno alla volta (shell_tasto) e si accumulano in una riga
 *   2. a Invio la riga si divide in parole: la prima e' il comando, le altre gli argomenti
 *   3. il comando si cerca in una tabella (nome -> funzione) e si chiama la funzione
 *
 * Per aggiungere un comando: scrivere la funzione e aggiungere una riga alla tabella. */
#include "kernel.h"
#include "io.h"
#include "gdt.h"
#include "pagine.h"
#include "kmalloc.h"
#include "timer.h"
#include "disco.h"
#include "mfs.h"
#include "compiti.h"
#include "paginazione.h"
#include "shell.h"

void saluto(void);   /* il programma in ring 3 (src/programmi/saluto.c) */

#define LUNGHEZZA_RIGA 128
#define MAX_PAROLE     8

static char riga[LUNGHEZZA_RIGA];
static int lunghezza;

/* --- piccoli aiuti sulle stringhe (il kernel non ha la libreria C) --- */

static int uguali(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

/* Da testo a numero (solo cifre decimali). -1 se non e' un numero. */
static int numero(const char *s) {
    int n = 0;
    if (!*s) return -1;
    for (; *s; s++) {
        if (*s < '0' || *s > '9') return -1;
        n = n * 10 + (*s - '0');
    }
    return n;
}

/* Da testo a indirizzo: decimale, o esadecimale se comincia con 0x. 1 se va bene. */
static int indirizzo(const char *s, unsigned int *v) {
    *v = 0;
    if (s[0] == '0' && s[1] == 'x') {
        if (!s[2]) return 0;
        for (s += 2; *s; s++) {
            int c = *s >= '0' && *s <= '9' ? *s - '0'
                  : *s >= 'a' && *s <= 'f' ? *s - 'a' + 10
                  : *s >= 'A' && *s <= 'F' ? *s - 'A' + 10 : -1;
            if (c < 0) return 0;
            *v = *v * 16 + c;
        }
        return 1;
    }
    int n = numero(s);
    if (n < 0) return 0;
    *v = n;
    return 1;
}

static unsigned char settore[SETTORE];   /* il settore letto o da scrivere */

/* Rimette insieme le parole da parole[da] in poi, separate da uno spazio.
 * Restituisce quanti byte ha scritto in 'dest' (al massimo 'max'). */
static int unisci(int n, char **parole, int da, char *dest, int max) {
    int pos = 0;
    for (int p = da; p < n; p++) {
        if (p > da && pos < max) dest[pos++] = ' ';
        for (const char *c = parole[p]; *c && pos < max; c++) dest[pos++] = *c;
    }
    return pos;
}

/* --- i comandi: ognuno riceve le parole della riga (parole[0] e' il comando) --- */

static void cmd_aiuto(int n, char **parole);

static void cmd_eco(int n, char **parole) {
    for (int i = 1; i < n; i++)
        kprintf("%s%s", parole[i], i < n - 1 ? " " : "");
    kprintf("\n");
}

static void cmd_mem(int n, char **parole) {
    (void)n; (void)parole;
    unsigned int blocchi, liberi;
    kmalloc_conta(&blocchi, &liberi);
    kprintf("Magazziniere: %u pagine libere = %u KB\n", pagine_libere(), pagine_libere() * 4);
    kprintf("kmalloc     : %u blocchi, %u KB liberi\n", blocchi, liberi / 1024);
    kprintf("Paginazione : %u tabelle (%u KB)\n", pag_tabelle(), pag_tabelle() * 4);
    if (n > 1 && uguali(parole[1], "blocchi"))
        kmalloc_mostra();
}

static void cmd_tempo(int n, char **parole) {
    (void)n; (void)parole;
    unsigned int t = timer_tick();
    kprintf("Acceso da %u secondi (%u tick a 100 Hz)\n", t / 100, t);
}

static void cmd_pulisci(int n, char **parole) {
    (void)n; (void)parole;
    console_pulisci();
}

static void cmd_gdt(int n, char **parole) {
    (void)n; (void)parole;
    gdt_mostra(1);
}

static int disco_presente;

static void cmd_disco(int n, char **parole) {
    (void)n; (void)parole;
    if (!disco_presente) { kprintf("Nessun disco collegato\n"); return; }
    kprintf("Disco: %s, %u settori da %u byte (%u KB)\n",
            disco_modello(), disco_settori(), SETTORE, disco_settori() / 2);
}

/* leggi N [inizio]: mostra 128 byte del settore N, da 'inizio' (0, 128, 256 o 384).
 * Ogni riga: posizione, 16 byte in esadecimale, gli stessi byte come testo. */
static void cmd_leggi(int n, char **parole) {
    int s = n > 1 ? numero(parole[1]) : -1;
    int da = n > 2 ? numero(parole[2]) : 0;
    if (s < 0 || da < 0 || da >= SETTORE) {
        kprintf("Uso: leggi N [inizio]   es. leggi 0, leggi 0 128\n");
        return;
    }
    da -= da % 16;   /* le righe partono a multipli di 16: cosi' non escono mai dal settore */
    if (!disco_presente || disco_leggi(s, settore) < 0) {
        kprintf("Errore nella lettura del settore %d\n", s);
        return;
    }
    for (int riga = da; riga < da + 128 && riga < SETTORE; riga += 16) {
        kprintf("%03x  ", riga);
        for (int i = 0; i < 16; i++) kprintf("%02x ", settore[riga + i]);
        kprintf(" ");
        for (int i = 0; i < 16; i++) {
            unsigned char c = settore[riga + i];
            console_putc(c >= 32 && c < 127 ? c : '.');   /* i byte non stampabili come '.' */
        }
        kprintf("\n");
    }
}

/* scrivi N testo...: scrive il testo all'inizio del settore N, il resto a zero. */
static void cmd_scrivi(int n, char **parole) {
    int s = n > 2 ? numero(parole[1]) : -1;
    if (s < 0) {
        kprintf("Uso: scrivi N testo   es. scrivi 5 ciao disco\n");
        return;
    }
    /* il settore si scrive sempre intero: si parte da tutti zeri e si copia il testo */
    memset(settore, 0, SETTORE);
    int pos = unisci(n, parole, 2, (char *)settore, SETTORE);
    if (!disco_presente || disco_scrivi(s, settore) < 0) {
        kprintf("Errore nella scrittura del settore %d\n", s);
        return;
    }
    kprintf("Scritti %d byte nel settore %d\n", pos, s);
}

/* --- il file system --- */

/* Se r e' un errore di mfs, lo stampa e restituisce 1. */
static int errore_mfs(int r, const char *percorso) {
    if (r >= 0) return 0;
    kprintf("%s: %s\n", percorso, mfs_messaggio(r));
    return 1;
}

static void cmd_formatta(int n, char **parole) {
    (void)n; (void)parole;
    if (!disco_presente) { kprintf("Nessun disco collegato\n"); return; }
    if (errore_mfs(mfs_formatta(), "formatta")) return;
    kprintf("Disco formattato con mfs: radice / vuota\n");
}

static void stampa_voce(const char *nome, int tipo, unsigned int dimensione) {
    int l = 0;
    while (nome[l]) l++;
    if (tipo == MFS_CARTELLA) {
        unsigned char vecchio = console_colore(CIANO);
        kprintf("  %s/\n", nome);
        console_colore(vecchio);
    } else {
        kprintf("  %s", nome);
        for (; l < 28; l++) kprintf(" ");
        kprintf("%u byte\n", dimensione);
    }
}

static void cmd_ls(int n, char **parole) {
    const char *percorso = n > 1 ? parole[1] : "/";
    errore_mfs(mfs_elenca(percorso, stampa_voce), percorso);
}

static void cmd_mkdir(int n, char **parole) {
    if (n < 2) { kprintf("Uso: mkdir /percorso\n"); return; }
    errore_mfs(mfs_mkdir(parole[1]), parole[1]);
}

static void cmd_crea(int n, char **parole) {
    if (n < 2) { kprintf("Uso: crea /percorso testo...\n"); return; }
    char testo[LUNGHEZZA_RIGA];
    int byte = unisci(n, parole, 2, testo, sizeof testo);
    if (errore_mfs(mfs_crea(parole[1], testo, byte), parole[1])) return;
    kprintf("Creato %s (%d byte)\n", parole[1], byte);
}

static void cmd_cat(int n, char **parole) {
    if (n < 2) { kprintf("Uso: cat /percorso\n"); return; }
    /* si legge un settore alla volta, finche' il file non finisce */
    unsigned int da = 0;
    int letti;
    char ultimo = '\n';
    while ((letti = mfs_leggi(parole[1], da, settore, SETTORE)) > 0) {
        for (int i = 0; i < letti; i++) console_putc(settore[i]);
        ultimo = settore[letti - 1];
        da += letti;
    }
    if (errore_mfs(letti, parole[1])) return;
    if (ultimo != '\n') kprintf("\n");
}

static void cmd_rm(int n, char **parole) {
    if (n < 2) { kprintf("Uso: rm /percorso\n"); return; }
    errore_mfs(mfs_rm(parole[1]), parole[1]);
}

static void cmd_spazio(int n, char **parole) {
    (void)n; (void)parole;
    unsigned int schede, settori;
    if (errore_mfs(mfs_spazio(&schede, &settori), "spazio")) return;
    kprintf("Liberi: %u schede su 128, %u settori (%u KB)\n", schede, settori, settori / 2);
}

/* --- i compiti --- */

/* Ogni contatore ha un posto nella barra di stato, scelto dal suo numero. */
#define POSTI_CONTATORI 4
static int colonna_contatore(int numero) { return 12 + (numero % POSTI_CONTATORI) * 13; }

/* Un compito di prova: conta, scrive il numero nella barra di stato e dorme. */
static void contatore(unsigned int ms) {
    int numero = compito_attuale()->numero;
    char testo[16];
    for (unsigned int n = 0;; n++) {
        ksnprintf(testo, sizeof testo, " #%d:%6u ", numero, n);
        console_stato(colonna_contatore(numero), GIALLO, testo);
        dormi(ms);
    }
}

/* Un compito "maleducato": un ciclo infinito che non dorme e non cede mai il turno.
 * Gli altri vanno avanti lo stesso: e' il timer a togliergli la CPU (preemption). */
static void ciclo(unsigned int non_usato) {
    (void)non_usato;
    int numero = compito_attuale()->numero;
    char testo[16];
    for (unsigned int n = 0;; n++) {
        if (n % 1000000 == 0) {
            ksnprintf(testo, sizeof testo, " #%d:%5uM ", numero, n / 1000000);
            console_stato(colonna_contatore(numero), ROSSO, testo);
        }
    }
}

/* Un compito che sbaglia apposta: scrive all'indirizzo che riceve.
 * Con la paginazione la CPU lo ferma, e si chiude solo lui. */
static void guasto(unsigned int dove) {
    dormi(300);   /* un attimo, cosi' la shell fa in tempo a scrivere il prompt */
    *(volatile unsigned int *)dove = 0xDEAD;
}

/* traduci INDIRIZZO: dove porta un indirizzo virtuale, e con quali permessi. */
static void cmd_traduci(int n, char **parole) {
    unsigned int virt, fis, permessi;
    if (n < 2 || !indirizzo(parole[1], &virt)) {
        kprintf("Uso: traduci INDIRIZZO   es. traduci 0xB8000, traduci 0\n");
        return;
    }
    if (!pag_traduci(virt, &fis, &permessi)) {
        kprintf("%08x -> nessuna pagina: un accesso farebbe page fault\n", virt);
        return;
    }
    kprintf("%08x -> fisico %08x   %s, %s\n", virt, fis,
            permessi & PAG_SCRITTURA ? "lettura e scrittura" : "sola lettura",
            permessi & PAG_UTENTE ? "anche ring 3" : "solo kernel");
}

/* Un compito che usa mfs a raffica: crea un file, lo rilegge, controlla che il
 * contenuto sia il suo, lo cancella, per GIRI_SCRITTORE volte. Ne partono alcuni
 * insieme: con il lucchetto non sbagliano mai; senza ("lucchetto no") si pestano i piedi. */
#define GIRI_SCRITTORE 300

static void scrittore(unsigned int non_usato) {
    (void)non_usato;
    int numero = compito_attuale()->numero;
    char percorso[32], scritto[48], letto[48], stato[16];
    int errori = 0;

    for (int giro = 1; giro <= GIRI_SCRITTORE; giro++) {
        ksnprintf(percorso, sizeof percorso, "/s%d_%d", numero, giro);
        int byte = ksnprintf(scritto, sizeof scritto, "compito %d, giro %d", numero, giro);

        if (mfs_crea(percorso, scritto, byte) < 0) errori++;
        memset(letto, 0, sizeof letto);
        if (mfs_leggi(percorso, 0, letto, sizeof letto - 1) != byte || memcmp(letto, scritto, byte))
            errori++;
        if (mfs_rm(percorso) < 0) errori++;

        ksnprintf(stato, sizeof stato, " #%d:%3d e%d ", numero, giro, errori);
        console_stato(colonna_contatore(numero), errori ? ROSSO : VERDE, stato);
    }
    kprintf("[scrittore %d: %d giri, %d errori]\n", numero, GIRI_SCRITTORE, errori);
    /* return: il compito finisce da solo (fine_compito) */
}

static void cmd_lucchetto(int n, char **parole) {
    if (n > 1 && uguali(parole[1], "si")) {
        mfs_lucchetto(1);
        kprintf("Lucchetto di mfs acceso\n");
    } else if (n > 1 && uguali(parole[1], "no")) {
        mfs_lucchetto(0);
        kprintf("Lucchetto di mfs SPENTO: solo per prova, il disco si puo' rovinare\n");
    } else {
        kprintf("Uso: lucchetto si | lucchetto no\n");
    }
}

static void cmd_compiti(int n, char **parole) {
    (void)n; (void)parole;
    compiti_elenca();
}

static void cmd_avvia(int n, char **parole) {
    if (n > 1 && uguali(parole[1], "guasto")) {
        /* avvia guasto          -> scrive all'indirizzo 0 (puntatore nullo)
         * avvia guasto codice   -> scrive sul codice del kernel (sola lettura) */
        unsigned int dove = n > 2 && uguali(parole[2], "codice") ? (unsigned int)cmd_avvia : 0;
        struct compito *c = compito_crea("guasto", guasto, dove);
        if (!c) { kprintf("Memoria finita\n"); return; }
        kprintf("Avviato il compito %d: tra poco scrivera' all'indirizzo %08x\n", c->numero, dove);
        return;
    }
    if (n > 1 && uguali(parole[1], "scrittore")) {
        /* tutti nello stesso istante, cosi' lavorano davvero insieme */
        int quanti = n > 2 ? numero(parole[2]) : 3;
        if (quanti < 1 || quanti > POSTI_CONTATORI) quanti = 3;
        kprintf("Avvio %d scrittori: %d giri ciascuno di crea, leggi, controlla, cancella\n",
                quanti, GIRI_SCRITTORE);
        for (int i = 0; i < quanti; i++)
            if (!compito_crea("scrittore", scrittore, 0)) kprintf("Memoria finita\n");
        return;
    }
    if (n > 1 && uguali(parole[1], "programma")) {
        /* il primo programma in ring 3 (src/programmi/saluto.c) */
        struct compito *c = compito_crea_programma("saluto", saluto);
        if (!c) { kprintf("Memoria finita\n"); return; }
        kprintf("Avviato il compito %d: un programma in ring 3\n", c->numero);
        return;
    }
    if (n > 1 && uguali(parole[1], "ciclo")) {
        struct compito *c = compito_crea("ciclo", ciclo, 0);
        if (!c) { kprintf("Memoria finita\n"); return; }
        kprintf("Avviato il compito %d: un ciclo infinito che non cede mai\n", c->numero);
        return;
    }
    if (n < 2 || !uguali(parole[1], "contatore")) {
        kprintf("Uso: avvia contatore [ms] | ciclo | guasto [codice] | scrittore | programma\n");
        return;
    }
    int ms = n > 2 ? numero(parole[2]) : 500;
    if (ms <= 0) { kprintf("Millisecondi non validi\n"); return; }
    struct compito *c = compito_crea("contatore", contatore, ms);
    if (!c) { kprintf("Memoria finita\n"); return; }
    kprintf("Avviato il compito %d (un passo ogni %d ms)\n", c->numero, ms);
}

static void cmd_ferma(int n, char **parole) {
    int num = n > 1 ? numero(parole[1]) : -1;
    if (num < 0) { kprintf("Uso: ferma N   (il numero lo dice: compiti)\n"); return; }
    int r = compito_ferma(num);
    if (r == -2) {
        kprintf("Il compito %d sta usando una risorsa (lucchetto): riprova tra poco\n", num);
        return;
    }
    if (r < 0) {
        kprintf("Non posso fermare il compito %d\n", num);
        return;
    }
    console_stato(colonna_contatore(num), GIALLO, "            ");   /* libera il suo posto */
    kprintf("Fermato il compito %d\n", num);
}

static void cmd_riavvia(int n, char **parole) {
    (void)n; (void)parole;
    kprintf("Riavvio...\n");
    /* Lo si chiede al controller della tastiera (8042): il comando 0xFE sulla sua
     * porta 0x64 abbassa per un attimo il filo di reset della CPU.
     * Prima si aspetta che il controller sia pronto a ricevere (bit 1 a zero). */
    while (inb(0x64) & 2) ;
    outb(0x64, 0xFE);
    for (;;) __asm__ volatile("cli; hlt");
}

/* La tabella dei comandi. */
static const struct {
    const char *nome;
    const char *descrizione;
    void (*esegui)(int n, char **parole);
} comandi[] = {
    /* generali */
    { "aiuto",    "l'elenco dei comandi; aiuto NOME: cosa fa un comando", cmd_aiuto },
    { "eco",      "ripete il testo: eco ciao mondo",               cmd_eco      },
    { "tempo",    "da quanto e' acceso il kernel",                 cmd_tempo    },
    { "pulisci",  "svuota lo schermo",                             cmd_pulisci  },
    { "riavvia",  "riavvia la macchina",                           cmd_riavvia  },
    /* memoria e CPU */
    { "mem",      "memoria libera (mem blocchi: i blocchi di kmalloc)", cmd_mem },
    { "traduci",  "indirizzo virtuale -> fisico: traduci 0xB8000", cmd_traduci  },
    { "gdt",      "le righe della GDT",                            cmd_gdt      },
    /* disco */
    { "disco",    "informazioni sul disco",                        cmd_disco    },
    { "leggi",    "mostra un settore: leggi N [inizio]",           cmd_leggi    },
    { "scrivi",   "scrive in un settore: scrivi N testo",          cmd_scrivi   },
    /* file */
    { "formatta", "scrive un mfs vuoto (CANCELLA TUTTO)",          cmd_formatta },
    { "ls",       "elenca una cartella: ls [/percorso]",           cmd_ls       },
    { "mkdir",    "crea una cartella: mkdir /percorso",            cmd_mkdir    },
    { "crea",     "crea un file: crea /percorso testo",            cmd_crea     },
    { "cat",      "mostra un file: cat /percorso",                 cmd_cat      },
    { "rm",       "cancella un file o una cartella vuota",         cmd_rm       },
    { "spazio",   "schede e settori liberi di mfs",                cmd_spazio   },
    /* compiti */
    { "compiti",  "l'elenco dei compiti e il loro stato",          cmd_compiti  },
    { "avvia",    "avvia contatore [ms] | ciclo | guasto [codice] | scrittore | programma", cmd_avvia },
    { "ferma",    "ferma un compito: ferma N",                     cmd_ferma    },
    { "lucchetto","lucchetto si|no: accende o spegne il lucchetto di mfs (prova)", cmd_lucchetto },
};
#define N_COMANDI (int)(sizeof comandi / sizeof comandi[0])

/* aiuto: i nomi in colonne. aiuto NOME: la descrizione di un comando. */
static void cmd_aiuto(int n, char **parole) {
    if (n > 1) {
        for (int i = 0; i < N_COMANDI; i++)
            if (uguali(parole[1], comandi[i].nome)) {
                kprintf("%s: %s\n", comandi[i].nome, comandi[i].descrizione);
                return;
            }
        kprintf("Non conosco il comando %s\n", parole[1]);
        return;
    }
    for (int i = 0; i < N_COMANDI; i++) {
        /* kprintf non sa allineare a sinistra: si aggiungono spazi fino a 12 caratteri */
        kprintf("  %s", comandi[i].nome);
        int l = 0;
        while (comandi[i].nome[l]) l++;
        for (; l < 12; l++) kprintf(" ");
        if (i % 5 == 4 || i == N_COMANDI - 1) kprintf("\n");
    }
    kprintf("Scrivi aiuto NOME per sapere cosa fa un comando.\n");
}

/* --- leggere ed eseguire la riga --- */

static void prompt(void) {
    unsigned char vecchio = console_colore(VERDE);
    kprintf("mkernel> ");
    console_colore(vecchio);
}

/* Divide la riga in parole, sul posto: mette uno 0 al posto di ogni spazio e
 * si ricorda dove comincia ogni parola. Restituisce quante parole ha trovato. */
static int dividi(char *s, char **parole) {
    int n = 0;
    while (*s) {
        while (*s == ' ') *s++ = 0;          /* salta gli spazi */
        if (!*s) break;
        if (n == MAX_PAROLE) break;
        parole[n++] = s;                     /* qui comincia una parola */
        while (*s && *s != ' ') s++;         /* vai alla fine della parola */
    }
    return n;
}

static void esegui(void) {
    char *parole[MAX_PAROLE];
    riga[lunghezza] = 0;
    int n = dividi(riga, parole);
    lunghezza = 0;
    if (n == 0) return;                      /* riga vuota */

    for (int i = 0; i < N_COMANDI; i++) {
        if (uguali(parole[0], comandi[i].nome)) {
            comandi[i].esegui(n, parole);
            return;
        }
    }
    kprintf("Comando sconosciuto: %s (scrivi aiuto)\n", parole[0]);
}

void shell_avvia(int c_e_il_disco) {
    disco_presente = c_e_il_disco;
    unsigned char vecchio = console_colore(BIANCO);
    kprintf("\nShell del kernel. Scrivi aiuto per l'elenco dei comandi.\n");
    console_colore(vecchio);
    prompt();
}

void shell_tasto(char c) {
    if (c == '\n') {
        console_putc('\n');
        esegui();
        prompt();
    } else if (c == '\b') {
        if (lunghezza > 0) {                 /* non si cancella oltre l'inizio della riga */
            lunghezza--;
            console_putc('\b');
        }
    } else {
        if (c == '\t') c = ' ';
        if (lunghezza < LUNGHEZZA_RIGA - 1) {  /* riga piena: i tasti in piu' si ignorano */
            riga[lunghezza++] = c;
            console_putc(c);
        }
    }
}
