/* Il PIC 8259: due chip in cascata, il secondo e' attaccato alla linea 2 del primo.
 *
 * All'accensione mandano IRQ 0-7 alle righe 8-15 della IDT, che sono gia' eccezioni
 * di Intel (la 8 e' il "doppio errore"): ogni tick del timer sembrerebbe un disastro.
 * Li riprogrammiamo per usare le righe 32-47, subito dopo le eccezioni. */
#include "io.h"
#include "pic.h"

#define PIC1_COMANDI 0x20   /* principale: IRQ 0-7 */
#define PIC1_DATI    0x21
#define PIC2_COMANDI 0xA0   /* secondario: IRQ 8-15 */
#define PIC2_DATI    0xA1

#define EOI 0x20            /* il byte del "ricevuto" */

void pic_prepara(void) {
    /* La sequenza di inizializzazione: quattro byte per chip, sempre in quest'ordine. */

    /* 1. "inizializzati, e aspettati altri 3 byte" */
    outb(PIC1_COMANDI, 0x11); io_attesa();
    outb(PIC2_COMANDI, 0x11); io_attesa();

    /* 2. da quale riga della IDT partire */
    outb(PIC1_DATI, IRQ_BASE);     io_attesa();   /* IRQ 0-7  -> righe 32-39 */
    outb(PIC2_DATI, IRQ_BASE + 8); io_attesa();   /* IRQ 8-15 -> righe 40-47 */

    /* 3. come sono collegati tra loro */
    outb(PIC1_DATI, 1 << 2); io_attesa();   /* al principale: "il secondario e' sulla tua linea 2" */
    outb(PIC2_DATI, 2);      io_attesa();   /* al secondario: "sei attaccato alla linea 2" */

    /* 4. "funziona in modalita' 8086/x86" */
    outb(PIC1_DATI, 0x01); io_attesa();
    outb(PIC2_DATI, 0x01); io_attesa();

    /* Maschere: un bit a 1 = linea chiusa. Chiudiamo tutto, poi si apre linea per linea,
     * solo quando c'e' un gestore. Lasciamo aperta la linea 2: e' il collegamento al secondario. */
    outb(PIC1_DATI, 0xFF & ~(1 << 2));
    outb(PIC2_DATI, 0xFF);
}

void pic_apri(int irq) {
    unsigned short porta = irq < 8 ? PIC1_DATI : PIC2_DATI;
    outb(porta, inb(porta) & ~(1 << (irq % 8)));
}

void pic_ricevuto(int irq) {
    /* se la chiamata e' passata dal secondario, va avvisato anche lui */
    if (irq >= 8) outb(PIC2_COMANDI, EOI);
    outb(PIC1_COMANDI, EOI);
}
