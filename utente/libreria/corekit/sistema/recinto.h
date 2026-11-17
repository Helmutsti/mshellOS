/* CoreKit, del sistema: IL RECINTO DI UN'APP (APPLICAZIONI.md, sezione 5, "L'app non vede
 * niente" e "I permessi sulle cartelle"). Lo costruisce solo il launcher, che e' del sistema,
 * quando fa partire un'app (avvia_app_nel_recinto, in privilegi.h); la prova `recinto`
 * (utente/prove/recinto.c) usa lo stesso filtro delle chiamate (recinto_seccomp).
 *
 * Com'e' fatto il mondo dell'app (interno/recinto.c fa il lavoro):
 *   - mondi nuovi del kernel: file montati, numeri dei programmi (l'app e' il numero 1 del
 *     suo mondo e non vede nessun altro), code dei messaggi (IPC), nome della macchina e,
 *     senza il permesso della rete, una rete vuota;
 *   - una radice nuova, vuota, in memoria (tmpfs), in sola lettura, dove si agganciano
 *     soltanto le cartelle dell'elenco qui sotto, ognuna ALLO STESSO PERCORSO DI FUORI
 *     (decisione della persona, 17 novembre 2026: cosi' le app non cambiano). Il resto del
 *     disco, /proc, /dev, /run (gli indirizzi dei servizi) nel recinto non ci sono;
 *   - uid e gid della persona, nessun gruppo, NO_NEW_PRIVS (mai piu' poteri) e il filtro
 *     delle chiamate (recinto_seccomp).
 * Gli agganci sono di tre modi: il pacchetto dell'app (sola lettura, l'unico da cui si puo'
 * eseguire un programma: "un'app non puo' eseguire codice che non stava nel suo pacchetto",
 * APPLICAZIONI.md, "La catena della sicurezza"), le cartelle in sola lettura e quelle in
 * lettura e scrittura. Si agganciano nell'ordine dell'elenco: una cartella dentro un'altra va
 * dopo (vince la piu' interna).
 *
 * Questo file non include corekit.h: lo usa anche la prova, che ha funzioni sue con gli
 * stessi nomi di CoreKit. */
#ifndef RECINTO_H
#define RECINTO_H

#define AGGANCIO_PROGRAMMA 0   /* sola lettura, e da qui si possono eseguire programmi */
#define AGGANCIO_LETTURA   1   /* sola lettura */
#define AGGANCIO_SCRITTURA 2   /* lettura e scrittura */

#define MAX_AGGANCI 8

struct aggancio {
    const char *percorso;      /* una cartella vera, che c'e': la stessa dentro e fuori */
    int modo;                  /* AGGANCIO_... */
};

struct recinto {
    struct aggancio agganci[MAX_AGGANCI];
    int quanti;
    int rete;                  /* 1: la rete del computer (network.use); 0: una rete vuota */
};

/* Il filtro delle chiamate al kernel (seccomp): quelle vietate rispondono EPERM (l'elenco e il
 * perche' sono in interno/recinto.c). Va messo dopo NO_NEW_PRIVS e prima di execve, e passa a
 * tutti i figli: chi lo ha non puo' piu' toglierlo. 0, o -1 con errno. */
int recinto_seccomp(void);

#endif
