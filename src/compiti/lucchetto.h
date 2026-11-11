/* Il lucchetto (mutex): per le risorse usate a lungo (il file system, i dischi).
 *
 * Come la chiave di un bagno: chi la prende entra, gli altri aspettano fuori
 * DORMENDO, in fila. Le interruzioni restano accese: chi non usa la risorsa
 * continua a lavorare normalmente.
 *
 *     static struct lucchetto mio = LUCCHETTO("nome");
 *     lucchetto_prendi(&mio);
 *     ... uso della risorsa, anche lungo ...
 *     lucchetto_lascia(&mio);
 */
#ifndef LUCCHETTO_H
#define LUCCHETTO_H

struct compito;

struct lucchetto {
    const char *nome;
    int preso;
    struct compito *padrone;           /* chi lo tiene */
    struct compito *fila, *fila_fine;  /* chi aspetta, in ordine di arrivo */
};

#define LUCCHETTO(n) { (n), 0, 0, 0, 0 }

/* Lo prende. Se e' gia' preso, il compito dorme finche' non tocca a lui. */
void lucchetto_prendi(struct lucchetto *l);

/* Lo lascia. Se qualcuno aspetta, passa direttamente al primo della fila. */
void lucchetto_lascia(struct lucchetto *l);

#endif
