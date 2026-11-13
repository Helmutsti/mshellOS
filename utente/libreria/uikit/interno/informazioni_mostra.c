/* UIKit, i componenti piccoli: apre la mini finestra delle informazioni (informazioni.h). E'
 * separata da informazioni.c perche' apre una finestra con il ciclo degli eventi (ciclo.c,
 * finestra_tk_apri_altra), cioe' parla con il compositore: l'anteprima
 * (strumenti/anteprima/anteprima.sh), che non ha ne' compositore ne' ciclo.c, la lascia fuori
 * come ciclo.c e compila tutto il resto. */
#include "uikit/uikit.h"

int informazioni_mostra(struct finestra_tk *principale, struct finestra_tk *mini,
                        struct informazioni_tk *in, const char *titolo) {
    (void)in;
    copia_testo(mini->titolo, titolo, MAX_TITOLO);
    int n = 0;
    while (titolo[n]) n++;
    if (n > MAX_TITOLO - 1) {
        mini->titolo[MAX_TITOLO - 4] = mini->titolo[MAX_TITOLO - 3] = mini->titolo[MAX_TITOLO - 2] = '.';
        mini->titolo[MAX_TITOLO - 1] = 0;
    }
    return finestra_tk_apri_altra(principale, mini);
}
