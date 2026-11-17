/* CoreKit, il recinto delle app, nella SANDBOX: al posto di corekit/interno/recinto.c.
 *
 * Docker toglie al container i poteri per costruire i recinti (montare, mondi nuovi del
 * kernel). Decisione della persona (18/11/2026): nella sandbox le app partono SENZA recinto,
 * con le loro porte e il loro uid come nel sistema vero; il recinto si prova in QEMU.
 *
 * Come per lo schermo (schermo_sandbox.c), e' un oggetto sciolto legato prima delle librerie,
 * solo nel launcher: definisce le tre funzioni che lancia (corekit.c) usa per il recinto, e
 * dall'archivio non si prende recinto.o. Nessun mondo nuovo (clone diventa una fork normale),
 * nessuna radice nuova, nessun filtro. */
#include "../../utente/libreria/corekit/interno/interno.h"

int recinto_mondi(const struct recinto *r) {
    (void)r;
    return 0;
}

int recinto_costruisci(const struct recinto *r) {
    (void)r;
    return 0;
}

int recinto_chiudi(void) {
    return 0;
}
