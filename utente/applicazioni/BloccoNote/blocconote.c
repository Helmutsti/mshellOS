/* blocconote: l'editor di testo grafico. Per ora (primo passo, PIANO_EDITOR.md) e' una finestra con
 * l'area di testo (uikit/area_testo.h) e un documento nuovo, vuoto: si scrive, si seleziona, si
 * scorre, ma non c'e' ancora niente sui file. I pulsanti Nuovo, Apri e Salva sono nella barra degli
 * strumenti come nel disegno (disegni/BloccoNote/) e per ora non fanno niente: arriveranno con il
 * picker (la finestra Apri/Salva), che dara' all'app solo il file scelto, quindi l'app non ha permessi.
 *
 * Le misure sono quelle del disegno: finestra 760 x 588 a aree (corpo con 8 di padding e 8 tra le
 * aree), la barra degli strumenti con tre pulsanti solo icona (Secondario, S), e sotto l'area di
 * testo, che e' il pannello chiaro incassato e riempie il resto. */
#include "uikit/uikit.h"

#define LARGHEZZA 760
#define ALTEZZA 588

static struct finestra_tk finestra;
static struct barra_strumenti strumenti;
static struct pulsante p_nuovo, p_apri, p_salva;
static struct area_testo area;

/* Un pulsante solo icona della barra: Secondario, S, con il suo suggerimento (obbligatorio sui
 * pulsanti senza testo). Il clic non fa ancora niente. */
static void pulsante_della_barra(struct pulsante *p, int icona, const char *suggerimento) {
    pulsante_prepara(p, "", icona, 0, 0);
    pulsante_stile(p, SECONDARIO, MISURA_S);
    widget_suggerimento(&p->base, suggerimento, 0);
    barra_strumenti_aggiungi(&strumenti, &p->base);
}

int main(void) {
    barra_strumenti_prepara(&strumenti);
    pulsante_della_barra(&p_nuovo, ICONA_PLUS, "Nuovo");
    pulsante_della_barra(&p_apri, ICONA_FOLDER, "Apri");
    pulsante_della_barra(&p_salva, ICONA_SAVE, "Salva");

    area_testo_prepara(&area);
    area_testo_segnaposto(&area, "Scrivi qui\xe2\x80\xa6");       /* U+2026, i tre puntini */

    finestra_tk_prepara(&finestra, "Senza titolo \xe2\x80\x93 Blocco note", &area.base);   /* U+2013, il trattino lungo */
    finestra_tk_barra(&finestra, &strumenti.base);
    finestra_tk_aree(&finestra);
    finestra_tk_dimensione(&finestra, LARGHEZZA, ALTEZZA);
    finestra_tk_dai_fuoco(&finestra, &area.base);       /* si scrive subito */
    return uikit_esegui(&finestra);
}
