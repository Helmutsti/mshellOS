/* I pulsanti colorati (Pericolo, Successo, Avviso, Informazione) in misura M con l'icona, nei
 * 4 stati (normale, sopra, premuto, spento), e sotto i pulsanti solo icona XS (24) di ogni
 * tipo. Gli stati si forzano a mano come in pulsanti.c.
 * Uso: TEMA=scuro anteprima.sh pulsanti_colori 2 */
#include "toolkit.h"
#include "piano.h"

#define COLORATI 4
#define STATI 4

static struct finestra_tk finestra;
static struct piano piano;
static struct contenitore colonna, righe[COLORATI + 1];
static struct pulsante colorati[COLORATI * STATI];
static struct pulsante piccoli[7];

static const int tipo[COLORATI] = { PERICOLO, SUCCESSO, AVVISO, INFORMAZIONE };
static const char *nome_stato[STATI] = { "Normale", "Sopra", "Premuto", "Spento" };

static void forza_stato(struct pulsante *b, int stato) {
    if (stato == 1) b->base.sopra = 1;
    if (stato == 2) { b->base.sopra = 1; b->premuto = 1; b->dentro = 1; }
    if (stato == 3) b->base.disabilitato = 1;
}

int main(void) {
    colonna_prepara(&colonna, 12);
    int n = 0;
    for (int t = 0; t < COLORATI; t++) {
        riga_prepara(&righe[t], 12);
        for (int s = 0; s < STATI; s++) {
            struct pulsante *b = &colorati[n++];
            pulsante_prepara(b, nome_stato[s], ICONA_CHECK, 0, 0);
            pulsante_stile(b, tipo[t], MISURA_M);
            forza_stato(b, s);
            widget_aggiungi(&righe[t].base, &b->base);
        }
        widget_aggiungi(&colonna.base, &righe[t].base);
    }
    /* XS solo icona: un pulsante per tipo (Primario, Secondario, Ghost e i quattro colorati) */
    riga_prepara(&righe[COLORATI], 12);
    for (int t = 0; t < 7; t++) {
        pulsante_prepara(&piccoli[t], "", ICONA_X, 0, 0);
        pulsante_stile(&piccoli[t], t, MISURA_XS);
        widget_aggiungi(&righe[COLORATI].base, &piccoli[t].base);
    }
    widget_aggiungi(&colonna.base, &righe[COLORATI].base);
    piano_prepara(&piano, &colonna.base);
    finestra_tk_prepara(&finestra, "Pulsanti colorati", &piano.base);
    return toolkit_esegui(&finestra);
}
