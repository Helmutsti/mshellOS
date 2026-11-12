/* I pulsanti del nuovo design: 3 tipi (Primario, Secondario, Ghost) x 3 misure (S, M, L) x
 * 4 stati (normale, sopra, premuto, disabilitato), e sotto i pulsanti solo icona (tondi).
 * L'anteprima disegna un fotogramma senza eventi, quindi gli stati si FORZANO a mano:
 * 'sopra' e' lo stesso campo che segna il toolkit quando il puntatore e' sopra, 'premuto'
 * e 'dentro' quelli del clic, 'disabilitato' quello di widget_disabilita.
 * Uso: TEMA=scuro anteprima.sh pulsanti 2 */
#include "toolkit.h"
#include "piano.h"

#define TIPI 3
#define MISURE 3
#define STATI 4

static struct finestra_tk finestra;
static struct piano piano;
static struct contenitore colonna, righe[TIPI * MISURE + 1];
static struct pulsante grande[TIPI * MISURE * STATI];
static struct pulsante tondi[TIPI * MISURE];

static const char *nome_tipo[TIPI] = { "Primario", "Secondario", "Ghost" };
static const char *nome_stato[STATI] = { "Normale", "Sopra", "Premuto", "Spento" };

static void forza_stato(struct pulsante *b, int stato) {
    if (stato == 1) b->base.sopra = 1;
    if (stato == 2) { b->base.sopra = 1; b->premuto = 1; b->dentro = 1; }
    if (stato == 3) b->base.disabilitato = 1;
}

int main(void) {
    colonna_prepara(&colonna, 12);
    int n = 0;
    for (int t = 0; t < TIPI; t++)
        for (int m = 0; m < MISURE; m++) {
            struct contenitore *riga = &righe[t * MISURE + m];
            riga_prepara(riga, 12);
            for (int s = 0; s < STATI; s++) {
                struct pulsante *b = &grande[n++];
                /* l'icona solo nelle misure M: si vedono entrambe le versioni */
                pulsante_prepara(b, nome_stato[s], m == MISURA_M ? ICONA_SAVE : NESSUNA_ICONA, 0, 0);
                pulsante_stile(b, t, m);
                forza_stato(b, s);
                widget_aggiungi(&riga->base, &b->base);
            }
            widget_aggiungi(&colonna.base, &riga->base);
        }
    /* i solo icona: tondi, tipo x misura, poi spento */
    struct contenitore *ultima = &righe[TIPI * MISURE];
    riga_prepara(ultima, 12);
    n = 0;
    for (int t = 0; t < TIPI; t++)
        for (int m = 0; m < MISURE; m++) {
            struct pulsante *b = &tondi[n++];
            pulsante_prepara(b, "", ICONA_SEARCH, 0, 0);
            pulsante_stile(b, t, m);
            if (t == 2 && m == 2) b->base.disabilitato = 1;
            widget_aggiungi(&ultima->base, &b->base);
        }
    widget_aggiungi(&colonna.base, &ultima->base);
    (void)nome_tipo;
    piano_prepara(&piano, &colonna.base);
    finestra_tk_prepara(&finestra, "Pulsanti", &piano.base);
    return toolkit_esegui(&finestra);
}
