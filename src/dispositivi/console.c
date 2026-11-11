/* La console: scrive testo sullo schermo in MODALITA' PIXEL, come la console grafica
 * di Linux (fbcon).
 *
 * Lo schermo e' una zona di memoria (il framebuffer) con un numero per ogni pixel:
 * 4 byte, cioe' rosso, verde, blu e un byte vuoto. Chi ci ha avviato (sul PC il firmware
 * UEFI, tramite il caricatore; su ARM il driver ramfb) ha acceso la modalita' pixel
 * e ci ha detto dove sta, quanto e' grande e in che ordine stanno i colori.
 * La scheda non sa piu' disegnare le lettere: lo facciamo noi, con il font 8x16
 * (font.h). Ogni carattere e' una CASELLA di 8x16 pixel: a 1280x800 sono 160 colonne
 * per 50 righe. Ci teniamo in memoria cosa c'e' in ogni casella (carattere e colore),
 * cosi' possiamo ridisegnarla, per esempio per togliere il cursore.
 *
 * Il CURSORE non lo disegna piu' la scheda: e' una linea sotto la casella dove si
 * scrivera' il prossimo carattere, e la disegniamo noi.
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
 * LO SCHERMO PRESTATO. Come in Linux, lo schermo si puo' dare a un programma (il
 * compositore): da quel momento la console non disegna piu'. Continua pero' a tenere
 * in memoria le caselle, come se scrivesse: quando il programma finisce lo schermo
 * torna alla console, che ridisegna tutto. Anche un panico se lo riprende, per farsi
 * vedere.
 *
 * Con lo scheduler piu' compiti possono scrivere: riga, colonna e colore sono
 * CONDIVISI, quindi ogni funzione pubblica e' una sezione critica (interruzioni
 * spente): nessun tick puo' fermarla a meta'. */
#include "kernel.h"
#include "font.h"

#define MAX_COLONNE 256
#define MAX_RIGHE   128

static struct schermo schermo;           /* com'e' fatto, come ce lo ha descritto l'avvio */
static const void *padrone;               /* a chi e' prestato lo schermo (0: e' nostro) */
static unsigned char *memoria;            /* il framebuffer */
static unsigned int passo;                /* byte da una riga di pixel alla successiva */
static int colonne, righe;                /* quante caselle di testo ci stanno */
static unsigned int tavolozza[16];        /* i 16 colori, gia' nel formato dello schermo */

static unsigned short caselle[MAX_COLONNE * MAX_RIGHE];   /* carattere | colore << 8 */
static int riga = 0, colonna = 0;
static unsigned char colore = (NERO << 4) | GRIGIO;

/* I 16 colori della vecchia modalita' testo (NERO, BLU, ... in kernel.h), in RGB. */
static const unsigned int colori_rgb[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};

/* --- disegnare --- */

/* Ridisegna la casella (r, c) da quello che ricordiamo. Con cursore=1 aggiunge la
 * linea del cursore nelle ultime due righe di pixel. */
static void disegna(int r, int c, int cursore) {
    if (padrone) return;   /* lo schermo e' prestato: si ricorda soltanto */
    unsigned short casella = caselle[r * colonne + c];
    const unsigned char *lettera = font_8x16[casella & 0xFF];
    unsigned int davanti = tavolozza[(casella >> 8) & 0x0F];
    unsigned int dietro  = tavolozza[(casella >> 12) & 0x0F];

    for (int y = 0; y < FONT_ALTEZZA; y++) {
        unsigned int *pixel = (unsigned int *)(memoria + (r * FONT_ALTEZZA + y) * passo)
                              + c * FONT_LARGHEZZA;
        unsigned char bit = lettera[y];
        if (cursore && y >= FONT_ALTEZZA - 2) bit = 0xFF;
        for (int x = 0; x < FONT_LARGHEZZA; x++)
            pixel[x] = (bit & (0x80 >> x)) ? davanti : dietro;
    }
}

static void metti_casella(int r, int c, char ch) {
    caselle[r * colonne + c] = (unsigned char)ch | ((unsigned short)colore << 8);
    disegna(r, c, 0);
}

/* Il cursore si toglie prima di cambiare qualcosa e si rimette alla fine. */
static void togli_cursore(void)   { disegna(riga, colonna, 0); }
static void mostra_cursore(void)  { disegna(riga, colonna, 1); }

/* Svuota lo schermo. Il cursore resta dov'e'. */
static void svuota_testo(void) {
    for (int r = 0; r < righe; r++)
        for (int c = 0; c < colonne; c++)
            metti_casella(r, c, ' ');
}

/* Le righe salgono di uno, l'ultima si svuota. Si spostano insieme i pixel (una sola
 * copia di memoria) e le caselle ricordate. */
static void scorri(void) {
    unsigned int byte_riga = passo * FONT_ALTEZZA;
    if (!padrone) memmove(memoria, memoria + byte_riga, byte_riga * (righe - 1));
    memmove(caselle, caselle + colonne, sizeof caselle[0] * colonne * (righe - 1));
    for (int c = 0; c < colonne; c++) metti_casella(righe - 1, c, ' ');
    riga = righe - 1;
}

static void a_capo(void) {
    colonna = 0;
    if (++riga == righe) scorri();
}

/* --- preparare --- */

int console_prepara(const struct schermo *s) {
    if (s->bpp != 32) return 0;   /* sappiamo disegnare solo pixel da 4 byte */
    schermo = *s;
    memoria = (unsigned char *)s->indirizzo;
    passo = s->passo;
    colonne = s->larghezza / FONT_LARGHEZZA;
    righe = s->altezza / FONT_ALTEZZA;
    if (colonne > MAX_COLONNE) colonne = MAX_COLONNE;
    if (righe > MAX_RIGHE) righe = MAX_RIGHE;

    /* i colori nell'ordine che vuole lo schermo: ogni componente al suo posto */
    for (int i = 0; i < 16; i++) {
        unsigned int rgb = colori_rgb[i];
        tavolozza[i] = ((rgb >> 16) & 0xFF) << s->rosso
                     | ((rgb >> 8) & 0xFF) << s->verde
                     | (rgb & 0xFF) << s->blu;
    }

    unsigned int st = interruzioni_spegni();
    svuota_testo();
    riga = colonna = 0;
    mostra_cursore();
    interruzioni_ripristina(st);
    return 1;
}

const struct schermo *console_schermo(void) { return memoria ? &schermo : 0; }

int console_presta(const void *chi) {
    unsigned int s = interruzioni_spegni();
    int libero = memoria && !padrone;
    if (libero) padrone = chi;
    interruzioni_ripristina(s);
    return libero;
}

void console_riprendi(const void *chi) {
    unsigned int s = interruzioni_spegni();
    if (padrone && (!chi || chi == padrone)) {
        padrone = 0;
        for (int r = 0; r < righe; r++)       /* si ridisegna tutto da quello che si ricorda */
            for (int c = 0; c < colonne; c++)
                disegna(r, c, 0);
        mostra_cursore();
    }
    interruzioni_ripristina(s);
}

unsigned char console_colore(unsigned char c) {
    unsigned int s = interruzioni_spegni();
    unsigned char vecchio = colore;
    colore = c;
    interruzioni_ripristina(s);
    return vecchio;
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
        riga = tra(r, 1, righe) - 1;
        colonna = tra(c, 1, colonne) - 1;
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

/* --- scrivere --- */

static void metti(char c) {
    if (dentro_codice(c)) return;
    switch (c) {
    case '\n': a_capo(); return;
    case '\r': colonna = 0; return;
    case '\t': do metti(' '); while (colonna % 8); return;
    case '\b':                       /* cancella: indietro di uno e svuota la casella */
        if (colonna > 0) metti_casella(riga, --colonna, ' ');
        return;
    }
    metti_casella(riga, colonna, c);
    if (++colonna == colonne) a_capo();
}

/* IL LUCCHETTO DELLA CONSOLE. Proteggere riga e colonna non basta: chi scrive vuole che
 * il SUO testo esca tutto insieme. Se un tick arriva a meta' di una riga e lo scheduler
 * fa scrivere un altro compito, sulla seriale e sullo schermo le due righe si mescolano:
 *   Cciao come va
 *   ompiti : scheduler acceso
 * Chi vuole scrivere un pezzo intero (kprintf, la chiamata scrivi) prende il lucchetto,
 * mette i suoi caratteri con console_metti, e lo lascia: nel mezzo nessun altro scrive.
 *
 * Oggi c'e' un solo core: basta spegnere le interruzioni (niente tick, quindi niente
 * cambio di compito, e nessun driver che scrive da un'interruzione). E si puo' prendere
 * due volte di fila (un panico dentro una kprintf): spegnere gia' spento non fa niente.
 * Con piu' core non basta piu': un altro core scrive comunque, con le interruzioni
 * accese o spente. Allora qui dentro si aggiunge uno spinlock (un intero che un core
 * alla volta porta da 0 a 1 con un'istruzione atomica; gli altri girano finche' torna
 * 0), SEMPRE insieme alle interruzioni spente: se no un'interruzione sullo stesso core
 * che ha il lucchetto vorrebbe riprenderlo e girerebbe per sempre. E il panico dovra'
 * forzarlo, perche' il lucchetto potrebbe averlo proprio il core che sta morendo.
 * Chi lo tiene deve fare presto: per tutto quel tempo i tick aspettano. */
unsigned int console_blocca(void)        { return interruzioni_spegni(); }
void console_sblocca(unsigned int stato) { interruzioni_ripristina(stato); }

/* Un carattere, con il lucchetto gia' preso da chi chiama. */
void console_metti(char c) {
    seriale_eco(c);                  /* anche sulla seriale: per leggere l'avvio da fuori */
    if (!memoria) return;            /* niente schermo: niente da fare */
    togli_cursore();
    metti(c);
    mostra_cursore();
}

/* Un carattere da solo: prende e lascia il lucchetto. */
void console_putc(char c) {
    unsigned int s = console_blocca();
    console_metti(c);
    console_sblocca(s);
}
