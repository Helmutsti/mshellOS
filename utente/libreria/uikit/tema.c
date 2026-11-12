/* UIKit: il tema in uso, letto dalle preferenze della persona (le funzioni sono
 * dichiarate in tema.h).
 *
 * IL TEMA (Chiaro o Scuro) si sceglie una volta sola, quando il programma parte: la persona
 * lo scrive nel file /data/users/1000/preferences/tema ("chiaro" o "scuro"). Se il file
 * manca o non si legge (primo avvio, /data non ancora montato, permesso negato) resta il
 * chiaro, quello di partenza di tema_corrente (tema.h).
 *
 * Il percorso e' quello della persona 1000 (UID_APPLICAZIONI) scritto per intero, non
 * "/home/..." : i servizi (compositore, scrivania) girano con un altro uid e per loro /home
 * sarebbe un'altra cartella, ma le preferenze sono sempre quelle della persona. Il sistema
 * (uid 0) puo' leggerle comunque.
 *
 * Perche' un "constructor": ogni programma lega tutti i file .c della libreria (anche
 * quelli di GKit e UIKit), e un constructor parte da solo prima di main. Cosi' nessun
 * programma deve ricordarsi di chiamare tema_leggi(): tutti, applicazioni e servizi, hanno
 * il tema giusto fin dal primo disegno. (Si puo' richiamare a mano per rileggere il file.) */
#include "libreria.h"
#include "tema.h"

#define FILE_TEMA "/data/users/1000/preferences/tema"

void tema_leggi(void) {
    char testo[16] = { 0 };
    int d = apri(FILE_TEMA, APRI_LEGGERE);
    if (d < 0) return;                       /* manca o illeggibile: resta com'e' */
    int n = leggi(d, testo, sizeof testo - 1);
    chiudi(d);
    if (n > 0) tema_scegli(testo);
}

__attribute__((constructor)) static void tema_all_avvio(void) { tema_leggi(); }
