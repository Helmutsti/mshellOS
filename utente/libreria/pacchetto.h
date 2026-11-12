/* I PACCHETTI DELLE APPLICAZIONI: leggere, controllare e installare un file .app.
 *
 * Un'app si distribuisce come un file unico con estensione .app: un archivio ZIP (le
 * regole sono in APPLICAZIONI.md, sezioni 3 e 4) con dentro
 *     manifest.toml   il manifesto (descrizione.h, manifest-guida.toml)
 *     code            il programma (o il nome che dice "programma" nel manifesto)
 *     resources/      le icone (icona-chiara.svg, icona-scura.svg, PNG facoltative) e i dati
 * Lo ZIP lo legge miniz (esterni/miniz, licenza MIT), che sa leggere l'indice dell'archivio
 * e prendere un file solo: cosi' per DESCRIVERE un pacchetto basta il suo indice e il
 * manifesto, senza scompattare niente.
 *
 * DESCRIVERE vuol dire controllarlo e dire cosa si e' trovato, in un resoconto con gli
 * ERRORI (l'app non si installa) e gli AVVISI (si installa lo stesso, ma la persona deve
 * saperlo), ognuno con una frase in italiano semplice che dice il problema e cosa vuol
 * dire per la persona. Errori: un file che non e' uno ZIP, il manifesto che manca o e'
 * scritto male, le voci obbligatorie che mancano (id, nome, versione, processore,
 * sistema_minimo, programma), l'identificativo scritto male o gia' di un'app del sistema,
 * il processore sbagliato, un sistema troppo vecchio, un permesso, una deviazione o una
 * funzione che il sistema non conosce (APPLICAZIONI.md, sezione 9), un percorso che esce
 * dalla cartella dell'app ("../", "/...") o un collegamento simbolico, il programma che
 * non c'e'. Avvisi: un'icona che manca (si usa l'altro tema o la misura piu' vicina), un
 * permesso di sistema chiesto da un'app che non e' del sistema (non lo ricevera'), le
 * deviazioni dichiarate (la persona va informata).
 *
 * INSTALLARE vuol dire scompattare il pacchetto in una cartella nuova dentro /data/apps
 * (.nuova-<id>-<numero>), con i permessi di quelle del sistema (del sistema, cartelle 755,
 * file 644, il programma 755), e poi rinominarla in /data/apps/<id>: l'app compare tutta
 * insieme o per niente. AGGIORNARE e' lo stesso, ma con un'app gia' installata con lo
 * stesso id e una versione piu' vecchia: le due cartelle si scambiano in un colpo solo
 * (renameat2 con RENAME_EXCHANGE), poi la vecchia si cancella. I dati della persona
 * (/data/users/<uid>/appsdata/<id>) non si toccano mai; se la cartella non c'e', la si
 * crea vuota.
 *
 * Chi puo' scrivere in /data/apps: solo il sistema. Per questo lo fa il launcher (uid 0),
 * quando un'app glielo chiede (launcher.h); questo modulo e' la parte che fa il lavoro.
 *
 * Questo header non tira dentro la libreria C: lo includono anche i programmi (per le
 * strutture che arrivano dal launcher). */
#ifndef PACCHETTO_H
#define PACCHETTO_H

#include "descrizione.h"

#define ESTENSIONE_PACCHETTO ".app"

/* Le misure delle icone piccole (PNG) che un'app dovrebbe avere per ogni tema. Se ne
 * manca una, il sistema rimpicciolisce l'SVG (un avviso, non un errore). */
#define MISURE_PNG_ATTESE { 18, 32 }

/* --- il resoconto --- */

#define MAX_PROBLEMI 24
#define MAX_TESTO_PROBLEMA 300          /* UTF-8, compreso lo 0 */

struct problema {
    int blocca;                          /* 1: errore, l'app non si installa; 0: avviso */
    char voce[24];                       /* di cosa parla: "pacchetto", "manifesto", "processore",
                                          * "sistema_minimo", "id", "permessi", "icone"... */
    char testo[MAX_TESTO_PROBLEMA];      /* la frase per la persona, in italiano (UTF-8) */
};

struct resoconto {
    int installabile;                    /* 1 se non ci sono errori: si puo' installare (o aggiornare) */
    int manifesto_letto;                 /* 1 se il manifesto c'e' ed e' TOML valido: m vale */
    struct manifesto m;
    int svg_presente[2];                 /* per tema (TEMA_CHIARO, TEMA_SCURO): l'SVG c'e' nel pacchetto */
    int png_presente[2][MAX_PNG];        /* e ogni PNG del manifesto (stesso ordine di m.icone[].png) */
    int gia_installata;                  /* 1 se in /data/apps c'e' gia' un'app con questo id */
    char versione_installata[MAX_VERSIONE];
    unsigned long long byte;             /* quanto occupa scompattata */
    int quanti_errori, quanti_avvisi;
    int quanti;                          /* i problemi, errori e avvisi mescolati, in ordine */
    struct problema problemi[MAX_PROBLEMI];
};

/* Cosa si vuole fare con il pacchetto: cambia quali controlli valgono. */
#define PACCHETTO_DESCRIVI 0   /* solo guardare: se l'app c'e' gia', vale come aggiornamento */
#define PACCHETTO_INSTALLA 1   /* l'app non deve esserci gia' */
#define PACCHETTO_AGGIORNA 2   /* l'app deve esserci, con una versione piu' vecchia */

/* Controlla il pacchetto che sta nel file aperto 'f' (in lettura, un file normale) senza
 * scompattarlo. 0 se si puo' fare 'cosa', -1 se no; il perche' e' in r. */
int pacchetto_descrivi(int f, int cosa, struct resoconto *r);

/* Controlla e poi installa (PACCHETTO_INSTALLA) o aggiorna (PACCHETTO_AGGIORNA). 0 se
 * fatto; -1 se no, e allora non e' cambiato niente (il perche' e' in r). Solo per il
 * sistema (uid 0). */
int pacchetto_installa(int f, int cosa, struct resoconto *r);

/* --- le app installate --- */

#define MAX_APP_ELENCO 32

struct app_installata {
    int di_sistema;                      /* 1 se sta in /system/apps */
    struct manifesto m;
};

struct elenco_app {
    int quante;
    struct app_installata app[MAX_APP_ELENCO];
};

/* Le app installate: prima quelle del sistema, poi quelle di /data/apps, ognuna in
 * ordine di identificativo. Solo le cartelle con un manifesto leggibile e l'id uguale al
 * nome della cartella. Quante sono. */
int pacchetto_elenco(struct elenco_app *e);

/* 1 se il programma 'programma' (il percorso completo, come lo da' programma_di) e' quello
 * di un'app del sistema che nel manifesto chiede il permesso "app.gestire" (installare e
 * aggiornare app): per ora, solo cosi' un'app puo' chiedere di installare. */
int pacchetto_puo_gestire(const char *programma);

/* Dove stanno le cartelle dei dati delle persone (/data/users): una variabile solo per le
 * prove sul computer che compila (test/pacchetti/). */
extern const char *cartella_persone;

#endif
