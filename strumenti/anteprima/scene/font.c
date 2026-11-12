/* I font IBM Plex Sans a tutte le misure, con una frase di prova. Rilancia:
 * anteprima.sh font 2 (il 2 ingrandisce, per vedere i pixel) */
#include "toolkit.h"

#define FRASE "Impostazioni di sistema - bilancio-novembre.txt 0123456789 \x85\x8a\x82\x8d\x95\x97"

static struct finestra_tk finestra;
static struct contenitore colonna;
static struct etichetta righe[20];
static int quante;

static void riga(const struct font *f, const char *testo) {
    etichetta_prepara(&righe[quante], testo, f);
    widget_aggiungi(&colonna.base, &righe[quante].base);
    quante++;
}

int main(void) {
    colonna_prepara(&colonna, 4);
    riga(&plex_12, "R12 " FRASE);
    riga(&plex_13, "R13 " FRASE);
    riga(&plex_14, "R14 " FRASE);
    riga(&plex_15, "R15 " FRASE);
    riga(&plex_16, "R16 " FRASE);
    riga(&plex_medio_12, "M12 " FRASE);
    riga(&plex_medio_13, "M13 " FRASE);
    riga(&plex_medio_14, "M14 " FRASE);
    riga(&plex_medio_16, "M16 " FRASE);
    riga(&plex_semigrassetto_12, "S12 " FRASE);
    riga(&plex_semigrassetto_13, "S13 " FRASE);
    riga(&plex_semigrassetto_14, "S14 " FRASE);
    riga(&plex_semigrassetto_15, "S15 " FRASE);
    riga(&plex_semigrassetto_24, "S24 Impostazioni 0123");
    riga(&plex_medio_24, "M24 Impostazioni 0123");
    riga(&plex_14, "Accenti R14: \x85\x8a\x82\x8d\x95\x97 \xfa 0123");
    riga(&plex_semigrassetto_14, "Accenti S14: \x85\x8a\x82\x8d\x95\x97 \xfa 0123");
    riga(&inter_14, "Inter 14 per confronto: " FRASE);
    finestra_tk_prepara(&finestra, "Font", &colonna.base);
    return toolkit_esegui(&finestra);
}
