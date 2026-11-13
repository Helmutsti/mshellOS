/* La libreria dei programmi: leggere il manifesto di un'applicazione (descrizione.h).
 *
 * Il manifesto e' TOML; lo legge tomlc99 (esterni/tomlc99: cktan/tomlc99, commit
 * 62ce93c, licenza MIT), che da' le voci come tabelle, elenchi e valori. Qui si copiano
 * nella struct manifesto, controllando tipo e lunghezza di ognuna: se una voce c'e' ma e'
 * del tipo sbagliato (un numero al posto di un testo) o troppo lunga, la si segna in
 * 'sbagliate', cosi' chi controlla il pacchetto (pacchetto.c) puo' spiegarlo.
 *
 * Usa solo la libreria C e le chiamate di Linux (open, read), non libreria.h: cosi' si
 * compila anche sul computer che compila, per le prove (test/pacchetti/). */
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "descrizione.h"
#include "esterni/tomlc99/toml.h"

const char *cartelle_applicazioni[QUANTE_CARTELLE_APPLICAZIONI] = { "/system/apps", "/data/apps" };

/* --- i controlli sui testi --- */

int id_valido(const char *id) {
    int n = 0;
    for (; id[n]; n++) {
        char c = id[n];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return 0;
    }
    return n > 0 && n < MAX_ID && id[0] != '-';
}

int versione_valida(const char *v) {
    int cifre = 0;
    for (; *v; v++) {
        if (*v >= '0' && *v <= '9') { if (++cifre > 9) return 0; }
        else if (*v == '.' && cifre) cifre = 0;
        else return 0;
    }
    return cifre > 0;
}

int confronta_versioni(const char *a, const char *b) {
    while (*a || *b) {
        long x = 0, y = 0;
        while (*a >= '0' && *a <= '9') x = x * 10 + (*a++ - '0');
        while (*b >= '0' && *b <= '9') y = y * 10 + (*b++ - '0');
        if (x != y) return x < y ? -1 : 1;
        if (*a == '.') a++;
        if (*b == '.') b++;
    }
    return 0;
}

int programma_valido(const char *nome) {
    int n = 0;
    for (; nome[n]; n++) {
        char c = nome[n];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
            return 0;
    }
    return n > 0 && n < MAX_PROGRAMMA && nome[0] != '.';
}

/* --- dal TOML alla struct --- */

/* Un testo: lo copia se c'e', e' un testo e ci sta; segna presenti e sbagliate. */
static void testo_in(const toml_table_t *t, const char *chiave, char *dove, int massimo,
                     unsigned int voce, struct manifesto *m) {
    if (!toml_key_exists(t, chiave)) return;
    m->presenti |= voce;
    toml_datum_t v = toml_string_in(t, chiave);
    if (!v.ok) { m->sbagliate |= voce; return; }
    if ((int)strlen(v.u.s) >= massimo) m->sbagliate |= voce;
    else strcpy(dove, v.u.s);
    free(v.u.s);
}

/* Un elenco di testi (permessi, deviazioni, funzioni). */
static void elenco_in(const toml_table_t *t, const char *chiave, char voci[][MAX_VOCE], int *quante,
                      unsigned int voce, struct manifesto *m) {
    if (!toml_key_exists(t, chiave)) return;
    m->presenti |= voce;
    toml_array_t *a = toml_array_in(t, chiave);
    if (!a) { m->sbagliate |= voce; return; }
    int n = toml_array_nelem(a);
    for (int i = 0; i < n; i++) {
        toml_datum_t v = toml_string_at(a, i);
        if (!v.ok) { m->sbagliate |= voce; continue; }
        if (*quante >= MAX_VOCI || (int)strlen(v.u.s) >= MAX_VOCE) m->sbagliate |= voce;
        else strcpy(voci[(*quante)++], v.u.s);
        free(v.u.s);
    }
}

/* Le icone di un tema: [icone.chiaro] o [icone.scuro]. */
static void tema_in(const toml_table_t *icone, const char *nome, struct icone_tema *t, struct manifesto *m) {
    if (!toml_key_exists(icone, nome)) return;
    const toml_table_t *tema = toml_table_in(icone, nome);
    if (!tema) { m->sbagliate |= VOCE_ICONE; return; }
    if (toml_key_exists(tema, "svg")) {
        toml_datum_t v = toml_string_in(tema, "svg");
        if (!v.ok || strlen(v.u.s) >= sizeof t->svg) m->sbagliate |= VOCE_ICONE;
        else strcpy(t->svg, v.u.s);
        if (v.ok) free(v.u.s);
    }
    if (!toml_key_exists(tema, "png")) return;
    const toml_table_t *png = toml_table_in(tema, "png");
    if (!png) { m->sbagliate |= VOCE_ICONE; return; }
    for (int i = 0; toml_key_in(png, i); i++) {
        const char *chiave = toml_key_in(png, i);
        char *fine;
        long misura = strtol(chiave, &fine, 10);
        toml_datum_t v = toml_string_in(png, chiave);
        if (*fine || misura <= 0 || misura > 1024 || !v.ok || strlen(v.u.s) >= MAX_PERCORSO_PACCHETTO ||
            t->quante_png >= MAX_PNG)
            m->sbagliate |= VOCE_ICONE;
        else {
            t->png_misura[t->quante_png] = (int)misura;
            strcpy(t->png[t->quante_png++], v.u.s);
        }
        if (v.ok) free(v.u.s);
    }
}

int manifesto_leggi(char *testo, struct manifesto *m, char *errore, int max_errore) {
    char scarto[8];
    memset(m, 0, sizeof *m);
    toml_table_t *t = toml_parse(testo, errore ? errore : scarto, errore ? max_errore : (int)sizeof scarto);
    if (!t) return -1;
    testo_in(t, "id", m->id, sizeof m->id, VOCE_ID, m);
    testo_in(t, "nome", m->nome, sizeof m->nome, VOCE_NOME, m);
    testo_in(t, "versione", m->versione, sizeof m->versione, VOCE_VERSIONE, m);
    testo_in(t, "autore", m->autore, sizeof m->autore, VOCE_AUTORE, m);
    testo_in(t, "descrizione", m->descrizione, sizeof m->descrizione, VOCE_DESCRIZIONE, m);
    testo_in(t, "processore", m->processore, sizeof m->processore, VOCE_PROCESSORE, m);
    testo_in(t, "programma", m->programma, sizeof m->programma, VOCE_PROGRAMMA, m);
    m->nella_barra = 1;
    if (toml_key_exists(t, "nella_barra")) {
        m->presenti |= VOCE_NELLA_BARRA;
        toml_datum_t v = toml_bool_in(t, "nella_barra");
        if (!v.ok) m->sbagliate |= VOCE_NELLA_BARRA;
        else m->nella_barra = v.u.b ? 1 : 0;
    }
    if (toml_key_exists(t, "sistema_minimo")) {
        m->presenti |= VOCE_SISTEMA_MINIMO;
        toml_datum_t v = toml_int_in(t, "sistema_minimo");
        if (!v.ok || v.u.i <= 0 || v.u.i > 1000000) m->sbagliate |= VOCE_SISTEMA_MINIMO;
        else m->sistema_minimo = (int)v.u.i;
    }
    elenco_in(t, "permessi", m->permessi, &m->quanti_permessi, VOCE_PERMESSI, m);
    elenco_in(t, "deviazioni", m->deviazioni, &m->quante_deviazioni, VOCE_DEVIAZIONI, m);
    elenco_in(t, "funzioni", m->funzioni, &m->quante_funzioni, VOCE_FUNZIONI, m);
    if (toml_key_exists(t, "icone")) {
        m->presenti |= VOCE_ICONE;
        const toml_table_t *icone = toml_table_in(t, "icone");
        if (!icone) m->sbagliate |= VOCE_ICONE;
        else {
            tema_in(icone, "chiaro", &m->icone[TEMA_CHIARO], m);
            tema_in(icone, "scuro", &m->icone[TEMA_SCURO], m);
        }
    }
    toml_free(t);
    return 0;
}

/* --- le app installate --- */

#define MAX_TESTO_MANIFESTO 16384

int manifesto_dell_app(const char *cartella_app, struct manifesto *m) {
    char percorso[MAX_PERCORSO_PROGRAMMA + 32];
    if (strlen(cartella_app) + 1 + sizeof NOME_MANIFESTO > sizeof percorso) return -1;
    strcpy(percorso, cartella_app);
    strcat(percorso, "/" NOME_MANIFESTO);
    int f = open(percorso, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (f < 0) return -1;
    char *testo = malloc(MAX_TESTO_MANIFESTO);
    int n = 0, letti = 0;
    if (testo)
        while (n < MAX_TESTO_MANIFESTO - 1 && (letti = (int)read(f, testo + n, (size_t)(MAX_TESTO_MANIFESTO - 1 - n))) > 0)
            n += letti;
    close(f);
    if (!testo || letti < 0 || n == 0) { free(testo); return -1; }
    testo[n] = 0;
    int esito = manifesto_leggi(testo, m, 0, 0);
    free(testo);
    return esito;
}

int leggi_descrizione(const char *app, struct descrizione *d) {
    memset(d, 0, sizeof *d);
    if (!id_valido(app)) return -1;       /* anche ".", "..", i nomi con "/" e le cartelle nascoste */
    struct manifesto *m = malloc(sizeof *m);
    if (!m) return -1;
    int esito = -1;
    for (int i = 0; i < QUANTE_CARTELLE_APPLICAZIONI && esito < 0; i++) {
        char cartella[MAX_PERCORSO_PROGRAMMA];
        if (strlen(cartelle_applicazioni[i]) + 1 + strlen(app) >= sizeof cartella) continue;
        strcpy(cartella, cartelle_applicazioni[i]);
        strcat(cartella, "/");
        strcat(cartella, app);
        if (manifesto_dell_app(cartella, m) < 0 || strcmp(m->id, app) || !programma_valido(m->programma) ||
            strlen(cartella) + 1 + strlen(m->programma) >= sizeof d->programma)
            continue;
        strcpy(d->id, m->id);
        strcpy(d->nome, m->nome[0] ? m->nome : m->id);
        strcpy(d->programma, cartella);
        strcat(d->programma, "/");
        strcat(d->programma, m->programma);
        d->di_sistema = i == CARTELLA_APP_SISTEMA;
        esito = 0;
    }
    free(m);
    return esito;
}
