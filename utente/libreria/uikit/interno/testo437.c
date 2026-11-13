/* UIKit: da UTF-8 alla tabella di caratteri dei font del sistema, la code page 437 (vedi
 * testo437.h). */
#include "uikit/testo437.h"

/* I caratteri Unicode da U+00A0 a U+00FF (Latin-1) nella 437: 0 se non c'e'. */
static const unsigned char latino_437[96] = {
    /* A0 */ ' ',  0xAD, 0x9B, 0x9C, 0,    0x9D, '|',  0,    0,    0,    0xA6, '"',  0xAA, '-',  0,    0,
    /* B0 */ 0xF8, 0xF1, 0xFD, 0,    '\'', 0xE6, 0,    0xFA, 0,    0,    0xA7, '"',  0xAC, 0xAB, 0,    0xA8,
    /* C0 */ 'A',  'A',  'A',  'A',  0x8E, 0x8F, 0x92, 0x80, 'E',  0x90, 'E',  'E',  'I',  'I',  'I',  'I',
    /* D0 */ 'D',  0xA5, 'O',  'O',  'O',  'O',  0x99, 'x',  'O',  'U',  'U',  'U',  0x9A, 'Y',  0,    0xE1,
    /* E0 */ 0x85, 0xA0, 0x83, 'a',  0x84, 0x86, 0x91, 0x87, 0x8A, 0x82, 0x88, 0x89, 0x8D, 0xA1, 0x8C, 0x8B,
    /* F0 */ 0,    0xA4, 0x95, 0xA2, 0x93, 'o',  0x94, 0xF6, 0,    0x97, 0xA3, 0x96, 0x81, 'y',  0,    0x98,
};

/* Copia 'da' (UTF-8) in 'dove' (code page 437), al massimo 'massimo' byte compreso lo 0. */
void in_437(char *dove, const char *da, int massimo) {
    const unsigned char *s = (const unsigned char *)(da ? da : "");
    int n = 0;
    while (*s && n < massimo - 1) {
        unsigned int c = *s, quanti = 0;
        if (c < 0x80) quanti = 0;
        else if ((c & 0xE0) == 0xC0) { c &= 0x1F; quanti = 1; }
        else if ((c & 0xF0) == 0xE0) { c &= 0x0F; quanti = 2; }
        else if ((c & 0xF8) == 0xF0) { c &= 0x07; quanti = 3; }
        else { s++; dove[n++] = '?'; continue; }            /* un byte che non comincia niente */
        s++;
        unsigned int k = 0;
        for (; k < quanti && (*s & 0xC0) == 0x80; k++, s++) c = (c << 6) | (*s & 0x3F);
        if (k < quanti) { dove[n++] = '?'; continue; }      /* finito a meta' */

        const char *piu = 0;                                /* piu' caratteri al posto di uno */
        unsigned char u = '?';
        if (c < 0x20) u = ' ';                              /* a capo e tabulazioni: uno spazio */
        else if (c < 0x7F) u = (unsigned char)c;
        else if (c >= 0xA0 && c <= 0xFF) u = latino_437[c - 0xA0] ? latino_437[c - 0xA0] : '?';
        else if (c == 0x2018 || c == 0x2019 || c == 0x201A) u = '\'';
        else if (c == 0x201C || c == 0x201D || c == 0x201E) u = '"';
        else if (c == 0x2013 || c == 0x2014 || c == 0x2212) u = '-';
        else if (c == 0x2022 || c == 0x2027) u = 0xFA;      /* il puntino a meta' altezza */
        else if (c == 0x2026) piu = "...";
        else if (c == 0x2192) piu = "->";
        else if (c == 0x20AC) piu = "EUR";
        if (piu) { while (*piu && n < massimo - 1) dove[n++] = *piu++; }
        else dove[n++] = (char)u;
    }
    dove[n] = 0;
}
