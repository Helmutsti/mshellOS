/* Le eccezioni su ARM64: il C chiamato da vettori.s (l'idt.c di x86).
 *
 * Il numero della casella dice che cosa e' successo (vedi vettori.s): 0-3 dal kernel con
 * SP_EL0 (non succede), 4-7 dal kernel, 8-11 da un programma, 12-15 da un programma a 32
 * bit. Dentro ogni gruppo: +0 sincrona, +1 IRQ, +2 FIQ, +3 SError.
 *
 * LE SINCRONE. Il motivo sta in ESR_EL1 ("Exception Syndrome Register"): i bit 31-26 sono
 * la CLASSE (0x15 = svc, 0x24/0x25 = errore di memoria dei dati, 0x20/0x21 = errore
 * eseguendo codice...), i bit sotto i dettagli. Per gli errori di memoria FAR_EL1 dice
 * l'indirizzo che ha dato problemi (come cr2 su x86). */
#include "eccezioni.h"
#include "gic.h"
#include "seriale.h"

#define CLASSE_SVC 0x15

void vettori_prepara(void) {
    extern char tabella_vettori[];
    __asm__ volatile("msr vbar_el1, %0; isb" : : "r"(tabella_vettori));
}

static const char *const nomi_tipo[] = { "sincrona", "IRQ", "FIQ", "SError" };
static const char *const nomi_da_dove[] = {
    "dal kernel (SP_EL0)", "dal kernel", "da un programma", "da un programma a 32 bit"
};

static __attribute__((noreturn)) void ferma(struct registri *r, unsigned long tipo) {
    unsigned long esr, far;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
    __asm__ volatile("mrs %0, far_el1" : "=r"(far));
    seriale_testo("\nPANICO: eccezione ");
    seriale_testo(nomi_tipo[tipo % 4]);
    seriale_testo(" ");
    seriale_testo(nomi_da_dove[tipo / 4]);
    seriale_testo("\n  classe (ESR 31-26): ");
    seriale_esadecimale(esr >> 26);
    seriale_testo("  ESR: ");
    seriale_esadecimale(esr);
    seriale_testo("\n  dove (ELR): ");
    seriale_esadecimale(r->elr);
    seriale_testo("  indirizzo (FAR): ");
    seriale_esadecimale(far);
    seriale_scrivi('\n');
    for (;;) __asm__ volatile("msr daifset, #0xF; wfi");
}

struct registri *eccezione(struct registri *r, unsigned long tipo) {
    switch (tipo % 4) {
    case 1:                                   /* IRQ: il centralino sa chi e' */
        gic_interruzione();
        return r;
    case 0: {                                 /* sincrona: perche'? */
        unsigned long esr;
        __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
        if (esr >> 26 == CLASSE_SVC) {
            /* svc #n: il numero n sta negli ultimi 16 bit. ELR punta gia' all'istruzione
             * DOPO la svc: si torna li'. (Sara' la chiamata di sistema dei programmi.) */
            seriale_testo("Eccezione: svc #");
            seriale_esadecimale(esr & 0xFFFF);
            seriale_testo(" ");
            seriale_testo(nomi_da_dove[tipo / 4]);
            seriale_testo(", x0 = ");
            seriale_esadecimale(r->x[0]);
            seriale_testo(": torno indietro\n");
            r->x[0] = 42;                     /* un "risultato", per vedere che arriva */
            return r;
        }
        ferma(r, tipo);
    }
    default:
        ferma(r, tipo);
    }
}
