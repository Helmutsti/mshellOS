/* catalog: prova il servizio delle impostazioni (corekit/catalog.h), e c'e' SOLO per le prove.
 *
 * Sta solo nel disco disco_catalog.img (build.sh; nella sandbox: avvio con "catalog"), in
 * /data/tests: init lo fa partire all'accensione, come sistema. Usa catalog con le funzioni di
 * CoreKit, come farebbe ogni programma, e scrive sulla seriale come va ogni passo
 * ("prova catalog: ... -> si'" o "... -> no FALLITA"):
 *   1. legge le impostazioni: nel disco delle prove il file del tema non c'e', quindi tema
 *      chiaro e sfondo predefinito;
 *   2. si fa avvisare (watch) su un collegamento;
 *   3. cambia il tema in scuro: il file del tema dice "scuro", una nuova lettura dice dark, e
 *      sul collegamento di watch arriva l'avviso con dark;
 *   4. cambia lo sfondo in un nome e poi di nuovo nel predefinito, con gli avvisi;
 *   5. i valori sbagliati (tema "blu", sfondo "../x", nessun valore) e una richiesta sconosciuta
 *      ricevono invalid, e non cambia niente;
 *   6. rimette il tema chiaro.
 * Alla fine "prova catalog: tutto bene" o "prova catalog: fallita". */
#include "corekit/corekit.h"
#include "corekit/messaggi.h"
#include "corekit/catalog.h"
#include <string.h>
#include <sys/stat.h>

static int fallite;

static void passo(const char *cosa, int giusto) {
    scrivi_testo("prova catalog: ");
    scrivi_testo(cosa);
    scrivi_testo(giusto ? " -> si'\n" : " -> no FALLITA\n");
    if (!giusto) fallite = 1;
}

static int uguali(const struct catalog_valori *v, const char *tema, const char *sfondo) {
    return !strcmp(v->tema, tema) && !strcmp(v->sfondo, sfondo);
}

/* Il contenuto del file (fino al primo a capo), o "" se non c'e'. */
static void leggi_file(const char *percorso, char *dove, int massimo) {
    dove[0] = 0;
    int d = apri(percorso, APRI_LEGGERE);
    if (d < 0) return;
    int n = leggi(d, dove, (unsigned int)massimo - 1);
    chiudi(d);
    dove[n < 0 ? 0 : n] = 0;
    for (int i = 0; dove[i]; i++) if (dove[i] == '\n') dove[i] = 0;
}

/* Aspetta un avviso su 'd' (al massimo 2 secondi) e lo mette in *v. 1 se e' arrivato. */
static int aspetta_avviso(int d, struct catalog_valori *v) {
    struct da_guardare g = { d, 0 };
    if (aspetta(&g, 1, 2000) != 1) return 0;
    return catalog_avviso(d, v) == 1;
}

/* Una richiesta sconosciuta, a mano: deve tornare invalid. */
static int sconosciuta_rifiutata(void) {
    static struct messaggio q, r;
    int d = collega(NOME_CATALOG);
    if (d < 0) return 0;
    messaggio_inizia(&q);
    messaggio_aggiungi(&q, CHIAVE_RICHIESTA, "boh");
    int giusto = chiedi(d, &q, &r) == 0 && messaggio_vale(&r, CHIAVE_ESITO, ESITO_NON_VALIDO);
    chiudi(d);
    return giusto;
}

int main(void) {
    struct catalog_valori v, avviso;
    /* catalog parte con il sistema: si aspetta che risponda, fino a 10 secondi */
    int pronto = 0;
    for (int volta = 0; volta < 100 && !pronto; volta++) {
        pronto = catalog_leggi(&v) == 0;
        if (!pronto) dormi(100);
    }
    passo("catalog risponde", pronto);
    passo("tema chiaro e sfondo predefinito all'inizio", pronto && uguali(&v, CATALOG_TEMA_CHIARO, ""));

    int osserva = catalog_osserva(&v);
    passo("watch", osserva >= 0 && uguali(&v, CATALOG_TEMA_CHIARO, ""));

    passo("set del tema scuro", catalog_cambia(CATALOG_TEMA_SCURO, 0) == 0);
    char file[64];
    leggi_file("/data/users/1000/preferences/tema", file, sizeof file);
    passo("il file del tema dice scuro", !strcmp(file, "scuro"));
    struct stat s;   /* catalog gira come la persona: il file che scrive e' suo */
    passo("il file del tema e' della persona (uid 1000)",
          stat("/data/users/1000/preferences/tema", &s) == 0 && s.st_uid == 1000);
    passo("get dice dark", catalog_leggi(&v) == 0 && uguali(&v, CATALOG_TEMA_SCURO, ""));
    passo("avviso con dark", aspetta_avviso(osserva, &avviso) && uguali(&avviso, CATALOG_TEMA_SCURO, ""));

    passo("set dello sfondo", catalog_cambia(0, "lago.jpg") == 0);
    passo("avviso con lo sfondo", aspetta_avviso(osserva, &avviso) && uguali(&avviso, CATALOG_TEMA_SCURO, "lago.jpg"));
    passo("set dello sfondo predefinito", catalog_cambia(0, "") == 0);
    passo("avviso con lo sfondo predefinito", aspetta_avviso(osserva, &avviso) && uguali(&avviso, CATALOG_TEMA_SCURO, ""));
    passo("set con lo stesso tema: nessun avviso", catalog_cambia(CATALOG_TEMA_SCURO, 0) == 0 &&
                                                   !aspetta_avviso(osserva, &avviso));

    passo("tema blu rifiutato", catalog_cambia("blu", 0) == CATALOG_NON_VALIDO);
    passo("sfondo ../x rifiutato", catalog_cambia(0, "../x") == CATALOG_NON_VALIDO);
    passo("set senza valori rifiutato", catalog_cambia(0, 0) == CATALOG_NON_VALIDO);
    passo("richiesta sconosciuta rifiutata", sconosciuta_rifiutata());
    passo("dopo i rifiuti non e' cambiato niente", catalog_leggi(&v) == 0 && uguali(&v, CATALOG_TEMA_SCURO, ""));

    passo("set del tema chiaro", catalog_cambia(CATALOG_TEMA_CHIARO, 0) == 0);
    passo("avviso con light", aspetta_avviso(osserva, &avviso) && uguali(&avviso, CATALOG_TEMA_CHIARO, ""));
    if (osserva >= 0) chiudi(osserva);

    scrivi_testo(fallite ? "prova catalog: fallita\n" : "prova catalog: tutto bene\n");
    return 0;
}
