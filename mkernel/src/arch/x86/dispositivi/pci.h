/* Il bus PCI: l'elenco delle schede del PC. */
#ifndef PCI_H
#define PCI_H

/* Una scheda trovata: dove sta (piano, stanza, letto) e cosa c'e' scritto sulla sua
 * scheda anagrafica. */
struct pci_scheda {
    unsigned char bus, slot, funzione;
    unsigned short vendor, device;
    unsigned char classe, sottoclasse, interfaccia;   /* il mestiere: es. 0C 03 00 = USB UHCI */
    unsigned char irq;                                /* la linea IRQ che il BIOS le ha dato */
    unsigned int bar[6];                              /* dove ha porte o memoria (grezzi) */
};

/* Bussa a tutte le stanze e si annota chi c'e'. Ritorna quante schede ha trovato. */
int pci_prepara(void);

/* Stampa l'elenco, una riga per scheda. */
void pci_mostra(void);

/* La prima scheda con questo mestiere (interfaccia -1 = qualsiasi), dopo 'dopo'
 * (0 = dall'inizio). Ritorna 0 se non ce n'e'. */
const struct pci_scheda *pci_trova(int classe, int sottoclasse, int interfaccia,
                                   const struct pci_scheda *dopo);

/* La scheda anagrafica, letta e scritta a 4 byte alla volta ('riga' multiplo di 4). */
unsigned int pci_leggi(const struct pci_scheda *s, unsigned char riga);
void pci_scrivi(const struct pci_scheda *s, unsigned char riga, unsigned int valore);

#endif
