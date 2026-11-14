/* CoreKit, del sistema: il servizio di rete e le richieste che gli si fanno. Chiedere come
 * sta la rete e, per chi ha il permesso, riavviarla o spegnerla. Lo usano i programmi del
 * sistema (la pagina della rete in Impostazioni, il comando network); non e' per le app
 * normali (vedi corekit.h).
 *
 * Il servizio network accende la scheda, le da' indirizzo, strada e DNS, e mette il
 * firewall (programmi/network.c). Oltre a questo, ascolta sul nome "network": chi vuole
 * qualcosa si collega, manda UNA richiesta, riceve UNA risposta, e il collegamento si
 * chiude (come il launcher: sistema/launcher.h).
 *
 * IL SERVIZIO E' IN SOLA LETTURA. Si puo' chiedere come sta, e riavviarlo o fermarlo; NON
 * si puo' cambiare la configurazione: network.conf e firewall.conf restano quelli del
 * sistema, in /system/config/network/, e il servizio li rilegge solo quando riparte. Una
 * configurazione della persona (in /data) non c'e'.
 *
 * I messaggi seguono lo STANDARD di SISTEMA.md (sezione 3; messaggi.h): testo
 * chiave=valore con la busta (request, id, result). Qui il CONTRATTO; le funzioni per chi
 * chiede (in fondo) lo nascondono, e stanno in interno/network.c.
 *
 * LE RICHIESTE (il valore di request):
 *
 *   status    nessun dato. Risposta: ok, con
 *       interface         la scheda scelta nella configurazione (es. eth0)
 *       state             starting (si sta accendendo: manca ancora l'indirizzo o la
 *                         scheda), up (accesa, con l'indirizzo), stopped (spenta con
 *                         stop: il firewall resta), failed (non si e' potuta accendere:
 *                         message dice perche'; il servizio resta fermo)
 *       link              yes se la scheda e' accesa e ha il collegamento (il cavo, o
 *                         quello virtuale), no se no o se la scheda non c'e'
 *       mode              dhcp o static (come nella configurazione)
 *       address           indirizzo/prefisso (es. 10.0.2.15/24); manca se non ce n'e'
 *       gateway           l'indirizzo della strada predefinita; manca se non c'e'
 *       dns               un server dei nomi; la chiave si ripete (fino a 3)
 *       lease             solo con dhcp e un indirizzo avuto: i secondi che restano al
 *                         prestito, oppure infinite se e' per sempre
 *       firewall          active se le regole sono nel kernel, inactive se no
 *       firewall_incoming, firewall_forward, firewall_outgoing, firewall_ping
 *                         allow o block: il riassunto delle regole (come in firewall.conf;
 *                         con incoming=allow il ping passa, quindi firewall_ping=allow).
 *                         Mancano con firewall=inactive.
 *       message           solo con state=failed: la frase del perche'
 *
 *   restart   nessun dato. Il servizio risponde ok e poi riparte da capo: rilegge le due
 *             configurazioni, rimette il firewall e riaccende la scheda. Per qualche
 *             istante il nome "network" non c'e' (il servizio si rimette in ascolto): chi
 *             chiede lo stato subito dopo puo' non avere risposta, e riprova. Permesso
 *             network.settings.
 *
 *   stop      nessun dato. Spegne la scheda: toglie indirizzo e strada, svuota i DNS e
 *             mette la scheda giu'. IL FIREWALL RESTA com'e' (la scelta piu' sicura: un
 *             computer senza rete ma ancora protetto, e non il contrario). Il servizio
 *             non finisce (init lo farebbe ripartire): resta acceso, risponde a status
 *             (state=stopped) e aspetta un restart. Permesso network.settings.
 *
 * LE RISPOSTE. result e' ok, denied (manca il permesso), invalid (richiesta sconosciuta).
 * Non c'e' mai error: restart e stop o si fanno, o il servizio non risponde.
 *
 * I PERMESSI (ha_permesso, sistema/servizi.h; oggi provvisoria: dice sempre si' e lo
 * scrive nel registro):
 *     network.use        status: chiunque lo abbia
 *     network.settings   restart, stop */
#ifndef NETWORK_H
#define NETWORK_H

#include "../corekit.h"

#define NOME_NETWORK "network"

/* le richieste (il valore di request) */
#define RETE_RICHIESTA_STATO    "status"
#define RETE_RICHIESTA_RIAVVIA  "restart"
#define RETE_RICHIESTA_FERMA    "stop"

/* le chiavi della risposta di status (oltre alla busta: messaggi.h) */
#define RETE_CHIAVE_SCHEDA      "interface"
#define RETE_CHIAVE_STATO       "state"
#define RETE_CHIAVE_COLLEGATA   "link"
#define RETE_CHIAVE_MODO        "mode"
#define RETE_CHIAVE_INDIRIZZO   "address"
#define RETE_CHIAVE_GATEWAY     "gateway"
#define RETE_CHIAVE_DNS         "dns"
#define RETE_CHIAVE_PRESTITO    "lease"
#define RETE_CHIAVE_FIREWALL    "firewall"
#define RETE_CHIAVE_F_ENTRATA   "firewall_incoming"
#define RETE_CHIAVE_F_INOLTRO   "firewall_forward"
#define RETE_CHIAVE_F_USCITA    "firewall_outgoing"
#define RETE_CHIAVE_F_PING      "firewall_ping"
#define RETE_CHIAVE_FRASE       "message"

/* i valori di state */
#define RETE_STATO_AVVIO   "starting"
#define RETE_STATO_ACCESA  "up"
#define RETE_STATO_FERMA   "stopped"
#define RETE_STATO_GUASTA  "failed"

/* i permessi che il servizio chiede */
#define PERMESSO_RETE_USARE        "network.use"
#define PERMESSO_RETE_IMPOSTAZIONI "network.settings"

/* --- per chi chiede ---
 * Le funzioni qui sotto rispondono con un numero: il result della risposta, tradotto. */
#define RETE_FATTO         0    /* ok */
#define RETE_ERRORE        -1   /* il servizio non c'e', non risponde in tempo, o la risposta non si capisce */
#define RETE_NON_PERMESSO  -2   /* denied */

/* Quante lettere, al piu', hanno i testi di struct stato_rete (compreso lo 0 finale). */
#define RETE_MAX_TESTO 64

/* prestito: i secondi sono >= 0; questi due valori dicono altro */
#define RETE_PRESTITO_NESSUNO    (-1)
#define RETE_PRESTITO_PER_SEMPRE (-2)

/* Lo stato della rete (la risposta di status, in forma di struct). I testi mancanti sono
 * "" (e quanti_dns 0). */
struct stato_rete {
    char scheda[RETE_MAX_TESTO];
    char stato[16];                 /* RETE_STATO_... */
    int collegata;                  /* 1 si', 0 no */
    char modo[16];                  /* "dhcp" o "static" */
    char indirizzo[RETE_MAX_TESTO]; /* "10.0.2.15/24" */
    char gateway[RETE_MAX_TESTO];
    char dns[3][RETE_MAX_TESTO];
    int quanti_dns;
    long prestito;                  /* secondi che restano, o RETE_PRESTITO_... */
    int firewall_attivo;            /* 1 si', 0 no */
    char f_entrata[8], f_inoltro[8], f_uscita[8], f_ping[8];   /* "allow", "block" o "" */
    char frase[160];                /* con state=failed */
};

/* Chiede lo stato (status): lo scrive in *s. RETE_FATTO, o l'errore (*s allora e' vuoto). */
int rete_stato(struct stato_rete *s);

/* Chiede di riavviare la rete e il firewall (restart). RETE_FATTO vuol dire che il
 * servizio ha accettato e sta ripartendo. */
int rete_riavvia(void);

/* Chiede di spegnere la scheda, lasciando il firewall (stop). */
int rete_ferma(void);

#endif
