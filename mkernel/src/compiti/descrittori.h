/* I descrittori: i numerini con cui un programma tiene in mano le cose aperte.
 *
 * Come in Linux, un programma non tocca mai direttamente un socket o la fila degli
 * eventi: il kernel li tiene per lui e gli da' un numero, il DESCRITTORE, che e' la
 * posizione nella sua tabella delle cose aperte. "Ricevi dal 3" vuol dire "ricevi da
 * quello che c'e' al posto 3 della MIA tabella": un programma non puo' nominare le cose
 * aperte da un altro.
 *
 * Ogni cosa aperta ha le sue OPERAZIONI (cosa vuol dire ricevere, mandare, chiudere per
 * lei): il kernel chiama quelle giuste senza sapere se e' un socket o la tastiera.
 * In Linux e' la "struct file" con le sue "file_operations". */
#ifndef DESCRITTORI_H
#define DESCRITTORI_H

#include "compiti.h"
#include "chiamate.h"

/* Chi dorme aspettando una cosa aperta. Il nodo sta sulla pila del kernel di chi
 * aspetta, e resta valido finche' lui dorme li'. */
struct attesa {
    struct compito *chi;
    struct attesa *dopo;
};

struct aperto;

struct operazioni {
    /* Riceve in 'dove' (al massimo 'byte'); dorme se non c'e' niente. Byte ricevuti,
     * 0 se dall'altra parte hanno chiuso, -1 se qui non si riceve. */
    int (*ricevi)(struct aperto *a, void *dove, unsigned int byte);
    /* Manda 'byte' da 'da'; puo' dormire. Byte mandati, o -1. */
    int (*manda)(struct aperto *a, const void *da, unsigned int byte);
    /* C'e' qualcosa da ricevere adesso (o la fine)? Non dorme: serve ad aspetta. */
    int (*pronto)(struct aperto *a);
    /* Il descrittore e' stato chiuso: si libera tutto. */
    void (*chiudi)(struct aperto *a);
};

struct aperto {
    const struct operazioni *op;
    struct attesa *attese;     /* chi dorme aspettando che succeda qualcosa qui */
};

/* Dorme finche' qualcuno non chiama aperto_sveglia su 'a'. Al risveglio non e' detto
 * che ci sia davvero qualcosa: chi la chiama ricontrolla, in un ciclo.
 * A interruzioni spente (le chiamate di sistema lo sono gia'). */
void aperto_aspetta(struct aperto *a);
/* Sveglia tutti quelli che dormono su 'a'. */
void aperto_sveglia(struct aperto *a);

/* Mette 'a' nel primo posto libero della tabella del compito attuale: il descrittore,
 * o -1 se la tabella e' piena. */
int descrittore_nuovo(struct aperto *a);
/* La cosa aperta al posto 'd' del compito attuale, o 0. */
struct aperto *descrittore(int d);
/* Chiude il posto 'd' del compito attuale. 0, o -1 se non c'era niente. */
int descrittore_chiudi(int d);
/* Chiude tutto quello che 'c' aveva aperto: lo chiama lo scheduler quando libera un
 * compito finito. */
void descrittori_chiudi_tutti(struct compito *c);

/* La chiamata aspetta: dorme finche' almeno uno dei descrittori ha qualcosa da
 * ricevere. Segna 'pronto' in ognuno; quanti sono pronti, o -1 se uno non esiste. */
int descrittori_aspetta(struct da_guardare *elenco, int quanti);

#endif
