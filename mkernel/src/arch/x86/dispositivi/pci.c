/* Il bus PCI: il registro dell'albergo.
 *
 * Ogni scheda del PC (controller USB, disco, video, rete...) sta in una stanza con tre
 * numeri: BUS (il piano, 0-255), SLOT (la stanza, 0-31), FUNZIONE (il letto, 0-7: una
 * scheda puo' fare piu' lavori, ognuno col suo letto). Ogni stanza ha una SCHEDA
 * ANAGRAFICA di 256 byte, lo "spazio di configurazione":
 *
 *   riga 0x00: vendor (chi l'ha fatta) e device (che modello e')
 *   riga 0x08: classe, sottoclasse, interfaccia (che mestiere fa), revisione
 *   riga 0x0C: tipo d'intestazione (bit 7: "ho altri letti oltre allo 0")
 *   righe 0x10-0x24: i sei BAR, dove la scheda ha le sue porte o la sua memoria
 *   riga 0x3C: la linea IRQ
 *
 * LO SPORTELLO. La scheda non si legge con un indirizzo di memoria: si scrive sulla
 * porta 0xCF8 "piano, stanza, letto, riga" (con il bit 31 acceso: "e' una domanda"),
 * e dalla porta 0xCFC si legge la risposta, 4 byte. Stanza vuota: vendor 0xFFFF.
 *
 * Si bussa a tutte le stanze di tutti i piani: 256 x 32 = 8192 domande, una frazione
 * di secondo. Un sistema vero segue i "ponti" (schede che portano a un altro piano)
 * per non bussare a vuoto; per noi la forza bruta basta. */
#include "kernel.h"
#include "io.h"
#include "pci.h"

#define PCI_DOMANDA  0xCF8
#define PCI_RISPOSTA 0xCFC
#define MAX_SCHEDE   64

static struct pci_scheda schede[MAX_SCHEDE];
static int quante;

static unsigned int indirizzo(unsigned char bus, unsigned char slot, unsigned char funzione,
                              unsigned char riga) {
    return 0x80000000u | (unsigned int)bus << 16 | (unsigned int)slot << 11 |
           (unsigned int)funzione << 8 | (riga & 0xFC);
}

static unsigned int leggi(unsigned char bus, unsigned char slot, unsigned char funzione,
                          unsigned char riga) {
    outl(PCI_DOMANDA, indirizzo(bus, slot, funzione, riga));
    return inl(PCI_RISPOSTA);
}

unsigned int pci_leggi(const struct pci_scheda *s, unsigned char riga) {
    return leggi(s->bus, s->slot, s->funzione, riga);
}

void pci_scrivi(const struct pci_scheda *s, unsigned char riga, unsigned int valore) {
    outl(PCI_DOMANDA, indirizzo(s->bus, s->slot, s->funzione, riga));
    outl(PCI_RISPOSTA, valore);
}

/* C'e' qualcuno in questo letto? Se si', lo si annota. */
static void annota(unsigned char bus, unsigned char slot, unsigned char funzione) {
    unsigned int id = leggi(bus, slot, funzione, 0x00);
    if ((id & 0xFFFF) == 0xFFFF || quante == MAX_SCHEDE) return;

    struct pci_scheda *s = &schede[quante++];
    s->bus = bus; s->slot = slot; s->funzione = funzione;
    s->vendor = id & 0xFFFF;
    s->device = id >> 16;
    unsigned int mestiere = leggi(bus, slot, funzione, 0x08);
    s->classe = mestiere >> 24;
    s->sottoclasse = (mestiere >> 16) & 0xFF;
    s->interfaccia = (mestiere >> 8) & 0xFF;
    s->irq = leggi(bus, slot, funzione, 0x3C) & 0xFF;
    for (int i = 0; i < 6; i++) s->bar[i] = leggi(bus, slot, funzione, 0x10 + 4 * i);
}

int pci_prepara(void) {
    quante = 0;
    for (int bus = 0; bus < 256; bus++)
        for (int slot = 0; slot < 32; slot++) {
            if ((leggi(bus, slot, 0, 0x00) & 0xFFFF) == 0xFFFF) continue;   /* stanza vuota */
            annota(bus, slot, 0);
            /* bit 7 del tipo d'intestazione: la scheda ha anche i letti 1-7 */
            if (leggi(bus, slot, 0, 0x0C) >> 16 & 0x80)
                for (int f = 1; f < 8; f++) annota(bus, slot, f);
        }
    return quante;
}

/* Il mestiere in parole: solo quelli che si incontrano su un PC (vero o virtuale). */
static const char *mestiere(const struct pci_scheda *s) {
    switch (s->classe << 8 | s->sottoclasse) {
    case 0x0101: return "controller dischi IDE";
    case 0x0106: return "controller dischi SATA";
    case 0x0108: return "controller dischi NVMe";
    case 0x0200: return "rete Ethernet";
    case 0x0300: return "scheda video";
    case 0x0401: return "audio";
    case 0x0403: return "audio HD";
    case 0x0600: return "ponte verso la CPU";
    case 0x0601: return "ponte verso l'ISA";
    case 0x0604: return "ponte PCI-PCI";
    case 0x0680: return "ponte (altro)";
    case 0x0700: return "porta seriale";
    case 0x0880: return "periferica di sistema";
    case 0x0C03:
        switch (s->interfaccia) {
        case 0x00: return "USB 1.1 (UHCI)";
        case 0x10: return "USB 1.1 (OHCI)";
        case 0x20: return "USB 2.0 (EHCI)";
        case 0x30: return "USB 3 (xHCI)";
        default:   return "USB";
        }
    case 0x0C05: return "SMBus";
    case 0x00FF: case 0xFF00: return "sconosciuto";
    }
    return "altro";
}

void pci_mostra(void) {
    for (int i = 0; i < quante; i++) {
        const struct pci_scheda *s = &schede[i];
        kprintf("  %02x:%02x.%u  %04x:%04x  %02x%02x%02x  irq %2u  %s\n",
                s->bus, s->slot, s->funzione, s->vendor, s->device,
                s->classe, s->sottoclasse, s->interfaccia, s->irq, mestiere(s));
    }
}

const struct pci_scheda *pci_trova(int classe, int sottoclasse, int interfaccia,
                                   const struct pci_scheda *dopo) {
    int i = dopo ? (int)(dopo - schede) + 1 : 0;
    for (; i < quante; i++)
        if (schede[i].classe == classe && schede[i].sottoclasse == sottoclasse &&
            (interfaccia < 0 || schede[i].interfaccia == interfaccia))
            return &schede[i];
    return 0;
}
