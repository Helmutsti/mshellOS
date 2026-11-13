/* Quello che UIKit da' al compositore: la sua UNICA dipendenza da UIKit, scritta e voluta
 * (vedi LIBRERIE.md, "Un punto che oggi attraversa il confine").
 *
 * Il compositore usa solo GKit: non vede stile, temi, font e icone del sistema. Ma alcune
 * cose del design le deve disegnare lui, fuori dall'immagine delle applicazioni:
 *   - LA BARRA DEL TITOLO CONGELATA. Un'applicazione che non risponde non puo' piu'
 *     ridisegnarsi: la sua barra la disegna il compositore, al posto della sua (il vetro con
 *     un velo rosso, "... non risponde", il pulsante chiudi rosso che chiude il programma).
 *     Deve coprire proprio quella dell'applicazione, quindi conosce anche la barra della
 *     mini finestra ('mini' = 1: piu' bassa, con il titolo piccolo e il solo chiudi);
 *   - L'OMBRA E IL VETRO SMERIGLIATO intorno e sotto le finestre: sono del design (il token
 *     "ombra/finestra", diverso nei temi Chiaro e Scuro), anche se stanno fuori dalla finestra;
 *   - IL TEMA IN USO, da cui dipendono le due cose sopra: quando cambia (CAMBIA_TEMA, gkit/sistema/protocollo.h)
 *     il compositore lo rilegge dalle preferenze.
 *
 * E' un header DEL SISTEMA (uikit/sistema/): lo include solo il compositore, un'app non lo
 * deve includere.
 *
 * Qui ci sono solo le dichiarazioni, con i tipi di GKit; il codice sta in
 * interno/uikit_compositore.c, che vede lo stile. Le coordinate sono in pixel dello schermo, tranne
 * dove e' detto. */
#ifndef UIKIT_COMPOSITORE_H
#define UIKIT_COMPOSITORE_H

#include "gkit/gkit_disegno.h"

/* Quante righe copre la barra congelata, dall'alto della finestra: quelle dell'applicazione
 * li' sotto non si disegnano. */
int barra_congelata_altezza(int mini);

/* Il punto (x, y), DENTRO la finestra larga 'larghezza', e' sul pulsante chiudi della barra
 * congelata? 1 o 0. */
int barra_congelata_sul_chiudi(int larghezza, int mini, int x, int y);

/* Disegna con 'p' la barra congelata della finestra 'titolo' che sta in (x, y), larga l e
 * alta a (la finestra intera: il contorno rosso la gira tutta). */
void barra_congelata_disegna(struct pennello *p, int x, int y, int l, int a, const char *titolo,
                             int mini);

/* L'ombra di una finestra nel tema in uso: la tabella degli strati (gkit_disegno.h) e, in
 * 'quanti', quante righe ha. Gli strati esterni li disegna il compositore, gli interni
 * (dentro la finestra) l'applicazione. */
const struct strato_ombra *ombra_finestra(int *quanti);

/* L'ombra di un pannello della scrivania nel tema in uso: quella di una barra ("ombra/barra",
 * la userbar) o, se 'sopra' e' 1, quella di cio' che si apre sopra il resto ("ombra/
 * sovrapposto", il vassoio delle app, come i menu). Come per le finestre, gli strati
 * esterni li disegna il compositore, gli interni la scrivania. */
const struct strato_ombra *ombra_pannello(int sopra, int *quanti);

/* Quanto si sfoca lo sfondo sotto il vetro di una finestra: il raggio di Figma, in pixel. */
int sfocatura_finestra(void);

/* Rilegge il tema dalle preferenze (tema_leggi, tema.h): da qui in poi barra congelata e
 * ombre sono quelle del tema nuovo. Le ombre gia' calcolate vanno rifatte. */
void compositore_rileggi_tema(void);

#endif
