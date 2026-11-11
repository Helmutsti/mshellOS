/* Il launcher: il servizio del sistema che fa partire le applicazioni.
 *
 * Non ha finestre, come LaunchServices sul Mac o systemd per GNOME: l'interfaccia (le
 * icone da cliccare) e' della scrivania, che gli chiede "apri Contatore". Lui:
 *   1. controlla che sia un'applicazione installata: una cartella con il suo manifest
 *      in /system/apps (quelle del sistema) o, se li' non c'e', in /data/apps (quelle
 *      installate dalla persona: descrizione.h);
 *   2. dal manifest prende quale programma far partire (code, nella stessa cartella);
 *   3. lo fa partire come APPLICAZIONE (uid 1000), con l'argomento della richiesta se
 *      c'e' (launcher.h), non come sistema: e' per questo che
 *      il launcher e' del sistema (uid 0), perche' solo il sistema puo' far partire un
 *      programma con un altro uid;
 *   4. risponde "fatto" (con il numero del compito) o "non si puo'".
 *
 * Il suo nome, "launcher", lo tiene init, che glielo presta al descrittore 0: se il
 * launcher cade, nessun altro puo' prenderlo, e chi chiede nel frattempo aspetta.
 * Chi chiede, per ora, puo' essere chiunque. */
#include "libreria.h"
#include "launcher.h"
#include "descrizione.h"

#define D_ASCOLTO 0      /* prestato da init */
#define MAX_CLIENTI 8

static void rispondi(int d) {
    struct richiesta_launcher r;
    int n = ricevi(d, &r, sizeof r);
    if (n != sizeof r || r.tipo != APRI_APPLICAZIONE) return;
    r.applicazione[MAX_NOME_FILE - 1] = 0;
    r.argomento[MAX_PERCORSO - 1] = 0;
    struct risposta_launcher risposta = { -1, -1 };
    struct descrizione desc;
    if (leggi_descrizione(r.applicazione, &desc) == 0) {
        risposta.numero = avvia_come_con(desc.programma, UID_APPLICAZIONI, r.argomento);
        if (risposta.numero >= 0) risposta.esito = 0;
    }
    manda_subito(d, &risposta, sizeof risposta);   /* chi chiede non ci blocca mai */
}

int main(void) {
    int clienti[MAX_CLIENTI];
    int quanti = 0;
    for (;;) {
        struct da_guardare g[1 + MAX_CLIENTI];
        g[0].descrittore = D_ASCOLTO;
        for (int i = 0; i < quanti; i++) g[1 + i].descrittore = clienti[i];
        if (aspetta(g, 1 + quanti, -1) < 0) return 1;

        /* le richieste: una per collegamento, poi lo si chiude */
        for (int i = quanti - 1; i >= 0; i--) {
            if (!g[1 + i].pronto) continue;
            rispondi(clienti[i]);
            chiudi(clienti[i]);
            clienti[i] = clienti[--quanti];
        }
        if (g[0].pronto) {
            int d = accetta(D_ASCOLTO);
            if (d >= 0 && quanti < MAX_CLIENTI) clienti[quanti++] = d;
            else if (d >= 0) chiudi(d);
        }
    }
}
