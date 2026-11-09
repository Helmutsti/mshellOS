/* La console: scrive caratteri sullo schermo in modalita' testo.
 * Lo schermo e' una zona di memoria a 0xB8000: 80x25 caselle da 2 byte,
 * il primo e' il carattere, il secondo il colore.
 *
 * Tutte le 25 righe sono del testo: quando si arriva in fondo, scorre in su.
 *
 * I CODICI DEI TERMINALI. Come i terminali di Linux, la console riconosce alcune
 * sequenze speciali dentro il testo: iniziano con ESC (il carattere 27) e '[', poi
 * qualche numero separato da ';', e finiscono con una lettera che dice cosa fare.
 * Non si stampano: si eseguono. Cosi' un programma, che puo' solo scrivere testo
 * (chiamata scrivi), puo' anche comandare lo schermo. Per ora:
 *   ESC [ H        cursore all'inizio (ESC [ riga ; colonna H: in quel punto, da 1)
 *   ESC [ 2 J      pulisce lo schermo (anche 3 J), senza spostare il cursore
 * E' quello che scrive il comando clear di Linux: ESC [ H ESC [ 2 J.
 * Le sequenze che non conosce le ignora.
 *
 * Con lo scheduler piu' compiti possono scrivere: riga, colonna e colore sono
 * CONDIVISI, quindi ogni funzione pubblica e' una sezione critica (interruzioni
 * spente): nessun tick puo' fermarla a meta'. */
#include "kernel.h"
#include "io.h"

#define COLONNE 80
#define RIGHE   25

static volatile unsigned short *const schermo = (unsigned short *)0xB8000;
static int riga = 0, colonna = 0;
static unsigned char colore = (NERO << 4) | GRIGIO;

/* Una casella dello schermo: il carattere nel byte basso, il colore in quello alto. */
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
    unsigned int s = interruzioni_spegni();
    unsigned char vecchio = colore;
    colore = c;
    interruzioni_ripristina(s);
    return vecchio;
}

/* Svuota lo schermo. Il cursore resta dov'e'. */
static void svuota_testo(void) {
    for (int i = 0; i < COLONNE * RIGHE; i++)
        schermo[i] = casella(' ');
}

void console_prepara(void) {
    unsigned int s = interruzioni_spegni();
    svuota_testo();
    riga = 0;
    colonna = 0;
    sposta_cursore();
    interruzioni_ripristina(s);
}

/* Le righe salgono di uno, l'ultima si svuota. */
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

/* --- i codici dei terminali --- */

#define ESC 27
#define MAX_NUMERI 2
static enum { TESTO, DOPO_ESC, IN_CODICE } stato_codice = TESTO;
static int numeri[MAX_NUMERI], quanti_numeri;

static int tra(int x, int minimo, int massimo) { return x < minimo ? minimo : x > massimo ? massimo : x; }

/* La lettera finale di una sequenza: si esegue. */
static void esegui_codice(char lettera) {
    switch (lettera) {
    case 'H': {   /* righe e colonne contano da 1; mancanti o 0 valgono 1 */
        int r = quanti_numeri > 0 && numeri[0] ? numeri[0] : 1;
        int c = quanti_numeri > 1 && numeri[1] ? numeri[1] : 1;
        riga = tra(r, 1, RIGHE) - 1;
        colonna = tra(c, 1, COLONNE) - 1;
        break;
    }
    case 'J':
        if (numeri[0] == 2 || numeri[0] == 3) svuota_testo();
        break;
    }
}

/* Un carattere che fa parte di una sequenza (o la comincia). 1 se lo era, 0 se e' testo. */
static int dentro_codice(char c) {
    switch (stato_codice) {
    case TESTO:
        if (c != ESC) return 0;
        stato_codice = DOPO_ESC;
        return 1;
    case DOPO_ESC:
        if (c == '[') {
            stato_codice = IN_CODICE;
            quanti_numeri = 0;
            for (int i = 0; i < MAX_NUMERI; i++) numeri[i] = 0;
        } else {
            stato_codice = TESTO;   /* ESC seguito da altro: non lo conosciamo */
        }
        return 1;
    case IN_CODICE:
        if (c >= '0' && c <= '9') {
            if (quanti_numeri == 0) quanti_numeri = 1;
            int *n = &numeri[quanti_numeri <= MAX_NUMERI ? quanti_numeri - 1 : MAX_NUMERI - 1];
            if (*n < 1000) *n = *n * 10 + (c - '0');
        } else if (c == ';') {
            if (quanti_numeri == 0) quanti_numeri = 1;
            if (quanti_numeri < MAX_NUMERI) quanti_numeri++;
        } else {
            esegui_codice(c);       /* la lettera finale */
            stato_codice = TESTO;
        }
        return 1;
    }
    return 0;
}

static void metti(char c) {
    if (dentro_codice(c)) return;
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
