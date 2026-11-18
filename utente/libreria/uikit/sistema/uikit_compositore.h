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
 *   - IL PUNTATORE: i colori delle sue forme nel tema in uso (cursore/contorno e cursore/riempimento) e
 *     se il tema e' lo scuro (l'ombra e' piu' forte);
 *   - IL TEMA IN USO, da cui dipendono le due cose sopra: quando cambia, catalog lo dice al
 *     compositore (corekit/catalog.h) e lui lo sceglie (compositore_scegli_tema).
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
#include "corekit/catalog.h"   /* struct catalog_valori */

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

/* LA CORNICE DELLE FINESTRE CHE NON SONO DI UIKIT (Wayland: il programma disegna solo il suo
 * contenuto). Il compositore la disegna con lo stesso disegno della finestra di UIKit: il vetro,
 * la testata con il titolo centrato, i comandi riduci e chiudi, il filo di luce interno del tema
 * scuro. L'immagine e' l x a (premoltiplicata, 0xAARRGGBB) su cui 'p' disegna: il contenuto del
 * programma va sotto la testata, da cornice_testata_altezza() in giu', sul vetro (che dopo
 * si ritaglia con gli angoli: cornice_raggio()). 'attiva': ha il fuoco (titolo e comandi pieni;
 * se no, titolo secondario e comandi al 50%). */
int cornice_testata_altezza(void);
int cornice_raggio(void);
void cornice_disegna(struct pennello *p, int l, int a, const char *titolo, int attiva);

/* Cosa c'e' nel punto (x, y), dentro la cornice larga l: */
#define CORNICE_NIENTE  0   /* sotto la testata: e' del programma */
#define CORNICE_RIDUCI  1
#define CORNICE_CHIUDI  2
#define CORNICE_TESTATA 3   /* il resto della testata: si trascina la finestra */
int cornice_comando(int l, int x, int y);

/* Il velo di una finestra coperta da una finestra di scelta (componente "Velo della finestra",
 * 138:455): SUPERFICIE_VELO sulla forma della finestra in (x, y), larga l e alta a, con gli
 * angoli di 'raggio'. */
void velo_finestra_disegna(struct pennello *p, int x, int y, int l, int a, int raggio);

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

/* Sceglie il tema dalle impostazioni date da catalog (una risposta o un avviso: tema_da_catalog,
 * tema.h): da qui in poi barra congelata e ombre sono quelle del tema nuovo. Le ombre gia'
 * calcolate vanno rifatte. */
void compositore_scegli_tema(const struct catalog_valori *v);

#endif
