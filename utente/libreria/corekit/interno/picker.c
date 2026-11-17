/* CoreKit: le funzioni per chiedere a picker (il contratto e' in picker.h). */
#include "corekit/corekit.h"
#include "corekit/messaggi.h"
#include "corekit/picker.h"
#include <string.h>

/* Una richiesta 'q' al servizio, e la risposta: il file (o un esito negativo). La persona decide
 * quando rispondere, quindi si aspetta PICKER_ATTESA_MS e non TEMPO_CHIEDI. */
static int chiedi_file(struct messaggio *q, char *nome, int massimo) {
    static struct messaggio r;
    int d = collega_servizio(NOME_PICKER);
    if (d < 0) return PICKER_NON_RISPONDE;
    int esito = PICKER_NON_RISPONDE;
    if (q->guasto) {
        esito = PICKER_NON_VALIDO;                    /* un nome che non sta in un messaggio */
    } else if (chiedi_entro(d, q, &r, PICKER_ATTESA_MS) == 0) {
        const char *scelto = messaggio_valore(&r, PICKER_CHIAVE_NOME);
        if (messaggio_vale(&r, CHIAVE_ESITO, ESITO_OK)) {
            if (r.allegato >= 0 && scelto && (int)strlen(scelto) < massimo) {
                strcpy(nome, scelto);
                esito = r.allegato;                   /* il file e' nostro: lo chiude chi lo riceve */
                r.allegato = -1;
            } else {
                esito = PICKER_ERRORE;
            }
        } else if (messaggio_vale(&r, CHIAVE_ESITO, ESITO_NON_VALIDO)) {
            esito = PICKER_NON_VALIDO;
        } else if (messaggio_vale(&r, CHIAVE_ESITO, ESITO_ERRORE)) {
            esito = messaggio_vale(&r, PICKER_CHIAVE_MOTIVO, PICKER_MOTIVO_ANNULLATO) ? PICKER_ANNULLATO
                                                                                      : PICKER_ERRORE;
        }
        if (r.allegato >= 0) chiudi(r.allegato);     /* un allegato che non serve */
    }
    chiudi(d);
    return esito;
}

int picker_apri(char *nome, int massimo) {
    static struct messaggio q;
    messaggio_inizia(&q);
    messaggio_aggiungi(&q, CHIAVE_RICHIESTA, PICKER_RICHIESTA_APRI);
    return chiedi_file(&q, nome, massimo);
}

int picker_salva(const char *proposto, char *nome, int massimo) {
    static struct messaggio q;
    messaggio_inizia(&q);
    messaggio_aggiungi(&q, CHIAVE_RICHIESTA, PICKER_RICHIESTA_SALVA);
    messaggio_aggiungi(&q, PICKER_CHIAVE_NOME, proposto ? proposto : "");
    return chiedi_file(&q, nome, massimo);
}
