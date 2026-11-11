/* Il protocollo del launcher: chiedere di aprire un'applicazione.
 *
 * Il launcher e' il servizio del sistema che fa partire le applicazioni (come
 * LaunchServices sul Mac, o systemd per GNOME). Non ha finestre: chi vuole aprire
 * un'applicazione (la scrivania, domani un'altra applicazione) si collega al nome
 * "launcher" e gli manda APRI_APPLICAZIONE con il nome della sua cartella (in
 * /system/apps o /data/apps). Il launcher controlla, legge il manifest, la fa partire
 * come applicazione (uid 1000) e risponde.
 *
 * L'ARGOMENTO. Con la richiesta si puo' mandare anche un testo per l'applicazione (vuoto:
 * niente): il launcher glielo passa all'avvio, e lei lo trova in main (argomenti[1], vedi
 * avvia_come_con in libreria.h). Per ora lo usa Files: e' la cartella da aprire. Cosi'
 * "aprire una cartella" (anche dalla scrivania) vuol dire "far partire Files su quella
 * cartella", come fa il Finder del Mac. Ogni richiesta con un argomento fa partire
 * l'applicazione un'altra volta, in una finestra sua. */
#ifndef LAUNCHER_H
#define LAUNCHER_H

#include "libreria.h"

#define NOME_LAUNCHER "launcher"

#define APRI_APPLICAZIONE 1

struct richiesta_launcher {
    unsigned int tipo;
    char applicazione[MAX_NOME_FILE];   /* il nome della sua cartella, per esempio "contatore" */
    char argomento[MAX_PERCORSO];       /* per l'applicazione (per Files: la cartella); "" niente */
};

struct risposta_launcher {
    int esito;      /* 0 fatto, -1 non c'e' o non si puo' avviare */
    int numero;     /* il numero del compito appena partito */
};

/* Chiede al launcher di aprire un'applicazione, con un argomento per lei (0 o "":
 * nessuno). Il numero del compito, o -1. */
static inline int apri_applicazione_con(const char *cartella, const char *argomento) {
    int d = collega(NOME_LAUNCHER);
    if (d < 0) return -1;
    struct richiesta_launcher r = { .tipo = APRI_APPLICAZIONE };
    for (int i = 0; i < MAX_NOME_FILE - 1 && cartella[i]; i++) r.applicazione[i] = cartella[i];
    for (int i = 0; argomento && i < MAX_PERCORSO - 1 && argomento[i]; i++) r.argomento[i] = argomento[i];
    struct risposta_launcher risposta = { -1, -1 };
    if (manda(d, &r, sizeof r) < 0 || ricevi(d, &risposta, sizeof risposta) != sizeof risposta)
        risposta.esito = -1;
    chiudi(d);
    return risposta.esito < 0 ? -1 : risposta.numero;
}

/* Chiede al launcher di aprire un'applicazione: il numero del compito, o -1. */
static inline int apri_applicazione(const char *cartella) { return apri_applicazione_con(cartella, 0); }

#endif
