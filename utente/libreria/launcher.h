/* Il protocollo del launcher: chiedere di aprire un'applicazione.
 *
 * Il launcher e' il servizio del sistema che fa partire le applicazioni (come
 * LaunchServices sul Mac, o systemd per GNOME). Non ha finestre: chi vuole aprire
 * un'applicazione (la scrivania, domani un'altra applicazione) si collega al nome
 * "launcher" e gli manda APRI_APPLICAZIONE con il nome della sua cartella (in
 * /system/apps o /data/apps). Il launcher controlla, legge il manifest, la fa partire
 * come applicazione (uid 1000) e risponde. */
#ifndef LAUNCHER_H
#define LAUNCHER_H

#include "libreria.h"

#define NOME_LAUNCHER "launcher"

#define APRI_APPLICAZIONE 1

struct richiesta_launcher {
    unsigned int tipo;
    char applicazione[MAX_NOME_FILE];   /* il nome della sua cartella, per esempio "contatore" */
};

struct risposta_launcher {
    int esito;      /* 0 fatto, -1 non c'e' o non si puo' avviare */
    int numero;     /* il numero del compito appena partito */
};

/* Chiede al launcher di aprire un'applicazione: il numero del compito, o -1. */
static inline int apri_applicazione(const char *cartella) {
    int d = collega(NOME_LAUNCHER);
    if (d < 0) return -1;
    struct richiesta_launcher r = { .tipo = APRI_APPLICAZIONE };
    for (int i = 0; i < MAX_NOME_FILE - 1 && cartella[i]; i++) r.applicazione[i] = cartella[i];
    struct risposta_launcher risposta = { -1, -1 };
    if (manda(d, &r, sizeof r) < 0 || ricevi(d, &risposta, sizeof risposta) != sizeof risposta)
        risposta.esito = -1;
    chiudi(d);
    return risposta.esito < 0 ? -1 : risposta.numero;
}

#endif
