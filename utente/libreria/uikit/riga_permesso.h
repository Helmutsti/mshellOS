/* LA RIGA DI PERMESSO, dal file Figma ("Riga di permesso", pagina "App - Store") (UIKit,
 * incluso da uikit.h). Una riga con un simbolo, un nome e la sua spiegazione: per i permessi,
 * le differenze dallo standard ("Cosa fa di diverso") e i ruoli. A destra, facoltativa,
 * un'etichetta di stato (Concesso / Non concesso / Nuovo). Il codice e' in
 * interno/riga_permesso.c.
 *
 * In riga a SPAZIO_12, centrati in altezza: il simbolo di 20 (TESTO_PRIMARIO), i testi (il nome
 * in FONT_CORPO_MEDIO alto 20, TESTO_PRIMARIO; sotto, la spiegazione in FONT_PICCOLO_NUOVO alta
 * 16 per riga, TESTO_SECONDARIO, che va a capo, al massimo RIGA_PERMESSO_RIGHE righe),
 * l'etichetta. Nessun margine (il disegno: le righe stanno a SPAZIO_16 l'una dall'altra, lo
 * spazio lo da' chi le contiene).
 * Larga quanto il suo contenitore (vedi pagina.h). Testi nella code page 437. */
#ifndef UIKIT_RIGA_PERMESSO_H
#define UIKIT_RIGA_PERMESSO_H

#define RIGA_PERMESSO_SIMBOLO 20
#define RIGA_PERMESSO_RIGHE   3

struct riga_permesso {
    struct widget base;
    int simbolo;                           /* ICONA_... */
    char nome[MAX_TESTO_WIDGET];
    char spiegazione[3 * MAX_TESTO_WIDGET];
    char stato[MAX_TESTO_WIDGET];          /* "" : niente etichetta */
    int tipo_stato;                        /* ETICHETTA_... */
};

/* Prepara la riga: il simbolo (ICONA_...), il nome, la spiegazione (0: niente), e l'etichetta
 * di stato con il suo testo e tipo (ETICHETTA_...); 'stato' 0 o "": senza etichetta. I testi si
 * tagliano alla misura dei loro posti nella struttura. */
void riga_permesso_prepara(struct riga_permesso *r, int simbolo, const char *nome,
                           const char *spiegazione, const char *stato, int tipo_stato);

#endif
