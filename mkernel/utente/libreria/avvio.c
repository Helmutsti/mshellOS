/* L'inizio di ogni programma.
 *
 * Il kernel fa partire un programma dall'indirizzo scritto nel suo file (l'"entry
 * point" dell'ELF): e' _start, qui. Viene legato dentro ogni programma, prima del suo
 * codice. Chiama main e, se main finisce, chiede al kernel di chiudere il programma:
 * cosi' chi scrive un programma non deve ricordarsi di farlo. */
#include "libreria.h"

int main(void);

void _start(void) {
    main();
    esci();
}
