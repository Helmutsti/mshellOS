/* I compiti e lo scheduler: piu' lavori che si alternano sulla stessa CPU. */
#ifndef COMPITI_H
#define COMPITI_H

#define INT_CEDI 0x81   /* l'interruzione software con cui un compito cede il turno */

enum stato_compito {
    PRONTO,          /* puo' girare, aspetta il suo turno */
    IN_ESECUZIONE,   /* sta girando adesso */
    DORME,           /* aspetta un certo tick (dormi) */
    ATTESA,          /* aspetta che qualcuno lo svegli (es. un tasto) */
    FINITO,          /* ha finito: lo scheduler liberera' scheda e pila */
};

/* La scheda di un compito. */
struct compito {
    int numero;
    char nome[16];
    enum stato_compito stato;
    unsigned int esp;            /* dove era arrivata la sua pila quando e' stato fermato */
    void *pila;                  /* la sua pila (0 per l'inattivo: usa quella dell'avvio).
                                  * Per un programma in ring 3 e' la sua pila DEL KERNEL */
    unsigned int mappa;          /* solo ring 3: la sua directory (0 = usa la mappa del kernel) */
    unsigned int sveglia;        /* se DORME: il tick a cui svegliarlo */
    struct compito *prossimo;    /* il successivo nella fila (circolare) */

    /* i lucchetti (lucchetto.c) */
    int lucchetti;                    /* quanti ne tiene adesso */
    struct lucchetto *aspetta;        /* il lucchetto per cui e' in fila (0 = nessuno) */
    struct compito *dopo_in_fila;     /* il successivo nella fila di quel lucchetto */
};

/* Trasforma il codice che sta girando (kernel_main) nel compito 0, "inattivo",
 * e accende lo scheduler. */
void compiti_prepara(void);

/* Crea un compito in RING 3 dal file eseguibile (ELF) 'percorso' sul disco.
 * Ha una mappa sua (le sue pagine piu' il kernel, condiviso) e due pile: una pagina per
 * il programma e una pila del kernel. Restituisce la scheda, oppure 0 e in *errore il
 * perche' (file che non c'e', non e' un eseguibile, memoria finita...). */
struct compito *compito_crea_programma(const char *percorso, const char **errore);

void cedi(void);                       /* passa il turno al prossimo */
void dormi(unsigned int millisecondi); /* non girare per almeno questo tempo */

/* Si mette in ATTESA finche' compito_sveglia non lo rimette pronto.
 * Va chiamata con le interruzioni SPENTE (vedi eventi_aspetta). */
void compito_aspetta(void);
void compito_sveglia(struct compito *c);

struct compito *compito_attuale(void);

/* Chiamato da interruzione() a ogni tick e a ogni cedi(): riceve l'ESP del compito
 * interrotto (con i suoi registri salvati sopra) e restituisce l'ESP di chi deve girare. */
unsigned int scheduler(unsigned int esp);
int scheduler_sta_riposando(void);     /* sta girando il compito inattivo? */

#endif
