/* IL REGISTRO DEI DRIVER: una libreria di driver, ognuno chiamato quando serve.
 *
 * Ogni driver dichiara CHE COSA SA GESTIRE, con la "carta d'identita'" dell'hardware:
 *   - sul bus PCI: costruttore e modello (vendor:device, es. 1af4:1052 una tastiera
 *     virtio), oppure il mestiere (la classe);
 *   - nel device tree (ARM): il "compatible" (es. "arm,pl011").
 * All'avvio il kernel elenca l'hardware che trova e, per ognuno, cerca nel registro un
 * driver che lo riconosce: se c'e', lo chiama ("e' tuo: preparalo").
 *
 * COME SI ISCRIVE UN DRIVER: con una riga, DRIVER(nome), nel suo file. La riga mette il
 * driver in una sezione speciale del file compilato ("registro_driver"); il linker le
 * mette tutte in fila, e crea da solo due nomi, __start_registro_driver e
 * __stop_registro_driver, all'inizio e alla fine della fila. Aggiungere un driver vuol
 * dire solo aggiungere il suo file: nessun elenco da tenere aggiornato. (Linux fa cosi'
 * con module_pci_driver e le sue "initcall".) */
#ifndef DRIVER_H
#define DRIVER_H

#include "pci.h"

/* Una carta d'identita' PCI. vendor/device 0xFFFF = qualsiasi; classe -1 = qualsiasi
 * (altrimenti 0xCCSSII: classe, sottoclasse, interfaccia). L'elenco finisce con
 * una voce tutta a zero. */
struct id_pci {
    unsigned short vendor, device;
    int classe;
};
#define PCI_QUALSIASI 0xFFFF

struct driver {
    const char *nome;
    const struct id_pci *pci;          /* cosa riconosce sul bus PCI (0: niente) */
    /* "e' tuo: preparalo". 1 se ce l'ha fatta, 0 se no (si prova un altro driver). */
    int (*prepara_pci)(const struct pci_scheda *s);
};

#define DRIVER(d) \
    static const struct driver *const iscrizione_##d \
        __attribute__((section("registro_driver"), used)) = &d

/* Per ogni scheda PCI, il primo driver che la riconosce. Racconta chi ha preso cosa. */
void driver_avvia_pci(void);

#endif
