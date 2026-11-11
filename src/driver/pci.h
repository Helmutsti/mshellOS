/* Il bus PCI: l'elenco delle schede della macchina. Neutro: il giro delle stanze e'
 * uguale su ogni CPU; cambia solo lo sportello per leggere la scheda anagrafica, che
 * da' la CPU (arch_pci_leggi, arch_pci_scrivi). */
#ifndef PCI_H
#define PCI_H

#include "kernel.h"   /* indirizzo_t */

/* Una scheda trovata: dove sta (piano, stanza, letto) e cosa c'e' scritto sulla sua
 * scheda anagrafica. */
struct pci_scheda {
    unsigned char bus, slot, funzione;
    unsigned short vendor, device;
    unsigned char classe, sottoclasse, interfaccia;   /* il mestiere: es. 0C 03 00 = USB UHCI */
    unsigned char irq;                                /* la linea IRQ che il BIOS le ha dato */
    unsigned int bar[6];                              /* dove ha porte o memoria (grezzi) */
};

/* --- lo sportello, dato dalla CPU (su x86 le porte 0xCF8/0xCFC, su ARM la memoria) ---
 * La scheda anagrafica, a 4 byte alla volta ('riga' multiplo di 4). 0 se la CPU non ha
 * un bus PCI. */
int arch_pci_prepara(void);
unsigned int arch_pci_leggi(unsigned char bus, unsigned char slot, unsigned char funzione,
                            unsigned char riga);
void arch_pci_scrivi(unsigned char bus, unsigned char slot, unsigned char funzione,
                     unsigned char riga, unsigned int valore);

/* Bussa a tutte le stanze e si annota chi c'e'. Ritorna quante schede ha trovato. */
int pci_prepara(void);

/* Stampa l'elenco, una riga per scheda. */
void pci_mostra(void);

/* Quante schede, e la numero i (0 = la prima). */
int pci_quante(void);
const struct pci_scheda *pci_scheda(int i);

/* La prima scheda con questo mestiere (interfaccia -1 = qualsiasi), dopo 'dopo'
 * (0 = dall'inizio). Ritorna 0 se non ce n'e'. */
const struct pci_scheda *pci_trova(int classe, int sottoclasse, int interfaccia,
                                   const struct pci_scheda *dopo);

/* La scheda anagrafica, letta e scritta a 4 byte alla volta ('riga' multiplo di 4). */
unsigned int pci_leggi(const struct pci_scheda *s, unsigned char riga);
void pci_scrivi(const struct pci_scheda *s, unsigned char riga, unsigned int valore);

/* Il BAR numero n: dov'e' la memoria della scheda (0 se e' vuoto o e' di porte).
 * Un BAR "a 64 bit" usa anche il numero dopo: lo si mette insieme qui. */
indirizzo_t pci_bar_memoria(const struct pci_scheda *s, int n);

/* Accende la scheda: puo' rispondere sulla sua memoria, e puo' leggere e scrivere da
 * sola nella RAM ("bus master": serve ai dispositivi che si passano dati col kernel). */
void pci_accendi(const struct pci_scheda *s);

/* Le "capacita'": un elenco di schede aggiuntive dentro la scheda anagrafica (virtio ci
 * descrive dove stanno le sue parti). La prima con questo tipo dopo la posizione 'dopo'
 * (0 = dall'inizio): la sua posizione, o 0. */
unsigned char pci_capacita(const struct pci_scheda *s, unsigned char tipo, unsigned char dopo);

#endif
