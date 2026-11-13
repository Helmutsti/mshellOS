/* messaggi: prova lo standard dei messaggi tra servizi e clienti (messaggi.h), e c'e' SOLO
 * per le prove automatiche (test/prove/messaggi.py).
 *
 * Sta solo nel disco disco_messaggi.img (build.sh), in /data/tests: init lo fa partire
 * all'accensione, come sistema. Fa da tutte e due le parti: apre una porta sua
 * ("prova-messaggi") e si sdoppia (fork): il figlio fa il SERVIZIO FINTO, il padre il
 * CLIENTE. Il cliente:
 *   1. prova a costruire messaggi sbagliati: chiave con maiuscole, con =, con a capo,
 *      vuota, valore con a capo, troppi dati: tutti rifiutati, e il messaggio guasto non
 *      si manda;
 *   2. chiede "echo" con chiedi: una chiave ripetuta (dns due volte), un numero negativo,
 *      un valore con dentro un =, e un foglio allegato con scritto "ciao". Il servizio
 *      prima manda una risposta con un altro id (chiedi deve buttarla), poi quella vera:
 *      result=ok, gli stessi dati, cosa c'era nel foglio, e chi chiede (uid e programma);
 *   3. manda a mano un pacchetto che non segue lo standard (una chiave maiuscola, una riga
 *      senza =): il servizio risponde result=invalid, senza id;
 *   4. chiede una richiesta che il servizio non conosce: result=invalid, con l'id;
 *   5. il tempo massimo: chiede "slow" con chiedi_entro e 300 ms; il servizio risponde
 *      solo dopo 800 ms. chiedi deve dare CHIEDI_SCADUTO dopo circa 300 ms, con
 *      result=timeout. Poi chiede "ping" sulla stessa connessione: la risposta tardiva a
 *      "slow" arriva prima, e chiedi deve buttarla (id vecchio) e dare quella di ping;
 *   6. i permessi provvisori: nella risposta a echo il servizio dice cosa ha risposto
 *      ha_permesso("test.example") (sempre si'), e la riga "permesso test.example a
 *      /data/tests/messaggi (uid 0): concesso (provvisorio)" va nel registro (la
 *      controlla test/prove/messaggi.py).
 * Ogni passo lo scrive nella seriale ("messaggi: ... -> esito"). Alla fine "messaggi:
 * tutto bene", oppure "messaggi: fallita". */
#include "messaggi.h"
#include <sys/wait.h>
#include <unistd.h>

#define PORTA "prova-messaggi"

static int fallite;

static int uguali(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *a == *b) a++, b++;
    return *a == *b;
}

static void passo(const char *cosa, int giusto) {
    scrivi_testo("messaggi: ");
    scrivi_testo(cosa);
    scrivi_testo(giusto ? " -> si'\n" : " -> no FALLITA\n");
    if (!giusto) fallite++;
}

/* --- il servizio finto --- */

static void servizio(int porta) {
    int d = accetta(porta);
    chiudi(porta);
    struct messaggio q, r;
    int e;
    while ((e = messaggio_ricevi(d, &q)) != 0 && e != -1) {
        if (e == MESSAGGIO_NON_VALIDO) {
            messaggio_risposta(&r, 0, ESITO_NON_VALIDO);
        } else if (messaggio_vale(&q, CHIAVE_RICHIESTA, "echo")) {
            /* prima una risposta con un altro id: chi chiede deve buttarla */
            messaggio_inizia(&r);
            messaggio_aggiungi(&r, CHIAVE_ID, "999999");
            messaggio_aggiungi(&r, CHIAVE_ESITO, ESITO_ERRORE);
            messaggio_manda(d, &r);
            /* poi quella vera: gli stessi dati, nello stesso ordine */
            messaggio_risposta(&r, &q, ESITO_OK);
            for (const char *v = 0; (v = messaggio_prossimo(&q, "dns", v));) messaggio_aggiungi(&r, "dns", v);
            long n;
            if (messaggio_numero(&q, "count", &n) == 0) messaggio_aggiungi_numero(&r, "count", n);
            const char *t = messaggio_valore(&q, "text");
            if (t) messaggio_aggiungi(&r, "text", t);
            /* cosa c'e' nel foglio allegato */
            if (q.allegato >= 0) {
                const char *foglio_letto = mappa(q.allegato, 0);
                if (foglio_letto) {
                    char c[5] = { foglio_letto[0], foglio_letto[1], foglio_letto[2], foglio_letto[3], 0 };
                    messaggio_aggiungi(&r, "attachment", c);
                    togli((void *)foglio_letto);
                }
                chiudi(q.allegato);
            }
            /* chi chiede, e se ha il permesso (provvisorio: sempre si') */
            struct chi_chiede c;
            if (chi_chiede(d, &c) == 0) {
                messaggio_aggiungi_numero(&r, "uid", c.uid);
                messaggio_aggiungi(&r, "program", c.programma);
                messaggio_aggiungi(&r, "permission", ha_permesso(&c, "test.example") ? "yes" : "no");
            }
        } else if (messaggio_vale(&q, CHIAVE_RICHIESTA, "slow")) {
            /* un servizio vivo ma lento: risponde dopo il tempo massimo di chi chiede */
            dormi(800);
            messaggio_risposta(&r, &q, ESITO_OK);
            messaggio_aggiungi(&r, "late", "1");
        } else if (messaggio_vale(&q, CHIAVE_RICHIESTA, "ping")) {
            messaggio_risposta(&r, &q, ESITO_OK);
            messaggio_aggiungi(&r, "pong", "1");
        } else {
            messaggio_risposta(&r, &q, ESITO_NON_VALIDO);
        }
        messaggio_manda(d, &r);
    }
    chiudi(d);
}

/* --- il cliente --- */

static void costruire(void) {
    struct messaggio m;
    messaggio_inizia(&m);
    passo("chiave con maiuscole rifiutata", messaggio_aggiungi(&m, "Dns", "x") < 0);
    passo("chiave con = rifiutata", messaggio_aggiungi(&m, "a=b", "x") < 0);
    passo("chiave con a capo rifiutata", messaggio_aggiungi(&m, "a\nb", "x") < 0);
    passo("chiave vuota rifiutata", messaggio_aggiungi(&m, "", "x") < 0);
    passo("valore con a capo rifiutato", messaggio_aggiungi(&m, "dns", "1.1.1.1\nrequest=x") < 0);
    passo("il messaggio rifiutato resta vuoto", m.byte == 0 && messaggio_valore(&m, "dns") == 0);
    passo("un messaggio guasto non si manda", messaggio_manda(-1, &m) < 0);

    messaggio_inizia(&m);
    passo("un messaggio vuoto non si manda", messaggio_manda(-1, &m) < 0);
    char lungo[1001];
    for (int i = 0; i < 1000; i++) lungo[i] = 'x';
    lungo[1000] = 0;
    int aggiunte = 0;
    while (messaggio_aggiungi(&m, "data", lungo) == 0) aggiunte++;
    passo("troppi dati: si ferma a 5 righe da 1000 (limite 5120 byte)", aggiunte == 5 && m.guasto);

    messaggio_inizia(&m);
    messaggio_aggiungi(&m, "count", "12a");
    long n;
    passo("un numero che non e' un numero", messaggio_numero(&m, "count", &n) < 0);
    passo("una chiave che non c'e'", messaggio_valore(&m, "nothing") == 0);
}

static void echo(int d) {
    struct messaggio q, r;
    messaggio_inizia(&q);
    messaggio_aggiungi(&q, CHIAVE_RICHIESTA, "echo");
    messaggio_aggiungi(&q, "dns", "1.1.1.1");
    messaggio_aggiungi(&q, "dns", "9.9.9.9");
    messaggio_aggiungi_numero(&q, "count", -42);
    messaggio_aggiungi(&q, "text", "a=b c");
    int f = foglio(4096);
    char *scritto = f >= 0 ? mappa(f, 0) : 0;
    if (scritto) {
        scritto[0] = 'c', scritto[1] = 'i', scritto[2] = 'a', scritto[3] = 'o';
        togli(scritto);
    }
    q.allegato = f;
    passo("chiedi echo", chiedi(d, &q, &r) == 0);
    chiudi(f);
    scrivi_testo("messaggi: risposta:");
    for (unsigned int i = 0; i < r.byte; i++) scrivi(r.testo[i] ? &r.testo[i] : " ", 1);
    scrivi_testo("\n");
    passo("result=ok", messaggio_vale(&r, CHIAVE_ESITO, ESITO_OK));
    long id;
    passo("la risposta ha l'id della richiesta (non 999999)",
          messaggio_numero(&r, CHIAVE_ID, &id) == 0 && id != 999999 && uguali(messaggio_valore(&q, CHIAVE_ID), messaggio_valore(&r, CHIAVE_ID)));
    const char *v1 = messaggio_prossimo(&r, "dns", 0);
    const char *v2 = messaggio_prossimo(&r, "dns", v1);
    const char *v3 = messaggio_prossimo(&r, "dns", v2);
    passo("dns ripetuto, nell'ordine", uguali(v1, "1.1.1.1") && uguali(v2, "9.9.9.9") && v3 == 0);
    long n;
    passo("count = -42", messaggio_numero(&r, "count", &n) == 0 && n == -42);
    passo("text con dentro un =", uguali(messaggio_valore(&r, "text"), "a=b c"));
    passo("il foglio allegato e' arrivato", uguali(messaggio_valore(&r, "attachment"), "ciao"));
    passo("chi chiede: uid 0", messaggio_vale(&r, "uid", "0"));
    passo("chi chiede: il programma", uguali(messaggio_valore(&r, "program"), "/data/tests/messaggi"));
    passo("permesso provvisorio: concesso", messaggio_vale(&r, "permission", "yes"));
}

static void tempo_massimo(int d) {
    struct messaggio q, r;
    messaggio_inizia(&q);
    messaggio_aggiungi(&q, CHIAVE_RICHIESTA, "slow");
    unsigned int prima = tempo();
    int e = chiedi_entro(d, &q, &r, 300);
    unsigned int passati = tempo() - prima;
    passo("slow: chiedi_entro 300 ms da' CHIEDI_SCADUTO", e == CHIEDI_SCADUTO);
    passo("slow: scaduto tra 250 e 700 ms", passati >= 250 && passati <= 700);
    passo("slow: result=timeout, con l'id della richiesta",
          messaggio_vale(&r, CHIAVE_ESITO, ESITO_SCADUTO) && uguali(messaggio_valore(&q, CHIAVE_ID), messaggio_valore(&r, CHIAVE_ID)));

    /* la risposta tardiva a slow arriva prima di quella di ping: chiedi deve buttarla */
    messaggio_inizia(&q);
    messaggio_aggiungi(&q, CHIAVE_RICHIESTA, "ping");
    passo("ping dopo il timeout: chiedi", chiedi(d, &q, &r) == 0);
    passo("ping: result=ok, pong, non la risposta tardiva",
          messaggio_vale(&r, CHIAVE_ESITO, ESITO_OK) && messaggio_vale(&r, "pong", "1") && !messaggio_valore(&r, "late") &&
          uguali(messaggio_valore(&q, CHIAVE_ID), messaggio_valore(&r, CHIAVE_ID)));
}

static void non_valido(int d) {
    const char *sbagliato = "Request=echo\nriga senza uguale\n";
    passo("pacchetto fuori standard mandato", manda(d, sbagliato, (unsigned int)lunghezza(sbagliato)) > 0);
    struct messaggio r;
    passo("risposta ricevuta", messaggio_ricevi(d, &r) == 1);
    passo("result=invalid, senza id", messaggio_vale(&r, CHIAVE_ESITO, ESITO_NON_VALIDO) && !messaggio_valore(&r, CHIAVE_ID));

    struct messaggio q;
    messaggio_inizia(&q);
    messaggio_aggiungi(&q, CHIAVE_RICHIESTA, "nope");
    passo("richiesta sconosciuta: chiedi", chiedi(d, &q, &r) == 0);
    passo("richiesta sconosciuta: result=invalid", messaggio_vale(&r, CHIAVE_ESITO, ESITO_NON_VALIDO));
}

int main(void) {
    int porta = ascolta(PORTA);
    if (porta < 0) {
        scrivi_testo("messaggi: non riesco ad aprire la porta\nmessaggi: fallita\n");
        return 1;
    }
    int figlio = fork();
    if (figlio == 0) {
        servizio(porta);
        _exit(0);
    }
    chiudi(porta);
    passo("servizio finto partito", figlio > 0);

    costruire();
    int d = collega(PORTA);
    passo("collegato alla porta", d >= 0);
    if (d >= 0) {
        echo(d);
        non_valido(d);
        tempo_massimo(d);
        chiudi(d);
    }
    if (figlio > 0) waitpid(figlio, 0, 0);

    scrivi_testo(fallite ? "messaggi: fallita\n" : "messaggi: tutto bene\n");
    return 0;
}
