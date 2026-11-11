/* L'applicazione Impostazioni, vera, senza toccarla: il suo main (rinominato da anteprima.c)
 * arriva a toolkit_esegui, che qui disegna e basta. */
#include "../../../utente/applicazioni/Impostazioni/impostazioni.c"

#define HA_RITOCCO
static void ritocco(struct finestra_tk *f) { (void)f; utente.base.fuoco = 1; }
