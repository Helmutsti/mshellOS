/* I messaggi tra i servizi e i loro clienti: lo standard di SISTEMA.md (sezione 3, "Le
 * porte"). Il perche' e il come, con un esempio di scambio, sono in testa a messaggi.c.
 *
 * In breve: un messaggio e' testo, righe "chiave=valore" separate da a capo; una chiave
 * puo' ripetersi; le chiavi sono in inglese minuscolo. Le chiavi della busta, uguali per
 * tutti i servizi, sono:
 *     request   quale richiesta (la mette chi chiede)
 *     id        un numero scelto da chi chiede; la risposta lo ripete (lo mette chiedi)
 *     result    nella risposta: ok, denied, invalid, error (ESITO_... qui sotto)
 * Il resto delle chiavi lo decide ogni servizio, e lo scrive accanto al suo codice.
 * Un messaggio viaggia come UN pacchetto sulle porte (ascolta, collega, accetta in
 * libreria.h), con al massimo un descrittore allegato. */
#ifndef MESSAGGI_H
#define MESSAGGI_H

#include "libreria.h"

/* Le chiavi della busta. */
#define CHIAVE_RICHIESTA "request"
#define CHIAVE_ID        "id"
#define CHIAVE_ESITO     "result"

/* Gli esiti (il valore di result). */
#define ESITO_OK         "ok"        /* fatto */
#define ESITO_NEGATO     "denied"    /* chi chiede non ha il permesso */
#define ESITO_NON_VALIDO "invalid"   /* la richiesta e' sbagliata: non si capisce, manca un dato... */
#define ESITO_ERRORE     "error"     /* la richiesta era giusta, ma non e' andata */

/* Le misure. Un messaggio, mandato, e' al massimo MAX_MESSAGGIO byte (chiamate.h: e' gia'
 * il limite di manda). Una chiave e' al massimo MAX_CHIAVE lettere, compreso lo 0 finale. */
#define MAX_CHIAVE 32

/* UN MESSAGGIO. Dentro, ogni riga finisce con uno 0 al posto dell'a capo: cosi' i valori
 * si leggono come testi normali, senza copiarli. I campi si usano solo con le funzioni qui
 * sotto, tranne 'allegato', che si legge e si scrive a mano:
 *   allegato  il descrittore che viaggia con il messaggio, o -1 (nessuno). Chi riceve ne
 *             diventa padrone: lo chiude lui quando non serve piu'. Chi manda resta
 *             padrone del suo (il kernel ne da' all'altro una copia).
 *   guasto    1 se un'aggiunta e' stata rifiutata: il messaggio non e' quello che si
 *             voleva, e messaggio_manda non lo manda. */
struct messaggio {
    unsigned int byte;              /* byte usati in testo */
    int allegato;
    int guasto;
    char testo[MAX_MESSAGGIO + 1];  /* + 1: per capire, ricevendo, se e' troppo lungo */
};

/* --- costruire --- */

/* Un messaggio vuoto, senza allegato. */
void messaggio_inizia(struct messaggio *m);

/* Aggiunge la riga chiave=valore. La chiave: da 1 a MAX_CHIAVE - 1 lettere tra a-z, 0-9 e
 * _ (niente =, a capo, spazi, maiuscole). Il valore: qualsiasi testo senza a capo, anche
 * vuoto o con dentro un =. 0, o -1 se la chiave o il valore non vanno bene o non c'e' piu'
 * posto: il messaggio non cambia, ma resta guasto e non si potra' mandare. */
int messaggio_aggiungi(struct messaggio *m, const char *chiave, const char *valore);

/* Come messaggio_aggiungi, con un numero scritto in cifre (anche negativo). */
int messaggio_aggiungi_numero(struct messaggio *m, const char *chiave, long numero);

/* Inizia la risposta a 'richiesta': la busta, cioe' id (lo stesso della richiesta, se
 * c'era) e result = 'esito' (ESITO_...). Poi si aggiungono gli altri dati. 'richiesta'
 * puo' essere 0 (una richiesta che non si e' potuta leggere: allora niente id). */
void messaggio_risposta(struct messaggio *risposta, const struct messaggio *richiesta, const char *esito);

/* --- leggere --- */

/* Il valore della chiave (il primo, se ce ne sono piu' di uno), o 0 se non c'e'. */
const char *messaggio_valore(const struct messaggio *m, const char *chiave);

/* Il valore della chiave che viene DOPO 'prima' (un valore dato da questa funzione o da
 * messaggio_valore), o il primo se 'prima' e' 0; 0 quando sono finiti. Per scorrere una
 * chiave che si ripete:
 *     for (const char *v = 0; (v = messaggio_prossimo(&m, "dns", v));) ... */
const char *messaggio_prossimo(const struct messaggio *m, const char *chiave, const char *prima);

/* Il valore della chiave come numero, in *numero. 0, o -1 se la chiave non c'e' o il
 * valore non e' un numero intero (solo cifre, con un - davanti se negativo). */
int messaggio_numero(const struct messaggio *m, const char *chiave, long *numero);

/* 1 se il valore della chiave e' proprio 'valore', 0 se no (o se la chiave non c'e').
 * Per esempio: messaggio_vale(&risposta, CHIAVE_ESITO, ESITO_OK). */
int messaggio_vale(const struct messaggio *m, const char *chiave, const char *valore);

/* --- mandare e ricevere --- */

/* Manda il messaggio sul collegamento 'd', con il suo allegato se c'e'. 0, o -1 se l'altro
 * ha chiuso, o se il messaggio e' vuoto o guasto. */
int messaggio_manda(int d, const struct messaggio *m);

/* Riceve un messaggio dal collegamento 'd' (dorme finche' non arriva), con il suo
 * allegato in m->allegato (o -1). 1 se e' arrivato, 0 se l'altro ha chiuso, -1 se errore,
 * MESSAGGIO_NON_VALIDO se e' arrivato un pacchetto che non segue lo standard (troppo
 * lungo, una riga senza =, una chiave sbagliata...): l'allegato, se c'era, e' gia' chiuso,
 * e il servizio risponde ESITO_NON_VALIDO (messaggio_risposta con richiesta 0). */
#define MESSAGGIO_NON_VALIDO (-2)
int messaggio_ricevi(int d, struct messaggio *m);

/* --- chi chiede --- */

/* Manda 'richiesta' sul collegamento 'd' con un id nuovo (lo aggiunge chiedi: la
 * richiesta non deve averlo gia'), e aspetta la risposta con lo stesso id; le risposte
 * con un altro id si buttano. 0 se la risposta e' arrivata (il suo esito e' in result),
 * -1 se no (la richiesta era guasta, o il servizio ha chiuso). Dorme senza limite: un
 * servizio che non risponde ferma chi chiede. */
int chiedi(int d, struct messaggio *richiesta, struct messaggio *risposta);

/* --- il servizio: chi c'e' dall'altra parte --- */

/* Chi c'e' all'altro capo di un collegamento: lo dice il kernel, non si puo' fingere. */
struct chi_chiede {
    int uid;                                  /* 0: il sistema; 1000...: una persona */
    char programma[MAX_PERCORSO_PROGRAMMA];   /* es. /system/apps/files/code; "" se non si sa */
};

/* Riempie *c per il collegamento 'd'. 0, o -1 se il kernel non lo sa (non e' un socket). */
int chi_chiede(int d, struct chi_chiede *c);

#endif
