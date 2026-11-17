/* La finestra "Apri/Salva" del servizio picker (picker_finestra.h; disegno: disegni/ApriSalva/).
 *
 *   +---------------------------------------------+
 *   |                  Apri / Salva           (x) |  la barra del titolo mini (29)
 *   |  [Salva] Nome  [ Senza titolo.txt        ]  |
 *   |  [<][>]  Home / Documenti                   |  la barra degli strumenti (32)
 *   |  +---------------------------------------+  |
 *   |  | Nome ^           Tipo       Dimensione|  |  il pannello (padding 8) con la tabella
 *   |  | [] Desktop       cartella   4 elementi|  |
 *   |  | ...                                   |  |
 *   |  +---------------------------------------+  |
 *   |                     [Annulla]  [Apri]       |  i pulsanti, a destra
 *   +---------------------------------------------+
 *
 * E' una mini finestra (finestra_tk_mini): il contenuto ha SPAZIO_16 intorno e SPAZIO_16 tra le
 * aree. Lo spazio lo da' solo lei (la colonna "contenuto"): la barra del nome, la barra degli
 * strumenti e la riga dei pulsanti non hanno padding; il pannello ha il suo (8, perche' le righe
 * della tabella hanno un fondo con 8 dentro). Quindi dal bordo della finestra al primo testo
 * della tabella: 16 + 8 + 8 = 32.
 *
 * Non apre mai un file e non guarda il disco: l'elenco delle cartelle e il "esiste gia'?"
 * vengono dalla picker_sorgente (il servizio). I percorsi sono relativi alla Home. */
#include <stddef.h>
#include "uikit/uikit.h"
#include "uikit/testo437.h"
#include "picker_finestra.h"

#define LARGHEZZA     480
#define ALTEZZA_APRI  421     /* 29 + 16 + 32 + 16 + 265 + 16 + 32 + 16 */
#define ALTEZZA_SALVA 469     /* + la barra del nome, 32 + 16 */
#define MAX_VOCI      256
#define MAX_PERC      1024
#define MAX_STORIA    32

/* --- la finestra e i suoi pezzi (tutto statico: una finestra alla volta) --- */

static struct finestra_tk finestra;
static struct contenitore contenuto, barra_nome, barra_pulsanti;
static struct etichetta etichetta_nome;
static struct campo campo;
static struct barra_strumenti strumenti;
static struct pillola frecce;
static struct pulsante_barra indietro, avanti;
static struct percorso dove;
static struct pannello_tk pannello;
static struct tabella_tk tabella;
static struct spazio vuoto;
static struct pulsante annulla, conferma;
static struct avviso sostituire;

/* --- lo stato --- */

static enum picker_modo modo;
static const struct picker_sorgente *sorgente;
static char qui[MAX_PERC];                   /* la cartella che si guarda, relativa alla Home */
static struct picker_voce voci[MAX_VOCI];
static int contenuti[MAX_VOCI];              /* per le cartelle: quante voci (-1: non letto, -2: non si sa) */
static int quante;
static int scelto = -1;                      /* la riga selezionata, o -1 */
static char storia[MAX_STORIA][MAX_PERC];    /* le cartelle viste, per indietro e avanti */
static int quante_storia, posto_storia = -1;
static int primo_nome;                       /* quanti nomi del percorso non si vedono, all'inizio */
static int fatto;                            /* 1: la persona ha scelto (e non annullato) */
static char scelta_nome[256];                /* la scelta: il nome del file */

/* --- piccole cose di testo --- */

static char *metti_testo(char *d, const char *s) {
    while (*s) *d++ = *s++;
    *d = 0;
    return d;
}

static char *metti_numero(char *d, unsigned long long n) {
    char cifre[24];
    int k = 0;
    do { cifre[k++] = (char)('0' + n % 10); n /= 10; } while (n);
    while (k) *d++ = cifre[--k];
    *d = 0;
    return d;
}

static int uguali(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

/* "512 byte", "2 KB", "3,2 MB" (come Files) */
static void scrivi_byte(char *d, unsigned long long byte) {
    if (byte < 1024) { metti_testo(metti_numero(d, byte), " byte"); return; }
    if (byte < 1024 * 1024) { metti_testo(metti_numero(d, (byte + 1023) / 1024), " KB"); return; }
    const char *unita = " MB";
    unsigned long long decimi = byte * 10 / (1024 * 1024);
    if (decimi >= 10240) { decimi = byte * 10 / (1024ULL * 1024 * 1024); unita = " GB"; }
    d = metti_numero(d, decimi / 10);
    *d++ = ',';
    d = metti_numero(d, decimi % 10);
    metti_testo(d, unita);
}

/* Che tipo di file: dall'estensione del nome (la colonna Tipo), come Files. */
static const char *const estensioni_immagini[] = { "png", "jpg", "jpeg", "gif", "bmp", "webp", 0 };
static const char *const estensioni_testi[] = {
    "txt", "md", "c", "h", "py", "sh", "json", "csv", "log", "ini", "conf", "xml", "html",
    "css", "js", 0
};

static char minuscola(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 'a' - 'A') : c; }

static int ha_estensione(const char *nome, const char *const *elenco) {
    const char *punto = 0;
    for (const char *c = nome; *c; c++) if (*c == '.') punto = c;
    if (!punto || punto == nome || !punto[1]) return 0;
    for (int i = 0; elenco[i]; i++) {
        const char *a = punto + 1, *b = elenco[i];
        while (*a && minuscola(*a) == *b) { a++; b++; }
        if (!*a && !*b) return 1;
    }
    return 0;
}

static const char *tipo_di(const struct picker_voce *v) {
    if (v->cartella) return "cartella";
    if (ha_estensione(v->nome, estensioni_immagini)) return "immagine";
    if (ha_estensione(v->nome, estensioni_testi)) return "testo";
    return "file";
}

/* --- i percorsi (relativi alla Home: "" e' la Home, "Documenti/Lavoro" una sottocartella) --- */

/* 'dentro' + "/" + 'nome', in 'out'. 0 se non ci sta. */
static int unisci(char *out, const char *cartella, const char *nome) {
    int n = 0;
    for (const char *c = cartella; *c; c++) { if (n >= MAX_PERC - 2) return 0; out[n++] = *c; }
    if (n) out[n++] = '/';
    for (const char *c = nome; *c; c++) { if (n >= MAX_PERC - 1) return 0; out[n++] = *c; }
    out[n] = 0;
    return 1;
}

/* I nomi del percorso 'qui' in 'copia' (separati da 0), con "Home" per primo: quanti sono. */
static int nomi_del_percorso(char *copia, const char **nomi, int massimo) {
    int n = 0;
    nomi[n++] = "Home";
    char *c = copia;
    for (const char *p = qui; *p && n < massimo; ) {
        nomi[n++] = c;
        while (*p && *p != '/') *c++ = *p++;
        *c++ = 0;
        if (*p == '/') p++;
    }
    return n;
}

static void aggiorna_percorso(void) {
    char copia[MAX_PERC + 1];
    const char *nomi[MAX_PERC / 2];
    int n = nomi_del_percorso(copia, nomi, MAX_PERC / 2);
    primo_nome = n > MAX_NOMI_PERCORSO ? n - MAX_NOMI_PERCORSO : 0;
    percorso_metti(&dove, nomi + primo_nome, n - primo_nome);
}

/* Un clic su un nome del percorso: si torna a quella cartella. */
static void vai(const char *nuova, int nella_storia);
static void clic_percorso(struct percorso *p, int indice, void *dati) {
    (void)p; (void)dati;
    char copia[MAX_PERC + 1], via[MAX_PERC];
    const char *nomi[MAX_PERC / 2];
    int n = nomi_del_percorso(copia, nomi, MAX_PERC / 2);
    indice += primo_nome;
    if (indice >= n - 1) return;
    via[0] = 0;
    for (int i = 1; i <= indice; i++) unisci(via, via, nomi[i]);
    vai(via, 1);
}

/* --- i pulsanti: quando si possono premere --- */

static void aggiorna_pulsanti(void) {
    int si = modo == PICKER_MODO_APRI ? scelto >= 0 && !voci[scelto].cartella
                                      : picker_nome_valido(campo.testo);
    widget_disabilita(&conferma.base, !si);
}

static void aggiorna_frecce(void) {
    widget_disabilita(&indietro.base, posto_storia <= 0);
    widget_disabilita(&avanti.base, posto_storia >= quante_storia - 1);
}

/* --- la tabella --- */

static const struct colonna_tk colonne[] = {
    { "Nome",       0,   0, 1, 0 },
    { "Tipo",       120, 0, 0, 0 },
    { "Dimensione", 100, 1, 0, 0 },
};

/* Quante voci ha una cartella: lo chiede alla sorgente, la prima volta che serve. */
static int quante_voci(int k) {
    if (contenuti[k] != -1) return contenuti[k];
    static struct picker_voce dentro[MAX_VOCI];
    char via[MAX_PERC];
    int n = -2;
    if (unisci(via, qui, voci[k].nome)) {
        n = sorgente->elenca(sorgente->ctx, via, dentro, MAX_VOCI);
        if (n < 0) n = -2;
    }
    return contenuti[k] = n;
}

static void dati_riga(struct tabella_tk *t, int k, struct riga_tk *r, void *d) {
    (void)t; (void)d;
    const struct picker_voce *v = &voci[k];
    in_437(r->testi[0], v->nome, MAX_CELLA);       /* i nomi sono UTF-8, i font del sistema 437 */
    copia_testo(r->testi[1], tipo_di(v), MAX_CELLA);
    if (!v->cartella) scrivi_byte(r->testi[2], (unsigned long long)v->byte);
    else {
        int n = quante_voci(k);
        if (n < 0) metti_testo(r->testi[2], "--");
        else if (n == 0) metti_testo(r->testi[2], "vuota");
        else metti_testo(metti_numero(r->testi[2], (unsigned long long)n), n == 1 ? " elemento" : " elementi");
    }
    r->oggetto = v->cartella ? OGGETTO_CARTELLA : oggetto_di_file(v->nome);
}

static void clic_tabella(struct tabella_tk *t, int k, int come, int x, int y, void *dati) {
    (void)x; (void)y; (void)dati;
    if (come == CLIC_DESTRO) return;
    if (k >= quante) k = -1;
    scelto = k;
    tabella_tk_scegli(t, k);
    aggiorna_pulsanti();
    if (come == CLIC_DOPPIO && k >= 0 && voci[k].cartella) {
        char via[MAX_PERC];
        if (unisci(via, qui, voci[k].nome)) vai(via, 1);
    }
}

/* --- andare in una cartella --- */

/* Va nella cartella 'nuova' (e se 'nella_storia', la mette nella storia). Se non si puo'
 * leggere, la finestra resta dov'era. */
static void vai(const char *nuova, int nella_storia) {
    int n = sorgente->elenca(sorgente->ctx, nuova, voci, MAX_VOCI);
    if (n < 0) return;
    copia_testo(qui, nuova, MAX_PERC);
    quante = n;
    for (int i = 0; i < quante; i++) contenuti[i] = voci[i].cartella ? -1 : 0;
    if (nella_storia && posto_storia >= 0 && uguali(storia[posto_storia], qui)) nella_storia = 0;
    if (nella_storia) {
        if (posto_storia == MAX_STORIA - 1) {            /* piena: si dimentica la piu' vecchia */
            for (int i = 1; i < MAX_STORIA; i++) copia_testo(storia[i - 1], storia[i], MAX_PERC);
            posto_storia--;
        }
        posto_storia++;
        copia_testo(storia[posto_storia], qui, MAX_PERC);
        quante_storia = posto_storia + 1;                 /* l'"avanti" di prima non vale piu' */
    }
    scelto = -1;
    tabella_tk_scegli(&tabella, -1);
    tabella.barra.scorrimento.posizione = 0;
    tabella_tk_aggiorna(&tabella, quante);
    aggiorna_percorso();
    aggiorna_frecce();
    aggiorna_pulsanti();
}

static void premi_indietro(struct pulsante_barra *b, void *dati) {
    (void)b; (void)dati;
    if (posto_storia <= 0) return;
    posto_storia--;
    vai(storia[posto_storia], 0);
}

static void premi_avanti(struct pulsante_barra *b, void *dati) {
    (void)b; (void)dati;
    if (posto_storia >= quante_storia - 1) return;
    posto_storia++;
    vai(storia[posto_storia], 0);
}

/* --- la scelta --- */

/* La persona ha scelto: il nome e' quello del campo (Salva) o del file selezionato (Apri). */
static void scegli(void) {
    copia_testo(scelta_nome, modo == PICKER_MODO_SALVA ? campo.testo : voci[scelto].nome,
                (int)sizeof scelta_nome);
    fatto = 1;
    finestra_tk_chiudi(&finestra);
}

static void risposta_avviso(struct avviso *a, int scelta, void *dati) {
    (void)a; (void)dati;
    if (scelta == AVVISO_CONFERMA) scegli();     /* Annulla: si torna alla finestra Salva */
}

static void premi_conferma(struct pulsante *b, void *dati) {
    (void)b; (void)dati;
    if (modo == PICKER_MODO_APRI) { scegli(); return; }
    if (!picker_nome_valido(campo.testo)) return;
    if (sorgente->esiste(sorgente->ctx, qui, campo.testo) == 1) {
        char messaggio[AVVISO_RIGHE * MAX_TESTO_WIDGET], *d = metti_testo(messaggio, "Il file ");
        d = metti_testo(d, campo.testo);
        in_437(d, " esiste gi\xc3\xa0. Vuoi sostituirlo?", (int)(sizeof messaggio - (size_t)(d - messaggio)));
        copia_testo(sostituire.messaggio, messaggio, (int)sizeof sostituire.messaggio);
        avviso_apri(&sostituire);
        return;
    }
    scegli();
}

static void premi_annulla(struct pulsante *b, void *dati) {
    (void)b; (void)dati;
    finestra_tk_chiudi(&finestra);
}

static void campo_cambiato(struct campo *c, void *dati) {
    (void)c; (void)dati;
    aggiorna_pulsanti();
}

/* Esc annulla (se c'e' l'avviso, Esc lo chiude lui: arriva prima di noi). */
static int tasto_fisico(struct finestra_tk *f, unsigned int codice, unsigned int modificatori,
                        void *dati) {
    (void)modificatori; (void)dati;
    if (codice != TASTO_ESC) return 0;
    finestra_tk_chiudi(f);
    return 1;
}

/* --- la costruzione --- */

/* Prepara tutta la finestra (da zero, a ogni richiesta) e la riempie con la Home: dopo, o la
 * si apre (picker_finestra) o la si disegna (l'anteprima: strumenti/anteprima/scene/picker_*.c). */
static void costruisci(enum picker_modo m, const char *proposto, const struct picker_sorgente *s) {
    modo = m;
    sorgente = s;
    qui[0] = 0;
    quante = 0;
    scelto = -1;
    quante_storia = 0;
    posto_storia = -1;
    fatto = 0;
    scelta_nome[0] = 0;

    /* la barra degli strumenti: indietro e avanti, e il percorso (prende il resto) */
    barra_strumenti_prepara(&strumenti);
    pillola_prepara(&frecce, 0);
    pulsante_barra_prepara(&indietro, ICONA_CHEVRON_LEFT, premi_indietro, 0);
    pulsante_barra_prepara(&avanti, ICONA_CHEVRON_RIGHT, premi_avanti, 0);
    pillola_aggiungi(&frecce, &indietro);
    pillola_aggiungi(&frecce, &avanti);
    widget_suggerimento(&indietro.base, "Indietro", 0);
    widget_suggerimento(&avanti.base, "Avanti", 0);
    percorso_prepara(&dove, clic_percorso, 0);
    barra_strumenti_aggiungi(&strumenti, &frecce.base);
    barra_strumenti_aggiungi(&strumenti, &dove.base);

    /* il pannello (padding 8) con la tabella */
    pannello_prepara(&pannello, SPAZIO_8);
    tabella_tk_prepara(&tabella, colonne, 3, dati_riga, clic_tabella, 0);
    tabella.ordinata = 0;                 /* la freccia sul Nome, come nel disegno: l'ordine lo da' la sorgente */
    tabella.verso = ORDINE_CRESCENTE;
    widget_aggiungi(&pannello.base, &tabella.base);

    /* i pulsanti, a destra */
    riga_prepara(&barra_pulsanti, SPAZIO_8);
    spazio_prepara(&vuoto);
    pulsante_prepara(&annulla, "Annulla", NESSUNA_ICONA, premi_annulla, 0);
    pulsante_stile(&annulla, SECONDARIO, MISURA_S);
    pulsante_prepara(&conferma, m == PICKER_MODO_APRI ? "Apri" : "Salva", NESSUNA_ICONA,
                     premi_conferma, 0);
    pulsante_stile(&conferma, PRIMARIO, MISURA_S);
    widget_aggiungi(&barra_pulsanti.base, &vuoto.base);
    widget_aggiungi(&barra_pulsanti.base, &annulla.base);
    widget_aggiungi(&barra_pulsanti.base, &conferma.base);

    /* il contenuto: padding 16 lo da' la mini finestra, 16 tra le aree questo contenitore */
    colonna_prepara(&contenuto, SPAZIO_16);
    if (m == PICKER_MODO_SALVA) {
        riga_prepara(&barra_nome, SPAZIO_8);
        etichetta_prepara(&etichetta_nome, "Nome", FONT_CORPO);
        etichetta_nome.secondaria = 1;
        campo_prepara(&campo, proposto ? proposto : "", "", NESSUNA_ICONA, campo_cambiato, 0);
        campo_stile(&campo, MISURA_S);
        campo.base.elastico = 1;
        widget_aggiungi(&barra_nome.base, &etichetta_nome.base);
        widget_aggiungi(&barra_nome.base, &campo.base);
        widget_aggiungi(&contenuto.base, &barra_nome.base);
    }
    widget_aggiungi(&contenuto.base, &strumenti.base);
    widget_aggiungi(&contenuto.base, &pannello.base);
    widget_aggiungi(&contenuto.base, &barra_pulsanti.base);

    finestra_tk_prepara(&finestra, m == PICKER_MODO_APRI ? "Apri" : "Salva", &contenuto.base);
    finestra_tk_mini(&finestra);
    finestra_tk_dimensione(&finestra, LARGHEZZA, m == PICKER_MODO_APRI ? ALTEZZA_APRI : ALTEZZA_SALVA);
    finestra_tk_ascolta_tasti(&finestra, tasto_fisico, 0);
    avviso_prepara(&sostituire, "Sostituire il file?", "", "Sostituisci", PERICOLO, 1,
                   risposta_avviso, 0);
    finestra_tk_avviso(&finestra, &sostituire);
    if (m == PICKER_MODO_SALVA) finestra_tk_dai_fuoco(&finestra, &campo.base);

    vai("", 1);
}

int picker_finestra(enum picker_modo m, const char *sopra, const char *proposto,
                    const struct picker_sorgente *s,
                    char *percorso, size_t n_percorso, char *nome, size_t n_nome) {
    costruisci(m, proposto, s);
    int r = uikit_esegui_scelta(&finestra, sopra);
    if (r != 0) return -1;                       /* non si e' potuta mostrare (o il compositore l'ha chiusa) */
    if (!fatto) return 1;                        /* Annulla, Esc, chiudi */
    copia_testo(percorso, qui, (int)n_percorso);
    copia_testo(nome, scelta_nome, (int)n_nome);
    return 0;
}
