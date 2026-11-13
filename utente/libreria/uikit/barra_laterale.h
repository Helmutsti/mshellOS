/* LA BARRA LATERALE, dal file Figma (la "Barra laterale" della finestra di Files e il componente
 * "Voce della barra laterale", pagina "Componenti complessi"). Una colonna larga
 * BARRA_LATERALE_LARGHEZZA (184), nel corpo della finestra (che ha SPAZIO_8 di padding e
 * SPAZIO_8 tra le aree: finestra_tk_aree), senza un fondo suo (si vede il vetro della
 * finestra), senza linea verso quello che ha a destra. PADDING ZERO: lo spazio lo da' il corpo
 * della finestra. SPAZIO_4 tra un pezzo e l'altro. I pezzi sono di due tipi:
 *
 *   - l'INTESTAZIONE di un gruppo ("Scorciatoie", "Filtri"): la stessa del menu (menu.h), con
 *     lo spazio orizzontale SPAZIO_8 che la allinea al testo delle voci;
 *   - la VOCE: alta ALTEZZA_S (32), padding SPAZIO_8 ai lati (ha un fondo suo), verticale 0, il
 *     simbolo da 16, SPAZIO_8, il nome in "corpo". Stati: Normale (piatta, senza fondo), Sopra
 *     (fondo CONTROLLO_SOPRA_VETRO), Selezionata (fondo CONTROLLO_SELEZIONE_VETRO e il nome in
 *     "corpo medio"). Raggio RAGGIO_CONTROLLO_NUOVO.
 *
 * Dal bordo della finestra al primo testo: 8 + 8 = 16 in x (corpo + padding della voce) e
 * 8 + 8 = 16 in y (corpo + lo spazio sopra il testo dell'intestazione). Il fondo della voce
 * scelta sta a 8 dal bordo della finestra e a 8 dal pannello (il gap del corpo).
 *
 * Quali voci sono selezionate lo chiede a 'selezionata' (anche piu' d'una: in Files la cartella
 * in cui si e' e il filtro acceso); un clic su una voce chiama 'al_clic' (scatta quando si
 * lascia, sulla stessa voce su cui si e' premuto, come un pulsante). Il codice e' in
 * interno/barra_laterale.c. */
#ifndef BARRA_LATERALE_H
#define BARRA_LATERALE_H

#define BARRA_LATERALE_LARGHEZZA 184
#define BARRA_LATERALE_GAP       SPAZIO_4
#define VOCE_LATERALE_ALTA       ALTEZZA_S
#define VOCE_LATERALE_DENTRO     SPAZIO_8
#define VOCE_LATERALE_GAP        SPAZIO_8
#define VOCE_LATERALE_SIMBOLO    ICONA_S
#define MAX_VOCI_LATERALI        16

enum { LATERALE_VOCE, LATERALE_INTESTAZIONE };

struct voce_laterale_tk {
    int tipo;                          /* LATERALE_VOCE o LATERALE_INTESTAZIONE */
    const char *testo;
    int icona;                         /* ICONA_... (le voci) */
};

struct barra_laterale_tk;
typedef void (*al_clic_laterale)(struct barra_laterale_tk *b, int voce, void *dati);
typedef int (*selezionata_laterale)(struct barra_laterale_tk *b, int voce, void *dati);

struct barra_laterale_tk {
    struct widget base;
    struct voce_laterale_tk voci[MAX_VOCI_LATERALI];
    int quante;
    int sopra;                         /* la voce sotto il puntatore, o -1 */
    int premuta;                       /* quella su cui si e' premuto, o -1 */
    selezionata_laterale selezionata;
    al_clic_laterale al_clic;
    void *dati;
};

/* Alta quanto il suo contenitore ('largo'), larga BARRA_LATERALE_LARGHEZZA. */
void barra_laterale_prepara(struct barra_laterale_tk *b, selezionata_laterale selezionata,
                            al_clic_laterale al_clic, void *dati);

/* Aggiunge in fondo l'intestazione di un gruppo. Oltre MAX_VOCI_LATERALI non aggiunge niente. */
void barra_laterale_intestazione(struct barra_laterale_tk *b, const char *testo);

/* Aggiunge una voce: il suo numero (per 'selezionata' e 'al_clic'), o -1 se ce ne sono gia'
 * MAX_VOCI_LATERALI. */
int barra_laterale_voce(struct barra_laterale_tk *b, const char *testo, int icona_);

#endif
