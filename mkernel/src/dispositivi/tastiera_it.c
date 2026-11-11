/* La tastiera italiana: dal TASTO premuto al CARATTERE. Neutro: lo usano tutti i driver
 * di tastiera (PS/2 su x86, virtio su x86 e ARM...).
 *
 * I CODICI DEI TASTI sono quelli di Linux (evdev, KEY_...): li usano i dispositivi virtio,
 * e per i tasti principali (1-88) sono IDENTICI agli scancode PS/2 "set 1" (KEY_A = 30 =
 * 0x1E, lo scancode della A). Cosi' la stessa tabella serve a tutti: il driver PS/2 passa
 * il suo scancode, quello virtio il codice che gli arriva. I tasti "estesi" hanno numeri
 * piu' alti: AltGr e' KEY_RIGHTALT (100).
 *
 * Qui: si tiene traccia di Shift, AltGr e Caps Lock, si traduce con la tabella della
 * tastiera italiana e si mette il carattere nella fila degli eventi (eventi.c), dove
 * lo prendera' il compositore.
 * Le lettere accentate usano i codici della tabella di caratteri della scheda video
 * (code page 437): e' quella che sa disegnare. */
#include "kernel.h"
#include "eventi.h"
#include "tastiera_it.h"

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

#define SHIFT_SX  0x2A   /* KEY_LEFTSHIFT */
#define SHIFT_DX  0x36   /* KEY_RIGHTSHIFT */
#define CAPS_LOCK 0x3A   /* KEY_CAPSLOCK */

static int shift, altgr, caps;

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

void tastiera_tasto(unsigned int codice, int premuto) {
    /* i tasti che cambiano lo stato, sia quando si premono sia quando si rilasciano */
    if (codice == SHIFT_SX || codice == SHIFT_DX) { shift = premuto; return; }
    if (codice == TASTO_ALTGR)                    { altgr = premuto; return; }
    if (!premuto) return;                                /* gli altri rilasci non ci interessano */
    if (codice == CAPS_LOCK) { caps = !caps; return; }
    if (codice >= sizeof normale) return;                /* frecce e simili: per ora no */

    char c;
    if (altgr) {
        c = con_altgr((unsigned char)codice);
    } else {
        c = shift ? maiuscolo[codice] : normale[codice];
        /* Caps Lock inverte maiuscole e minuscole, ma solo per le lettere */
        if (caps && c >= 'a' && c <= 'z') c -= 'a' - 'A';
        else if (caps && c >= 'A' && c <= 'Z') c += 'a' - 'A';
    }
    if (c) {
        struct evento e = { .tipo = EVENTO_TASTO, .tasto = (unsigned char)c };
        eventi_metti(&e);
    }
}
