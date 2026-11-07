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
#include "shell.h"

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

static unsigned char settore[SETTORE];   /* il settore letto o da scrivere */

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
    for (int i = 0; i < SETTORE; i++) settore[i] = 0;
    int pos = 0;
    for (int p = 2; p < n; p++) {            /* rimette insieme le parole con uno spazio */
        if (p > 2 && pos < SETTORE) settore[pos++] = ' ';
        for (const char *c = parole[p]; *c && pos < SETTORE; c++) settore[pos++] = *c;
    }
    if (!disco_presente || disco_scrivi(s, settore) < 0) {
        kprintf("Errore nella scrittura del settore %d\n", s);
        return;
    }
    kprintf("Scritti %d byte nel settore %d\n", pos, s);
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
    { "aiuto",   "questo elenco",                          cmd_aiuto   },
    { "eco",     "ripete il testo: eco ciao mondo",        cmd_eco     },
    { "mem",     "memoria libera (mem blocchi: i blocchi)", cmd_mem    },
    { "tempo",   "da quanto e' acceso il kernel",          cmd_tempo   },
    { "gdt",     "le righe della GDT",                     cmd_gdt     },
    { "disco",   "informazioni sul disco",                 cmd_disco   },
    { "leggi",   "mostra un settore: leggi N [inizio]",    cmd_leggi   },
    { "scrivi",  "scrive in un settore: scrivi N testo",   cmd_scrivi  },
    { "pulisci", "svuota lo schermo",                      cmd_pulisci },
    { "riavvia", "riavvia la macchina",                    cmd_riavvia },
};
#define N_COMANDI (int)(sizeof comandi / sizeof comandi[0])

static void cmd_aiuto(int n, char **parole) {
    (void)n; (void)parole;
    for (int i = 0; i < N_COMANDI; i++) {
        /* kprintf non sa allineare a sinistra: si aggiungono spazi fino a 10 caratteri */
        kprintf("  %s", comandi[i].nome);
        int l = 0;
        while (comandi[i].nome[l]) l++;
        for (; l < 10; l++) kprintf(" ");
        kprintf("%s\n", comandi[i].descrizione);
    }
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
