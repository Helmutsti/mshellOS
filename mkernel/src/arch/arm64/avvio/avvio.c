/* L'avvio su ARM64.
 *
 * Passo B1: il kernel parte e si presenta sulla seriale (livello e CPU).
 * Passo B2: le eccezioni (la tabella dei vettori), il centralino (GIC) e il timer
 * generico: il kernel riceve il tick, 100 volte al secondo, e lo racconta ogni secondo.
 *
 * Per ora non c'e' altro: niente schermo, niente memoria, niente compiti. Si leggono due
 * registri che x86 non ha:
 *   CurrentEL : a che livello sta girando la CPU (bit 3-2): 0 programmi, 1 kernel,
 *               2 hypervisor, 3 firmware. Il kernel deve essere a EL1.
 *   MIDR_EL1  : chi ha fatto la CPU (bit 31-24: 0x41 = ARM, 0x61 = Apple) e che modello e'.
 * Si leggono con l'istruzione mrs ("dal registro di sistema a un registro normale"). */
#include "seriale.h"
#include "eccezioni.h"
#include "gic.h"
#include "timer.h"

extern char kernel_start[], kernel_end[];   /* da linker.ld */

static unsigned long leggi_livello(void) {
    unsigned long v;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(v));
    return (v >> 2) & 3;
}

static unsigned long leggi_midr(void) {
    unsigned long v;
    __asm__ volatile("mrs %0, midr_el1" : "=r"(v));
    return v;
}

static const char *costruttore(unsigned long midr) {
    switch ((midr >> 24) & 0xFF) {
    case 0x41: return "ARM";
    case 0x61: return "Apple";
    default:   return "un altro";
    }
}

static void numero(unsigned long n) {
    char cifre[24];
    int i = 0;
    do { cifre[i++] = (char)('0' + n % 10); n /= 10; } while (n);
    while (i) seriale_scrivi(cifre[--i]);
}

/* Una svc dal kernel stesso: e' l'istruzione delle chiamate di sistema (l'int 0x80 di
 * x86). In x0 un valore; l'eccezione lo stampa e ci rimette 42. */
static unsigned long prova_svc(unsigned long valore) {
    register unsigned long x0 __asm__("x0") = valore;
    __asm__ volatile("svc #7" : "+r"(x0) : : "memory");
    return x0;
}

void kernel_main(void) {
    seriale_testo("\nciao, sono mkernel su ARM64\n");
    seriale_testo("Livello : EL");
    seriale_scrivi((char)('0' + leggi_livello()));
    seriale_testo(leggi_livello() == 1 ? " (il kernel: giusto)\n" : " (non e' EL1!)\n");
    unsigned long midr = leggi_midr();
    seriale_testo("CPU     : MIDR ");
    seriale_esadecimale(midr);
    seriale_testo(", fatta da ");
    seriale_testo(costruttore(midr));
    seriale_testo("\nKernel  : ");
    seriale_esadecimale((unsigned long)kernel_start);
    seriale_testo(" -> ");
    seriale_esadecimale((unsigned long)kernel_end);
    seriale_scrivi('\n');

    /* Le eccezioni: da qui un errore stampa il panico invece di bloccare tutto in silenzio. */
    vettori_prepara();
    seriale_testo("Vettori : caricati (VBAR_EL1)\n");
    unsigned long risposta = prova_svc(0x1234);
    seriale_testo("Prova   : la svc ha risposto ");
    numero(risposta);
    seriale_testo(risposta == 42 ? " (giusto)\n" : " (sbagliato!)\n");

    /* Il centralino e il metronomo, poi si aprono le interruzioni. L'ordine conta, come
     * su x86: prima chi le gestisce, poi le si lascia arrivare. */
    gic_prepara();
    seriale_testo("GIC     : versione ");
    numero((unsigned long)gic_versione());
    seriale_scrivi('\n');
    timer_prepara(100);
    unsigned long frequenza;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(frequenza));
    seriale_testo("Timer   : ");
    numero(frequenza);
    seriale_testo(" colpi al secondo, uno squillo ogni ");
    numero(frequenza / 100);
    seriale_testo(" (100 al secondo)\n");
    /* DAIF sono i quattro "rubinetti" delle eccezioni: D debug, A SError, I IRQ, F FIQ.
     * daifclr #2 apre il rubinetto I: da qui arrivano le IRQ (lo sti di x86). */
    __asm__ volatile("msr daifclr, #2");
    seriale_testo("Interruzioni accese\n");

    /* Si dorme fino alla prossima interruzione; ogni 100 tick (un secondo) si racconta. */
    unsigned int ultimo = 0;
    for (;;) {
        __asm__ volatile("wfi");   /* "aspetta un'interruzione": l'hlt di ARM */
        unsigned int adesso = timer_tick();
        if (adesso / 100 != ultimo / 100) {
            seriale_testo("tick ");
            numero(adesso);
            seriale_testo(" (");
            numero(adesso / 100);
            seriale_testo(" s)\n");
        }
        ultimo = adesso;
    }
}
