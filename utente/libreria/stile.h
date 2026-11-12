/* Lo stile del sistema: colori, misure e font dell'interfaccia, dal file Figma
 * "mshell - interfaccia" (le sue variabili hanno gli stessi nomi, qui in maiuscolo).
 *
 * Lo usano il toolkit, per dare a tutte le applicazioni lo stesso aspetto, e il
 * compositore, che disegna la barra di un'applicazione che non risponde (lei non puo'
 * piu' ridisegnarsi). Cambiare un valore qui cambia l'aspetto di tutto il sistema.
 *
 * Ci sono DUE stili. Il nuovo (in alto): scala di grigi, colori semantici letti dal tema in
 * uso (Chiaro o Scuro, scelto a tempo di esecuzione: tema.h). Il vecchio (in fondo): "a
 * vetro", pannelli bianchi semitrasparenti sopra lo sfondo, che si toglie quando tutto sara'
 * migrato. I colori sono 0xAARRGGBB (vedi disegno.h): VELATO(0xFFFFFF, 36) e' il bianco al 14%. */
#ifndef STILE_H
#define STILE_H

#include "disegno.h"
#include "tema.h"

/* =====================================================================================
 * IL NUOVO STILE (fase 0C): scala di grigi, colori semantici, due temi
 * =====================================================================================
 * Valori copiati a mano dal file Figma (raccolte "Semantici" e "Misure"). Il vecchio stile "a vetro scuro" (sotto, in
 * "VECCHIO STILE") resta finche' toolkit, Files, Attivita, scrivania e compositore non sono
 * migrati; poi si cancella. Dove un nome nuovo sarebbe uguale a uno vecchio, il nuovo ha il
 * suffisso _NUOVO (si toglie, insieme al vecchio, a fine migrazione). */

/* --- i colori: SEMANTICI, dipendono dal tema in uso ---
 * Ogni macro e' un'espressione di colore (0xAARRGGBB non premoltiplicato, come OPACO e
 * VELATO) che legge dal tema corrente a ogni uso: funziona ovunque un colore e' ammesso,
 * anche dentro altre macro, e cambia da sola se cambia 'tema_corrente' (tema.h).
 * CONTROLLO_SFONDO e SCORRIMENTO_CURSORE_TRASCINATO esistono gia' nel vecchio stile (valori
 * "a vetro"): i nuovi hanno il suffisso _NUOVO. */
#define SUPERFICIE_SCENA                         (tema_corrente->superficie_scena)
#define SUPERFICIE_ELEVATA                       (tema_corrente->superficie_elevata)
#define SUPERFICIE_VETRO                         (tema_corrente->superficie_vetro)
#define SUPERFICIE_INCASSATA                     (tema_corrente->superficie_incassata)
#define CONTROLLO_SFONDO_NUOVO                   (tema_corrente->controllo_sfondo)   /* vecchio CONTROLLO_SFONDO: vedi sotto */
#define CONTROLLO_SFONDO_SOPRA                   (tema_corrente->controllo_sfondo_sopra)
#define CONTROLLO_SFONDO_PREMUTO                 (tema_corrente->controllo_sfondo_premuto)
#define CONTROLLO_SFONDO_DISABILITATO            (tema_corrente->controllo_sfondo_disabilitato)
#define AZIONE_SFONDO                            (tema_corrente->azione_sfondo)
#define AZIONE_SFONDO_SOPRA                      (tema_corrente->azione_sfondo_sopra)
#define AZIONE_SFONDO_PREMUTO                    (tema_corrente->azione_sfondo_premuto)
#define AZIONE_TESTO                             (tema_corrente->azione_testo)
#define TESTO_PRIMARIO                           (tema_corrente->testo_primario)
#define TESTO_SECONDARIO                         (tema_corrente->testo_secondario)
#define TESTO_DISABILITATO                       (tema_corrente->testo_disabilitato)
#define TESTO_INVERSO                            (tema_corrente->testo_inverso)
#define BORDO_CONTROLLO                          (tema_corrente->bordo_controllo)
#define BORDO_SEPARATORE                         (tema_corrente->bordo_separatore)
#define BORDO_FOCUS                              (tema_corrente->bordo_focus)
#define CURSORE_RIEMPIMENTO                      (tema_corrente->cursore_riempimento)
#define CURSORE_CONTORNO                         (tema_corrente->cursore_contorno)
#define OGGETTO_RIEMPIMENTO                      (tema_corrente->oggetto_riempimento)
#define OGGETTO_RIEMPIMENTO_FORTE                (tema_corrente->oggetto_riempimento_forte)
#define OGGETTO_TRATTO                           (tema_corrente->oggetto_tratto)
#define SCORRIMENTO_CURSORE                      (tema_corrente->scorrimento_cursore)
#define SCORRIMENTO_CURSORE_SOPRA                (tema_corrente->scorrimento_cursore_sopra)
#define SCORRIMENTO_CURSORE_TRASCINATO_NUOVO     (tema_corrente->scorrimento_cursore_trascinato)   /* vecchio SCORRIMENTO_CURSORE_TRASCINATO: vedi sotto */
#define SUGGERIMENTO_SFONDO                      (tema_corrente->suggerimento_sfondo)
#define SUGGERIMENTO_TESTO                       (tema_corrente->suggerimento_testo)
#define SUGGERIMENTO_TESTO_SECONDARIO            (tema_corrente->suggerimento_testo_secondario)
#define SUPERFICIE_VELO                          (tema_corrente->superficie_velo)
/* i colori di ruolo: pieno per i pulsanti (sfondo, sopra, premuto, testo), tenue per i fondi leggeri */
#define SUCCESSO_SFONDO                          (tema_corrente->successo_sfondo)
#define SUCCESSO_SFONDO_SOPRA                    (tema_corrente->successo_sfondo_sopra)
#define SUCCESSO_SFONDO_PREMUTO                  (tema_corrente->successo_sfondo_premuto)
#define SUCCESSO_TESTO                           (tema_corrente->successo_testo)
#define SUCCESSO_TENUE                           (tema_corrente->successo_tenue)
#define SUCCESSO_TESTO_TENUE                     (tema_corrente->successo_testo_tenue)
#define AVVISO_SFONDO                            (tema_corrente->avviso_sfondo)
#define AVVISO_SFONDO_SOPRA                      (tema_corrente->avviso_sfondo_sopra)
#define AVVISO_SFONDO_PREMUTO                    (tema_corrente->avviso_sfondo_premuto)
#define AVVISO_TESTO                             (tema_corrente->avviso_testo)
#define AVVISO_TENUE                             (tema_corrente->avviso_tenue)
#define AVVISO_TESTO_TENUE                       (tema_corrente->avviso_testo_tenue)
#define INFORMAZIONE_SFONDO                      (tema_corrente->informazione_sfondo)
#define INFORMAZIONE_SFONDO_SOPRA                (tema_corrente->informazione_sfondo_sopra)
#define INFORMAZIONE_SFONDO_PREMUTO              (tema_corrente->informazione_sfondo_premuto)
#define INFORMAZIONE_TESTO                       (tema_corrente->informazione_testo)
#define INFORMAZIONE_TENUE                       (tema_corrente->informazione_tenue)
#define INFORMAZIONE_TESTO_TENUE                 (tema_corrente->informazione_testo_tenue)
#define PERICOLO_SFONDO                          (tema_corrente->pericolo_sfondo)
#define PERICOLO_SFONDO_SOPRA                    (tema_corrente->pericolo_sfondo_sopra)
#define PERICOLO_SFONDO_PREMUTO                  (tema_corrente->pericolo_sfondo_premuto)
#define PERICOLO_TESTO                           (tema_corrente->pericolo_testo)
#define PERICOLO_TENUE                           (tema_corrente->pericolo_tenue)
#define PERICOLO_TESTO_TENUE                     (tema_corrente->pericolo_testo_tenue)
/* i colori di sistema: SISTEMA_SATURO(COLORE_BLU), SISTEMA_PASTELLO(...), SISTEMA_TESTO_PASTELLO(...) */
#define SISTEMA_SATURO(c)                        (tema_corrente->sistema[(c)].saturo)
#define SISTEMA_PASTELLO(c)                      (tema_corrente->sistema[(c)].pastello)
#define SISTEMA_TESTO_PASTELLO(c)                (tema_corrente->sistema[(c)].testo_pastello)
/* --- le misure del nuovo design (non dipendono dal tema), in pixel ---
 * Controlli S/M/L: altezza, icona, padding ai lati, gap tra icona e testo. */
#define ALTEZZA_XS              24   /* il pulsante icona piu' piccolo (chiusura della mini finestra) */
#define ALTEZZA_S               32
#define ALTEZZA_M               40
#define ALTEZZA_L               48
#define ALTEZZA_BARRA           40
#define ALTEZZA_BARRA_MINI      28   /* la barra del titolo della mini finestra */
#define ICONA_XS                14
#define ICONA_S                 16
#define ICONA_M                 18
#define ICONA_L                 20
#define ICONA_OGGETTO           48   /* l'icona di un file o di una cartella (grande) */
#define ICONA_OGGETTO_PICCOLA   24   /* la stessa, piccola */
#define PADDING_S               12
#define PADDING_M               16
#define PADDING_L               20
#define GAP_S                    6
#define GAP_M                    8
#define GAP_L                    8
#define GRUPPO_PADDING           4   /* dentro un gruppo di pulsanti (la pillola) */
/* la scala degli spazi */
#define SPAZIO_0                 0
#define SPAZIO_4                 4
#define SPAZIO_8                 8
#define SPAZIO_12               12
#define SPAZIO_16               16
#define SPAZIO_24               24
#define SPAZIO_32               32
/* i raggi. Azioni (pulsanti, gruppi, ricerca) = pillola: il raggio e' meta' altezza, quindi
 * RAGGIO_PILLOLA e' un raggio "grande a piacere" che il disegno limita a meta' altezza. I
 * campi di testo sono rettangoli arrotondati (RAGGIO_CAMPO_S/M/L, uno per altezza).
 * Nomi uguali a quelli vecchi: RAGGIO_CONTROLLO (vecchio 5) e RAGGIO_FINESTRA (vecchio 12,
 * lo usa il codice) hanno qui il suffisso _NUOVO. Valori "C sobrio" (scelti il 12/11/2026). */
#define RAGGIO_PILLOLA         999
#define RAGGIO_CASELLA           3
#define RAGGIO_CONTROLLO_NUOVO   6
#define RAGGIO_PANNELLO         10
#define RAGGIO_FINESTRA_NUOVO   12
#define RAGGIO_CAMPO_S           6
#define RAGGIO_CAMPO_M           6
#define RAGGIO_CAMPO_L           8
#define TILE_LARGHEZZA          96   /* un elemento a icone (file, cartella) */
/* la barra di scorrimento: la striscia (zona sensibile) e' larga 14, il binario 12, il
 * cursore 8 per almeno 64 di lunghezza. SCORRIMENTO_CURSORE_NUOVO e' lo spessore del cursore
 * (il vecchio SCORRIMENTO_SPESSO/SOTTILE restano); il colore SCORRIMENTO_CURSORE e' un'altra cosa. */
#define SCORRIMENTO_ZONA        14
#define SCORRIMENTO_BINARIO     12
#define SCORRIMENTO_CURSORE_NUOVO     8
#define SCORRIMENTO_CURSORE_LUNGHEZZA 64
#define INDICATORE_SPESSORE      2   /* la lineetta sotto la scheda scelta */
/* la finestra: titolo alto 40 con 4 di margine intorno ai comandi, contenuto con 24 di padding */
#define TITOLO_ALTEZZA          40
#define TITOLO_MARGINE           4
#define FINESTRA_PADDING        24
#define TITOLO_DIVISORE          1   /* la linea (BORDO_SEPARATORE) sotto la testata, da bordo a bordo */
#define TITOLO_COMANDI_LARGHEZZA (2 * ALTEZZA_S + 3 * TITOLO_MARGINE)   /* riduci e chiudi, con i margini: 76 */
#define OPACITA_COMANDI_INATTIVI 128   /* i comandi di una finestra sullo sfondo: 50% */
/* la barra degli strumenti e il gruppo di pulsanti (la pillola) */
#define STRUMENTI_MARGINE_LATI  SPAZIO_12   /* ai lati della barra (sopra e sotto c'e' TITOLO_MARGINE) */
#define GRUPPO_ELEMENTO         24   /* l'altezza di una voce dentro il gruppo: ALTEZZA_S - 2 * GRUPPO_PADDING */
#define SCORRIMENTO_CURSORE_MARGINE 2   /* tra il binario e il cursore, per lato */

/* --- i font del nuovo design ---
 * Il disegno usa IBM Plex Sans (font plex_*, generati da strumenti/grafica). Ogni stile di testo
 * (Plex dimensione/interlinea/spaziatura) ha il suo font; dove il disegno chiede una
 * dimensione o un peso che non e' stato generato si usa il piu' vicino:
 *   titolo          SemiBold 24/32 (-1)   ->  plex_semigrassetto_24
 *   titolo-finestra SemiBold 14/20        ->  plex_semigrassetto_14
 *   sezione         SemiBold 15/20        ->  plex_semigrassetto_15
 *   corpo           Regular  14/20        ->  plex_14
 *   corpo-medio     Medium   14/20        ->  plex_medio_14
 *   piccolo         Regular  12/16        ->  plex_12
 *   etichetta       Medium   12/16 (+2)   ->  plex_medio_12
 *   pulsante-s/m/l  Medium   13/16, 14/20, 16/24  ->  plex_medio_13, _14, _16
 * (FONT_TITOLO, FONT_SEZIONE e FONT_PICCOLO esistono gia' nel vecchio stile: _NUOVO.) */
#define FONT_TITOLO_NUOVO       (&plex_semigrassetto_24)
#define FONT_TITOLO_FINESTRA    (&plex_semigrassetto_14)
#define FONT_SEZIONE_NUOVO      (&plex_semigrassetto_15)
#define FONT_CORPO              (&plex_14)
#define FONT_CORPO_MEDIO        (&plex_medio_14)
#define FONT_PICCOLO_NUOVO      (&plex_12)
#define FONT_ETICHETTA          (&plex_medio_12)
#define FONT_PULSANTE_S         (&plex_medio_13)
#define FONT_PULSANTE_M         (&plex_medio_14)
#define FONT_PULSANTE_L         (&plex_medio_16)
/* l'interlinea (altezza della riga) di ogni stile, e la spaziatura delle lettere in pixel
 * (negativa = piu' stretto; l'etichetta e' spaziata di 2) */
#define INTERLINEA_TITOLO       32
#define INTERLINEA_TITOLO_FINESTRA 20
#define INTERLINEA_SEZIONE      20
#define INTERLINEA_CORPO        20
#define INTERLINEA_PICCOLO      16
#define INTERLINEA_ETICHETTA    16
#define INTERLINEA_PULSANTE_S   16
#define INTERLINEA_PULSANTE_M   20
#define INTERLINEA_PULSANTE_L   24
#define SPAZIATURA_TITOLO       (-1)
#define SPAZIATURA_ETICHETTA     2

/* --- le ombre: strati sovrapposti ---
 * Una tabella di 'struct strato_ombra' (disegno.h) per ogni livello di profondita' e per ogni
 * tema, nell'ordine dei token: la PRIMA riga sta sopra a tutte (vedi il commento delle ombre
 * a strati in disegno.h per l'ordine in cui si disegnano). Livelli: incassato (campi,
 * binari), sollevato (pulsanti), sollevato_pieno (pulsante d'azione), sovrapposto (menu,
 * suggerimento), barra, finestra, focus (l'anello), puntatore. Si sceglie la tabella del
 * tema in uso (tema_corrente == &tema_scuro ? ..._scuro : ..._chiaro) e si passa a
 * ombre_strati_fuori / ombre_strati_dentro; il numero di righe e' OMBRA_STRATI(tabella).
 * Il JSON ha anche degli strati "BB" (sfocatura dello sfondo, solo un raggio) per barra e
 * finestra: struct strato_ombra non li puo' dire, e non sono ombre; sono le due misure
 * qui sotto, per chi sfoca quello che sta sotto. */
#define OMBRA_STRATI(tabella)          ((int)(sizeof(tabella) / sizeof((tabella)[0])))
#define OMBRA_BARRA_SFOCATURA_SFONDO   20
#define OMBRA_FINESTRA_SFOCATURA_SFONDO 28
static const struct strato_ombra ombra_incassato_chiaro[] = {
    { VELATO(0x14141A, 18), OMBRA_INTERNA, 0, 1, 2, 0 },
    { VELATO(0x14141A, 10), OMBRA_ESTERNA, 0, 1, 0, 0 },
};
static const struct strato_ombra ombra_sollevato_chiaro[] = {
    { VELATO(0x14141A, 15), OMBRA_ESTERNA, 0, 1, 1, 0 },
    { VELATO(0x14141A, 18), OMBRA_ESTERNA, 0, 2, 4, -1 },
    { VELATO(0x14141A, 15), OMBRA_ESTERNA, 0, 6, 12, -4 },
};
static const struct strato_ombra ombra_sollevato_pieno_chiaro[] = {
    { VELATO(0x1F1F26, 26), OMBRA_ESTERNA, 0, 1, 1, 0 },
    { VELATO(0x1F1F26, 41), OMBRA_ESTERNA, 0, 4, 8, -2 },
    { VELATO(0x1F1F26, 46), OMBRA_ESTERNA, 0, 10, 20, -6 },
};
static const struct strato_ombra ombra_sovrapposto_chiaro[] = {
    { VELATO(0x14141A, 13), OMBRA_ESTERNA, 0, 1, 2, 0 },
    { VELATO(0x14141A, 20), OMBRA_ESTERNA, 0, 8, 16, -4 },
    { VELATO(0x14141A, 36), OMBRA_ESTERNA, 0, 20, 40, -12 },
};
static const struct strato_ombra ombra_barra_chiaro[] = {
    { VELATO(0x14141A, 15), OMBRA_ESTERNA, 0, 1, 2, 0 },
    { VELATO(0x14141A, 26), OMBRA_ESTERNA, 0, 6, 14, -4 },
    { VELATO(0x14141A, 41), OMBRA_ESTERNA, 0, 16, 32, -10 },
};
static const struct strato_ombra ombra_finestra_chiaro[] = {
    { VELATO(0x14141A, 13), OMBRA_ESTERNA, 0, 2, 4, 0 },
    { VELATO(0x14141A, 26), OMBRA_ESTERNA, 0, 12, 24, -6 },
    { VELATO(0x14141A, 51), OMBRA_ESTERNA, 0, 32, 64, -16 },
};
static const struct strato_ombra ombra_focus_chiaro[] = {
    { VELATO(0x14141A, 18), OMBRA_INTERNA, 0, 1, 2, 0 },
    { VELATO(0x14141A, 36), OMBRA_ESTERNA, 0, 0, 0, 3 },
};
static const struct strato_ombra ombra_puntatore_chiaro[] = {
    { VELATO(0x000000, 51), OMBRA_ESTERNA, 0, 1, 1, 0 },
    { VELATO(0x000000, 66), OMBRA_ESTERNA, 0, 3, 6, -1 },
};
static const struct strato_ombra ombra_incassato_scuro[] = {
    { VELATO(0x000000, 89), OMBRA_INTERNA, 0, 1, 3, 0 },
};
static const struct strato_ombra ombra_sollevato_scuro[] = {
    { VELATO(0xFFFFFF, 15), OMBRA_INTERNA, 0, 1, 0, 0 },
    { VELATO(0x000000, 51), OMBRA_ESTERNA, 0, 1, 1, 0 },
    { VELATO(0x000000, 61), OMBRA_ESTERNA, 0, 3, 6, -1 },
    { VELATO(0x000000, 56), OMBRA_ESTERNA, 0, 8, 14, -4 },
};
static const struct strato_ombra ombra_sollevato_pieno_scuro[] = {
    { VELATO(0xFFFFFF, 77), OMBRA_INTERNA, 0, 1, 0, 0 },
    { VELATO(0x000000, 56), OMBRA_ESTERNA, 0, 1, 1, 0 },
    { VELATO(0x000000, 77), OMBRA_ESTERNA, 0, 4, 8, -2 },
    { VELATO(0x000000, 77), OMBRA_ESTERNA, 0, 12, 22, -6 },
};
static const struct strato_ombra ombra_sovrapposto_scuro[] = {
    { VELATO(0xFFFFFF, 13), OMBRA_INTERNA, 0, 1, 0, 0 },
    { VELATO(0x000000, 51), OMBRA_ESTERNA, 0, 2, 3, 0 },
    { VELATO(0x000000, 77), OMBRA_ESTERNA, 0, 10, 20, -4 },
    { VELATO(0x000000, 97), OMBRA_ESTERNA, 0, 24, 44, -12 },
};
static const struct strato_ombra ombra_barra_scuro[] = {
    { VELATO(0xFFFFFF, 18), OMBRA_INTERNA, 0, 1, 0, 0 },
    { VELATO(0x000000, 61), OMBRA_ESTERNA, 0, 1, 2, 0 },
    { VELATO(0x000000, 82), OMBRA_ESTERNA, 0, 8, 16, -4 },
    { VELATO(0x000000, 107), OMBRA_ESTERNA, 0, 20, 36, -10 },
};
static const struct strato_ombra ombra_finestra_scuro[] = {
    { VELATO(0xFFFFFF, 18), OMBRA_INTERNA, 0, 1, 0, 0 },
    { VELATO(0x000000, 56), OMBRA_ESTERNA, 0, 2, 4, 0 },
    { VELATO(0x000000, 87), OMBRA_ESTERNA, 0, 16, 28, -6 },
    { VELATO(0x000000, 128), OMBRA_ESTERNA, 0, 40, 72, -16 },
};
static const struct strato_ombra ombra_focus_scuro[] = {
    { VELATO(0x000000, 89), OMBRA_INTERNA, 0, 1, 3, 0 },
    { VELATO(0xFFFFFF, 36), OMBRA_ESTERNA, 0, 0, 0, 3 },
};
static const struct strato_ombra ombra_puntatore_scuro[] = {
    { VELATO(0x000000, 140), OMBRA_ESTERNA, 0, 1, 1, 0 },
    { VELATO(0x000000, 140), OMBRA_ESTERNA, 0, 3, 6, -1 },
};

/* Le tabelle del tema in uso: OMBRA_TAB(sollevato) da' la tabella giusta per il tema
 * corrente, e OMBRA_N(sollevato) il suo numero di righe. Si usano con ombre_strati_fuori /
 * ombre_strati_dentro. (Le due varianti hanno la stessa forma, ma righe diverse.) */
/* ATTENZIONE: il tema in uso si riconosce dal VALORE (la superficie di scena, diversa nei due
 * temi), non dall'indirizzo. tema_scuro e' 'static const' in tema.h: ogni file .c ne ha una
 * copia sua, e 'tema_corrente' (debole, unico per programma) punta a quella del file che ha
 * chiamato tema_scegli (libreria.c), che non e' la copia di toolkit.h: confrontare gli
 * indirizzi darebbe sempre "chiaro" nelle applicazioni. */
#define TEMA_SCURO_IN_USO (tema_corrente->superficie_scena == tema_scuro.superficie_scena)
#define OMBRA_TAB(nome)  (TEMA_SCURO_IN_USO ? ombra_##nome##_scuro : ombra_##nome##_chiaro)
#define OMBRA_N(nome)    (TEMA_SCURO_IN_USO ? OMBRA_STRATI(ombra_##nome##_scuro) \
                                                       : OMBRA_STRATI(ombra_##nome##_chiaro))

/* altre misure del nuovo design nate nei controlli (non hanno un token) */
/* Quanto un widget puo' disegnare fuori dal suo rettangolo (il toolkit ritaglia li'): l'anello
 * del fuoco (3) e le ombre a strati, la piu' lunga e' quella del pulsante primario (giu' di
 * 10 + sfocatura 20 - diffusione 6 = 24). */
#define WIDGET_SBORDO           24
#define CASELLA_LATO            18   /* il quadratino della casella e il tondo della scelta */
#define CASELLA_GAP              8   /* tra il quadratino e il suo testo */
#define SCELTA_PUNTO             8   /* il punto al centro della scelta accesa */
#define BORDO_FOCUS_SPESSORE   384   /* 1,5 px in 256esimi: il bordo di un campo con il fuoco */
#define RICERCA_LARGHEZZA_NUOVA 180

/* =====================================================================================
 * VECCHIO STILE (da eliminare a fine migrazione)
 * =====================================================================================
 * Il tema scuro "vetro" di prima, con i nomi di prima: tutto il sistema lo usa ancora. */


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
