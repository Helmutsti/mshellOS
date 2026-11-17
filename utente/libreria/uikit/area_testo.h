/* L'AREA DI TESTO, dal componente "Area di testo" del file Figma (UIKit, incluso da uikit.h): il
 * posto dove si scrive un testo lungo (il Blocco note; ogni app che ne ha bisogno). Il componente
 * E' il pannello: fondo SUPERFICIE_ELEVATA, angoli RAGGIO_PANNELLO, SPAZIO_16 di padding su tutti i
 * lati, e quello che scorre si ritaglia sul bordo del pannello (non sul padding). Sta nel corpo della
 * finestra a aree (finestra_tk_aree) senza un altro pannello intorno: e' elastico e largo, prende lo
 * spazio che avanza.
 *
 * IL TESTO E' A LARGHEZZA FISSA, DISEGNATO A GRIGLIA come il Terminale: IBM Plex Mono 13, righe da 20,
 * ogni lettera in una casella (la colonna k comincia a k * 7,8 pixel, arrotondato:
 * AREA_TESTO_CASELLA_X). Il testo e' UTF-8: una lettera, cioe' un carattere Unicode, e' una casella,
 * qualunque sia il numero di byte. Se il font non ha una lettera, si vede il segno sostitutivo.
 *
 * IL TESTO LO TIENE IL COMPONENTE, in un BUFFER CON IL BUCO (gap buffer): i byte stanno in un blocco di
 * memoria con un vuoto, che segue il cursore; scrivere e cancellare dove si e' costa quanto il byte
 * scritto, non tutto il testo. Il blocco cresce da solo. Il testo non ha un limite, e non ha mai uno 0
 * dentro (i controlli, tranne a capo e tabulazione, non entrano).
 *
 * A CAPO AUTOMATICO alla larghezza dell'area: una riga che non ci sta continua sotto. Si va a capo A
 * PAROLA: dopo l'ultimo spazio (o tabulazione) che ci sta; una parola piu' lunga di una riga intera si
 * spezza a carattere. Gli spazi non fanno mai andare a capo (restano in fondo alla riga, anche oltre il
 * bordo, invisibili). L'a capo vero (Invio) e' il carattere '\n'. Le righe cosi' ottenute sono "righe
 * visive"; "riga" qui sotto e' una riga visiva, tranne dove si dice "riga del testo" (quella tra due
 * '\n').
 *
 * LA TABULAZIONE e' UN CARATTERE: '\t' nel testo, mostrato fino alla prossima colonna multipla di
 * AREA_TESTO_TAB (4), contando dall'inizio della riga visiva. Tab inserisce un '\t'. Con piu' righe
 * del testo selezionate, Tab ne aggiunge uno all'inizio di ognuna; Maiusc+Tab toglie il rientro
 * all'inizio di ogni riga selezionata (o della riga del cursore): un '\t', o fino a 4 spazi. Come
 * Visual Studio Code.
 *
 * LA TASTIERA. Scrivere (anche le lettere accentate), Invio, Backspace, Canc. Il cursore: frecce
 * (su e giu' tengono la colonna voluta tra righe di lunghezza diversa; sulla prima riga su va
 * all'inizio del testo, sull'ultima giu' alla fine), Home e Fine (inizio e fine della riga), PagSu e
 * PagGiu (una pagina), Ctrl+sinistra e Ctrl+destra (parola per parola), Ctrl+Home e Ctrl+Fine (inizio e
 * fine del testo). Con Maiusc, ognuno di questi muove un capo della selezione e l'altro resta fermo;
 * Ctrl+A seleziona tutto. Scrivere con una selezione la sostituisce; Backspace e Canc la cancellano.
 * Le lettere arrivano come caratteri ('tasto' del widget), le altre come tasti fisici
 * ('tasto_fisico' del widget, uikit.h): l'app non deve fare niente. Non ci sono ancora annulla e
 * ripeti, copia e incolla.
 *
 * IL MOUSE. Un clic mette il cursore, trascinare seleziona (la vista segue il cursore), un doppio
 * clic seleziona la parola. La rotella scorre; la barra di scorrimento e' quella della libreria, a filo
 * del bordo destro del pannello, e compare con la rotella o con il puntatore sopra.
 *
 * IL FUOCO. Con il fuoco: il cursore (una barra di 2 x 16, TESTO_CURSORE) e il testo selezionato con
 * lo sfondo TESTO_SELEZIONE e il colore INFORMAZIONE_TESTO_TENUE. Senza: niente cursore, la selezione
 * resta (sfondo TESTO_SELEZIONE_INATTIVA, grigio) e il testo non cambia colore. Vuota, mostra il
 * segnaposto in TESTO_SECONDARIO.
 *
 *     static struct area_testo area;
 *     area_testo_prepara(&area);
 *     area_testo_segnaposto(&area, "Scrivi qui\xe2\x80\xa6");
 *     finestra_tk_prepara(&finestra, "Blocco note", &area.base);
 *     finestra_tk_aree(&finestra);
 *     finestra_tk_dai_fuoco(&finestra, &area.base);   // si scrive subito
 *
 * Il codice e' in interno/area_testo.c. */
#ifndef AREA_TESTO_H
#define AREA_TESTO_H

#define AREA_TESTO_FONT        (&plexmono_13)
#define AREA_TESTO_RIGA        20                  /* l'altezza di una riga */
#define AREA_TESTO_DENTRO      SPAZIO_16           /* il padding del pannello */
#define AREA_TESTO_TAB         4                   /* la larghezza della tabulazione, in caselle */
#define AREA_TESTO_CURSORE_L   2                   /* il cursore: una barra di 2 x 16, al centro della riga */
#define AREA_TESTO_CURSORE_A   16
/* Dove comincia la casella 'k', dall'inizio del testo: le lettere di Plex Mono 13 avanzano di 7,8
 * pixel (Figma non li arrotonda), quindi k * 39 / 5, arrotondato. Il font ha l'avanzo intero (8):
 * ogni lettera si disegna da sola al suo posto. */
#define AREA_TESTO_CASELLA_X(k) (((k) * 39 + 2) / 5)

struct riga_area_testo {
    int ini, fine;                     /* i byte del testo: da 'ini' a 'fine' (esclusa; senza il '\n') */
};

struct area_testo {
    struct widget base;
    struct barra_scorrimento barra;
    /* il buffer con il buco: i byte [0, buco_ini) e [buco_fin, cap) sono il testo, in mezzo il vuoto */
    char *testo;
    int cap, buco_ini, buco_fin;
    /* il cursore e il capo fermo della selezione (uguali: niente selezione), in byte dall'inizio del testo */
    int cursore, ancora;
    int voluta;                        /* la colonna voluta andando su e giu' (-1: quella del cursore) */
    int a_fine_riga;                   /* 1: a capo automatico, il cursore sta in fondo alla riga di sopra e non all'inizio di questa */
    /* le righe visive, rifatte a ogni cambiamento di testo o di larghezza */
    struct riga_area_testo *righe;
    int n_righe, cap_righe, colonne;
    const char *segnaposto;            /* il testo grigio quando e' vuota (UTF-8), o 0 */
    int premuto;                       /* il pulsante del mouse e' giu' qui (si sta trascinando) */
    unsigned int clic_prima;           /* tempo() dell'ultimo clic, e dove, per il doppio clic */
    int clic_su;
};

/* Prepara un'area di testo vuota, elastica e larga (con la misura del disegno, 480 x 240, se nessuno
 * le dice altro). Non ha il fuoco: per scrivere subito finestra_tk_dai_fuoco. */
void area_testo_prepara(struct area_testo *a);

/* Il segnaposto, grigio, quando non c'e' testo ("Scrivi qui..."). La stringa non si copia: deve
 * restare viva. 0: niente. */
void area_testo_segnaposto(struct area_testo *a, const char *testo);

/* Sostituisce tutto il testo con 'utf8' (uno 0 alla fine): il cursore va all'inizio, niente
 * selezione. I '\r' si saltano e gli altri controlli (tranne '\n' e '\t') si tolgono. */
void area_testo_imposta(struct area_testo *a, const char *utf8);

/* Quanti byte ha il testo. */
int area_testo_lunghezza(const struct area_testo *a);

/* Copia il testo in 'dove' (UTF-8, con lo 0 alla fine, al massimo 'massimo' byte compreso lo 0, senza
 * tagliare un carattere a meta'). Dice quanti byte ha il testo intero (come area_testo_lunghezza). */
int area_testo_copia(const struct area_testo *a, char *dove, int massimo);

#endif
