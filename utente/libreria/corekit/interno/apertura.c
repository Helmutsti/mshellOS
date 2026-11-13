/* CoreKit: aprire un'app o un file (apertura.h), sopra il protocollo del launcher
 * (sistema/launcher.h). Ogni funzione e' UNA richiesta al launcher, con launcher_chiedi:
 * si collega al nome "launcher", manda la richiesta, aspetta la risposta. Gli esiti del
 * launcher che un'app puo' vedere qui sono gli stessi numeri degli APERTURA_...: launcher.h
 * li definisce da loro. */
#include "../apertura.h"
#include "../sistema/launcher.h"

int apri_applicazione_con(const char *id, const char *argomento) {
    struct richiesta_launcher r = { .tipo = APRI_APPLICAZIONE };
    struct risposta_launcher risposta;
    launcher_copia(r.applicazione, id, MAX_NOME_FILE);
    launcher_copia(r.argomento, argomento, MAX_PERCORSO);
    return launcher_chiedi(&r, -1, &risposta, 0, 0) < 0 ? -1 : risposta.numero;
}

int apri_applicazione(const char *id) { return apri_applicazione_con(id, 0); }

int apri_file(const char *percorso, char *perche, unsigned int byte) {
    struct richiesta_launcher r = { .tipo = APRI_FILE };
    struct risposta_launcher risposta;
    launcher_copia(r.argomento, percorso, MAX_PERCORSO);
    int esito = launcher_chiedi(&r, -1, &risposta, 0, 0);
    if (perche && byte > 0) {
        /* la frase, tagliata se non ci sta, sempre con lo 0 finale (launcher_chiedi lo
         * mette in fondo al messaggio) */
        unsigned int n = 0;
        for (; n < byte - 1 && risposta.messaggio[n]; n++) perche[n] = risposta.messaggio[n];
        perche[n] = 0;
    }
    return esito;
}
