/* CoreKit, del sistema: il lato del SERVIZIO nei messaggi (messaggi.h). Chi chiede usa
 * solo messaggi.h (la busta, chiedi); un servizio, quando riceve una richiesta, vuole
 * anche sapere chi c'e' dall'altra parte e se ha il permesso: queste funzioni sono qui,
 * perche' le usano solo i servizi del sistema. Il codice sta in interno/messaggi.c, con il
 * resto dei messaggi; il perche' e il come, in testa a quel file. */
#ifndef SERVIZI_H
#define SERVIZI_H

#include "../messaggi.h"

#define MAX_PERMESSI_APP  8    /* i permessi concessi a un'app: quanti al massimo (come MAX_VOCI) */
#define MAX_NOME_PERMESSO 32   /* byte del nome di un permesso, compreso lo 0 */

/* Chi c'e' all'altro capo di un collegamento. Per un collegamento per nome lo dice il kernel
 * (chi_chiede); per uno arrivato dalla porta di un'app lo ha detto il launcher (attach), con i
 * permessi concessi all'app. Non si puo' fingere in nessuno dei due casi. */
struct chi_chiede {
    int uid;                                  /* 0: il sistema; 1000...: una persona */
    char programma[MAX_PERCORSO_PROGRAMMA];   /* es. /system/apps/files/code; "" se non si sa */
    int app;                                  /* 1: un'app, dalla sua porta: valgono 'permessi' */
    int quanti_permessi;
    char permessi[MAX_PERMESSI_APP][MAX_NOME_PERMESSO];
};

/* Riempie *c per il collegamento 'd' (per nome: non e' un'app). 0, o -1 se il kernel non lo sa
 * (non e' un socket). */
int chi_chiede(int d, struct chi_chiede *c);

/* I PERMESSI (APPLICAZIONI.md, sezione 9). 1 se 'nome' e' un permesso di sistema (poteri sul
 * sistema intero: processes.*, network.settings, network.firewall, settings.system,
 * system.power, apps.manage, apps.open, system.shell), 0 se no. */
int permesso_di_sistema(const char *nome);

/* 1 se 'nome' e' un permesso che il sistema conosce (normale o di sistema), 0 se no. */
int permesso_conosciuto(const char *nome);

/* Chi chiede ha il permesso 'nome' (es. "network.settings")? 1 si', 0 no. La regola (decisa
 * con la persona il 16 novembre 2026):
 *   - il sistema (uid 0): tutti;
 *   - un'app, dalla sua porta: solo quelli che il launcher ha detto concessi (oggi: quelli
 *     dichiarati nel manifesto, e quelli di sistema solo alle app del sistema);
 *   - un altro programma della persona (un comando, che non parte dal launcher): i permessi
 *     normali, nessuno di sistema.
 * Ogni rifiuto si scrive nel registro: "permesso <nome> a <programma> (uid <uid>): negato". */
int ha_permesso(const struct chi_chiede *c, const char *nome);

/* LE PORTE DELLE APP, il lato del sistema (messaggi.h, "le porte delle app"). Il launcher, quando
 * fa partire un'app, da' a ogni servizio il capo della porta di quell'app: si collega al servizio
 * per nome e manda request=attach, con program (il programma dell'app), permission (ogni permesso
 * concesso, la chiave si ripete) e il capo allegato: diventano il chi_chiede dei collegamenti
 * dalla porta (uid UID_APPLICAZIONI, app = 1). Il servizio accetta attach solo dal sistema
 * (uid 0), risponde ok, e da li' tratta ogni request=connect che arriva sulla porta come un
 * collegamento di quell'app. */
#define RICHIESTA_ATTACCA "attach"
#define CHIAVE_PROGRAMMA  "program"
#define CHIAVE_PERMESSO   "permission"

/* (il launcher) Da' al servizio 'nome' il capo 'capo' della porta dell'app 'app' (programma e
 * permessi concessi). 0 se il servizio ha risposto ok, -1 se no (non risponde entro un secondo,
 * o rifiuta). */
int attacca_porta(const char *nome, const struct chi_chiede *app, int capo);

/* (il servizio) Legge da un attach chi e' l'app (programma e permessi) in *app. 0, o -1 se
 * l'attach e' sbagliato (manca il programma, un permesso troppo lungo o troppi). */
int app_da_attach(const struct messaggio *attach, struct chi_chiede *app);

#endif
