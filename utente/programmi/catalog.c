/* catalog: il servizio delle impostazioni della persona (SISTEMA.md, sezione 1). Oggi il tema
 * (chiaro o scuro) e lo sfondo. Il contratto (richieste, chiavi, permessi) e' in
 * corekit/catalog.h; le funzioni per chi chiede in corekit/interno/catalog.c.
 *
 * E' l'unico che legge e scrive i file delle preferenze della persona:
 *     /data/users/1000/preferences/tema     "chiaro" o "scuro" (nei messaggi: light, dark)
 *     /data/users/1000/preferences/sfondo   il nome del file dello sfondo, o vuoto
 * (gli stessi di oggi: uikit/interno/tema.c e sfondo.c, che per ora li leggono ancora da soli).
 * Non tiene i valori in memoria: li rilegge a ogni richiesta, cosi' resta giusto anche se
 * qualcuno scrive ancora il file da se' (oggi Impostazioni). Scrive in un file nuovo e poi lo
 * mette al posto del vecchio (rinomina): chi legge trova sempre il file intero.
 *
 * Gira come la persona (uid 1000, lo fa partire cosi' init), non come sistema: tocca solo i suoi
 * file, e i file che crea sono di lei. Il nome "catalog" glielo presta init al descrittore 0.
 *
 * Tiene aperti fino a MAX_CLIENTI collegamenti insieme: chi ha chiesto watch resta collegato e
 * riceve gli avvisi. Gli avvisi partono senza aspettare (messaggio_manda_subito): se la casella
 * di uno e' piena, lo si scollega. */
#include "corekit/corekit.h"
#include "corekit/messaggi.h"
#include "corekit/catalog.h"
#include "corekit/sistema/servizi.h"     /* chi_chiede, ha_permesso */
#include <string.h>

#define D_ASCOLTO 0       /* prestato da init */
#define MAX_CLIENTI 32
#define PERMESSO_CAMBIA "settings.system"

#define CARTELLA   "/data/users/1000/preferences"
#define FILE_TEMA   CARTELLA "/tema"
#define FILE_SFONDO CARTELLA "/sfondo"

static struct {
    int d;                  /* -1: posto libero */
    int osserva;            /* ha chiesto watch: riceve gli avvisi */
    struct chi_chiede chi;  /* chi c'e' dall'altra parte (lo dice il kernel all'arrivo) */
} clienti[MAX_CLIENTI];

/* LE PORTE DELLE APP (messaggi.h e sistema/servizi.h, "le porte delle app"): il capo che il
 * launcher ci ha dato con attach, e chi e' l'app (dal launcher). Ogni connect sulla porta diventa
 * un cliente con quell'identita'. Una porta si chiude quando l'app la chiude (o finisce). */
#define MAX_PORTE MAX_CLIENTI
static struct {
    int d;                  /* -1: posto libero */
    int usata;              /* e' gia' arrivato un connect (per scriverlo nel registro una volta) */
    struct chi_chiede chi;
} porte[MAX_PORTE];

static struct messaggio richiesta, risposta;

static void registro(const char *a, const char *b) { scrivi_riga("catalog", a, b); }

/* --- i file --- */

/* Il contenuto del file (al massimo 'massimo' - 1 byte), fino al primo a capo. "" se non c'e'. */
static void leggi_file(const char *percorso, char *dove, int massimo) {
    dove[0] = 0;
    int d = apri(percorso, APRI_LEGGERE);
    if (d < 0) return;
    int n = leggi(d, dove, (unsigned int)massimo - 1);
    chiudi(d);
    if (n < 0) n = 0;
    dove[n] = 0;
    for (int i = 0; i < n; i++) if (dove[i] == '\n') dove[i] = 0;
}

/* Scrive 'testo' nel file: prima in <percorso>.nuovo, poi al posto del vecchio. 0, o -1. */
static int scrivi_file(const char *percorso, const char *testo) {
    char nuovo[MAX_PERCORSO];
    if (strlen(percorso) + 7 >= sizeof nuovo) return -1;
    strcpy(nuovo, percorso);
    strcat(nuovo, ".nuovo");
    int d = apri(nuovo, APRI_SCRIVERE | APRI_CREARE | APRI_SVUOTARE);
    if (d < 0) return -1;
    int l = (int)strlen(testo);
    int scritti = l ? scrivi_in(d, testo, (unsigned int)l) : 0;
    chiudi(d);
    if (scritti != l || rinomina(nuovo, percorso) < 0) return -1;
    return 0;
}

/* Un nome di sfondo valido (catalog.h): vuoto, o niente "/", non comincia con ".", corto. */
static int sfondo_valido(const char *s) {
    if (strlen(s) >= CATALOG_MAX_SFONDO) return 0;
    if (s[0] == '.') return 0;
    return strchr(s, '/') == 0;
}

/* Le impostazioni di ora, dai file. */
static void valori(struct catalog_valori *v) {
    char tema[16];
    leggi_file(FILE_TEMA, tema, sizeof tema);
    strcpy(v->tema, strcmp(tema, "scuro") ? CATALOG_TEMA_CHIARO : CATALOG_TEMA_SCURO);
    leggi_file(FILE_SFONDO, v->sfondo, CATALOG_MAX_SFONDO);
    if (!sfondo_valido(v->sfondo)) v->sfondo[0] = 0;
}

static void aggiungi_valori(struct messaggio *m, const struct catalog_valori *v) {
    messaggio_aggiungi(m, CATALOG_CHIAVE_TEMA, v->tema);
    messaggio_aggiungi(m, CATALOG_CHIAVE_SFONDO, v->sfondo);
}

/* --- gli avvisi --- */

static void scollega(int k) {
    chiudi(clienti[k].d);
    clienti[k].d = -1;
    clienti[k].osserva = 0;
}

/* A tutti quelli che osservano: notice=changed con i valori di ora. */
static void avvisa(const struct catalog_valori *v) {
    static struct messaggio avviso;
    messaggio_inizia(&avviso);
    messaggio_aggiungi(&avviso, CATALOG_CHIAVE_AVVISO, CATALOG_AVVISO_CAMBIATE);
    aggiungi_valori(&avviso, v);
    for (int k = 0; k < MAX_CLIENTI; k++)
        if (clienti[k].d >= 0 && clienti[k].osserva && messaggio_manda_subito(clienti[k].d, &avviso) < 0)
            scollega(k);   /* casella piena o chiuso: non si aspetta nessuno */
}

/* --- le richieste --- */

/* set chiede settings.system (catalog.h): per un'app vale quello che ha detto il launcher. */
static int puo_cambiare(const struct chi_chiede *c) { return ha_permesso(c, PERMESSO_CAMBIA); }

/* set: risponde in 'risposta'; se qualcosa e' cambiato davvero, avvisa. */
static void cambia(int k) {
    const char *tema = messaggio_valore(&richiesta, CATALOG_CHIAVE_TEMA);
    const char *sfondo = messaggio_valore(&richiesta, CATALOG_CHIAVE_SFONDO);
    if ((!tema && !sfondo) ||
        (tema && strcmp(tema, CATALOG_TEMA_CHIARO) && strcmp(tema, CATALOG_TEMA_SCURO)) ||
        (sfondo && !sfondo_valido(sfondo))) {
        messaggio_risposta(&risposta, &richiesta, ESITO_NON_VALIDO);
        return;
    }
    if (!puo_cambiare(&clienti[k].chi)) {
        messaggio_risposta(&risposta, &richiesta, ESITO_NEGATO);
        return;
    }
    struct catalog_valori prima, dopo;
    valori(&prima);
    int fatto = 1;
    if (tema && strcmp(tema, prima.tema))
        fatto = scrivi_file(FILE_TEMA, strcmp(tema, CATALOG_TEMA_SCURO) ? "chiaro" : "scuro") == 0;
    if (fatto && sfondo && strcmp(sfondo, prima.sfondo)) fatto = scrivi_file(FILE_SFONDO, sfondo) == 0;
    valori(&dopo);
    if (!fatto) {
        registro("non riesco a scrivere le preferenze in ", CARTELLA);
        messaggio_risposta(&risposta, &richiesta, ESITO_ERRORE);
    } else {
        messaggio_risposta(&risposta, &richiesta, ESITO_OK);
        aggiungi_valori(&risposta, &dopo);
    }
    if (strcmp(prima.tema, dopo.tema) || strcmp(prima.sfondo, dopo.sfondo)) avvisa(&dopo);
}

/* attach dal cliente k: solo dal sistema (il launcher), con il capo e il programma dell'app. */
static void attacca(int k) {
    int capo = richiesta.allegato;
    richiesta.allegato = -1;
    int p = 0;
    while (p < MAX_PORTE && porte[p].d >= 0) p++;
    if (clienti[k].chi.uid != UID_SISTEMA) {
        messaggio_risposta(&risposta, &richiesta, ESITO_NEGATO);
    } else if (capo < 0 || p == MAX_PORTE || app_da_attach(&richiesta, &porte[p].chi) < 0) {
        messaggio_risposta(&risposta, &richiesta, ESITO_NON_VALIDO);
    } else {
        porte[p].d = capo;
        porte[p].usata = 0;
        capo = -1;
        messaggio_risposta(&risposta, &richiesta, ESITO_OK);
    }
    if (capo >= 0) chiudi(capo);
}

/* Un collegamento nuovo 'd', di chi e' 'c'. */
static void nuovo_collegamento(int d, const struct chi_chiede *c) {
    int k = 0;
    while (k < MAX_CLIENTI && clienti[k].d >= 0) k++;
    if (k == MAX_CLIENTI) { chiudi(d); return; }   /* troppi */
    clienti[k].d = d;
    clienti[k].osserva = 0;
    clienti[k].chi = *c;
}

/* Un messaggio sulla porta p: connect con il capo allegato diventa un cliente dell'app della
 * porta. L'app chiusa (o un messaggio sbagliato): la porta si chiude. */
static void da_porta(int p) {
    static struct messaggio m;
    int n = messaggio_ricevi(porte[p].d, &m);
    if (n == 1 && m.allegato >= 0 && messaggio_vale(&m, CHIAVE_RICHIESTA, RICHIESTA_COLLEGA)) {
        if (!porte[p].usata) registro("collegamento dalla porta di ", porte[p].chi.programma);
        porte[p].usata = 1;
        nuovo_collegamento(m.allegato, &porte[p].chi);
        return;
    }
    if (n == 1 && m.allegato >= 0) chiudi(m.allegato);
    chiudi(porte[p].d);
    porte[p].d = -1;
}

/* Un messaggio dal cliente k. */
static void da_cliente(int k) {
    int n = messaggio_ricevi(clienti[k].d, &richiesta);
    if (n == 0 || n == -1) { scollega(k); return; }   /* ha chiuso */
    if (n == MESSAGGIO_NON_VALIDO) {
        messaggio_risposta(&risposta, 0, ESITO_NON_VALIDO);
    } else if (messaggio_vale(&richiesta, CHIAVE_RICHIESTA, RICHIESTA_ATTACCA)) {
        attacca(k);
    } else {
        if (richiesta.allegato >= 0) { chiudi(richiesta.allegato); richiesta.allegato = -1; }
        struct catalog_valori v;
        if (messaggio_vale(&richiesta, CHIAVE_RICHIESTA, CATALOG_RICHIESTA_LEGGI) ||
            messaggio_vale(&richiesta, CHIAVE_RICHIESTA, CATALOG_RICHIESTA_OSSERVA)) {
            if (messaggio_vale(&richiesta, CHIAVE_RICHIESTA, CATALOG_RICHIESTA_OSSERVA)) clienti[k].osserva = 1;
            valori(&v);
            messaggio_risposta(&risposta, &richiesta, ESITO_OK);
            aggiungi_valori(&risposta, &v);
        } else if (messaggio_vale(&richiesta, CHIAVE_RICHIESTA, CATALOG_RICHIESTA_CAMBIA)) {
            cambia(k);
        } else {
            messaggio_risposta(&risposta, &richiesta, ESITO_NON_VALIDO);
        }
    }
    if (clienti[k].d >= 0 && messaggio_manda(clienti[k].d, &risposta) < 0) scollega(k);
}

/* Un collegamento per nome: chi e' lo dice il kernel. */
static void nuovo_cliente(void) {
    int d = accetta(D_ASCOLTO);
    if (d < 0) return;
    struct chi_chiede c;
    if (chi_chiede(d, &c) < 0) { chiudi(d); return; }   /* sconosciuto */
    nuovo_collegamento(d, &c);
}

int main(void) {
    for (int k = 0; k < MAX_CLIENTI; k++) clienti[k].d = -1;
    for (int p = 0; p < MAX_PORTE; p++) porte[p].d = -1;
    for (;;) {
        struct da_guardare g[1 + MAX_CLIENTI + MAX_PORTE];
        int chi[1 + MAX_CLIENTI + MAX_PORTE];   /* un cliente (>= 0), una porta (-2 - p), o -1 */
        int n = 0;
        g[n].descrittore = D_ASCOLTO;
        chi[n++] = -1;
        for (int k = 0; k < MAX_CLIENTI; k++)
            if (clienti[k].d >= 0) { g[n].descrittore = clienti[k].d; chi[n++] = k; }
        for (int p = 0; p < MAX_PORTE; p++)
            if (porte[p].d >= 0) { g[n].descrittore = porte[p].d; chi[n++] = -2 - p; }
        if (aspetta(g, n, -1) < 0) {
            registro("non riesco ad aspettare: ", "mi fermo (init mi fara' ripartire)");
            return 1;
        }
        /* prima i messaggi, poi i collegamenti nuovi (un nuovo potrebbe riusare un numero) */
        for (int i = 1; i < n; i++)
            if (g[i].pronto && chi[i] >= 0 && clienti[chi[i]].d == g[i].descrittore) da_cliente(chi[i]);
        for (int i = 1; i < n; i++)
            if (g[i].pronto && chi[i] <= -2 && porte[-2 - chi[i]].d == g[i].descrittore) da_porta(-2 - chi[i]);
        if (g[0].pronto) nuovo_cliente();
    }
}
