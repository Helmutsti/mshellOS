/* Il lucchetto (mutex).
 *
 * Le due operazioni sono a loro volta sezioni critiche brevissime ("e' libero?
 * allora lo prendo" non deve essere interrotto a meta'), quindi girano a
 * interruzioni spente. Ma la RISORSA protetta si usa con le interruzioni accese.
 *
 * PASSAGGIO DI CONSEGNE: quando chi lo tiene lo lascia e qualcuno aspetta, il
 * lucchetto non torna libero: passa direttamente al primo della fila, che viene
 * svegliato gia' padrone. Se tornasse libero, tra il risveglio e la ripartenza un
 * terzo compito potrebbe prenderlo prima: quello svegliato dovrebbe tornare a
 * dormire, e potrebbe succedergli sempre (si chiama "fame", starvation). */
#include "kernel.h"
#include "compiti.h"
#include "lucchetto.h"

void lucchetto_prendi(struct lucchetto *l) {
    unsigned int s = interruzioni_spegni();
    struct compito *io = compito_attuale();   /* 0 prima dello scheduler: c'e' solo kernel_main */

    if (!l->preso) {                           /* libero: e' mio, e si va avanti */
        l->preso = 1;
        l->padrone = io;
        if (io) io->lucchetti++;
        interruzioni_ripristina(s);
        return;
    }

    if (!io || l->padrone == io)
        panico("lucchetto %s: preso due volte dallo stesso compito (si aspetterebbe da solo)", l->nome);

    /* preso: in fondo alla fila, e a dormire */
    io->dopo_in_fila = 0;
    if (l->fila_fine) l->fila_fine->dopo_in_fila = io;
    else l->fila = io;
    l->fila_fine = io;
    io->aspetta = l;

    compito_aspetta();   /* ci si risveglia gia' padroni: ce lo passa lucchetto_lascia */

    interruzioni_ripristina(s);
}

void lucchetto_lascia(struct lucchetto *l) {
    unsigned int s = interruzioni_spegni();
    struct compito *io = compito_attuale();

    if (!l->preso || l->padrone != io)
        panico("lucchetto %s: lasciato da chi non lo teneva", l->nome);
    if (io) io->lucchetti--;

    struct compito *primo = l->fila;
    if (primo) {                               /* passaggio di consegne */
        l->fila = primo->dopo_in_fila;
        if (!l->fila) l->fila_fine = 0;
        primo->aspetta = 0;
        primo->lucchetti++;
        l->padrone = primo;                    /* resta preso: ora e' suo */
        compito_sveglia(primo);
    } else {
        l->preso = 0;
        l->padrone = 0;
    }
    interruzioni_ripristina(s);
}
