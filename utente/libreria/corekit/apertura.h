/* CoreKit, pubblico: aprire un'altra applicazione, o un file. Un'app non fa partire da
 * sola gli altri programmi: lo chiede al launcher, il servizio del sistema che fa partire
 * le applicazioni (come LaunchServices sul Mac: "open" e NSWorkspace). Qui ci sono solo
 * le richieste che puo' fare qualsiasi app; dietro c'e' il protocollo del launcher, che e'
 * del sistema (sistema/launcher.h) e puo' cambiare senza che un'app se ne accorga.
 *
 * Le funzioni stanno in interno/apertura.c. */
#ifndef APERTURA_H
#define APERTURA_H

#include "corekit.h"

/* Gli esiti di apri_file. */
#define APERTURA_FATTO            0
#define APERTURA_ERRORE          -1   /* il launcher non risponde, o non si e' riusciti */
#define APERTURA_NON_INSTALLATA  -2   /* l'app chiesta non e' installata */
#define APERTURA_SERVE_STORE     -3   /* un pacchetto .app: va installato, e lo Store non c'e' */
#define APERTURA_NON_SUPPORTATO  -4   /* un file che il sistema non sa ancora aprire */

/* Apre l'applicazione 'id' (la sua cartella in /system/apps o /data/apps, per esempio
 * "files"). Si aprono solo le app installate. Il numero del compito, o -1. */
int apri_applicazione(const char *id);

/* Come apri_applicazione, con un ARGOMENTO per lei (0 o "": nessuno), che trova in main
 * come argomenti[1]: per Files e' la cartella da aprire. Il numero del compito, o -1. */
int apri_applicazione_con(const char *id, const char *argomento);

/* Apre il file 'percorso' con l'app che sa aprirlo. Per ora l'unico tipo di file che si
 * apre cosi' e' il pacchetto .app, che apre lo Store (e' lo Store a mostrare cosa c'e'
 * dentro e a chiedere se installarla: un pacchetto non parte mai da solo).
 * APERTURA_FATTO, o un altro APERTURA_...; se 'perche' non e' 0, ci va la frase per la
 * persona (UTF-8, al massimo 'byte' byte compreso lo 0; "" se non c'e'). */
int apri_file(const char *percorso, char *perche, unsigned int byte);

#endif
