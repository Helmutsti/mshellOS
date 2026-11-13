/* IL MESSAGGIO IN LINEA, dal file Figma ("Messaggio in linea", pagina "App - Store") (UIKit,
 * incluso da uikit.h). Un messaggio DENTRO la finestra, non una finestra a parte: un avviso, un
 * errore o un'informazione, con un titolo e una spiegazione. Per esempio "<app> e' stata tolta",
 * "Dati svuotati", "Manca l'icona per il tema scuro", "Non si puo' installare". Il codice e' in
 * interno/messaggio.c.
 *
 * Un riquadro con il fondo tenue del suo ruolo (AVVISO_TENUE, PERICOLO_TENUE,
 * INFORMAZIONE_TENUE), raggio RAGGIO_PANNELLO (10), SPAZIO_12 di margine interno. Dentro, in riga
 * a SPAZIO_12: il simbolo di 20 (triangolo, cerchio con la X, cerchio con la i) e i testi, a
 * SPAZIO_4 l'uno dall'altro: il titolo in FONT_CORPO_MEDIO, la spiegazione in FONT_CORPO (va a
 * capo, al massimo MESSAGGIO_RIGHE righe), tutti e due alti 20 e nel colore *_TESTO_TENUE del
 * ruolo (anche il simbolo).
 *
 * E' largo quanto il suo contenitore (la pagina gli da' la larghezza prima di misurarlo, vedi
 * pagina.h); alto quanto serve alla spiegazione. Testi nella code page 437. */
#ifndef UIKIT_MESSAGGIO_H
#define UIKIT_MESSAGGIO_H

enum { MESSAGGIO_AVVISO, MESSAGGIO_ERRORE, MESSAGGIO_INFORMAZIONE };

#define MESSAGGIO_SIMBOLO 20
#define MESSAGGIO_RIGHE   4
#define MESSAGGIO_TESTO   400

struct messaggio {
    struct widget base;
    int tipo;
    char titolo[MAX_TESTO_WIDGET * 2];
    char spiegazione[MESSAGGIO_TESTO];
};

/* Prepara il messaggio del tipo 'tipo' (MESSAGGIO_...) con il titolo e la spiegazione (0: niente;
 * tagliati alla misura dei loro posti nella struttura). */
void messaggio_prepara(struct messaggio *m, int tipo, const char *titolo, const char *spiegazione);

#endif
