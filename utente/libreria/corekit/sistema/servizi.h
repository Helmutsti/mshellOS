/* CoreKit, del sistema: il lato del SERVIZIO nei messaggi (messaggi.h). Chi chiede usa
 * solo messaggi.h (la busta, chiedi); un servizio, quando riceve una richiesta, vuole
 * anche sapere chi c'e' dall'altra parte e se ha il permesso: queste funzioni sono qui,
 * perche' le usano solo i servizi del sistema. Il codice sta in interno/messaggi.c, con il
 * resto dei messaggi; il perche' e il come, in testa a quel file. */
#ifndef SERVIZI_H
#define SERVIZI_H

#include "../messaggi.h"

/* Chi c'e' all'altro capo di un collegamento: lo dice il kernel, non si puo' fingere. */
struct chi_chiede {
    int uid;                                  /* 0: il sistema; 1000...: una persona */
    char programma[MAX_PERCORSO_PROGRAMMA];   /* es. /system/apps/files/code; "" se non si sa */
};

/* Riempie *c per il collegamento 'd'. 0, o -1 se il kernel non lo sa (non e' un socket). */
int chi_chiede(int d, struct chi_chiede *c);

/* Chi chiede (riempito da chi_chiede) ha il permesso 'nome' (es. "network.settings")?
 * 1 si', 0 no. PROVVISORIO: oggi risponde sempre si' e scrive la domanda nel registro;
 * cosa fara' quando sara' vera e' spiegato accanto al codice, in interno/messaggi.c. */
int ha_permesso(const struct chi_chiede *c, const char *nome);

/* LE PORTE DELLE APP, il lato del sistema (messaggi.h, "le porte delle app"). Il launcher, quando
 * fa partire un'app, da' a ogni servizio il capo della porta di quell'app: si collega al servizio
 * per nome e manda request=attach, con program (il programma dell'app, che diventa il suo
 * chi_chiede: uid UID_APPLICAZIONI e quel programma) e il capo allegato. Il servizio accetta
 * attach solo dal sistema (uid 0), risponde ok, e da li' tratta ogni request=connect che arriva
 * sulla porta come un collegamento di quell'app. */
#define RICHIESTA_ATTACCA "attach"
#define CHIAVE_PROGRAMMA  "program"

/* (il launcher) Da' al servizio 'nome' il capo 'capo' della porta dell'app 'programma'. 0 se il
 * servizio ha risposto ok, -1 se no (non risponde entro un secondo, o rifiuta). */
int attacca_porta(const char *nome, const char *programma, int capo);

#endif
