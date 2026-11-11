/* Lo sportello del PCI su ARM64: la finestra ECAM, e i BAR da assegnare.
 *
 * LA FINESTRA ECAM. Su ARM non ci sono le porte 0xCF8/0xCFC. La scheda anagrafica di ogni
 * stanza sta in un pezzo di memoria fisso: dall'inizio della finestra, 1 MB per piano,
 * 32 KB per stanza, 4 KB per letto:
 *     indirizzo = inizio + (bus << 20) + (slot << 15) + (funzione << 12) + riga
 * Dov'e' la finestra, lo dice il device tree: il nodo "pci-host-ecam-generic", nel suo reg
 * (su QEMU "virt", a 0x40_1000_0000: 256 GB). La si mappa nella zona dei dispositivi.
 *
 * I BAR, DA ASSEGNARE. Su un PC il BIOS da' a ogni scheda il suo indirizzo (i BAR) prima
 * di avviare il kernel. Qui QEMU avvia il kernel direttamente, senza firmware: i BAR sono
 * a zero, e tocca a noi. Per ogni BAR:
 *   1. "quanto sei grande?": ci si scrive tutto a 1 e si rilegge. La scheda tiene a 1 solo
 *      i bit che puo' cambiare: gli zeri in fondo dicono la misura (es. ...FFFFC000 = 16 KB);
 *   2. gli si da' un posto nella FINESTRA DELLA MEMORIA PCI, allineato alla sua misura
 *      (ogni BAR vuole stare a un multiplo della sua grandezza).
 * La finestra la dice il device tree, nella proprieta' "ranges" del nodo PCI: una lista
 * di voci (spazio PCI: 3 celle, indirizzo per la CPU: 2 celle, misura: 2 celle). Si usa
 * quella della "memoria a 32 bit" (spazio 0x02): su "virt" da 0x10000000, 750 MB.
 * (Linux fa lo stesso quando il firmware non l'ha fatto: "pci=assign-busses".) */
#include "kernel.h"
#include "pci.h"
#include "albero.h"
#include "paginazione.h"

static unsigned long ecam;                 /* l'inizio della finestra ECAM (dove la vede il kernel) */
static unsigned long finestra, finestra_fine;   /* la memoria PCI a 32 bit, da distribuire */

static volatile unsigned int *cella(unsigned char bus, unsigned char slot, unsigned char funzione,
                                    unsigned char riga) {
    return (volatile unsigned int *)(ecam + ((unsigned long)bus << 20) + ((unsigned long)slot << 15) +
                                     ((unsigned long)funzione << 12) + (riga & 0xFC));
}

unsigned int arch_pci_leggi(unsigned char bus, unsigned char slot, unsigned char funzione,
                            unsigned char riga) {
    return *cella(bus, slot, funzione, riga);
}

void arch_pci_scrivi(unsigned char bus, unsigned char slot, unsigned char funzione,
                     unsigned char riga, unsigned int valore) {
    *cella(bus, slot, funzione, riga) = valore;
}

/* La finestra della memoria a 32 bit, dal "ranges" del nodo PCI. */
static int trova_finestra(void) {
    const void *r;
    unsigned long byte;
    if (!albero_proprieta("pci-host-ecam-generic", "ranges", &r, &byte)) return 0;
    for (unsigned long v = 0; (v + 1) * 7 * 4 <= byte; v++) {
        unsigned int spazio = (albero_cella(r, v * 7) >> 24) & 3;   /* 1 porte, 2 mem32, 3 mem64 */
        unsigned long cpu = (unsigned long)albero_cella(r, v * 7 + 3) << 32 | albero_cella(r, v * 7 + 4);
        unsigned long misura = (unsigned long)albero_cella(r, v * 7 + 5) << 32 | albero_cella(r, v * 7 + 6);
        if (spazio == 2) {
            finestra = cpu;
            finestra_fine = cpu + misura;
            return 1;
        }
    }
    return 0;
}

/* Un posto di 'misura' byte (una potenza di 2), allineato, nella finestra. 0 se e' finita. */
static unsigned long prendi_posto(unsigned long misura) {
    unsigned long a = (finestra + misura - 1) & ~(misura - 1);
    if (a + misura > finestra_fine || a + misura < a) return 0;
    finestra = a + misura;
    return a;
}

/* I BAR di una funzione: misura, posto, scrittura. Solo quelli di memoria (i dispositivi
 * moderni non usano le porte). Un BAR "a 64 bit" usa anche il numero dopo: lo si mette
 * comunque sotto i 4 GB, nella finestra a 32 bit. */
static void assegna_bar(unsigned char bus, unsigned char slot, unsigned char funzione) {
    unsigned int tipo = arch_pci_leggi(bus, slot, funzione, 0x0C) >> 16 & 0x7F;
    int quanti = tipo == 0 ? 6 : tipo == 1 ? 2 : 0;   /* una scheda normale ne ha 6, un ponte 2 */
    unsigned int comandi = arch_pci_leggi(bus, slot, funzione, 0x04);
    arch_pci_scrivi(bus, slot, funzione, 0x04, comandi & ~0x3u);   /* spenta mentre si misura */

    for (int n = 0; n < quanti; n++) {
        unsigned char riga = (unsigned char)(0x10 + 4 * n);
        unsigned int prima = arch_pci_leggi(bus, slot, funzione, riga);
        if (prima & 1) continue;                          /* porte: le saltiamo */
        int a64 = ((prima >> 1) & 3) == 2;
        arch_pci_scrivi(bus, slot, funzione, riga, 0xFFFFFFFF);
        unsigned int maschera = arch_pci_leggi(bus, slot, funzione, riga) & ~0xFu;
        unsigned int alta = 0xFFFFFFFF;
        if (a64) {
            arch_pci_scrivi(bus, slot, funzione, riga + 4, 0xFFFFFFFF);
            alta = arch_pci_leggi(bus, slot, funzione, riga + 4);
        }
        unsigned long misura = ~((unsigned long)alta << 32 | maschera) + 1;
        unsigned long posto = maschera ? prendi_posto(misura) : 0;
        arch_pci_scrivi(bus, slot, funzione, riga, (unsigned int)posto | (prima & 0xF));
        if (a64) arch_pci_scrivi(bus, slot, funzione, riga + 4, (unsigned int)(posto >> 32));
        if (a64) n++;                                      /* il BAR dopo era la meta' alta */
    }
    arch_pci_scrivi(bus, slot, funzione, 0x04, comandi);
}

int arch_pci_prepara(void) {
    unsigned long dimensione;
    unsigned long fisico;
    if (!albero_cerca("pci-host-ecam-generic", &fisico, &dimensione)) return 0;
    /* la finestra nella zona dei dispositivi (256 MB su "virt": a blocchi da 2 MB) */
    if (dimensione > 0xFFFFFFFFUL) dimensione = 0x10000000UL;   /* 256 piani bastano */
    ecam = pag_mappa_dispositivo(fisico, (unsigned int)dimensione);
    if (!ecam) return 0;
    if (!trova_finestra()) return 0;
    /* il piano 0 (su "virt" i dispositivi stanno tutti li'): a ogni funzione i suoi BAR */
    for (unsigned char slot = 0; slot < 32; slot++)
        for (unsigned char f = 0; f < 8; f++) {
            unsigned int id = arch_pci_leggi(0, slot, f, 0x00);
            if ((id & 0xFFFF) == 0xFFFF) { if (f == 0) break; continue; }
            assegna_bar(0, slot, f);
            if (f == 0 && !(arch_pci_leggi(0, slot, 0, 0x0C) >> 16 & 0x80)) break;   /* un letto solo */
        }
    return 1;
}
