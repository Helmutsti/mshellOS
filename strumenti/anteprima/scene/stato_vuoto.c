/* Lo stato vuoto del toolkit, nei tre casi di Files: cartella vuota (senza icona), cartella
 * che non si legge, permesso negato. Uso: TEMA=scuro anteprima.sh stato_vuoto */
#include "toolkit.h"

static struct finestra_tk finestra;
static struct contenitore riga;
static struct stato_vuoto vuota, errore, permesso;

int main(void) {
    riga_prepara(&riga, SPAZIO_8);
    stato_vuoto_prepara(&vuota, NESSUNA_ICONA, "Questa cartella e' vuota",
                        "Usa il tasto destro per creare una nuova cartella.");
    stato_vuoto_prepara(&errore, ICONA_TRIANGLE_ALERT, "Impossibile leggere la cartella",
                        "La chiavetta non ha risposto. Scollegala, ricollegala e riprova.");
    stato_vuoto_prepara(&permesso, ICONA_LOCK, "Non hai il permesso di aprire questa cartella",
                        "E' lo spazio privato dell'app Contatore: solo l'app puo' leggerlo.");
    widget_aggiungi(&riga.base, &vuota.base);
    widget_aggiungi(&riga.base, &errore.base);
    widget_aggiungi(&riga.base, &permesso.base);
    finestra_tk_prepara(&finestra, "Stato vuoto", &riga.base);
    finestra_tk_dimensione(&finestra, 1440, 320);
    return toolkit_esegui(&finestra);
}
