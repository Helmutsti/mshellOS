/* La seriale PL011, sulla macchina "virt" di QEMU.
 *
 * SU ARM NON CI SONO LE PORTE. Su x86 i dispositivi si parlano con le istruzioni in e out,
 * in uno spazio di indirizzi a parte. Su ARM ogni dispositivo e' un PEZZO DI MEMORIA
 * ("I/O mappato in memoria", MMIO): i suoi registri hanno un indirizzo, e leggerli o
 * scriverli con una normale load/store vuol dire parlare con lui. La seriale sta a
 * 0x09000000:
 *   +0x00  DR  (dato): scrivere un byte qui lo spedisce
 *   +0x18  FR  (stato): il bit 5 (TXFF) dice "la coda d'uscita e' piena, aspetta"
 * QEMU la lascia gia' pronta: non serve prepararla.
 *
 * VOLATILE. Il compilatore pensa che scrivere due volte nello stesso posto sia inutile,
 * e che rileggere un valore appena letto pure: per la memoria normale e' vero, per un
 * dispositivo no (ogni scrittura e' una lettera spedita, ogni lettura puo' dare un
 * valore nuovo). 'volatile' gli dice: fai ogni accesso, esattamente come scritto. */
#include "seriale.h"

#define SERIALE   0x09000000UL
#define DR        0x00
#define FR        0x18
#define FR_PIENA  (1u << 5)

static inline volatile unsigned int *registro(unsigned long scostamento) {
    return (volatile unsigned int *)(SERIALE + scostamento);
}

void seriale_scrivi(char c) {
    if (c == '\n') seriale_scrivi('\r');   /* i terminali vogliono anche "torna a capo" */
    while (*registro(FR) & FR_PIENA)
        ;                                  /* coda piena: si aspetta */
    *registro(DR) = (unsigned char)c;
}

void seriale_testo(const char *s) {
    while (*s) seriale_scrivi(*s++);
}

void seriale_esadecimale(unsigned long n) {
    seriale_testo("0x");
    int cominciato = 0;
    for (int b = 60; b >= 0; b -= 4) {
        int cifra = (n >> b) & 0xF;
        if (cifra || cominciato || b == 0) {
            seriale_scrivi("0123456789abcdef"[cifra]);
            cominciato = 1;
        }
    }
}
