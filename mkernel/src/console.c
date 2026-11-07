/* La console: scrive caratteri sullo schermo in modalita' testo.
 * Lo schermo e' una zona di memoria a 0xB8000: 80x25 caselle da 2 byte,
 * il primo e' il carattere, il secondo il colore. */
#include "kernel.h"
#include "io.h"

#define COLONNE 80
#define RIGHE   25

static volatile unsigned short *const schermo = (unsigned short *)0xB8000;
static int riga = 0, colonna = 0;
static unsigned char colore = (NERO << 4) | GRIGIO;

static unsigned short casella(char c) {
    return (unsigned short)(unsigned char)c | ((unsigned short)colore << 8);
}

/* Il cursore lampeggiante non e' in memoria: e' della scheda video, e lo si sposta
 * dalle sue porte. 0x3D4 sceglie quale registro della scheda, 0x3D5 ci scrive. */
static void sposta_cursore(void) {
    unsigned short pos = riga * COLONNE + colonna;
    outb(0x3D4, 0x0F); outb(0x3D5, pos & 0xFF);   /* parte bassa della posizione */
    outb(0x3D4, 0x0E); outb(0x3D5, pos >> 8);     /* parte alta */
}

unsigned char console_colore(unsigned char c) {
    unsigned char vecchio = colore;
    colore = c;
    return vecchio;
}

void console_pulisci(void) {
    for (int i = 0; i < COLONNE * RIGHE; i++)
        schermo[i] = casella(' ');
    riga = colonna = 0;
    sposta_cursore();
}

void console_dove(int *r, int *c) { *r = riga; *c = colonna; }

void console_vai(int r, int c) {
    riga = r;
    colonna = c;
    sposta_cursore();
}

/* Tutte le righe salgono di uno, l'ultima si svuota. */
static void scorri(void) {
    for (int i = 0; i < COLONNE * (RIGHE - 1); i++)
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
    metti(c);
    sposta_cursore();
}

void console_scrivi(const char *s) {
    while (*s) metti(*s++);
    sposta_cursore();
}
