/* Campi di testo (normale, attivo, errore, disabilitato) nelle tre misure, la ricerca a
 * pillola, e caselle e scelte (a riposo, accese, miste, disabilitate).
 * Il fuoco, l'errore e il disabilitato si forzano a mano: l'anteprima non ha eventi.
 * Uso: TEMA=scuro anteprima.sh campo 2 */
#include "uikit/uikit.h"
#include "piano.h"

#define STATI 4   /* righe: normale, attivo, errore, disabilitato; colonne: S, M, L */

static struct finestra_tk finestra;
static struct piano piano;
static struct contenitore colonna, righe[STATI], fila_ricerche, fila_caselle, fila_scelte;
static struct campo campi[STATI][3], ricerche[3];
static struct casella caselle[6], scelte[6];
static int gruppo_a, gruppo_b;

#define HA_RITOCCO
static void ritocco(struct finestra_tk *f) {
    (void)f;
    for (int m = 0; m < 3; m++) campi[1][m].base.fuoco = 1;   /* "attivo" */
    ricerche[1].base.fuoco = 1;
}

int main(void) {
    colonna_prepara(&colonna, 12);
    for (int s = 0; s < STATI; s++) {
        riga_prepara(&righe[s], 12);
        for (int m = 0; m < 3; m++) {
            struct campo *c = &campi[s][m];
            if (s == 0) campo_prepara(c, "mario.rossi", "Nome utente", ICONA_USER, 0, 0);
            if (s == 1) campo_prepara(c, "mario.ro", "Nome utente", ICONA_USER, 0, 0);
            if (s == 2) { campo_prepara(c, "mario!", "Nome utente", ICONA_USER, 0, 0); campo_errore(c, 1); }
            if (s == 3) { campo_prepara(c, "", "Disabilitato", ICONA_USER, 0, 0); c->base.disabilitato = 1; }
            campo_stile(c, m);
            c->base.largo = 0;                /* in una riga non si allargano: misura propria */
            widget_aggiungi(&righe[s].base, &c->base);
        }
        widget_aggiungi(&colonna.base, &righe[s].base);
    }
    riga_prepara(&fila_ricerche, 12);
    for (int m = 0; m < 3; m++) {
        ricerca_prepara(&ricerche[m], m == 2 ? "Cerca (spenta)" : "Cerca", 0, 0);
        if (m == 1) campo_prepara(&ricerche[m], "docum", "", ICONA_SEARCH, 0, 0), ricerche[m].piccolo = 1, ricerche[m].base.largo = 0, ricerche[m].misura = MISURA_S;
        if (m == 2) ricerche[m].base.disabilitato = 1;
        widget_aggiungi(&fila_ricerche.base, &ricerche[m].base);
    }
    widget_aggiungi(&colonna.base, &fila_ricerche.base);

    riga_prepara(&fila_caselle, 16);
    riga_prepara(&fila_scelte, 16);
    const char *nomi[6] = { "Spenta", "Accesa", "Mista", "Sopra", "Spenta (off)", "Accesa (off)" };
    int valori[6] = { SPENTO, ACCESO, MISTO, ACCESO, SPENTO, ACCESO };
    for (int i = 0; i < 6; i++) {
        casella_prepara(&caselle[i], nomi[i], valori[i], 0, 0);
        if (i == 3) caselle[i].base.sopra = 1;
        if (i >= 4) caselle[i].base.disabilitato = 1;
        widget_aggiungi(&fila_caselle.base, &caselle[i].base);
        scelta_prepara(&scelte[i], nomi[i], i >= 4 ? &gruppo_b : &gruppo_a, valori[i] == ACCESO, 0, 0);
        if (i == 2) scelte[i].valore = SPENTO;
        if (i == 3) scelte[i].base.sopra = 1;
        if (i >= 4) scelte[i].base.disabilitato = 1;
        widget_aggiungi(&fila_scelte.base, &scelte[i].base);
    }
    widget_aggiungi(&colonna.base, &fila_caselle.base);
    widget_aggiungi(&colonna.base, &fila_scelte.base);
    piano_prepara(&piano, &colonna.base);
    finestra_tk_prepara(&finestra, "Campo", &piano.base);
    return uikit_esegui(&finestra);
}
