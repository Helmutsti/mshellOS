/* CoreKit: le richieste al servizio di rete, dalla parte di chi chiede (sistema/network.h,
 * dove c'e' il contratto: le richieste, le chiavi, le risposte, i permessi).
 *
 * Ogni funzione e' UNA richiesta: si fa il messaggio (messaggi.h), ci si collega al nome
 * "network", si manda con chiedi, si aspetta la risposta e si chiude il collegamento (il
 * servizio serve una richiesta per collegamento). Poi la risposta si traduce in un numero,
 * RETE_..., o, per lo stato, nella struct stato_rete: chi chiama non vede i messaggi. */
#include <string.h>

#include "../messaggi.h"
#include "../sistema/network.h"

/* Una richiesta senza dati: la risposta in *r. RETE_FATTO, RETE_NON_PERMESSO o RETE_ERRORE. */
static int chiedi_alla_rete(const char *richiesta, struct messaggio *r) {
    struct messaggio q;
    messaggio_inizia(&q);
    messaggio_aggiungi(&q, CHIAVE_RICHIESTA, richiesta);
    int d = collega(NOME_NETWORK);
    int e = d >= 0 ? chiedi(d, &q, r) : -1;
    if (d >= 0) chiudi(d);
    if (e != 0) {
        messaggio_risposta(r, 0, ESITO_ERRORE);   /* il servizio non c'e', ha chiuso o e' scaduto */
        return RETE_ERRORE;
    }
    if (r->allegato >= 0) { chiudi(r->allegato); r->allegato = -1; }   /* non ne aspetta nessuno */
    if (messaggio_vale(r, CHIAVE_ESITO, ESITO_OK)) return RETE_FATTO;
    if (messaggio_vale(r, CHIAVE_ESITO, ESITO_NEGATO)) return RETE_NON_PERMESSO;
    return RETE_ERRORE;
}

/* Copia il testo in 'dove' (al massimo 'byte', compreso lo 0). */
static void copia(const char *v, char *dove, unsigned int byte) {
    unsigned int n = 0;
    for (; v && n < byte - 1 && v[n]; n++) dove[n] = v[n];
    dove[n] = 0;
}

int rete_stato(struct stato_rete *s) {
    struct messaggio r;
    memset(s, 0, sizeof *s);
    s->prestito = RETE_PRESTITO_NESSUNO;
    int e = chiedi_alla_rete(RETE_RICHIESTA_STATO, &r);
    if (e != RETE_FATTO) return e;
    copia(messaggio_valore(&r, RETE_CHIAVE_SCHEDA), s->scheda, sizeof s->scheda);
    copia(messaggio_valore(&r, RETE_CHIAVE_STATO), s->stato, sizeof s->stato);
    s->collegata = messaggio_vale(&r, RETE_CHIAVE_COLLEGATA, "yes");
    copia(messaggio_valore(&r, RETE_CHIAVE_MODO), s->modo, sizeof s->modo);
    copia(messaggio_valore(&r, RETE_CHIAVE_INDIRIZZO), s->indirizzo, sizeof s->indirizzo);
    copia(messaggio_valore(&r, RETE_CHIAVE_GATEWAY), s->gateway, sizeof s->gateway);
    for (const char *v = 0; s->quanti_dns < 3 && (v = messaggio_prossimo(&r, RETE_CHIAVE_DNS, v));)
        copia(v, s->dns[s->quanti_dns++], RETE_MAX_TESTO);
    long n;
    if (messaggio_vale(&r, RETE_CHIAVE_PRESTITO, "infinite")) s->prestito = RETE_PRESTITO_PER_SEMPRE;
    else if (messaggio_numero(&r, RETE_CHIAVE_PRESTITO, &n) == 0) s->prestito = n;
    s->firewall_attivo = messaggio_vale(&r, RETE_CHIAVE_FIREWALL, "active");
    copia(messaggio_valore(&r, RETE_CHIAVE_F_ENTRATA), s->f_entrata, sizeof s->f_entrata);
    copia(messaggio_valore(&r, RETE_CHIAVE_F_INOLTRO), s->f_inoltro, sizeof s->f_inoltro);
    copia(messaggio_valore(&r, RETE_CHIAVE_F_USCITA), s->f_uscita, sizeof s->f_uscita);
    copia(messaggio_valore(&r, RETE_CHIAVE_F_PING), s->f_ping, sizeof s->f_ping);
    copia(messaggio_valore(&r, RETE_CHIAVE_FRASE), s->frase, sizeof s->frase);
    return RETE_FATTO;
}

int rete_riavvia(void) {
    struct messaggio r;
    return chiedi_alla_rete(RETE_RICHIESTA_RIAVVIA, &r);
}

int rete_ferma(void) {
    struct messaggio r;
    return chiedi_alla_rete(RETE_RICHIESTA_FERMA, &r);
}
