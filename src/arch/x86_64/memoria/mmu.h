/* La mappa del kernel sul PC: la parte della paginazione che si fa all'avvio (in
 * paginazione.c, come su ARM in mmu.c). */
#ifndef MMU_H
#define MMU_H

#include "consegna.h"

/* Costruisce la mappa del kernel (la meta' alta della pianta di pagine.h: la RAM della
 * consegna nella mappa diretta, la zona dei dispositivi, il kernel stesso), la da' alla
 * CPU e accende la sola lettura anche per il kernel. Da qui le tabelle provvisorie di
 * kernel_head.s non servono piu'. Restituisce dove finisce la RAM mappata (fisico). */
unsigned long mmu_prepara(const struct consegna *c);

/* Quanti pezzi da 2 MB sono stati spezzati in pagine da 4 KB. */
unsigned int mmu_pezzi_spezzati(void);

#endif
