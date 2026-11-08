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
    void *pila;                  /* la sua pila (0 per la shell: usa quella del kernel) */
    unsigned int sveglia;        /* se DORME: il tick a cui svegliarlo */
    struct compito *prossimo;    /* il successivo nella fila (circolare) */

    /* i lucchetti (lucchetto.c) */
    int lucchetti;                    /* quanti ne tiene adesso */
    struct lucchetto *aspetta;        /* il lucchetto per cui e' in fila (0 = nessuno) */
    struct compito *dopo_in_fila;     /* il successivo nella fila di quel lucchetto */
};

/* Trasforma il codice che sta girando (kernel_main) nel compito "shell",
 * crea il compito "inattivo" e accende lo scheduler. */
void compiti_prepara(void);

/* Crea un compito che esegue funzione(argomento). Restituisce la sua scheda, o 0. */
struct compito *compito_crea(const char *nome, void (*funzione)(unsigned int), unsigned int argomento);

void cedi(void);                       /* passa il turno al prossimo */
void dormi(unsigned int millisecondi); /* non girare per almeno questo tempo */

/* Si mette in ATTESA finche' compito_sveglia non lo rimette pronto.
 * Va chiamata con le interruzioni SPENTE (vedi tastiera_aspetta). */
void compito_aspetta(void);
void compito_sveglia(struct compito *c);

struct compito *compito_attuale(void);
/* 0 se fermato, -1 se non si puo' (shell, inattivo, chi gira),
 * -2 se sta usando un lucchetto (riprovare piu' tardi) */
int compito_ferma(int numero);
void compiti_elenca(void);             /* stampa la tabella dei compiti */

/* Chiamato da interruzione() a ogni tick e a ogni cedi(): riceve l'ESP del compito
 * interrotto (con i suoi registri salvati sopra) e restituisce l'ESP di chi deve girare. */
unsigned int scheduler(unsigned int esp);
int scheduler_sta_riposando(void);     /* sta girando il compito inattivo? */

#endif
