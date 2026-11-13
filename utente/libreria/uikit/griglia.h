/* LA GRIGLIA DI ICONE, dal file Figma ("Griglia" e "Icona", Layout Tile, pagina "Componenti
 * complessi"): la vista a icone di Files (e un domani la scrivania, le finestre Apri/Salva).
 * PADDING ZERO (lo spazio intorno lo da' il pannello che la contiene), SPAZIO_8 tra un tile e
 * l'altro, in righe (quanti ne stanno).
 *
 * IL TILE, largo TILE_LARGHEZZA (96) e alto TILE_ALTEZZA (116, fisso: due righe di nome), senza
 * un fondo suo, in colonna, il riquadro in cima e SPAZIO_4 tra i pezzi:
 *
 *     +----------------+
 *     |   +--------+   |   il riquadro dell'icona, 64 x 64, raggio RAGGIO_CONTROLLO_NUOVO, con
 *     |   | oggetto|   |   l'oggetto grande (48, oggetti.h) al centro (8 dal bordo del riquadro)
 *     |   +--------+   |   4
 *     |  nome del file |   l'etichetta: "corpo" centrato, al massimo due righe (poi "..."),
 *     |  lungo...      |   SPAZIO_8 di padding ai lati (ha un fondo), raggio RAGGIO_CASELLA
 *     +----------------+
 *
 * Stati (piatti): Normale (niente fondi), Sopra (il riquadro dell'icona CONTROLLO_SFONDO),
 * Selezionato (il riquadro CONTROLLO_SFONDO_SOPRA e l'etichetta evidenziata: fondo
 * AZIONE_SFONDO largo quanto il tile, testo AZIONE_TESTO). Rinomina: al posto dell'etichetta
 * il campo del nome (oggetti.h), alto 24.
 *
 * Gli elementi li da' l'applicazione ('elemento'); un clic su un elemento (o sul vuoto: -1),
 * il doppio clic e il tasto destro arrivano ad 'al_clic', come per la tabella. Scorre con la
 * rotella e la barra di scorrimento di UIKit. Il codice e' in interno/griglia.c. */
#ifndef GRIGLIA_H
#define GRIGLIA_H

#define GRIGLIA_PADDING   SPAZIO_0
#define GRIGLIA_TRA       SPAZIO_8
#define TILE_ALTEZZA      116
#define TILE_DENTRO       SPAZIO_4      /* tra riquadro ed etichetta */
#define TILE_RIQUADRO     64
#define TILE_RIGHE        2
#define TILE_ETICHETTA_DENTRO SPAZIO_8  /* ai lati del testo dell'etichetta */
#define TILE_CAMPO_ALTO   (INTERLINEA_CORPO + SPAZIO_4)   /* il campo del nome nuovo: 24 */

struct elemento_griglia_tk {
    char nome[MAX_CELLA];
    int oggetto;                       /* OGGETTO_... */
};

struct griglia_tk;
typedef void (*dati_elemento_griglia)(struct griglia_tk *g, int i, struct elemento_griglia_tk *e, void *dati);
typedef void (*al_clic_griglia)(struct griglia_tk *g, int i, int come, int x, int y, void *dati);
typedef void (*al_tasto_griglia)(struct griglia_tk *g, unsigned int tasto, void *dati);

struct griglia_tk {
    struct widget base;
    struct barra_scorrimento barra;
    int quanti;
    int scelto, sopra, rinomina;       /* -1: nessuno */
    const char *testo_rinomina;
    dati_elemento_griglia elemento;
    al_clic_griglia al_clic;
    al_tasto_griglia al_tasto;
    void *dati;
    unsigned int clic_prima, pulsanti_prima;
    int clic_su;
    int riserva;                       /* i pixel a destra che i tile non usano (griglia_impagina) */
};

/* Il tile nel punto (x, y) della finestra, o -1. */
int griglia_in(const struct griglia_tk *g, int x, int y);

/* Una griglia elastica e larga (prende lo spazio che le si da'), vuota: 'elemento' da' il nome e
 * l'oggetto di ogni tile, 'al_clic' riceve i clic, con 'dati'. */
void griglia_prepara(struct griglia_tk *g, dati_elemento_griglia elemento, al_clic_griglia al_clic,
                     void *dati);

/* Gli elementi sono cambiati (quanti, o cosa c'e' dentro). */
void griglia_aggiorna(struct griglia_tk *g, int quanti);

/* Seleziona il tile i (-1: nessuno), senza chiamare al_clic. */
void griglia_scegli(struct griglia_tk *g, int i);

/* Il nome del tile i diventa un campo con 'testo' (che l'applicazione cambia mentre si
 * scrive); -1 lo spegne. */
void griglia_rinomina(struct griglia_tk *g, int i, const char *testo);

#endif
