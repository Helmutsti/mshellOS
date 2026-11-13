/* LE SCHEDE, dal file Figma ("Schede" e "Scheda", Componenti base) (UIKit, incluso da uikit.h).
 *
 * Una barra di schede piatte, tutte larghe uguali, a SPAZIO_4 l'una dall'altra, che si divide la
 * larghezza che riceve (e' elastica: nella barra degli strumenti prende lo spazio che avanza).
 * Sotto resta 1 pixel vuoto dove Figma disegna la linea di base: la linea non si disegna (sul
 * vetro sarebbe una riga bianca: decisione della persona, 12/11/2026). Ne e' scelta una
 * sola. Ogni scheda e' alta ALTEZZA_S (32), senza fondo e senza ombra (forma piatta), con il
 * testo del pulsante S (FONT_PULSANTE_S) al centro:
 *   - Normale: testo TESTO_SECONDARIO;
 *   - Selezionata: testo TESTO_PRIMARIO e sotto il testo l'INDICATORE, una pillola alta 2 larga
 *     quanto la scheda meno il suo padding (PADDING_S per lato, come il testo), AZIONE_SFONDO;
 *   - Sopra (il puntatore sopra una scheda non scelta): il disegno non la mostra; qui il testo
 *     diventa TESTO_PRIMARIO, come le voci dei gruppi di pulsanti.
 * Il testo sta al centro dello spazio sopra l'indicatore (32 - 2).
 *
 * Un clic (quando si lascia, sopra la stessa scheda) la sceglie e chiama al_cambio.
 *
 *     schede_prepara(&schede, cambia_pagina, 0);
 *     schede_aggiungi(&schede, "Processi");
 *     schede_aggiungi(&schede, "Memoria");
 *
 * Il codice e' in interno/schede.c. */
#ifndef UIKIT_SCHEDE_H
#define UIKIT_SCHEDE_H

#define MAX_SCHEDE          8
#define SCHEDE_TRA          SPAZIO_4
#define SCHEDE_INDICATORE   2
#define SCHEDE_LINEA        1

struct schede;
typedef void (*al_cambio_schede)(struct schede *s, int scelta, void *dati);

struct schede {
    struct widget base;
    char nomi[MAX_SCHEDE][MAX_TESTO_WIDGET];
    int quante;
    int scelta;                            /* la scheda scelta */
    int sopra;                             /* quella sotto il puntatore, o -1 */
    int premuta;                           /* quella su cui si e' premuto, o -1 */
    al_cambio_schede al_cambio;
    void *dati;
};

/* Sceglie la scheda i (senza chiamare al_cambio). */
void schede_scegli(struct schede *s, int i);

/* Prepara la barra di schede, vuota ed elastica: 'al_cambio' riceve la scheda scelta con un
 * clic, con 'dati'. */
void schede_prepara(struct schede *s, al_cambio_schede al_cambio, void *dati);

/* Aggiunge una scheda in fondo; la prima e' quella scelta. Oltre MAX_SCHEDE non aggiunge niente. */
void schede_aggiungi(struct schede *s, const char *nome);

#endif
