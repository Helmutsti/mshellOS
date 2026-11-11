/* Il disco ATA (IDE) dei PC. */
#ifndef ATA_H
#define ATA_H

#include "disco.h"

/* Cerca il disco principale; se c'e', lo iscrive allo sportello dei dischi. 1 se c'e'. */
int ata_prepara(void);

#endif
