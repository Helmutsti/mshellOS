/* kmalloc: la memoria a pezzi piccoli per le strutture del kernel. */
#ifndef KMALLOC_H
#define KMALLOC_H

/* Prepara la zona iniziale (1 MB) chiedendola al magazziniere. */
void kmalloc_prepara(void);

/* Un pezzo di almeno 'byte' byte, allineato a 8. Restituisce 0 se la memoria e' finita. */
void *kmalloc(unsigned int byte);

/* Restituisce un pezzo avuto da kmalloc. kfree(0) non fa niente. */
void kfree(void *p);

/* Stampa l'elenco dei blocchi: indirizzo, dimensione, libero o occupato. */
void kmalloc_mostra(void);

/* Quanti blocchi ci sono e quanti byte liberi in tutto. */
void kmalloc_conta(unsigned int *blocchi, unsigned int *liberi);

#endif
