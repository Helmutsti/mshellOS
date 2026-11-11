/* La MMU di ARM64: la traduzione degli indirizzi e i permessi. */
#ifndef MMU_H
#define MMU_H

/* Costruisce la mappa del kernel (identica: virtuale = fisico) e accende la MMU e le
 * cache. Vedi mmu.c. */
void mmu_prepara(void);

/* L'indirizzo della tabella L0 del kernel (la radice della sua mappa). */
unsigned long mmu_tabella_l0(void);

#endif
