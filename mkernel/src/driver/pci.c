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
 * LO SPORTELLO lo da' la CPU (arch_pci_leggi): su x86 sono le porte 0xCF8/0xCFC
 * (arch/x86/dispositivi/pci_porte.c), su ARM un pezzo di memoria ("ECAM") dove ogni
 * stanza ha la sua scheda a un indirizzo fisso. Questo file e' NEUTRO: il giro delle
 * stanze e' uguale per tutte. Stanza vuota: vendor 0xFFFF.
 *
 * Si bussa a tutte le stanze di tutti i piani: 256 x 32 = 8192 domande, una frazione
 * di secondo. Un sistema vero segue i "ponti" (schede che portano a un altro piano)
 * per non bussare a vuoto; per noi la forza bruta basta. */
#include "kernel.h"
#include "pci.h"

#define MAX_SCHEDE   64

static struct pci_scheda schede[MAX_SCHEDE];
static int quante;

static unsigned int leggi(unsigned char bus, unsigned char slot, unsigned char funzione,
                          unsigned char riga) {
    return arch_pci_leggi(bus, slot, funzione, riga);
}

unsigned int pci_leggi(const struct pci_scheda *s, unsigned char riga) {
    return leggi(s->bus, s->slot, s->funzione, riga);
}

void pci_scrivi(const struct pci_scheda *s, unsigned char riga, unsigned int valore) {
    arch_pci_scrivi(s->bus, s->slot, s->funzione, riga, valore);
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
    if (!arch_pci_prepara()) return 0;   /* questa macchina non ha un bus PCI */
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

int pci_quante(void) { return quante; }
const struct pci_scheda *pci_scheda(int i) { return i >= 0 && i < quante ? &schede[i] : 0; }

/* I BAR: il bit 0 dice "porte" (1) o "memoria" (0); per la memoria i bit 2-1 dicono
 * "a 32 bit" (00) o "a 64 bit" (10: la parte alta sta nel BAR dopo). L'indirizzo e'
 * il resto, senza i 4 bit bassi. */
indirizzo_t pci_bar_memoria(const struct pci_scheda *s, int n) {
    if (n < 0 || n > 5) return 0;
    unsigned int bar = s->bar[n];
    if (bar & 1) return 0;                          /* sono porte */
    unsigned long long a = bar & ~0xFu;
    if (((bar >> 1) & 3) == 2 && n < 5)              /* a 64 bit */
        a |= (unsigned long long)s->bar[n + 1] << 32;
    if (a != (indirizzo_t)a) return 0;              /* non ci sta in un indirizzo di questa CPU */
    return (indirizzo_t)a;
}

/* Il registro dei comandi (riga 0x04, i 16 bit bassi): bit 1 "rispondi sulla tua
 * memoria", bit 2 "bus master". */
void pci_accendi(const struct pci_scheda *s) {
    unsigned int c = pci_leggi(s, 0x04);
    pci_scrivi(s, 0x04, (c & 0xFFFF) | 0x6);
}

/* Le capacita': se il bit 4 dello stato (riga 0x04, parte alta) e' acceso, la riga 0x34
 * dice dove comincia l'elenco; ogni voce ha il tipo (1 byte) e dove sta la prossima. */
unsigned char pci_capacita(const struct pci_scheda *s, unsigned char tipo, unsigned char dopo) {
    if (!(pci_leggi(s, 0x04) >> 16 & 0x10)) return 0;
    unsigned char p = dopo ? (pci_leggi(s, dopo) >> 8) & 0xFC : pci_leggi(s, 0x34) & 0xFC;
    for (int giri = 0; p && giri < 48; giri++) {    /* un limite: un elenco rovinato non gira per sempre */
        unsigned int v = pci_leggi(s, p);
        if ((v & 0xFF) == tipo) return p;
        p = (v >> 8) & 0xFC;
    }
    return 0;
}
