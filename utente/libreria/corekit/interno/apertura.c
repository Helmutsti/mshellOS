/* CoreKit: aprire un'app o un file (apertura.h), con le richieste open_app e open_file al
 * launcher (il contratto e' in sistema/launcher.h). Ogni funzione e' UNA richiesta, fatta
 * con launcher_chiedi (interno/launcher.c): si collega al nome "launcher", manda la
 * richiesta, aspetta la risposta al massimo TEMPO_CHIEDI. Gli esiti del launcher che
 * un'app puo' vedere qui sono gli stessi numeri degli APERTURA_...: launcher.h li definisce
 * da loro. */
#include "../apertura.h"
#include "../messaggi.h"
#include "../sistema/launcher.h"
#include "interno.h"

int apri_applicazione_con(const char *id, const char *argomento) {
    struct messaggio q, r;
    messaggio_inizia(&q);
    messaggio_aggiungi(&q, CHIAVE_RICHIESTA, APRI_APPLICAZIONE);
    messaggio_aggiungi(&q, CHIAVE_APP, id ? id : "");
    if (argomento && argomento[0]) messaggio_aggiungi(&q, CHIAVE_ARGOMENTO, argomento);
    long n;
    if (launcher_chiedi(&q, &r, TEMPO_CHIEDI) != LAUNCHER_FATTO || messaggio_numero(&r, CHIAVE_COMPITO, &n) < 0 || n < 0)
        return -1;
    return (int)n;
}

int apri_applicazione(const char *id) { return apri_applicazione_con(id, 0); }

int apri_file(const char *percorso, char *perche, unsigned int byte) {
    struct messaggio q, r;
    messaggio_inizia(&q);
    messaggio_aggiungi(&q, CHIAVE_RICHIESTA, APRI_FILE);
    messaggio_aggiungi(&q, CHIAVE_PERCORSO, percorso ? percorso : "");
    int esito = launcher_chiedi(&q, &r, TEMPO_CHIEDI);
    launcher_frase(&r, perche, byte);
    return esito;
}
