/* La MMU di ARM64: la traduzione degli indirizzi e i permessi. */
#ifndef MMU_H
#define MMU_H

/* Con la MMU spenta (la chiama kernel_head.s): il kernel in alto, la mappa provvisoria
 * (virtuale = fisico) in basso, e i registri della traduzione. Vedi mmu.c. */
void mmu_avvio(void);

/* In alto, letto il device tree: la mappa diretta della RAM e la voce dei dispositivi. */
void mmu_prepara(void);

/* La tabella L1 della zona dei dispositivi (la usa pag_mappa_dispositivo). */
unsigned long *mmu_l1_dispositivi(void);

#endif
