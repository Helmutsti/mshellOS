/* La fila degli eventi.
 *
 * Tastiera e mouse sono dispositivi diversi, ma chi li usa (il compositore) li vuole
 * in un posto solo: i due gestori mettono tutto in un'unica fila. Il compositore la
 * apre come un descrittore e ci riceve un evento alla volta; con aspetta puo' guardarla
 * insieme ai socket delle applicazioni. (In Linux sono i file di /dev/input.)
 *
 * La fila e' un anello: i gestori scrivono in 'testa', chi legge prende da 'coda'.
 * Quando arrivano in fondo ricominciano dall'inizio.
 *
 * GLI SPOSTAMENTI SI SOMMANO. Il mouse manda decine di spostamenti al secondo. Se il
 * compositore e' lento a prenderli, non serve tenerli tutti: se l'ultimo evento in fila
 * e' uno spostamento con gli stessi pulsanti, il nuovo si somma a quello. Il puntatore
 * arriva nello stesso posto, con meno lavoro. (Linux fa lo stesso.) */
#include "kernel.h"
#include "descrittori.h"
#include "eventi.h"

#define DIM_FILA 64
static struct evento fila[DIM_FILA];
static unsigned int testa, coda;

static struct aperto aperto;   /* la fila come cosa aperta: una sola, di un solo compito */
static int presa;

void eventi_metti(const struct evento *e) {
    if (testa != coda && e->tipo == EVENTO_MOUSE) {
        struct evento *ultimo = &fila[(testa + DIM_FILA - 1) % DIM_FILA];
        if (ultimo->tipo == EVENTO_MOUSE && ultimo->pulsanti == e->pulsanti) {
            ultimo->dx += e->dx;
            ultimo->dy += e->dy;
            return;   /* chi aspetta e' gia' stato svegliato dal primo */
        }
    }
    unsigned int prossima = (testa + 1) % DIM_FILA;
    if (prossima == coda) return;   /* piena: si perde */
    fila[testa] = *e;
    testa = prossima;
    aperto_sveglia(&aperto);
}

/* --- le operazioni della fila come cosa aperta --- */

static int ricevi(struct aperto *a, void *dove, unsigned int byte, struct aperto **allegato) {
    *allegato = 0;
    if (byte < sizeof(struct evento)) return -1;   /* un evento non si spezza */
    while (testa == coda) aperto_aspetta(a);
    *(struct evento *)dove = fila[coda];
    coda = (coda + 1) % DIM_FILA;
    return sizeof(struct evento);
}

static int manda(struct aperto *a, const void *da, unsigned int byte, struct aperto *allegato) {
    (void)a; (void)da; (void)byte; (void)allegato;
    return -1;   /* gli eventi li mette solo il kernel */
}

static int pronto(struct aperto *a) { (void)a; return testa != coda; }

static void chiudi(struct aperto *a) { (void)a; presa = 0; }

static const struct operazioni operazioni = { ricevi, manda, pronto, chiudi };

struct aperto *eventi_apri(void) {
    if (presa) return 0;
    presa = 1;
    aperto.op = &operazioni;
    return &aperto;
}
