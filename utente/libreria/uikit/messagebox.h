/* IL MESSAGGIO IN LINEA, dal file Figma ("Messaggio in linea", pagina "App - Store") (UIKit,
 * incluso da uikit.h). Un messaggio DENTRO la finestra, non una finestra a parte: un avviso, un
 * errore o un'informazione, con un titolo e una spiegazione. Per esempio "<app> e' stata tolta",
 * "Dati svuotati", "Manca l'icona per il tema scuro", "Non si puo' installare". Il codice e' in
 * interno/messagebox.c.
 *
 * Un riquadro con il fondo tenue del suo ruolo (AVVISO_TENUE, PERICOLO_TENUE,
 * INFORMAZIONE_TENUE), raggio RAGGIO_PANNELLO (10), SPAZIO_12 di margine interno. Dentro, in riga
 * a SPAZIO_12: il simbolo di 20 (triangolo, cerchio con la X, cerchio con la i) e i testi, a
 * SPAZIO_4 l'uno dall'altro: il titolo in FONT_CORPO_MEDIO, la spiegazione in FONT_CORPO (va a
 * capo, al massimo MESSAGEBOX_RIGHE righe), tutti e due alti 20 e nel colore *_TESTO_TENUE del
 * ruolo (anche il simbolo).
 *
 * E' largo quanto il suo contenitore (la pagina gli da' la larghezza prima di misurarlo, vedi
 * pagina.h); alto quanto serve alla spiegazione. Testi nella code page 437. */
#ifndef UIKIT_MESSAGEBOX_H
#define UIKIT_MESSAGEBOX_H

enum { MESSAGEBOX_AVVISO, MESSAGEBOX_ERRORE, MESSAGEBOX_INFORMAZIONE };

#define MESSAGEBOX_SIMBOLO 20
#define MESSAGEBOX_RIGHE   4
#define MESSAGEBOX_TESTO   400

struct messagebox {
    struct widget base;
    int tipo;
    char titolo[MAX_TESTO_WIDGET * 2];
    char spiegazione[MESSAGEBOX_TESTO];
};

/* Prepara il messaggio del tipo 'tipo' (MESSAGEBOX_...) con il titolo e la spiegazione (0: niente;
 * tagliati alla misura dei loro posti nella struttura). */
void messagebox_prepara(struct messagebox *m, int tipo, const char *titolo, const char *spiegazione);

#endif
