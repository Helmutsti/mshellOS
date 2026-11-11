/* Il device tree: la descrizione della macchina. Vedi albero.c per come funziona. */
#ifndef ALBERO_H
#define ALBERO_H

/* Controlla e ricorda il device tree che sta a 'dtb'.
 * 1 se valido (numero magico, versione >= 16, misure coerenti), 0 se no. */
int albero_prepara(const void *dtb);

/* La prima zona di RAM: il primo intervallo del "reg" del nodo /memory
 * (nome "memory" o "memory@..."), letto con #address-cells/#size-cells della radice.
 * 1 se trovata. */
int albero_memoria(unsigned long *inizio, unsigned long *dimensione);

/* Il primo nodo che ha 'compatibile' tra le stringhe della sua proprieta' "compatible"
 * (e' una lista di stringhe separate da \0): restituisce il primo intervallo del suo "reg",
 * letto con #address-cells/#size-cells del nodo PADRE (default 2 e 1 se il padre non li
 * dice). 1 se trovato. */
int albero_cerca(const char *compatibile, unsigned long *indirizzo, unsigned long *dimensione);

/* Come albero_cerca, ma l'intervallo numero 'quale' (0 = il primo) del "reg".
 * Serve per il GIC v3, che ha nel reg prima il distributore e poi i redistributori. */
int albero_cerca_n(const char *compatibile, int quale,
                   unsigned long *indirizzo, unsigned long *dimensione);

/* La dimensione totale del device tree (per proteggerlo dal magazziniere delle pagine). */
unsigned long albero_dimensione(void);

/* Una proprieta' qualsiasi ('nome') del primo nodo compatibile: dove sta il suo valore
 * (dentro il device tree) e quanti byte e'. 1 se trovata. I numeri dentro sono "celle"
 * da 32 bit big-endian: albero_cella(valore, i) da' la cella numero i. */
int albero_proprieta(const char *compatibile, const char *nome, const void **valore,
                     unsigned long *byte);
unsigned int albero_cella(const void *valore, unsigned long i);

#endif
