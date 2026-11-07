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

void shell_avvia(void) {
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
