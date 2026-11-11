/* Il registro dei driver (vedi driver.h). */
#include "kernel.h"
#include "driver.h"

/* L'inizio e la fine della fila delle iscrizioni: li crea il linker. "weak": se nessun
 * driver si e' iscritto, la sezione non c'e', e valgono 0 (la fila e' vuota). */
extern const struct driver *const __start_registro_driver[] __attribute__((weak));
extern const struct driver *const __stop_registro_driver[] __attribute__((weak));

static int riconosce(const struct id_pci *id, const struct pci_scheda *s) {
    for (; id->vendor || id->device || id->classe; id++) {
        if (id->vendor != PCI_QUALSIASI && id->vendor != s->vendor) continue;
        if (id->device != PCI_QUALSIASI && id->device != s->device) continue;
        int classe = s->classe << 16 | s->sottoclasse << 8 | s->interfaccia;
        if (id->classe != -1 && id->classe != classe) continue;
        return 1;
    }
    return 0;
}

void driver_avvia_pci(void) {
    for (int i = 0; i < pci_quante(); i++) {
        const struct pci_scheda *s = pci_scheda(i);
        for (const struct driver *const *d = __start_registro_driver; d < __stop_registro_driver; d++) {
            if (!(*d)->pci || !(*d)->prepara_pci || !riconosce((*d)->pci, s)) continue;
            kprintf("Driver  : %s per %02x:%02x.%u (%04x:%04x)", (*d)->nome,
                    s->bus, s->slot, s->funzione, s->vendor, s->device);
            int ok = (*d)->prepara_pci(s);
            kprintf(ok ? "\n" : " -- non ce l'ha fatta\n");
            if (ok) break;
        }
    }
}
