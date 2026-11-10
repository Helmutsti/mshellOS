/* Il compositore: il programma padrone dello schermo.
 *
 * Come in un sistema vero, il kernel non disegna finestre: presta lo schermo a un solo
 * programma, questo. Sara' lui a disegnare lo sfondo, le finestre e il puntatore, e le
 * applicazioni gli manderanno ORDINI ("un rettangolo qui", "questo testo la'") invece
 * di disegnare da sole.
 *
 * Per ora: chiede lo schermo, disegna lo sfondo e due finestre finte, e resta li'.
 *
 * LO SCHERMO, VISTO DA QUI. Dopo prendi_schermo la memoria dello schermo e' nella
 * nostra mappa, a SCHERMO_PROGRAMMA: scrivere li' un numero vuol dire accendere un
 * pixel. Riga dopo riga, da sinistra a destra, 4 byte per pixel. Tra l'inizio di una
 * riga e la successiva ci sono 'passo' byte (puo' essere piu' della larghezza * 4). */
#include "libreria.h"

static struct info_schermo schermo;
static unsigned char *memoria = (unsigned char *)SCHERMO_PROGRAMMA;

/* Un colore scritto come 0xRRGGBB, messo nell'ordine che vuole lo schermo. */
static unsigned int colore(unsigned int rgb) {
    return ((rgb >> 16) & 0xFF) << schermo.rosso
         | ((rgb >> 8) & 0xFF) << schermo.verde
         | (rgb & 0xFF) << schermo.blu;
}

/* Un rettangolo pieno. Quello che esce dallo schermo si taglia. */
static void rettangolo(int x, int y, int larghezza, int altezza, unsigned int rgb) {
    int x2 = x + larghezza, y2 = y + altezza;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x2 > (int)schermo.larghezza) x2 = schermo.larghezza;
    if (y2 > (int)schermo.altezza) y2 = schermo.altezza;
    unsigned int c = colore(rgb);
    for (int r = y; r < y2; r++) {
        unsigned int *riga = (unsigned int *)(memoria + r * schermo.passo);
        for (int k = x; k < x2; k++) riga[k] = c;
    }
}

/* Una finestra finta: un bordo, la barra del titolo e il contenuto. */
static void finestra(int x, int y, int larghezza, int altezza, unsigned int barra) {
    rettangolo(x, y, larghezza, altezza, 0x202020);                 /* il bordo */
    rettangolo(x + 1, y + 1, larghezza - 2, 22, barra);             /* la barra del titolo */
    rettangolo(x + 1, y + 23, larghezza - 2, altezza - 24, 0xF0F0F0); /* il contenuto */
}

int main(void) {
    if (prendi_schermo(&schermo) < 0) {
        scrivi_testo("compositore: lo schermo e' gia' di un altro programma\n");
        return 0;
    }

    /* lo sfondo: una sfumatura dall'alto in basso, una riga di colore alla volta */
    for (unsigned int y = 0; y < schermo.altezza; y++) {
        unsigned int t = y * 255 / schermo.altezza;
        rettangolo(0, y, schermo.larghezza, 1, (0x20 + t / 8) << 16 | (0x40 + t / 4) << 8 | (0x70 + t / 3));
    }

    finestra(80, 70, 360, 260, 0x3A6EA5);
    finestra(320, 220, 400, 300, 0x5A8F4E);   /* sopra la prima: disegnata dopo */

    for (;;)
        dormi(60 * 1000);
}
