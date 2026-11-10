/* I compiti e lo scheduler: piu' lavori che si alternano sulla stessa CPU. */
#ifndef COMPITI_H
#define COMPITI_H

#include "chiamate.h"   /* MAX_MAPPATI */

#define INT_CEDI 0x81   /* l'interruzione software con cui un compito cede il turno */

enum stato_compito {
    PRONTO,          /* puo' girare, aspetta il suo turno */
    IN_ESECUZIONE,   /* sta girando adesso */
    DORME,           /* aspetta un certo tick (dormi) */
    ATTESA,          /* aspetta che qualcuno lo svegli (un messaggio, un evento) */
    FINITO,          /* ha finito: lo scheduler liberera' scheda e pila */
};

#define MAX_APERTI 16   /* cose aperte (descrittori) per compito */

struct aperto;

/* La scheda di un compito. */
struct compito {
    int numero;
    char nome[16];
    char percorso[64];           /* il programma da cui e' nato (MAX_PERCORSO) */
    enum stato_compito stato;
    unsigned int esp;            /* dove era arrivata la sua pila quando e' stato fermato */
    void *pila;                  /* la sua pila (0 per l'inattivo: usa quella dell'avvio).
                                  * Per un programma in ring 3 e' la sua pila DEL KERNEL */
    unsigned int mappa;          /* solo ring 3: la sua directory (0 = usa la mappa del kernel) */
    unsigned int uid;            /* chi e': 0 il sistema, 1000 le applicazioni (chiamate.h) */
    unsigned int dati_inizio;    /* solo ring 3: dove comincia la zona dinamica (dinamica.c) */
    unsigned int rottura;        /* ... e dove finisce adesso: il confine */
    unsigned int sveglia;        /* se DORME: il tick a cui svegliarlo */
    unsigned int scadenza;       /* se in ATTESA: il tick a cui svegliarlo comunque (0 = mai) */
    int da_terminare;            /* qualcuno ha chiesto di chiuderlo (compito_termina) */
    struct compito *prossimo;    /* il successivo nella fila (circolare) */
    struct aperto *aperti[MAX_APERTI];   /* le sue cose aperte: il descrittore e' il posto */
    struct aperto *mappati[MAX_MAPPATI]; /* i fogli nella sua memoria: il posto i sta a
                                          * FOGLI_INIZIO + i * FOGLIO_MAX */

    /* i lucchetti (lucchetto.c) */
    int lucchetti;                    /* quanti ne tiene adesso */
    struct lucchetto *aspetta;        /* il lucchetto per cui e' in fila (0 = nessuno) */
    struct compito *dopo_in_fila;     /* il successivo nella fila di quel lucchetto */
};

/* Trasforma il codice che sta girando (kernel_main) nel compito 0, "inattivo",
 * e accende lo scheduler. */
void compiti_prepara(void);

/* Crea un compito in RING 3 dal file eseguibile (ELF) 'percorso' sul disco, con 'uid'.
 * Ha una mappa sua (le sue pagine piu' il kernel, condiviso) e due pile: una pagina per
 * il programma e una pila del kernel. Restituisce la scheda, oppure 0 e in *errore il
 * perche' (file che non c'e', non e' un eseguibile, memoria finita...). */
struct compito *compito_crea_programma(const char *percorso, unsigned int uid, const char **errore);

void cedi(void);                       /* passa il turno al prossimo */
void dormi(unsigned int millisecondi); /* non girare per almeno questo tempo */

/* Si mette in ATTESA finche' compito_sveglia non lo rimette pronto.
 * Va chiamata con le interruzioni SPENTE (vedi aperto_aspetta). */
void compito_aspetta(void);
/* Come compito_aspetta, ma si sveglia comunque al tick 'scadenza'. */
void compito_aspetta_fino(unsigned int scadenza);
void compito_sveglia(struct compito *c);

/* CHIUDERE UN PROGRAMMA DA FUORI. Non lo si puo' cancellare subito: potrebbe dormire
 * dentro il kernel, con i suoi segnalini nelle file d'attesa delle cose aperte, che
 * resterebbero a puntare a una pila liberata. Allora lo si SEGNA e lo si sveglia:
 *   - se era fermo in ring 3 (stava girando il suo codice), lo scheduler lo chiude
 *     al suo turno: li' non ha niente di lasciato a meta' nel kernel;
 *   - se dormiva nel kernel, si sveglia, si toglie dalle file e si chiude da solo
 *     (compito_fine_se_terminato, nei punti in cui si dorme).
 * Chi aspetta un lucchetto non si sveglia: quando lo avra' avuto e lasciato, tornera'
 * in ring 3 e lo scheduler lo chiudera' li'. */
void compito_termina(struct compito *c);
/* Se il compito attuale e' segnato da chiudere, si chiude qui e non torna. Va chiamata
 * dopo un'attesa, quando non si tiene niente (segnalini tolti, nessun lucchetto). */
void compito_fine_se_terminato(void);

struct compito *compito_attuale(void);

/* Il compito 'numero' c'e' ancora (non e' stato liberato)? */
int compito_esiste(int numero);
/* Il numero che avra' il prossimo compito: quelli sotto sono gia' stati dati. */
int compito_prossimo_numero(void);

/* Chiamato da interruzione() a ogni tick e a ogni cedi(): riceve l'ESP del compito
 * interrotto (con i suoi registri salvati sopra) e restituisce l'ESP di chi deve girare. */
unsigned int scheduler(unsigned int esp);
int scheduler_sta_riposando(void);     /* sta girando il compito inattivo? */

#endif
