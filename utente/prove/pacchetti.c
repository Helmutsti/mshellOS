/* pacchetti: prova le richieste sui pacchetti al launcher (launcher.h), e c'e' SOLO per le
 * prove automatiche (test/prove/pacchetti.py).
 *
 * Sta solo nel disco disco_pacchetti.img (build.sh), in /data/tests: init lo fa partire
 * all'accensione, come sistema (uid 0, che puo' chiedere di installare). Fa, una dopo
 * l'altra, le cose che domani fara' lo Store con contatore.app (sulla Scrivania della
 * persona), e scrive nella seriale come va ogni passo. Alla fine "pacchetti: tutto bene",
 * oppure "pacchetti: fallita" con il passo. */
#include "libreria.h"
#include "launcher.h"

#define PACCHETTO "/data/users/1000/home/Scrivania/contatore.app"
#define NON_PACCHETTO "/data/users/1000/home/leggimi.txt"

static struct resoconto r;
static struct elenco_app e;
static int fallite;

static void scrivi_numero(int n) {
    char cifre[12];
    int k = 0;
    if (n < 0) { scrivi_testo("-"); n = -n; }
    do cifre[k++] = (char)('0' + n % 10); while (n /= 10);
    while (k) scrivi(&cifre[--k], 1);
}

static void passo(const char *cosa, int esito, int atteso) {
    scrivi_testo("pacchetti: ");
    scrivi_testo(cosa);
    scrivi_testo(" -> ");
    scrivi_numero(esito);
    if (esito != atteso) {
        scrivi_testo(" (atteso ");
        scrivi_numero(atteso);
        scrivi_testo(") FALLITA");
        fallite++;
    }
    scrivi_testo("\n");
}

static int ha_contatore(void) {
    for (int i = 0; i < e.quante; i++)
        if (descrizione_uguali(e.app[i].m.id, "contatore")) return e.app[i].di_sistema ? 2 : 1;
    return 0;
}

int main(void) {
    dormi(1000);    /* che i servizi siano partiti */
    passo("elenco", elenco_app_installate(&e), LAUNCHER_FATTO);
    passo("app nell'elenco (5 del sistema)", e.quante, 5);
    passo("contatore nell'elenco prima", ha_contatore(), 0);

    struct risposta_launcher risposta;
    passo("apri il file contatore.app", apri_file(PACCHETTO, &risposta), LAUNCHER_SERVE_STORE);
    scrivi_testo("pacchetti: messaggio: ");
    scrivi_testo(risposta.messaggio);
    scrivi_testo("\n");
    passo("apri l'app contatore (non installata)", apri_applicazione("contatore") >= 0, 0);

    passo("descrivi un file che non e' un pacchetto", launcher_pacchetto(DESCRIVI_PACCHETTO, NON_PACCHETTO, &r),
          LAUNCHER_PACCHETTO_NON_VALIDO);
    passo("descrivi contatore.app", launcher_pacchetto(DESCRIVI_PACCHETTO, PACCHETTO, &r), LAUNCHER_FATTO);
    passo("  id contatore", descrizione_uguali(r.m.id, "contatore"), 1);
    passo("  errori", r.quanti_errori, 0);
    passo("  icone svg chiara e scura", r.svg_presente[0] + r.svg_presente[1], 2);
    passo("aggiorna (non e' installata)", launcher_pacchetto(AGGIORNA_PACCHETTO, PACCHETTO, &r),
          LAUNCHER_PACCHETTO_NON_VALIDO);
    passo("installa contatore.app", launcher_pacchetto(INSTALLA_PACCHETTO, PACCHETTO, &r), LAUNCHER_FATTO);
    passo("installa di nuovo", launcher_pacchetto(INSTALLA_PACCHETTO, PACCHETTO, &r), LAUNCHER_PACCHETTO_NON_VALIDO);
    passo("aggiorna alla stessa versione", launcher_pacchetto(AGGIORNA_PACCHETTO, PACCHETTO, &r),
          LAUNCHER_PACCHETTO_NON_VALIDO);
    passo("elenco dopo", elenco_app_installate(&e), LAUNCHER_FATTO);
    passo("contatore nell'elenco dopo (installata dalla persona)", ha_contatore(), 1);
    passo("apri l'app contatore (installata)", apri_applicazione("contatore") >= 0, 1);

    scrivi_testo(fallite ? "pacchetti: fallita\n" : "pacchetti: tutto bene\n");
    return 0;
}
