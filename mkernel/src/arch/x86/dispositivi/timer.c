/* Il PIT 8253/8254.
 *
 * Dentro c'e' un oscillatore a 1193182 colpi al secondo (un valore ereditato dalla
 * frequenza delle TV americane, usata dal primo PC per risparmiare un quarzo).
 * Gli diamo un divisore N: ogni N colpi suona la linea IRQ 0 e ricomincia a contare. */
#include "io.h"
#include "pic.h"
#include "idt.h"
#include "timer.h"

#define PIT_CANALE0 0x40
#define PIT_COMANDI 0x43
#define PIT_FREQUENZA 1193182

static volatile unsigned int tick;   /* volatile: cambia "da solo", dentro l'interruzione */

static void al_tick(struct registri *r) {
    (void)r;
    tick++;
    timer_esegui_lavori();   /* i lavoretti dei driver (timer.h) */
}

void timer_prepara(unsigned int hz) {
    unsigned int divisore = PIT_FREQUENZA / hz;

    /* 0x36 = canale 0, ti mando il divisore in due byte (prima il basso), ripeti all'infinito */
    outb(PIT_COMANDI, 0x36);
    outb(PIT_CANALE0, divisore & 0xFF);
    outb(PIT_CANALE0, (divisore >> 8) & 0xFF);

    irq_collega(IRQ_TIMER, al_tick);
    pic_apri(IRQ_TIMER);
}

unsigned int timer_tick(void) { return tick; }
