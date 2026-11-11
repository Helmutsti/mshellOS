/* riempitivo: un programma che non fa niente, e c'e' SOLO per le prove automatiche.
 *
 * Non apre finestre e non usa la CPU: dorme per sempre. Serve a riempire l'elenco dei
 * processi, per esempio perche' la tabella di Attivita' abbia piu' righe di quante ne
 * stanno e si possa provare a scorrerla (test/prove/scorrimento.py).
 *
 * Nel disco normale non c'e'. build.sh prepara anche un secondo disco, disco_prove.img,
 * uguale al normale ma con qualche copia di questo programma in /data/tests/
 * (riempitivo-1, riempitivo-2...: Linux chiama ogni processo come il suo file, quindi in
 * Attivita' ognuna si vede con il suo nome). Init, all'accensione, fa partire tutto
 * quello che trova in quella cartella, se c'e'. Le prove che vogliono i riempitivi
 * accendono il sistema con quel disco (DISCO in test/prove/). */
#include "libreria.h"

int main(void) {
    for (;;) dormi(60 * 60 * 1000);   /* un'ora alla volta, per sempre */
}
