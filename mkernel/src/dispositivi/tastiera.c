/* La tastiera PS/2.
 *
 * A ogni pressione E a ogni rilascio, il controller 8042 mette un byte sulla porta 0x60
 * e suona IRQ 1. Il byte e' uno "scancode": il numero del tasto FISICO, non la lettera.
 *   premuto:  numero del tasto (es. 0x1E = il tasto della A)
 *   rilasciato: lo stesso numero con il bit alto acceso (0x1E | 0x80 = 0x9E)
 * Alcuni tasti aggiunti dopo (AltGr, frecce...) arrivano preceduti dal byte 0xE0.
 *
 * Qui: si tiene traccia di Shift, AltGr e Caps Lock, si traduce con la tabella della
 * tastiera italiana e si mette il carattere in un buffer. Il resto del kernel lo prende
 * da li' con tastiera_leggi(), con calma, fuori dall'interruzione.
 *
 * Le lettere accentate usano i codici della tabella di caratteri della scheda video
 * (code page 437): e' quella che sa disegnare. */
#include "io.h"
#include "pic.h"
#include "idt.h"
#include "tastiera.h"
#include "kernel.h"
#include "compiti.h"

#define PORTA_DATI 0x60

static struct compito *in_attesa;   /* il compito che dorme aspettando un tasto */

/* codici della code page 437 */
#define A_GRAVE 0x85   /* à */
#define E_GRAVE 0x8A   /* è */
#define E_ACUTA 0x82   /* é */
#define I_GRAVE 0x8D   /* ì */
#define O_GRAVE 0x95   /* ò */
#define U_GRAVE 0x97   /* ù */
#define C_CEDIG 0x87   /* ç */
#define STERLINA 0x9C  /* £ */
#define GRADO   0xF8   /* ° */
#define PARAGR  0x15   /* § */

/* Tastiera italiana: scancode -> carattere, senza e con Shift. 0 = nessun carattere. */
static const unsigned char normale[0x59] = {
    [0x02] = '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '\'', I_GRAVE, '\b',
    [0x0F] = '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', E_GRAVE, '+', '\n',
    [0x1E] = 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', O_GRAVE, A_GRAVE, '\\',
    [0x2B] = U_GRAVE, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '-',
    [0x37] = '*', [0x39] = ' ', [0x56] = '<',
};

static const unsigned char maiuscolo[0x59] = {
    [0x02] = '!', '"', STERLINA, '$', '%', '&', '/', '(', ')', '=', '?', '^', '\b',
    [0x0F] = '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', E_ACUTA, '*', '\n',
    [0x1E] = 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', C_CEDIG, GRADO, '|',
    [0x2B] = PARAGR, 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ';', ':', '_',
    [0x37] = '*', [0x39] = ' ', [0x56] = '>',
};

#define SHIFT_SX  0x2A
#define SHIFT_DX  0x36
#define ALT       0x38   /* preceduto da 0xE0 e' AltGr */
#define CAPS_LOCK 0x3A
#define ESTESO    0xE0

static int shift, altgr, caps, esteso;

/* --- il buffer: un anello di caratteri ---
 * Il gestore scrive in 'testa', tastiera_leggi legge da 'coda'. Quando arrivano in fondo
 * ricominciano dall'inizio. Se testa raggiunge coda il buffer e' pieno e i tasti in piu'
 * si perdono (servirebbe scrivere 128 caratteri senza che nessuno li legga). */
#define DIM_BUFFER 128
static volatile char buffer[DIM_BUFFER];
static volatile unsigned int testa, coda;

static void metti_nel_buffer(char c) {
    unsigned int prossima = (testa + 1) % DIM_BUFFER;
    if (prossima == coda) return;   /* pieno */
    buffer[testa] = c;
    testa = prossima;
}

char tastiera_leggi(void) {
    if (coda == testa) return 0;    /* vuoto */
    char c = buffer[coda];
    coda = (coda + 1) % DIM_BUFFER;
    return c;
}

/* Aspetta un tasto DORMENDO: il compito non consuma CPU finche' il gestore della
 * tastiera non lo sveglia.
 * Perche' le interruzioni spente: tra "il buffer e' vuoto" e "mi metto in attesa"
 * potrebbe arrivare proprio il tasto. Il gestore non vedrebbe nessuno da svegliare,
 * e noi ci addormenteremmo con un tasto gia' nel buffer (la "sveglia persa").
 * A interruzioni spente il tasto puo' arrivare solo prima o dopo, mai in mezzo. */
char tastiera_aspetta(void) {
    for (;;) {
        unsigned int s = interruzioni_spegni();
        char c = tastiera_leggi();
        if (c) { interruzioni_ripristina(s); return c; }
        in_attesa = compito_attuale();
        compito_aspetta();          /* dorme qui; al risveglio si ricontrolla il buffer */
        interruzioni_ripristina(s);
    }
}

/* Combinazioni con AltGr sulla tastiera italiana. */
static char con_altgr(unsigned char sc) {
    switch (sc) {
    case 0x1A: return shift ? '{' : '[';   /* tasto è */
    case 0x1B: return shift ? '}' : ']';   /* tasto + */
    case 0x27: return '@';                 /* tasto ò */
    case 0x28: return '#';                 /* tasto à */
    default:   return 0;
    }
}

/* --- il gestore: gira dentro l'interruzione, quindi corto e veloce --- */

static void al_tasto(struct registri *r) {
    (void)r;
    unsigned char sc = inb(PORTA_DATI);   /* va letto sempre, o il controller si blocca */

    if (sc == ESTESO) { esteso = 1; return; }   /* il vero codice arriva al prossimo IRQ */
    int era_esteso = esteso;
    esteso = 0;

    int rilasciato = sc & 0x80;
    sc &= 0x7F;

    /* i tasti che cambiano lo stato, sia quando si premono sia quando si rilasciano */
    if (sc == SHIFT_SX || sc == SHIFT_DX) { shift = !rilasciato; return; }
    if (sc == ALT && era_esteso)          { altgr = !rilasciato; return; }
    if (rilasciato) return;                              /* gli altri rilasci non ci interessano */
    if (sc == CAPS_LOCK) { caps = !caps; return; }
    if (era_esteso) return;                              /* frecce e simili: per ora no */
    if (sc >= sizeof normale) return;

    char c;
    if (altgr) {
        c = con_altgr(sc);
    } else {
        c = shift ? maiuscolo[sc] : normale[sc];
        /* Caps Lock inverte maiuscole e minuscole, ma solo per le lettere */
        if (caps && c >= 'a' && c <= 'z') c -= 'a' - 'A';
        else if (caps && c >= 'A' && c <= 'Z') c += 'a' - 'A';
    }
    if (c) {
        metti_nel_buffer(c);
        if (in_attesa) {            /* qualcuno aspettava un tasto: si sveglia */
            compito_sveglia(in_attesa);
            in_attesa = 0;
        }
    }
}

void tastiera_prepara(void) {
    while (inb(0x64) & 1)       /* svuota eventuali byte rimasti dal BIOS */
        inb(PORTA_DATI);
    irq_collega(IRQ_TASTIERA, al_tasto);
    pic_apri(IRQ_TASTIERA);
}
