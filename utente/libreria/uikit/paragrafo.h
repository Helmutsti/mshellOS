/* IL PARAGRAFO: un testo che va a capo da solo, con l'interlinea del design (UIKit, incluso da
 * uikit.h). E' il "testo" dei frame del file Figma: un font, un'altezza di riga (20 per il corpo,
 * 16 per il piccolo, 32 per il titolo) e un colore. Diverso dall'etichetta, che e' alta quanto il
 * font e non va a capo.
 *
 * LA LARGHEZZA. Il paragrafo e' 'largo': prende la larghezza che gli da' il suo contenitore e va
 * a capo li'. Per questo, misurato, dice larghezza 0 (sta in quella che riceve) e l'altezza delle
 * righe che gli servono nella larghezza che ha gia' (w->l: la decide la pagina, vedi pagina.h,
 * prima di misurarlo). Con 'largo' a 0 invece e' largo quanto il suo testo, su una riga sola.
 *
 *     paragrafo_prepara(&nome, "Note", FONT_TITOLO_NUOVO, INTERLINEA_TITOLO, COLORE_ICONA_PRIMARIO);
 *
 * IL COLORE non e' un valore ma un modo della raccolta "Colore icona" (tema.h: COLORE_ICONA_PRIMARIO,
 * _SECONDARIO...), come il testo dei pulsanti della barra: il valore lo si chiede quando si
 * disegna, cosi' il paragrafo segue il tema anche quando cambia (TEMA_CAMBIATO).
 *
 * Il testo e' gia' nella code page 437 del font (come per l'etichetta). Il codice e' in
 * interno/paragrafo.c. */
#ifndef UIKIT_PARAGRAFO_H
#define UIKIT_PARAGRAFO_H

#define PARAGRAFO_TESTO 400                /* lettere al massimo, compreso lo 0 */
#define PARAGRAFO_RIGHE 6                  /* righe al massimo: oltre, l'ultima finisce con "..." */
#define PARAGRAFO_RIGA  200                /* lettere di una riga, compreso lo 0 */

enum { PARAGRAFO_SINISTRA, PARAGRAFO_CENTRO, PARAGRAFO_DESTRA };

/* Come testo_a_capo (dentro UIKit), ma con righe lunghe fino a PARAGRAFO_RIGA lettere (quella
 * e' fatta per i nomi dei file, e spezza dopo 92): spezza 'testo' in righe larghe al massimo
 * 'largo' pixel, al massimo 'massimo' righe, andando a capo dopo uno spazio o un trattino; se non
 * ci sta tutto, l'ultima riga finisce con "...". Quante righe. */
int paragrafo_a_capo(const struct font *f, const char *testo, int largo, char righe[][PARAGRAFO_RIGA], int massimo);

struct paragrafo {
    struct widget base;
    char testo[PARAGRAFO_TESTO];
    const struct font *font;
    int interlinea;                        /* l'altezza di una riga (INTERLINEA_...) */
    int colore;                            /* COLORE_ICONA_... (tema.h): il modo, non il valore */
    int allineamento;                      /* PARAGRAFO_SINISTRA, _CENTRO, _DESTRA */
};

/* Prepara il paragrafo, largo: 'testo' (anche 0: vuoto; tagliato a PARAGRAFO_TESTO), il
 * font, l'altezza di una riga e il colore (COLORE_ICONA_...). Allineato a sinistra. */
void paragrafo_prepara(struct paragrafo *g, const char *testo, const struct font *font,
                       int interlinea, int colore);

/* Cambia il testo. Se cambia il numero delle righe, chi lo contiene rifa' l'impaginazione
 * (uikit_ridisponi). */
void paragrafo_testo(struct paragrafo *g, const char *testo);

#endif
