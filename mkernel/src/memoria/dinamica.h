/* La memoria dinamica dei programmi: il confine (la "rottura") e le pagine promesse. */
#ifndef DINAMICA_H
#define DINAMICA_H

/* La chiamata rottura(nuova): sposta il confine del compito attuale. Restituisce il
 * confine dopo la chiamata (quello di prima, se 'nuova' non va bene; con 0 si chiede
 * solo dov'e'). */
unsigned int memoria_rottura(unsigned int nuova);

/* L'indirizzo 'virt' del compito attuale e' una pagina promessa (sotto il confine) che
 * non c'e' ancora? Allora la si da' adesso, piena di zeri: 1. Se no 0 (e' un errore
 * vero). La chiama il gestore del page fault, e il kernel prima di toccare la memoria
 * di un programma. */
int memoria_promessa(unsigned int virt);

#endif
