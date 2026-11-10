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
#define BARRA_ATTIVA            VELATO(0xFFFFFF, 46)     /* 18% */
#define BARRA_INATTIVA          VELATO(0xFFFFFF, 20)     /*  8% */
#define BARRA_BORDO_ATTIVA      VELATO(0xFFFFFF, 77)     /* 30% */
#define FINESTRA_OMBRA          VELATO(0x050A24, 97)     /* 38%, in primo piano */
#define FINESTRA_OMBRA_SFONDO   VELATO(0x050A24, 56)     /* 22%, sullo sfondo */
#define FINESTRA_OMBRA_GIU      18
#define FINESTRA_OMBRA_SFOCATA  24

/* --- gli stati --- */
#define STATO_ERRORE            OPACO(0xEF4444)
#define STATO_ERRORE_VELO       VELATO(0xEF4444, 115)    /* 45% */
#define STATO_ATTIVO            OPACO(0x3B82F6)

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

/* --- i font --- */
#define FONT_TESTO        (&inter_14)
#define FONT_TITOLO       (&inter_semigrassetto_15)
#define FONT_BARRA        (&inter_semigrassetto_13)

#endif
