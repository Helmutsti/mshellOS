/* Lo stile del sistema: colori, misure e font dell'interfaccia, dal file Figma
 * "mshell - interfaccia" (le sue variabili hanno gli stessi nomi, qui in maiuscolo).
 *
 * Lo usano il toolkit, per dare a tutte le applicazioni lo stesso aspetto, e il
 * compositore, che disegna la barra di un'applicazione che non risponde (lei non puo'
 * piu' ridisegnarsi). Cambiare un valore qui cambia l'aspetto di tutto il sistema.
 *
 * L'aspetto e' "a vetro": pannelli bianchi semitrasparenti sopra lo sfondo. I colori
 * sono 0xAARRGGBB (vedi disegno.h): VELATO(0xFFFFFF, 36) e' il bianco al 14%. */
#ifndef STILE_H
#define STILE_H

#include "disegno.h"

/* --- il testo --- */
#define CONTENUTO_PRINCIPALE    OPACO(0xFFFFFF)
#define CONTENUTO_SECONDARIO    VELATO(0xFFFFFF, 140)    /* 55% */

/* --- il vetro: pulsanti e superfici --- */
#define VETRO_SUPERFICIE        VELATO(0xFFFFFF, 36)     /* 14% */
#define VETRO_SOPRA             VELATO(0xFFFFFF, 56)     /* 22% */
#define VETRO_PREMUTO           VELATO(0xFFFFFF, 20)     /*  8% */
#define VETRO_BORDO             VELATO(0xFFFFFF, 71)     /* 28% */
#define VETRO_LUCE              VELATO(0xFFFFFF, 56)     /* la riga di luce in alto, 22% */
#define PULSANTE_BORDO_COLORATO VELATO(0xFFFFFF, 46)     /* 18% */
#define PULSANTE_OMBRA          VELATO(0x081233, 64)     /* 25%, spostata di 2 in giu', sfocata 3 */

/* --- i campi e i controlli (caselle, scelte) --- */
#define CAMPO_SFONDO            VELATO(0xFFFFFF, 26)     /* 10% */
#define CAMPO_SFONDO_ATTIVO     VELATO(0xFFFFFF, 41)     /* 16% */
#define CAMPO_BORDO             VELATO(0xFFFFFF, 64)     /* 25% */
#define CONTROLLO_SFONDO        VELATO(0xFFFFFF, 31)     /* 12% */
#define CONTROLLO_BORDO         VELATO(0xFFFFFF, 140)    /* 55% */
#define ANELLO_SOPRA            VELATO(0x3B82F6, 89)     /* 35%, l'anello di 3 pixel intorno */

/* --- la finestra --- */
/* Il fondo scuro sotto il vetro: senza, sopra lo sfondo colorato il testo bianco non si
 * legge. (Non c'e' nel disegno, che conta sulla sfocatura da sola: e' la variante
 * "vetro scuro".) */
#define VETRO_FONDO             VELATO(0x0F172A, 128)    /* 50% */
#define BARRA_ATTIVA            VELATO(0xFFFFFF, 46)     /* 18% */
#define BARRA_INATTIVA          VELATO(0xFFFFFF, 20)     /*  8% */
#define BARRA_BORDO_ATTIVA      VELATO(0xFFFFFF, 77)     /* 30% */
#define FINESTRA_OMBRA          VELATO(0x050A24, 97)     /* 38%, in primo piano */
#define FINESTRA_OMBRA_SFONDO   VELATO(0x050A24, 56)     /* 22%, sullo sfondo */
#define FINESTRA_OMBRA_GIU      18
#define FINESTRA_OMBRA_SFOCATA  24

/* --- la barra degli strumenti ---
 * Il fondo e' lo stesso vetro della barra del titolo (BARRA_ATTIVA, o BARRA_INATTIVA
 * quando la finestra non e' in primo piano): titolo e strumenti sono un pezzo solo, e il
 * corpo comincia sotto. La pillola dei pulsanti e' di VETRO_SUPERFICIE. I pulsanti della pillola nel disegno vanno "al contrario" di quelli
 * normali: sotto il puntatore appena velati, premuti (o accesi) piu' chiari. */
#define SEGMENTO_SOPRA          VETRO_PREMUTO            /*  8% */
#define SEGMENTO_PREMUTO        VETRO_SOPRA              /* 22%, anche quello acceso */

/* --- gli elementi di un elenco (le righe di una tabella, i file) ---
 * Quello scelto e' di vetro chiaro, non blu; quello sotto il puntatore appena velato. */
#define ELEMENTO_SCELTO         VETRO_SOPRA              /* 22% */
#define ELEMENTO_SOPRA          VELATO(0xFFFFFF, 20)     /*  8% */

/* --- il menu del tasto destro (il menu contestuale) ---
 * Un pannello di vetro con le voci; la voce sotto il puntatore e' VETRO_SOPRA, le voci si
 * dividono in gruppi con una linea. L'ombra e' quella dei pannelli (giu' di 10, sfocata
 * 15). Il menu sta DENTRO la finestra, sopra il suo contenuto: quello che ha sotto lo
 * sfoca l'applicazione stessa (il compositore sfoca solo quello che sta sotto le
 * finestre), se no il testo di sotto si confonderebbe con le voci. */
#define MENU_SEPARATORE         VELATO(0xFFFFFF, 36)     /* 14% */
#define MENU_OMBRA              VELATO(0x050A24, 71)     /* 28% */

/* --- gli stati --- */
#define STATO_ERRORE            OPACO(0xEF4444)
#define STATO_ERRORE_VELO       VELATO(0xEF4444, 115)    /* 45% */
#define STATO_ATTIVO            OPACO(0x3B82F6)

/* --- la barra di scorrimento ---
 * Il cursore a riposo e sotto il puntatore e' CONTENUTO_SECONDARIO, la guida (che si vede
 * solo sotto il puntatore) VETRO_PREMUTO; mentre lo si trascina, il cursore e' questo. */
#define SCORRIMENTO_CURSORE_TRASCINATO VELATO(0xFFFFFF, 204)   /* 80% */

/* --- i colori dei pulsanti: normale, sopra, premuto --- */
#define BLU_NORMALE     OPACO(0x3B82F6)
#define BLU_SOPRA       OPACO(0x5B9BFF)
#define BLU_PREMUTO     OPACO(0x2563EB)
#define VERDE_NORMALE   OPACO(0x16A34A)
#define VERDE_SOPRA     OPACO(0x22C55E)
#define VERDE_PREMUTO   OPACO(0x15803D)
#define ROSSO_NORMALE   OPACO(0xDC2626)
#define ROSSO_SOPRA     OPACO(0xEF4444)
#define ROSSO_PREMUTO   OPACO(0xB91C1C)
#define ARANCIO_NORMALE OPACO(0xEA6A0A)
#define ARANCIO_SOPRA   OPACO(0xF98316)
#define ARANCIO_PREMUTO OPACO(0xC2570C)

#define OPACITA_DISABILITATO 102    /* 40% */
#define OPACITA_INATTIVO     153    /* 60%: i comandi della barra di una finestra sullo sfondo */

/* --- i raggi degli angoli --- */
#define RAGGIO_CONTROLLO  5
#define RAGGIO_PICCOLO    6
#define RAGGIO_MEDIO      8
#define RAGGIO_GRANDE    10
#define RAGGIO_FINESTRA  12

/* --- le misure --- */
#define BARRA_ALTEZZA     38
#define BARRA_MARGINE     10    /* ai lati della barra */
#define BARRA_COMANDO     22    /* i pulsanti tondi riduci e chiudi */
#define BARRA_EQUILIBRIO  50    /* lo spazio vuoto a sinistra, per centrare il titolo */
#define FINESTRA_MARGINE  20    /* intorno al contenuto */
#define SPAZIO_TRA        14    /* tra un widget e l'altro, in colonna */
#define CONTROLLO_LATO    18    /* il quadratino della casella, il tondo della scelta */
#define CAMPO_ALTEZZA     36
#define CAMPO_CURSORE_SPAZIO  1   /* tra l'ultima lettera scritta e il cursore */
#define SCORRIMENTO_LARGHEZZA  14   /* la striscia della barra di scorrimento, sopra il contenuto */
#define SCORRIMENTO_SOTTILE     6   /* il cursore mentre si scorre, a 4 pixel dal bordo */
#define SCORRIMENTO_SPESSO     10   /* il cursore sotto il puntatore o trascinato, a 2 */
#define SCORRIMENTO_MARGINE     2   /* "spazio/2": tra i capi della striscia e il cursore */
#define SCORRIMENTO_MINIMO     32   /* il cursore non e' mai piu' corto di cosi' */

/* la barra degli strumenti: sotto la barra del titolo, larga quanto la finestra */
#define STRUMENTI_ALTEZZA      40   /* 6 sopra + 28 di contenuto + 6 sotto */
#define STRUMENTI_CONTENUTO    28   /* l'altezza delle cose dentro (centrate in altezza) */
#define STRUMENTI_MARGINE      10   /* a sinistra e a destra */
#define STRUMENTI_SPAZIO        8   /* tra una cosa e l'altra */
/* la pillola: un gruppo di pulsanti solo icona, attaccati */
#define PILLOLA_MARGINE         2   /* tra il bordo della pillola e i pulsanti */
#define PILLOLA_SPAZIO          2   /* tra un pulsante e l'altro */
#define SEGMENTO_LATO          24   /* il pulsante della pillola, quadrato */
#define SEGMENTO_ICONA         16
#define RAGGIO_SEGMENTO        (RAGGIO_PICCOLO - PILLOLA_MARGINE)   /* 4: segue la pillola */
#define SEGMENTO_TESTO_MARGINE 10   /* il pulsante con un testo (le schede): a sinistra e a destra */
/* la ricerca piccola: un campo di testo basso, senza bordo, con la lente */
#define RICERCA_LARGHEZZA     180
#define RICERCA_ALTEZZA        28
#define RICERCA_MARGINE         8   /* a sinistra e a destra, dentro */
#define RICERCA_SPAZIO          6   /* tra la lente e il testo */
#define RICERCA_ICONA          14
/* il percorso: la casetta, poi i nomi delle cartelle separati da una freccetta */
#define PERCORSO_SPAZIO         4
#define PERCORSO_CASA          16   /* l'icona della casetta */
#define PERCORSO_FRECCIA       14   /* la freccetta tra un nome e l'altro */

/* il menu del tasto destro: un pannello largo 248, con 6 di margine dentro e 2 tra una voce
 * e l'altra. La voce e' alta 28 (8 ai lati, icona 16, 8 tra icona ed etichetta); la
 * linea che divide i gruppi occupa 9 (4 sopra, 1 di linea, 4 sotto) */
#define MENU_LARGHEZZA        248
#define MENU_MARGINE            6
#define MENU_SPAZIO             2
#define MENU_VOCE              28
#define MENU_VOCE_MARGINE       8
#define MENU_ICONA             16
#define MENU_SEPARA             9
/* gli elementi dei file (Files e la scrivania). Vista a icone: largo 84, 8 di margine
 * dentro, icona 56, 6 tra icona e nome, il nome al massimo su due righe; tra un elemento
 * e l'altro 8. Vista a elenco: righe alte 32 (10 ai lati e tra le colonne, icona 18),
 * 2 tra una riga e l'altra; le colonne Tipo, Dimensione e Modificato sono larghe 90, 80
 * e 110 */
#define ELEMENTO_LARGHEZZA     84
#define ELEMENTO_MARGINE        8
#define ELEMENTO_ICONA         56
#define ELEMENTO_SPAZIO         6
#define ELEMENTO_RIGHE_NOME     2
#define ELEMENTO_TRA            8
#define RIGA_FILE_ALTEZZA      32
#define RIGA_FILE_MARGINE      10
#define RIGA_FILE_ICONA        18
#define RIGA_FILE_TRA           2

/* --- i tempi (in millisecondi) --- */
#define DOPPIO_CLIC           500   /* due pressioni piu' vicine di cosi' sono un doppio clic */
#define SCORRIMENTO_COMPARE   100   /* la barra compare quando si comincia a scorrere */
#define SCORRIMENTO_RESTA    1000   /* resta ancora un secondo dopo l'ultimo scatto */
#define SCORRIMENTO_SVANISCE  300   /* poi svanisce */
#define SCORRIMENTO_ALLARGA   150   /* sotto il puntatore il cursore si allarga */

/* --- la rotella: quanto scorre uno scatto (come sul Mac) ---
 * Il Mac, con un mouse a scatti, ACCELERA: uno scatto lento sposta il contenuto di poco,
 * girando in fretta ogni scatto sposta di piu'. Apple non pubblica la sua curva; questa
 * e' un'imitazione semplice, con tre numeri:
 *   - uno scatto DA SOLO sposta di SCORRIMENTO_RIGA pixel. Sul Mac uno scatto lento vale
 *     "una riga", e una riga sono 10 pixel (il valore normale di Quartz, la grafica del
 *     Mac: CGEventSourceGetPixelsPerLine). Qui sono poco piu' di mezza riga di testo
 *     (FONT_TESTO e' alto 18) e un terzo di una riga di tabella (30): poco, apposta, per
 *     poter scorrere con precisione;
 *   - gli scatti che arrivano a meno di SCORRIMENTO_PAUSA ms l'uno dall'altro, nello stesso
 *     verso, sono una RAFFICA: il primo vale 1 riga, il secondo 2, il terzo 3... fino a
 *     SCORRIMENTO_SLANCIO righe a scatto. Quattro scatti veloci fanno 10+20+30+40 = 100
 *     pixel; quattro lenti, uno ogni mezzo secondo, 40;
 *   - una pausa piu' lunga, o un giro al contrario, chiude la raffica: lo scatto dopo
 *     riparte da una riga.
 * Gli scatti che arrivano insieme (il compositore li somma, se l'applicazione e' indietro)
 * sono stati girati in fretta: contano come scatti di una raffica. Vale lo stesso per la
 * rotella orizzontale. */
#define SCORRIMENTO_RIGA       10   /* pixel di uno scatto da solo */
#define SCORRIMENTO_SLANCIO     8   /* al massimo uno scatto vale 8 righe (80 pixel) */
#define SCORRIMENTO_PAUSA     200   /* ms senza scatti che chiudono una raffica */

/* --- i font --- */
#define FONT_TESTO        (&inter_14)
#define FONT_TITOLO       (&inter_semigrassetto_15)
#define FONT_BARRA        (&inter_semigrassetto_13)
/* La barra degli strumenti, come nel disegno: Inter normale a 13 per il testo dei campi e
 * i nomi del percorso; l'ultimo nome (la cartella in cui si e') e' semigrassetto a 13. */
#define FONT_STRUMENTI    (&inter_13)
#define FONT_PERCORSO_QUI (&inter_semigrassetto_13)
/* le schede della barra degli strumenti (i pulsanti della pillola con un testo) */
#define FONT_SEGMENTO     (&inter_medio_13)
/* le note piccole, in grigio (per esempio il riassunto sotto una tabella) */
#define FONT_PICCOLO      (&inter_13)
/* i file: il nome sotto l'icona (medio 12), le colonne dell'elenco e le scorciatoie dei
 * menu (12), i titoli delle sezioni e delle colonne (semigrassetto 12), il titolo dello
 * stato di una cartella (semigrassetto 14). Le voci dei menu sono FONT_STRUMENTI (13) */
#define FONT_NOME_FILE    (&inter_medio_12)
#define FONT_DETTAGLIO    (&inter_12)
#define FONT_SEZIONE      (&inter_semigrassetto_12)
#define FONT_STATO        (&inter_semigrassetto_14)

#endif
