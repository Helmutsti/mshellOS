/* La seriale del PC (COM1, porta 0x3F8): una copia di tutto quello che il kernel scrive
 * sulla console. Serve a leggere l'avvio da fuori (QEMU: -serial stdio), anche quando lo
 * schermo e' gia' del compositore. Come "console=ttyS0" di Linux.
 *   +0  il dato: scriverci un byte lo spedisce
 *   +5  lo stato: il bit 5 dice "pronta a spedire"
 * QEMU la lascia gia' pronta: non serve prepararla. Se non c'e' nessuna seriale, la
 * porta risponde 0xFF ("pronta") e i byte si perdono: nessun danno. */
#include "kernel.h"
#include "io.h"

#define COM1 0x3F8

void seriale_eco(char c) {
    if (c == '\n') seriale_eco('\r');
    for (int i = 0; i < 10000 && !(inb(COM1 + 5) & 0x20); i++)
        ;
    outb(COM1, (unsigned char)c);
}
