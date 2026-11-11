/* Una fila di pulsanti: colori, dimensioni, icona, disabilitato; e caselle e scelte. */
#include "toolkit.h"

static struct finestra_tk finestra;
static struct contenitore colonna, fila1, fila2, fila3;
static struct pulsante p[8];
static struct casella c1, c2, c3, s1, s2;
static int gruppo;

int main(void) {
    colonna_prepara(&colonna, SPAZIO_TRA);
    riga_prepara(&fila1, 8); riga_prepara(&fila2, 8); riga_prepara(&fila3, 16);
    pulsante_prepara(&p[0], "Neutro", NESSUNA_ICONA, 0, 0);
    pulsante_prepara(&p[1], "Salva", ICONA_SAVE, 0, 0);      pulsante_stile(&p[1], BLU, MEDIO);
    pulsante_prepara(&p[2], "Ok", NESSUNA_ICONA, 0, 0);      pulsante_stile(&p[2], VERDE, MEDIO);
    pulsante_prepara(&p[3], "Elimina", NESSUNA_ICONA, 0, 0); pulsante_stile(&p[3], ROSSO, MEDIO);
    pulsante_prepara(&p[4], "Avviso", NESSUNA_ICONA, 0, 0);  pulsante_stile(&p[4], ARANCIO, MEDIO);
    pulsante_prepara(&p[5], "Piccolo", NESSUNA_ICONA, 0, 0); pulsante_stile(&p[5], NEUTRO, PICCOLO);
    pulsante_prepara(&p[6], "Grande", ICONA_SAVE, 0, 0);     pulsante_stile(&p[6], BLU, GRANDE);
    pulsante_prepara(&p[7], "Spento", NESSUNA_ICONA, 0, 0);  p[7].base.disabilitato = 1;
    for (int i = 0; i < 5; i++) widget_aggiungi(&fila1.base, &p[i].base);
    for (int i = 5; i < 8; i++) widget_aggiungi(&fila2.base, &p[i].base);
    casella_prepara(&c1, "Accesa", ACCESO, 0, 0);
    casella_prepara(&c2, "Spenta", SPENTO, 0, 0);
    casella_prepara(&c3, "Mista", MISTO, 0, 0);
    scelta_prepara(&s1, "Scelta uno", &gruppo, 1, 0, 0);
    scelta_prepara(&s2, "Scelta due", &gruppo, 0, 0, 0);
    widget_aggiungi(&fila3.base, &c1.base); widget_aggiungi(&fila3.base, &c2.base);
    widget_aggiungi(&fila3.base, &c3.base); widget_aggiungi(&fila3.base, &s1.base);
    widget_aggiungi(&fila3.base, &s2.base);
    widget_aggiungi(&colonna.base, &fila1.base);
    widget_aggiungi(&colonna.base, &fila2.base);
    widget_aggiungi(&colonna.base, &fila3.base);
    finestra_tk_prepara(&finestra, "Pulsanti", &colonna.base);
    return toolkit_esegui(&finestra);
}
