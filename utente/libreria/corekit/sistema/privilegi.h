/* CoreKit, del sistema: le funzioni che riescono solo al SISTEMA (uid 0). Far partire un
 * programma con un altro uid, e dare un file a un altro proprietario: per Unix sono cose
 * che solo il sistema puo' fare (lo controlla il kernel), e un'app normale non ha motivo
 * di chiederle. Le usa il launcher (che fa partire le app come la persona, uid 1000, e la
 * shell come sistema). Le funzioni stanno con le altre, in interno/corekit.c e
 * interno/file.c: qui sono solo dichiarate, fuori dall'header pubblico (corekit.h).
 *
 * Gli uid (UID_SISTEMA, UID_APPLICAZIONI) e i permessi dei file sono in chiamate.h. */
#ifndef PRIVILEGI_H
#define PRIVILEGI_H

#include "../corekit.h"

/* --- far partire un programma con un altro uid --- */

/* Come avvia (corekit.h), ma il programma nuovo ha quell'uid. Il suo numero, o -1. */
int avvia_come(const char *percorso, unsigned int uid);

/* Come avvia_come, con un ARGOMENTO per il programma nuovo (un testo, per esempio la
 * cartella da aprire): lui lo trova in main, come secondo argomento,
 *     int main(int quanti, char **argomenti)   ->   argomenti[1] (se quanti > 1)
 * Un argomento vuoto (o 0) e' come nessun argomento. */
int avvia_come_con(const char *percorso, unsigned int uid, const char *argomento);

/* Come avvia_con (corekit.h), e il programma nuovo ha anche quell'uid. Il suo numero, o
 * -1. */
int avvia_come_con_descrittori(const char *percorso, unsigned int uid, const int *descrittori, int quanti);

/* --- i file --- */

/* Da' il file (o la cartella) 'percorso' al proprietario 'proprietario' (un uid). Se e'
 * gia' suo non fa niente (e allora riesce a chiunque). I permessi restano quelli di
 * prima: per cambiarli c'e' cambia_permessi (corekit.h). 0, o un ERRORE_...
 * (ERRORE_PERMESSO se chi chiede non e' il sistema). */
int cambia_proprietario(const char *percorso, unsigned int proprietario);

#endif
