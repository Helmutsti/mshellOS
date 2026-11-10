/* Il timer su ARM64: il "timer generico", che sta DENTRO la CPU (su x86 era il PIT, un
 * chip a parte). Implementa la stessa interfaccia neutra di x86 (timer.h).
 *
 * C'e' un contatore che sale da solo a una frequenza fissa, scritta in CNTFRQ_EL0 (sulla
 * macchina "virt" emulata 62,5 milioni di colpi al secondo; sull'M1 24 milioni). Gli si
 * dice "suona fra N colpi" (CNTV_TVAL_EL0) e lui, quando e' ora, suona sulla linea 27 del
 * GIC. Dopo ogni suono bisogna ricaricarlo: non riparte da solo come il PIT.
 *
 * Si usa il timer "virtuale" (CNTV), quello che il kernel ha sempre a disposizione, anche
 * dentro una macchina virtuale (hvf). */
#include "timer.h"
#include "gic.h"

#define LINEA_TIMER 27

static volatile unsigned int tick;   /* volatile: cambia "da solo", dentro l'interruzione */
static unsigned long colpi_per_tick;

static void ricarica(void) {
    __asm__ volatile("msr cntv_tval_el0, %0" : : "r"(colpi_per_tick));
}

static void al_tick(void) {
    tick++;
    ricarica();
}

void timer_prepara(unsigned int hz) {
    unsigned long frequenza;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(frequenza));
    colpi_per_tick = frequenza / hz;
    gic_collega(LINEA_TIMER, al_tick);
    ricarica();
    __asm__ volatile("msr cntv_ctl_el0, %0" : : "r"(1UL));   /* acceso, non mascherato */
}

unsigned int timer_tick(void) { return tick; }
