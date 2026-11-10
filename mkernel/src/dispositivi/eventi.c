/* La fila degli eventi.
 *
 * Tastiera e mouse sono dispositivi diversi, ma chi li usa (il compositore) deve
 * aspettarli INSIEME: se dormisse aspettando un tasto, non si accorgerebbe che il mouse
 * si muove. Allora i due gestori mettono tutto in un'unica fila, e il compositore ha
 * una sola cosa da fare: "dammi il prossimo evento".
 *
 * La fila e' un anello: i gestori scrivono in 'testa', chi legge prende da 'coda'.
 * Quando arrivano in fondo ricominciano dall'inizio.
 *
 * GLI SPOSTAMENTI SI SOMMANO. Il mouse manda decine di spostamenti al secondo. Se il
 * compositore e' lento a prenderli, non serve tenerli tutti: se l'ultimo evento in fila
 * e' uno spostamento con gli stessi pulsanti, il nuovo si somma a quello. Il puntatore
 * arriva nello stesso posto, con meno lavoro. (Linux fa lo stesso.) */
#include "kernel.h"
#include "compiti.h"
#include "eventi.h"

#define DIM_FILA 64
static struct evento fila[DIM_FILA];
static unsigned int testa, coda;

/* Il compito che dorme aspettando un evento. UNO SOLO: se fossero due, ogni evento
 * andrebbe a uno a caso. */
static struct compito *in_attesa;

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
    if (in_attesa) {
        compito_sveglia(in_attesa);
        in_attesa = 0;
    }
}

/* Perche' le interruzioni spente: tra "la fila e' vuota" e "mi metto in attesa"
 * potrebbe arrivare proprio un evento. Il gestore non vedrebbe nessuno da svegliare, e
 * noi dormiremmo con un evento gia' in fila (la "sveglia persa"). */
int eventi_aspetta(struct evento *e) {
    for (;;) {
        unsigned int s = interruzioni_spegni();
        if (testa != coda) {
            *e = fila[coda];
            coda = (coda + 1) % DIM_FILA;
            interruzioni_ripristina(s);
            return 1;
        }
        if (in_attesa && in_attesa != compito_attuale()) { interruzioni_ripristina(s); return 0; }
        in_attesa = compito_attuale();
        compito_aspetta();          /* dorme qui; al risveglio si ricontrolla la fila */
        interruzioni_ripristina(s);
    }
}
