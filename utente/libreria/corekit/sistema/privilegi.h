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
#include "recinto.h"

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

/* Tutto insieme: l'uid, l'argomento (come avvia_come_con) e i descrittori da prestare (come
 * avvia_con: il programma nuovo li trova a 0, 1, 2...). Lo usa il launcher per le app, che
 * ricevono cosi' i loro canali gia' aperti (APPLICAZIONI.md, "Come funzionano le chiavi"), e le
 * variabili d'ambiente 'ambiente' ("NOME=valore", l'elenco finisce con 0; 0: nessuna): l'elenco
 * delle porte (messaggi.h, VARIABILE_PORTE), cosi' l'app sa quali canali sono porte senza
 * doverlo indovinare, e WAYLAND_SOCKET, la sua connessione Wayland. */
int avvia_app(const char *percorso, unsigned int uid, const char *argomento, const int *descrittori, int quanti,
              const char *const *ambiente);

/* Come avvia_app, ma l'app parte nel suo RECINTO (recinto.h): mondi nuovi del kernel, una radice
 * con dentro solo le cartelle dell'elenco, poi l'uid, NO_NEW_PRIVS e il filtro delle chiamate.
 * Il programma 'percorso' deve stare dentro una cartella dell'elenco con il modo
 * AGGANCIO_PROGRAMMA (il pacchetto dell'app). Il numero dell'app (quello vero, visto da fuori),
 * o -1 se non e' partita. Lo usa il launcher per tutte le app tranne il Terminale. */
int avvia_app_nel_recinto(const char *percorso, unsigned int uid, const char *argomento, const int *descrittori,
                          int quanti, const char *const *ambiente, const struct recinto *recinto);

/* Come avvia_app (senza recinto), ma con l'elenco intero degli argomenti 'argomenti' (argv,
 * finito da 0; il primo e' il nome del programma). Lo usa il launcher per le app Linux, che
 * partono con il caricatore di musl del loro pacchetto: percorso = il caricatore, argomenti =
 * { caricatore, "--library-path", "...", programma, 0 }. */
int avvia_app_con_argomenti(const char *percorso, unsigned int uid, const char *const *argomenti,
                            const int *descrittori, int quanti, const char *const *ambiente);

/* /dev/null, aperto in lettura e scrittura: da prestare come ingresso, uscita ed errori a un
 * programma che non ne ha (apri, in corekit.h, i dispositivi non li apre). Il descrittore, o -1. */
int apri_nulla(void);

/* --- i file --- */

/* Da' il file (o la cartella) 'percorso' al proprietario 'proprietario' (un uid). Se e'
 * gia' suo non fa niente (e allora riesce a chiunque). I permessi restano quelli di
 * prima: per cambiarli c'e' cambia_permessi (corekit.h). 0, o un ERRORE_...
 * (ERRORE_PERMESSO se chi chiede non e' il sistema). */
int cambia_proprietario(const char *percorso, unsigned int proprietario);

#endif
