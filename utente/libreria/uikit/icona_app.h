/* L'ICONA DI UN'APP, dal file Figma ("Icona dell'app": il quadrato arrotondato del pacchetto)
 * (UIKit, incluso da uikit.h). Nel disegno e' l'icona vera dell'app (il suo SVG, o la PNG
 * piccola). Oggi UIKit non sa ancora disegnare gli SVG ne' leggere le PNG dal disco (STATO.md,
 * "Spostare tutte le risorse su disco"): al suo posto c'e' un SEGNAPOSTO con la stessa forma, un
 * quadrato arrotondato CONTROLLO_SFONDO_SOPRA con il simbolo "pacchetto" in TESTO_SECONDARIO.
 * Il codice e' in interno/icona_app.c.
 *
 * Le misure sono quelle del disegno, in proporzione al lato: raggio 22/96 (7,5 a 32, 14,7 a 64),
 * simbolo 20/32 del lato (le icone del sistema hanno solo alcune misure: la piu' vicina). */
#ifndef UIKIT_ICONA_APP_H
#define UIKIT_ICONA_APP_H

struct icona_app { struct widget base; int lato; };

/* Prepara l'icona, un quadrato di lato 'lato' pixel (nel disegno 32 negli elenchi, 64 nel
 * dettaglio). */
void icona_app_prepara(struct icona_app *i, int lato);

#endif
