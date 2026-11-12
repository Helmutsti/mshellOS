/* L'avviso del toolkit, aperto sopra una finestra semplice. Variabile BLOCCANTE=0 per
 * quello non bloccante (senza velo, la finestra resta attiva); senza, bloccante.
 * Uso: TEMA=scuro BLOCCANTE=0 anteprima.sh avviso 2 */
#include <stdlib.h>
#include "toolkit.h"

static struct finestra_tk finestra;
static struct contenitore colonna, riga;
static struct etichetta righe[6];
static struct spazio vuoto;
static struct pulsante pulsante_riavvia, pulsante_termina;
static struct avviso avviso;

static const char *processi[6] = {
    "1   init            sistema         dorme",
    "2   compositore     sistema         in attesa",
    "3   launcher        sistema         in attesa",
    "4   scrivania       sistema         in attesa",
    "5   attivita        applicazioni    in esecuzione",
    "6   contatore       applicazioni    in attesa",
};

int main(void) {
    const char *b = getenv("BLOCCANTE");
    int bloccante = !(b && b[0] == '0');
    colonna_prepara(&colonna, SPAZIO_8);
    for (int i = 0; i < 6; i++) {
        etichetta_prepara(&righe[i], processi[i], FONT_CORPO);
        widget_aggiungi(&colonna.base, &righe[i].base);
    }
    riga_prepara(&riga, SPAZIO_8);
    spazio_prepara(&vuoto);
    pulsante_prepara(&pulsante_riavvia, "Riavvia", NESSUNA_ICONA, 0, 0);
    pulsante_stile(&pulsante_riavvia, SECONDARIO, MISURA_S);
    pulsante_prepara(&pulsante_termina, "Termina processo", ICONA_X, 0, 0);
    pulsante_stile(&pulsante_termina, PERICOLO, MISURA_S);
    widget_aggiungi(&riga.base, &vuoto.base);
    widget_aggiungi(&riga.base, &pulsante_riavvia.base);
    widget_aggiungi(&riga.base, &pulsante_termina.base);
    widget_aggiungi(&colonna.base, &riga.base);
    finestra_tk_prepara(&finestra, bloccante ? "Attivita" : "Files", &colonna.base);
    finestra_tk_dimensione(&finestra, 600, 420);
    const char *tre = getenv("TRE");
    if (tre && tre[0] == '1') {         /* TRE=1: l'avviso con tre scelte dello Store */
        avviso_prepara(&avviso, "Togliere \"Contatore\"?",
                       "Puoi togliere solo l'app e tenere i suoi dati, o togliere tutto.",
                       "Togli app e dati", PERICOLO, 1, 0, 0);
        avviso_alternativa(&avviso, "Togli solo l'app", SECONDARIO);
    } else if (bloccante)
        avviso_prepara(&avviso, "Riavviare \"contatore\"?",
                       "Il processo viene chiuso e aperto di nuovo. I dati non salvati andranno persi.",
                       "Riavvia", PERICOLO, 1, 0, 0);
    else {
        avviso_prepara(&avviso, "Copia completata", "12 file copiati in Documenti.",
                       "OK", PRIMARIO, 0, 0, 0);
        avviso_annulla(&avviso, "Apri cartella");
    }
    finestra_tk_avviso(&finestra, &avviso);
    avviso_apri(&avviso);
    return toolkit_esegui(&finestra);
}
