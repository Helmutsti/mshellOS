/* La tastiera PS/2.
 *
 * A ogni pressione E a ogni rilascio, il controller 8042 mette un byte sulla porta 0x60
 * e suona IRQ 1. Il byte e' uno "scancode": il numero del tasto FISICO, non la lettera.
 *   premuto:  numero del tasto (es. 0x1E = il tasto della A)
 *   rilasciato: lo stesso numero con il bit alto acceso (0x1E | 0x80 = 0x9E)
 * Alcuni tasti aggiunti dopo (AltGr, frecce...) arrivano preceduti dal byte 0xE0.
 *
 * Qui: si tiene traccia di Shift, AltGr e Caps Lock, si traduce con la tabella della
 * tastiera italiana e si mette il carattere nella fila degli eventi (eventi.c), dove
 * lo prendera' il compositore.
 *
 * Le lettere accentate usano i codici della tabella di caratteri della scheda video
 * (code page 437): e' quella che sa disegnare. */
#include "io.h"
#include "pic.h"
#include "idt.h"
#include "tastiera.h"
#include "kernel.h"
#include "tastiera_it.h"

#define PORTA_DATI 0x60

#define ESTESO    0xE0
#define ALT       0x38   /* preceduto da 0xE0 e' AltGr */

static int esteso;

/* --- il gestore: gira dentro l'interruzione, quindi corto e veloce --- */

static void al_tasto(struct registri *r) {
    (void)r;
    unsigned char sc = inb(PORTA_DATI);   /* va letto sempre, o il controller si blocca */

    if (sc == ESTESO) { esteso = 1; return; }   /* il vero codice arriva al prossimo IRQ */
    int era_esteso = esteso;
    esteso = 0;

    int rilasciato = sc & 0x80;
    sc &= 0x7F;

    if (era_esteso && sc == ALT) tastiera_tasto(TASTO_ALTGR, !rilasciato);
    else if (!era_esteso) tastiera_tasto(sc, !rilasciato);
    /* gli altri estesi (frecce e simili): per ora no */
}

void tastiera_prepara(void) {
    while (inb(0x64) & 1)       /* svuota eventuali byte rimasti dal BIOS */
        inb(PORTA_DATI);
    irq_collega(IRQ_TASTIERA, al_tasto);
    pic_apri(IRQ_TASTIERA);
}
