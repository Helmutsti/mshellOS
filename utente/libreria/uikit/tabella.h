/* LA TABELLA, dal file Figma (pagina "Componenti complessi", sezione "Tabella"): la vista a
 * elenco di Files, la tabella dei processi di Attivita, gli elenchi. Composta da:
 *
 *   - l'INTESTAZIONE DI COLONNA: alta ALTEZZA_S (32), il nome della colonna in "etichetta"
 *     TESTO_SECONDARIO, allineata a sinistra (o a destra, per i numeri). Sulla colonna per cui
 *     si ordina, dopo il nome (SPAZIO_4), la freccia da 16: in su Crescente, in giu'
 *     Decrescente. Un clic sull'intestazione ordina per quella colonna (un altro clic gira il
 *     verso);
 *   - una linea BORDO_SEPARATORE da bordo a bordo;
 *   - le RIGHE: subito sotto la linea, alte ALTEZZA_M (40), a SPAZIO_4 l'una dall'altra (due
 *     fondi arrotondati non si toccano mai), raggio RAGGIO_CONTROLLO_NUOVO, padding SPAZIO_8 ai
 *     lati. Stati: Normale (piatta), Sopra (fondo CONTROLLO_SFONDO), Selezionata (fondo
 *     CONTROLLO_SFONDO_SOPRA). Le celle in "corpo", con "..." se non ci stanno; la prima colonna
 *     puo' avere l'oggetto piccolo (24, oggetti.h) e SPAZIO_8 prima del nome.
 *
 * LE RIGHE AD ALBERO (la tabella dei processi di Attivita): una colonna (colonna_albero) mostra
 * un albero. Ogni riga ha un livello e un ramo (RAMO_NESSUNO, RAMO_APERTO, RAMO_CHIUSO). Nella
 * cella: il rientro, livello x TABELLA_RIENTRO (24: la freccia 16 + SPAZIO_8, cosi' la freccia
 * del figlio comincia sotto il nome del padre), poi il posto della freccia (16 x 16: chevron giu'
 * aperto, chevron destra chiuso, vuoto se la riga non ha figli), poi SPAZIO_8 e il testo. Il
 * rientro e' un valore della riga, non un padding. Un clic sulla freccia arriva ad 'al_ramo'
 * (non sceglie la riga): l'applicazione apre o chiude il ramo e rifa' le righe, aiutata da
 * tabella_tk_albero, che mette in fila i nodi di un albero saltando i rami chiusi.
 *
 * Tra una colonna e l'altra SPAZIO_24, nell'intestazione e nelle righe (stesso padding: le
 * colonne si allineano). Una colonna ha la sua larghezza, o 0: si divide lo spazio che avanza.
 * La tabella non ha padding suo verso il contenitore: il primo testo e' a 8 dal bordo (il padding
 * della riga, che ha un fondo) e, nell'intestazione, a 8 dalla cima (il testo da 16 centrato
 * nei 32). L'intestazione resta ferma; le righe scorrono con la rotella e la barra di
 * scorrimento di UIKit.
 *
 * I dati li da' l'applicazione, riga per riga, quando servono ('riga'); un clic su una riga (o
 * sul vuoto: riga -1), il doppio clic e il tasto destro arrivano ad 'al_clic'. Lo stato
 * Rinomina (il nome della prima colonna diventa un campo: oggetti.h) si accende con
 * tabella_rinomina. Il codice e' in interno/tabella.c. */
#ifndef TABELLA_H
#define TABELLA_H

#define MAX_COLONNE_TABELLA 6
#define MAX_CELLA           96
#define TABELLA_ALTEZZA_TESTATA     ALTEZZA_S
#define TABELLA_ALTEZZA_RIGA        ALTEZZA_M
#define TABELLA_TRA_RIGHE   SPAZIO_4
#define TABELLA_SOPRA_RIGHE SPAZIO_0
#define TABELLA_DENTRO      SPAZIO_8      /* il padding della riga e dell'intestazione, ai lati */
#define TABELLA_TRA_COLONNE SPAZIO_24
#define TABELLA_FRECCIA     ICONA_S
#define TABELLA_OGGETTO_GAP SPAZIO_8
#define TABELLA_RIENTRO     SPAZIO_24     /* per livello dell'albero: la freccia 16 + SPAZIO_8 */
#define TABELLA_RAMO        ICONA_S       /* il posto della freccia del ramo, 16 x 16 */
#define TABELLA_RAMO_GAP    SPAZIO_8      /* tra la freccia e il testo */

enum { ORDINE_NESSUNO, ORDINE_CRESCENTE, ORDINE_DECRESCENTE };
enum { CLIC_SEMPLICE, CLIC_DOPPIO, CLIC_DESTRO };
enum { RAMO_NESSUNO, RAMO_APERTO, RAMO_CHIUSO };

struct colonna_tk {
    const char *etichetta;
    int larghezza;                     /* 0: prende lo spazio che avanza */
    int destra;                        /* allineata a destra (i numeri) */
    int primaria;                      /* testo TESTO_PRIMARIO (se no TESTO_SECONDARIO) */
    int ordinabile;                    /* un clic sull'intestazione ordina */
};

/* Quello che c'e' in una riga: un testo per colonna, l'oggetto della prima (o -1) e, se la
 * tabella ha una colonna ad albero, il livello e il ramo (RAMO_...) della riga. */
struct riga_tk {
    char testi[MAX_COLONNE_TABELLA][MAX_CELLA];
    int oggetto;
    int livello;
    int ramo;
};

struct tabella_tk;
typedef void (*dati_riga_tabella)(struct tabella_tk *t, int riga, struct riga_tk *r, void *dati);
typedef void (*al_clic_tabella)(struct tabella_tk *t, int riga, int come, int x, int y, void *dati);
typedef void (*al_ordine_tabella)(struct tabella_tk *t, void *dati);
typedef void (*al_tasto_tabella)(struct tabella_tk *t, unsigned int tasto, void *dati);
typedef void (*al_ramo_tabella)(struct tabella_tk *t, int riga, void *dati);

struct tabella_tk {
    struct widget base;
    struct barra_scorrimento barra;    /* sua figlia: sa anche dove si e' scorso */
    const struct colonna_tk *colonne;
    int quante_colonne;
    int righe;
    int scelta;                        /* la riga selezionata, o -1 */
    int sopra;                         /* quella sotto il puntatore, o -1 */
    int ordinata, verso;               /* la colonna per cui si ordina (o -1) e ORDINE_... */
    int rinomina;                      /* la riga il cui nome e' un campo, o -1 */
    const char *testo_rinomina;
    int colonna_albero;                /* la colonna con l'albero, o -1 */
    dati_riga_tabella riga;
    al_clic_tabella al_clic;
    al_ordine_tabella al_ordine;
    al_tasto_tabella al_tasto;
    al_ramo_tabella al_ramo;           /* un clic sulla freccia di un ramo */
    void *dati;
    unsigned int clic_prima, pulsanti_prima;
    int clic_su;
};

/* La riga nel punto (x, y) dello schermo, o -1. */
int tabella_tk_riga_in(const struct tabella_tk *t, int x, int y);

/* La riga k va ridisegnata: solo lei (il campo per rinominare sporge di poco). */
void tabella_tk_riga_cambiata(struct tabella_tk *t, int k);

/* Una tabella elastica e larga (prende lo spazio che le si da'). 'ordinata' e 'verso': come
 * e' ordinata all'inizio (-1, ORDINE_NESSUNO: per niente). */
void tabella_tk_prepara(struct tabella_tk *t, const struct colonna_tk *colonne, int quante,
                        dati_riga_tabella riga, al_clic_tabella al_clic, void *dati);

/* Le righe sono cambiate (quante, o cosa c'e' dentro). */
void tabella_tk_aggiorna(struct tabella_tk *t, int righe);

/* Seleziona la riga 'riga' (-1: nessuna), senza chiamare al_clic. */
void tabella_tk_scegli(struct tabella_tk *t, int riga);

/* Il nome della prima colonna della riga diventa un campo con 'testo' (che l'applicazione
 * cambia mentre si scrive); -1 lo spegne. */
void tabella_tk_rinomina(struct tabella_tk *t, int riga, const char *testo);

/* L'ALBERO IN FILA. 'n' nodi; genitore[i] e' l'indice del padre (o -1: sta al primo livello);
 * i figli di uno stesso padre vengono nell'ordine dei loro indici. chiuso[i] (o chiuso nullo:
 * tutti aperti) dice se il ramo di i e' chiuso. Scrive, riga per riga, l'indice del nodo
 * (nodi), il livello e il ramo (RAMO_...); i discendenti di un ramo chiuso non ci sono.
 * Ritorna quante righe ha scritto (al massimo n). */
int tabella_tk_albero(int n, const int *genitore, const unsigned char *chiuso,
                      int *nodi, int *livelli, int *rami);

#endif
