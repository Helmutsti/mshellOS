/* Lo sportello del PCI su x86: due porte.
 *
 * Su 0xCF8 si scrive "piano, stanza, letto, riga" (con il bit 31 acceso: "e' una
 * domanda"), e da 0xCFC si legge (o ci si scrive) la risposta, 4 byte. Il giro delle
 * stanze e il resto sono neutri: driver/pci.c. */
#include "kernel.h"
#include "io.h"
#include "pci.h"

#define PCI_DOMANDA  0xCF8
#define PCI_RISPOSTA 0xCFC

static unsigned int indirizzo(unsigned char bus, unsigned char slot, unsigned char funzione,
                              unsigned char riga) {
    return 0x80000000u | (unsigned int)bus << 16 | (unsigned int)slot << 11 |
           (unsigned int)funzione << 8 | (riga & 0xFC);
}

int arch_pci_prepara(void) { return 1; }   /* su un PC il bus PCI c'e' sempre */

unsigned int arch_pci_leggi(unsigned char bus, unsigned char slot, unsigned char funzione,
                            unsigned char riga) {
    outl(PCI_DOMANDA, indirizzo(bus, slot, funzione, riga));
    return inl(PCI_RISPOSTA);
}

void arch_pci_scrivi(unsigned char bus, unsigned char slot, unsigned char funzione,
                     unsigned char riga, unsigned int valore) {
    outl(PCI_DOMANDA, indirizzo(bus, slot, funzione, riga));
    outl(PCI_RISPOSTA, valore);
}
