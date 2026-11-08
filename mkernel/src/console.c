/* La console: scrive caratteri sullo schermo in modalita' testo.
 * Lo schermo e' una zona di memoria a 0xB8000: 80x25 caselle da 2 byte,
 * il primo e' il carattere, il secondo il colore.
 *
 * La riga 0 e' la BARRA DI STATO: ci scrivono i compiti (orologio, contatori) con
 * console_stato, senza toccare il cursore. Il testo normale vive nelle righe 1-24
 * e scorre li' dentro.
 *
 * Con lo scheduler piu' compiti possono scrivere: riga, colonna e colore sono
 * CONDIVISI, quindi ogni funzione pubblica e' una sezione critica (interruzioni
 * spente): nessun tick puo' fermarla a meta'. */
#include "kernel.h"
#include "io.h"

#define COLONNE 80
#define RIGHE   25
#define PRIMA_RIGA 1                           /* la riga 0 e' la barra di stato */
#define COLORE_BARRA(f) ((BLU << 4) | (f))     /* testo 'f' su sfondo blu */

static volatile unsigned short *const schermo = (unsigned short *)0xB8000;
static int riga = PRIMA_RIGA, colonna = 0;
static unsigned char colore = (NERO << 4) | GRIGIO;

static unsigned short casella_colore(char c, unsigned char col) {
    return (unsigned short)(unsigned char)c | ((unsigned short)col << 8);
}

static unsigned short casella(char c) { return casella_colore(c, colore); }

/* Il cursore lampeggiante non e' in memoria: e' della scheda video, e lo si sposta
 * dalle sue porte. 0x3D4 sceglie quale registro della scheda, 0x3D5 ci scrive. */
static void sposta_cursore(void) {
    unsigned short pos = riga * COLONNE + colonna;
    outb(0x3D4, 0x0F); outb(0x3D5, pos & 0xFF);   /* parte bassa della posizione */
    outb(0x3D4, 0x0E); outb(0x3D5, pos >> 8);     /* parte alta */
}

unsigned char console_colore(unsigned char c) {
    unsigned int s = interruzioni_spegni();
    unsigned char vecchio = colore;
    colore = c;
    interruzioni_ripristina(s);
    return vecchio;
}

void console_pulisci(void) {
    unsigned int s = interruzioni_spegni();
    for (int i = PRIMA_RIGA * COLONNE; i < COLONNE * RIGHE; i++)
        schermo[i] = casella(' ');
    riga = PRIMA_RIGA;
    colonna = 0;
    sposta_cursore();
    interruzioni_ripristina(s);
}

void console_prepara(void) {
    for (int i = 0; i < COLONNE; i++)
        schermo[i] = casella_colore(' ', COLORE_BARRA(BIANCO));
    console_pulisci();
}

void console_stato(int col, unsigned char primo_piano, const char *testo) {
    unsigned int s = interruzioni_spegni();
    for (; *testo && col < COLONNE; col++, testo++)
        schermo[col] = casella_colore(*testo, COLORE_BARRA(primo_piano));
    interruzioni_ripristina(s);
}

void console_dove(int *r, int *c) {
    unsigned int s = interruzioni_spegni();
    *r = riga;
    *c = colonna;
    interruzioni_ripristina(s);
}

void console_vai(int r, int c) {
    unsigned int s = interruzioni_spegni();
    riga = r < PRIMA_RIGA ? PRIMA_RIGA : r;
    colonna = c;
    sposta_cursore();
    interruzioni_ripristina(s);
}

/* Le righe del testo salgono di uno, l'ultima si svuota. La barra resta ferma. */
static void scorri(void) {
    for (int i = PRIMA_RIGA * COLONNE; i < COLONNE * (RIGHE - 1); i++)
        schermo[i] = schermo[i + COLONNE];
    for (int i = 0; i < COLONNE; i++)
        schermo[COLONNE * (RIGHE - 1) + i] = casella(' ');
    riga = RIGHE - 1;
}

static void a_capo(void) {
    colonna = 0;
    if (++riga == RIGHE) scorri();
}

static void metti(char c) {
    switch (c) {
    case '\n': a_capo(); return;
    case '\r': colonna = 0; return;
    case '\t': do metti(' '); while (colonna % 8); return;
    case '\b':                       /* cancella: indietro di uno e svuota la casella */
        if (colonna > 0) {
            colonna--;
            schermo[riga * COLONNE + colonna] = casella(' ');
        }
        return;
    }
    schermo[riga * COLONNE + colonna] = casella(c);
    if (++colonna == COLONNE) a_capo();
}

void console_putc(char c) {
    unsigned int s = interruzioni_spegni();
    metti(c);
    sposta_cursore();
    interruzioni_ripristina(s);
}

void console_scrivi(const char *str) {
    unsigned int s = interruzioni_spegni();
    while (*str) metti(*str++);
    sposta_cursore();
    interruzioni_ripristina(s);
}
