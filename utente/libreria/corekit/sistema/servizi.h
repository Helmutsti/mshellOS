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
 * system.power, apps.manage, apps.open), 0 se no. */
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

/* INIT CUSTODISCE LE PORTE (SISTEMA.md, sezione 3). Il capo "servizio" di ogni porta sta nel
 * servizio: se il servizio cade, si chiude, e l'app resterebbe con una porta morta. Allora init ne
 * tiene una copia (come il "file descriptor store" di systemd) e la riconsegna al servizio quando
 * riparte. Catalog, launcher e network hanno un canale con init (una coppia di socket: init tiene
 * un capo, presta l'altro al servizio, al suo descrittore D_CUSTODE) su cui passa:
 *   servizio -> init   request=keep       program, permission (ripetuta), capo allegato
 *   init -> servizio   request=restore    le stesse cose, per ogni porta che init tiene
 *   init -> servizio   request=restored   alla fine: le porte sono finite (anche se erano zero)
 * Nessuna risposta in nessun caso. Init non legge mai dalle porte: le guarda solo per sapere
 * quando l'app le chiude, e allora le butta. */
#define RICHIESTA_CUSTODISCI   "keep"
#define RICHIESTA_RICONSEGNA   "restore"
#define RICHIESTA_RICONSEGNATE "restored"

/* Manda sul canale 'canale' un messaggio 'richiesta' (RICHIESTA_CUSTODISCI o RICHIESTA_RICONSEGNA)
 * per la porta dell'app 'app' con il capo 'capo' allegato (il capo resta di chi manda). 0, o -1. */
int manda_porta_app(int canale, const char *richiesta, const struct chi_chiede *app, int capo);

/* (il servizio) Ha una porta nuova di un'app (da attach, o creata da lui): ne manda una copia a
 * init. Se non riesce non e' un guasto: la porta funziona lo stesso, ma non sopravvivera' a una
 * caduta del servizio. 0, o -1. */
int custodisci_porta(int custode, const struct chi_chiede *app, int capo);

/* (il servizio, appena parte) Legge dal canale le porte che init riconsegna, fino a
 * RICHIESTA_RICONSEGNATE (al massimo 2 secondi di attesa per messaggio). Per ognuna chiama
 * 'riprendi' con l'app (identita' e permessi) e il capo: il servizio lo mette nella sua tabella
 * (ne e' padrone) e lo segna come gia' scritto nel registro. Ritorna quante porte ha riavuto, o
 * -1 se il canale non c'e' o non e' finito bene. */
int riprendi_porte(int custode, void (*riprendi)(const struct chi_chiede *app, int capo, void *dato), void *dato);

#endif
