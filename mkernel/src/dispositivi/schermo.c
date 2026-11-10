/* Lo schermo come cosa aperta.
 *
 * Come /dev/fb0 in Linux: chi vuole disegnare sullo schermo lo APRE e riceve un
 * descrittore; poi lo mette nella sua memoria (mappa_schermo) e ci scrive i pixel.
 * Finche' qualcuno tiene aperto lo schermo, la console del kernel non ci disegna.
 *
 * PERCHE' UN DESCRITTORE. Un descrittore si puo' tenere in mano in due e passare a un
 * altro programma. Cosi' e' init ad aprire lo schermo e a prestarlo al compositore: se
 * il compositore cade, init lo tiene ancora, e nessun altro puo' prenderlo nel frattempo.
 * Lo schermo torna alla console solo quando NESSUNO lo tiene piu'. */
#include "kernel.h"
#include "descrittori.h"
#include "schermo.h"

static struct aperto aperto;
static int aperto_ora;

static int ricevi(struct aperto *a, void *dove, unsigned int byte, struct aperto **allegato) {
    (void)a; (void)dove; (void)byte;
    *allegato = 0;
    return -1;   /* lo schermo non si legge: si mappa */
}

static int manda(struct aperto *a, const void *da, unsigned int byte, struct aperto *allegato,
                 int subito) {
    (void)a; (void)da; (void)byte; (void)allegato; (void)subito;
    return -1;
}

static int pronto(struct aperto *a) { (void)a; return 0; }

/* L'ultimo l'ha lasciato: lo schermo torna alla console, che ridisegna il suo testo. */
static void chiudi(struct aperto *a) {
    console_riprendi(a);
    aperto_ora = 0;
}

static const struct operazioni operazioni = { ricevi, manda, pronto, chiudi };

struct aperto *schermo_apri(void) {
    if (aperto_ora || !console_presta(&aperto)) return 0;
    aperto_ora = 1;
    aperto.op = &operazioni;
    return &aperto;
}

int e_lo_schermo(struct aperto *a) { return a->op == &operazioni; }
