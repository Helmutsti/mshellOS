/* L'ETICHETTA DI STATO, dal file Figma ("Etichetta di stato", pagina "App - Store") (UIKit,
 * incluso da uikit.h). Una piccola pillola per uno stato: un permesso concesso o no, una cosa
 * nuova, un aggiornamento disponibile. Il codice e' in interno/etichetta_stato.c.
 *
 * Alta 24 (SPAZIO_4 + 16 + SPAZIO_4), larga quanto il testo piu' SPAZIO_8 per lato; il testo in
 * FONT_ETICHETTA. Quattro tipi, coi colori tenui dei ruoli:
 *   - Neutra: fondo CONTROLLO_SFONDO, testo TESTO_SECONDARIO ("Non concesso");
 *   - Successo: SUCCESSO_TENUE e SUCCESSO_TESTO_TENUE ("Concesso");
 *   - Informazione: INFORMAZIONE_TENUE e INFORMAZIONE_TESTO_TENUE ("Nuovo");
 *   - Avviso: AVVISO_TENUE e AVVISO_TESTO_TENUE.
 * (Nel disegno il testo ha 0,24 pixel di spaziatura tra le lettere: il font di oggi non la sa
 * fare.) */
#ifndef UIKIT_ETICHETTA_STATO_H
#define UIKIT_ETICHETTA_STATO_H

enum { ETICHETTA_NEUTRA, ETICHETTA_SUCCESSO, ETICHETTA_INFORMAZIONE, ETICHETTA_AVVISO };

#define ETICHETTA_STATO_ALTEZZA (2 * SPAZIO_4 + INTERLINEA_ETICHETTA)

struct etichetta_stato {
    struct widget base;
    char testo[MAX_TESTO_WIDGET];
    int tipo;
};

/* Prepara l'etichetta con il testo 'testo' (code page 437, tagliato a MAX_TESTO_WIDGET) e il
 * tipo 'tipo' (ETICHETTA_...). E' larga quanto il suo testo. */
void etichetta_stato_prepara(struct etichetta_stato *e, const char *testo, int tipo);

#endif
