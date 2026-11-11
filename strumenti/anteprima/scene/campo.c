/* Un campo di testo con il fuoco e il cursore, a meta' scrittura. Ritocca i numeri in
 * toolkit.h (cursore: campo_disegna) e rilancia: anteprima.sh campo 3 */
#include "toolkit.h"

static struct finestra_tk finestra;
static struct contenitore colonna;
static struct campo vuoto, scritto, lungo, sbagliato;

#define HA_RITOCCO
static void ritocco(struct finestra_tk *f) {
    (void)f;
    scritto.base.fuoco = 1;      /* il cursore si vede solo con il fuoco */
    lungo.base.fuoco = 1;
}

int main(void) {
    colonna_prepara(&colonna, SPAZIO_TRA);
    campo_prepara(&vuoto, "", "Nome utente", ICONA_USER, 0, 0);
    campo_prepara(&scritto, "mario.ro", "Nome utente", ICONA_USER, 0, 0);
    campo_prepara(&lungo, "un testo molto piu' lungo del campo che scorre", "", NESSUNA_ICONA, 0, 0);
    campo_prepara(&sbagliato, "errore", "", NESSUNA_ICONA, 0, 0);
    campo_errore(&sbagliato, 1);
    widget_aggiungi(&colonna.base, &vuoto.base);
    widget_aggiungi(&colonna.base, &scritto.base);
    widget_aggiungi(&colonna.base, &lungo.base);
    widget_aggiungi(&colonna.base, &sbagliato.base);
    finestra_tk_prepara(&finestra, "Campo", &colonna.base);
    return toolkit_esegui(&finestra);
}
