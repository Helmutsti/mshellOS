/* Attivita': il task manager. Nella barra degli strumenti tre schede, per tre pagine:
 *   - Processi: i programmi che girano (numero, nome, utente, stato, memoria), aggiornati
 *     ogni secondo. Se ne sceglie uno con un clic e lo si puo' terminare con la X a
 *     destra nella barra. Sotto la tabella, quanti sono e quanta memoria usano.
 *   - Memoria: com'e' usata la RAM, come la pagina "Archiviazione" del Mac: una barra
 *     divisa in pezzi colorati, e sotto la legenda con quanto pesa ogni pezzo.
 *   - Disco: lo stesso per i file: quanto pesano il sistema, le applicazioni, gli utenti,
 *     e quanto e' libero.
 *
 * Tutto quello che mostra lo chiede alla libreria (elenco_processi, stato_memoria,
 * stato_disco, le cartelle), che lo legge da Linux (/proc, statvfs). Terminare segue la
 * regola di Unix: Attivita' gira come utente "applicazioni", quindi puo' terminare solo
 * le applicazioni; i servizi del sistema no. */
#include "toolkit.h"

#define MAX_PROCESSI 64

/* --- i numeri in testo --- */

static char *metti_numero(char *d, unsigned int n) {
    char cifre[12];
    int k = 0;
    do { cifre[k++] = '0' + n % 10; n /= 10; } while (n);
    while (k) *d++ = cifre[--k];
    *d = 0;
    return d;
}

static char *metti_testo(char *d, const char *s) {
    while (*s) *d++ = *s++;
    *d = 0;
    return d;
}

/* "512 KB", "23,4 MB" */
static char *metti_byte(char *d, unsigned long long byte) {
    unsigned int kb = (unsigned int)(byte / 1024);
    if (kb < 1024) return metti_testo(metti_numero(d, kb), " KB");
    unsigned int decimi = (unsigned int)((unsigned long long)kb * 10 / 1024);
    d = metti_numero(d, decimi / 10);
    *d++ = ',';
    d = metti_numero(d, decimi % 10);
    return metti_testo(d, " MB");
}

static const char *nome_utente(unsigned int uid) {
    return uid == UID_SISTEMA ? "sistema" : uid == UID_APPLICAZIONI ? "applicazioni" : "?";
}

static const char *nome_stato(unsigned int stato) {
    switch (stato) {
    case STATO_PRONTO:        return "pronto";
    case STATO_IN_ESECUZIONE: return "in esecuzione";
    case STATO_DORME:         return "dorme";
    case STATO_IN_ATTESA:     return "in attesa";
    default:                  return "finito";
    }
}

/* --- la tabella dei processi: un widget fatto qui, per questa applicazione ---
 *
 * Le righe che non ci stanno si vedono SCORRENDO: con la rotella sopra la tabella, o
 * trascinando la barra di scorrimento del toolkit, che sta sopra le righe, sul bordo
 * destro. La testata resta ferma; le righe scorrono sotto di lei, pixel per pixel. */

#define TABELLA_TESTATA 34
#define TABELLA_RIGA    30
#define TABELLA_MARGINE 14

struct tabella {
    struct widget base;
    struct info_processo righe[MAX_PROCESSI];
    int quante;
    int scelto;                        /* il numero del processo scelto, o -1 */
    void (*al_cambio)(struct tabella *t);
    struct barra_scorrimento barra;    /* sua figlia: sa anche dove si e' scorso */
};

/* Le righe stanno sotto la testata, a partire da 2 pixel, e si vedono fino a 4 pixel dal
 * fondo (il bordo arrotondato). */
#define RIGHE_SOPRA (TABELLA_TESTATA + 2)

static int tabella_vista(const struct widget *w) { return w->a - TABELLA_TESTATA - 4; }

/* Quanto sono alte tutte le righe, e quanto se ne vede: lo deve sapere lo scorrimento. */
static void tabella_misure(struct tabella *t) {
    scorrimento_misure(&t->barra.scorrimento, 2 + t->quante * TABELLA_RIGA, tabella_vista(&t->base));
}

/* dove comincia ogni colonna (la memoria e' allineata a destra) */
static int colonna_x(const struct widget *w, int i) {
    static const int posti[] = { 0, 70, 250, 380 };
    return w->x + TABELLA_MARGINE + posti[i];
}

static void tabella_misura(struct widget *w, int *l, int *a) {
    (void)w;
    *l = 560;
    *a = TABELLA_TESTATA + 4 * TABELLA_RIGA;
}

static void tabella_disegna(struct widget *w, struct pennello *p) {
    struct tabella *t = (struct tabella *)w;
    struct tondo fondo = tondo(w->x, w->y, w->l, w->a, RAGGIO_MEDIO);
    pennello_colore(p, CAMPO_SFONDO);
    riempi_forma(p, &fondo);
    pennello_colore(p, CAMPO_BORDO);
    contorna_forma(p, &fondo, 256);

    /* la testata */
    static const char *titoli[] = { "Numero", "Nome", "Utente", "Stato" };
    const struct font *piccolo = &inter_medio_13;
    pennello_colore(p, CONTENUTO_SECONDARIO);
    for (int i = 0; i < 4; i++)
        testo_centrato(p, piccolo, colonna_x(w, i), w->y, TABELLA_TESTATA, titoli[i]);
    int destra = w->x + w->l - TABELLA_MARGINE;
    testo_centrato(p, piccolo, destra - larghezza_testo(piccolo, "Memoria"), w->y,
                   TABELLA_TESTATA, "Memoria");
    pennello_colore(p, CAMPO_BORDO);
    riempi_rettangolo(p, w->x + 1, w->y + TABELLA_TESTATA - 1, w->l - 2, 1);

    /* le righe: spostate in su di quanto si e' scorso, e ritagliate nella vista */
    pennello_salva(p);
    pennello_ritaglia(p, w->x, w->y + TABELLA_TESTATA, w->l, tabella_vista(w));
    int scorso = t->barra.scorrimento.posizione;
    for (int r = 0; r < t->quante; r++) {
        const struct info_processo *pr = &t->righe[r];
        int y = w->y + RIGHE_SOPRA + r * TABELLA_RIGA - scorso;
        if (y + TABELLA_RIGA <= w->y + TABELLA_TESTATA || y >= w->y + w->a) continue;   /* fuori */
        if (pr->numero == t->scelto) {
            pennello_colore(p, ELEMENTO_SCELTO);     /* vetro chiaro, non blu */
            riempi_tondo(p, w->x + 4, y, w->l - 8, TABELLA_RIGA - 2, RAGGIO_PICCOLO);
        }
        char numero[12], memoria[16];
        metti_numero(numero, pr->numero);
        metti_byte(memoria, pr->memoria);
        int ty = y - 1;
        pennello_colore(p, CONTENUTO_PRINCIPALE);
        testo_centrato(p, FONT_TESTO, colonna_x(w, 0), ty, TABELLA_RIGA, numero);
        testo_centrato(p, FONT_TESTO, colonna_x(w, 1), ty, TABELLA_RIGA, pr->nome);
        testo_centrato(p, FONT_TESTO, colonna_x(w, 2), ty, TABELLA_RIGA, nome_utente(pr->uid));
        testo_centrato(p, FONT_TESTO, colonna_x(w, 3), ty, TABELLA_RIGA, nome_stato(pr->stato));
        testo_centrato(p, FONT_TESTO, destra - larghezza_testo(FONT_TESTO, memoria), ty,
                       TABELLA_RIGA, memoria);
    }
    pennello_ripristina(p);
}

/* Un clic su una riga la sceglie. La riga e' quella che si vede li': si conta anche
 * quanto si e' scorso. */
static void tabella_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    struct tabella *t = (struct tabella *)w;
    (void)x;
    if (!(pulsanti & PULSANTE_SINISTRO)) return;
    if (y < w->y + TABELLA_TESTATA || y >= w->y + TABELLA_TESTATA + tabella_vista(w)) return;
    int nel_contenuto = y - w->y - RIGHE_SOPRA + t->barra.scorrimento.posizione;
    if (nel_contenuto < 0) return;
    int r = nel_contenuto / TABELLA_RIGA;
    if (r >= t->quante) return;
    if (t->righe[r].numero == t->scelto) return;
    t->scelto = t->righe[r].numero;
    widget_cambiato(w);
    if (t->al_cambio) t->al_cambio(t);
}

/* La barra di scorrimento sta sopra le righe (sotto la testata, fino al fondo). */
static void tabella_disponi(struct widget *w) {
    struct tabella *t = (struct tabella *)w;
    barra_scorrimento_posto(&t->barra, w->x, w->y + TABELLA_TESTATA, w->l, w->a - TABELLA_TESTATA);
    tabella_misure(t);
}

static void tabella_rotella(struct widget *w, int sx, int sy) {
    (void)sx;
    barra_scorrimento_rotella(&((struct tabella *)w)->barra, sy);
}

static const struct tipo_widget tipo_tabella = {
    tabella_misura, tabella_disponi, tabella_disegna, tabella_puntatore, 0, tabella_rotella, 0, 0
};

/* --- i grafici: la barra a pezzi e la legenda, per la memoria e per il disco ---
 *
 * Un grafico e' un elenco di pezzi (nome, colore, quanti byte). La barra e la legenda
 * sono due widget che leggono lo stesso grafico: la pagina Memoria e la pagina Disco ne
 * hanno uno ciascuna, con gli stessi widget. */

#define MAX_PEZZI 6
#define NESSUNO   (-1)

struct grafico {
    int quanti;
    const char *const *nomi;
    const unsigned int *colori;
    unsigned long long byte[MAX_PEZZI];
    /* la barra intera, in byte; 0: la somma dei pezzi (la barra e' piena di loro) */
    unsigned long long totale;
    /* il pezzo che e' "il resto" (la memoria libera): non si disegna sopra la barra, ne
     * e' il fondo; nella legenda il suo pallino ha il bordo. NESSUNO se non c'e'. */
    int resto;
};

struct barra { struct widget base; struct grafico *grafico; };

static void barra_misura(struct widget *w, int *l, int *a) {
    (void)w;
    *l = 400;
    *a = 28;
}

/* Ogni pezzo e' la barra intera ritagliata nella sua striscia: cosi' gli angoli tondi
 * restano solo ai due capi. La striscia di un pezzo va da dove finisce la somma dei
 * pezzi prima di lui a dove finisce la somma con lui, in proporzione al totale. */
static void barra_disegna(struct widget *w, struct pennello *p) {
    const struct grafico *g = ((struct barra *)w)->grafico;
    struct tondo t = tondo(w->x, w->y, w->l, w->a, RAGGIO_MEDIO);
    pennello_colore(p, g->resto != NESSUNO ? g->colori[g->resto] : VETRO_SUPERFICIE);
    riempi_forma(p, &t);
    unsigned long long totale = g->totale;
    if (!totale)
        for (int i = 0; i < g->quanti; i++) totale += g->byte[i];
    if (totale) {
        int x = w->x;
        unsigned long long somma = 0;
        for (int i = 0; i < g->quanti; i++) {
            if (i == g->resto) continue;
            somma += g->byte[i];
            int fine = w->x + (int)(somma * (unsigned int)w->l / totale);
            if (fine <= x) continue;
            pennello_salva(p);
            pennello_ritaglia(p, x, w->y, fine - x, w->a);
            pennello_colore(p, g->colori[i]);
            riempi_forma(p, &t);
            pennello_ripristina(p);
            x = fine;
        }
    }
    pennello_colore(p, CAMPO_BORDO);
    contorna_forma(p, &t, 256);
}

static const struct tipo_widget tipo_barra = { barra_misura, 0, barra_disegna, 0, 0, 0, 0, 0 };

#define LEGENDA_RIGA 30

struct legenda { struct widget base; struct grafico *grafico; };

static void legenda_misura(struct widget *w, int *l, int *a) {
    *l = 400;
    *a = ((struct legenda *)w)->grafico->quanti * LEGENDA_RIGA;
}

static void legenda_disegna(struct widget *w, struct pennello *p) {
    const struct grafico *g = ((struct legenda *)w)->grafico;
    for (int i = 0; i < g->quanti; i++) {
        int y = w->y + i * LEGENDA_RIGA;
        pennello_colore(p, g->colori[i]);
        riempi_tondo(p, w->x, y + (LEGENDA_RIGA - 12) / 2, 12, 12, 6);
        if (i == g->resto) {
            pennello_colore(p, CONTROLLO_BORDO);
            contorna_tondo(p, w->x, y + (LEGENDA_RIGA - 12) / 2, 12, 12, 6, 256);
        }
        char quanto[16];
        metti_byte(quanto, g->byte[i]);
        pennello_colore(p, CONTENUTO_PRINCIPALE);
        testo_centrato(p, FONT_TESTO, w->x + 24, y, LEGENDA_RIGA, g->nomi[i]);
        pennello_colore(p, CONTENUTO_SECONDARIO);
        testo_centrato(p, FONT_TESTO, w->x + w->l - larghezza_testo(FONT_TESTO, quanto), y,
                       LEGENDA_RIGA, quanto);
    }
}

static const struct tipo_widget tipo_legenda = { legenda_misura, 0, legenda_disegna, 0, 0, 0, 0, 0 };

/* Una pagina con un grafico: il riassunto in alto (un titolo), la barra, la legenda. */
struct pagina_grafico {
    struct contenitore colonna;
    struct etichetta riassunto;
    struct barra barra;
    struct legenda legenda;
    struct grafico grafico;
};

static void pagina_grafico_prepara(struct pagina_grafico *pg, int quanti, const char *const *nomi,
                                   const unsigned int *colori, int resto) {
    colonna_prepara(&pg->colonna, SPAZIO_TRA + 4);
    pg->colonna.base.elastico = 1;
    pg->colonna.base.nascosto = 1;
    pg->grafico = (struct grafico){ .quanti = quanti, .nomi = nomi, .colori = colori, .resto = resto };
    etichetta_prepara(&pg->riassunto, "", FONT_TITOLO);
    pg->riassunto.base.largo = 1;
    pg->barra = (struct barra){ .base = { .tipo = &tipo_barra, .largo = 1 }, .grafico = &pg->grafico };
    pg->legenda = (struct legenda){ .base = { .tipo = &tipo_legenda, .largo = 1 },
                                    .grafico = &pg->grafico };
    widget_aggiungi(&pg->colonna.base, &pg->riassunto.base);
    widget_aggiungi(&pg->colonna.base, &pg->barra.base);
    widget_aggiungi(&pg->colonna.base, &pg->legenda.base);
}

/* I numeri sono cambiati: si ridisegnano barra e legenda. */
static void pagina_grafico_cambiata(struct pagina_grafico *pg) {
    widget_cambiato(&pg->barra.base);
    widget_cambiato(&pg->legenda.base);
}

/* La memoria: il kernel, i servizi del sistema, le applicazioni, la parte condivisa, e
 * la libera (il resto della barra). */
enum { KERNEL, SISTEMA, APPLICAZIONI, CONDIVISA, LIBERA, PEZZI_MEMORIA };
static const char *const nomi_memoria[PEZZI_MEMORIA] = {
    "Kernel", "Servizi del sistema", "Applicazioni", "Memoria condivisa (finestre)", "Libera"
};
static const unsigned int colori_memoria[PEZZI_MEMORIA] = {
    OPACO(0x94A3B8), BLU_NORMALE, VERDE_SOPRA, ARANCIO_SOPRA, VETRO_SUPERFICIE
};

/* Il disco: le due partizioni con i file, system ("/", in sola lettura) e data (/data),
 * viste come una cosa sola. Il TOTALE e il LIBERO li dice Linux (stato_disco, cioe'
 * statvfs, per tutte e due); quanto pesano il sistema, le applicazioni e gli utenti si
 * conta camminando nelle loro cartelle:
 *   - Sistema: tutto /system, tranne le applicazioni;
 *   - Applicazioni: /system/apps (quelle del sistema) e /data/apps (quelle installate);
 *   - Utenti: /data/users (le case, i dati delle applicazioni, le preferenze);
 *   - Altro: il resto di quello che e' in uso. Sono le tabelle e il giornale di ext4 (il
 *     posto che il file system usa per se'), e i file che Attivita' non ha il permesso
 *     di vedere (per esempio quelli dei servizi, in /data/services). Come "Kernel" per la
 *     memoria;
 *   - Libero: il fondo della barra (come per la memoria). */
enum { DISCO_SISTEMA, DISCO_APPLICAZIONI, DISCO_UTENTI, DISCO_ALTRO, DISCO_LIBERO, PEZZI_DISCO };
static const char *const nomi_disco[PEZZI_DISCO] = {
    "Sistema", "Applicazioni", "Utenti (home)", "Altro (file system)", "Libero"
};
static const unsigned int colori_disco[PEZZI_DISCO] = {
    BLU_NORMALE, VERDE_SOPRA, ARANCIO_SOPRA, OPACO(0x94A3B8), VETRO_SUPERFICIE
};
static const char *const partizioni[] = { "/system", "/data" };

/* Quanto pesano i file di una cartella, contando anche quelli delle cartelle dentro.
 * 'percorso' e' lungo 'lungo' lettere, in un posto di MAX_PERCORSO: ci si aggiungono i
 * nomi delle sottocartelle (e poi si tolgono). Una cartella che non si puo' aprire (non
 * si ha il permesso di leggerla) conta 0: si somma quello che si puo' vedere. Oltre
 * PROFONDITA_DISCO cartelle una dentro l'altra non si scende. */
#define PROFONDITA_DISCO 8

static unsigned long long peso_cartella(char *percorso, int lungo, int profondita) {
    int d = apri(percorso, APRI_LEGGERE);
    if (d < 0) return 0;
    unsigned long long somma = 0;
    struct voce_cartella v;
    while (leggi(d, &v, sizeof v) == sizeof v) {
        v.nome[MAX_NOME_FILE - 1] = 0;
        if (v.tipo == TIPO_FILE) {
            somma += v.dimensione;
            continue;
        }
        if (v.tipo != TIPO_CARTELLA || profondita >= PROFONDITA_DISCO) continue;
        if (v.nome[0] == '.' && (!v.nome[1] || (v.nome[1] == '.' && !v.nome[2]))) continue;
        int n = lunghezza(v.nome);
        if (lungo + 1 + n >= MAX_PERCORSO) continue;      /* il percorso sarebbe troppo lungo */
        percorso[lungo] = '/';
        metti_testo(percorso + lungo + 1, v.nome);
        somma += peso_cartella(percorso, lungo + 1 + n, profondita + 1);
        percorso[lungo] = 0;
    }
    chiudi(d);
    return somma;
}

/* --- la finestra --- */

static struct finestra_tk finestra;
static struct barra_strumenti strumenti;
static struct pillola schede, azioni;
static struct pulsante_barra scheda_processi, scheda_memoria, scheda_disco, pulsante_termina;
static struct spazio dopo_schede;
static struct contenitore colonna, pagina_processi, piede;
static struct etichetta messaggio, riassunto_processi;
static struct tabella tabella;
static struct pagina_grafico memoria, disco;

/* CHI SIAMO. Per sapere quali processi si possono terminare serve il proprio uid. Non
 * c'e' una chiamata apposta, ma l'elenco dei processi lo dice gia': mentre il kernel lo
 * scrive, il processo "in esecuzione" e' per forza quello che l'ha chiesto, cioe' noi
 * (c'e' una CPU sola, e sta servendo la nostra chiamata). -1: non si sa. */
static int mio_uid = -1;

/* La stessa regola del kernel (SIS_TERMINA_PROCESSO): l'inattivo (0) e init (1) mai,
 * un processo finito no; il sistema termina chiunque, gli altri solo i loro processi. */
static int si_puo_terminare(const struct info_processo *pr) {
    if (pr->numero <= 1 || pr->stato == STATO_FINITO || mio_uid < 0) return 0;
    return (unsigned int)mio_uid == UID_SISTEMA || pr->uid == (unsigned int)mio_uid;
}

/* "Termina processo" (la X nella barra) si accende solo se il processo scelto si puo'
 * terminare davvero. */
static void aggiorna_termina(void) {
    int si_puo = 0;
    for (int i = 0; i < tabella.quante; i++)
        if (tabella.righe[i].numero == tabella.scelto) si_puo = si_puo_terminare(&tabella.righe[i]);
    if (pulsante_termina.base.disabilitato == !si_puo) return;   /* niente di nuovo */
    widget_disabilita(&pulsante_termina.base, !si_puo);
}

/* Sotto la tabella: "8 processi \xFA 34,6 MB in uso" (\xFA e' il punto a mezza
 * altezza). L'etichetta e' larga quanto il suo testo quando si e' fatta l'impaginazione:
 * se il testo cambia larghezza, la si rifa'. */
static void aggiorna_riassunto_processi(void) {
    unsigned long long in_uso = 0;
    for (int i = 0; i < tabella.quante; i++) in_uso += tabella.righe[i].memoria;
    char testo[MAX_TESTO_WIDGET], *d = metti_numero(testo, (unsigned int)tabella.quante);
    d = metti_testo(d, tabella.quante == 1 ? " processo \xFA " : " processi \xFA ");
    d = metti_byte(d, in_uso);
    metti_testo(d, " in uso");
    int prima = larghezza_testo(riassunto_processi.font, riassunto_processi.testo);
    etichetta_testo(&riassunto_processi, testo);
    if (larghezza_testo(riassunto_processi.font, testo) != prima) toolkit_ridisponi(&finestra);
}

/* Rilegge processi e memoria dal kernel (ogni secondo). */
static void aggiorna(struct finestra_tk *f, void *dati) {
    (void)f; (void)dati;
    int n = elenco_processi(tabella.righe, MAX_PROCESSI);
    tabella.quante = n < 0 ? 0 : n;
    tabella_misure(&tabella);                         /* le righe possono essere di piu' o di meno */
    for (int i = 0; i < tabella.quante; i++)          /* noi: vedi "chi siamo" */
        if (tabella.righe[i].stato == STATO_IN_ESECUZIONE) mio_uid = (int)tabella.righe[i].uid;

    /* il processo scelto non c'e' piu' (e' finito): non e' piu' scelto */
    int trovato = 0;
    for (int i = 0; i < tabella.quante; i++)
        if (tabella.righe[i].numero == tabella.scelto) trovato = 1;
    if (!trovato) tabella.scelto = -1;
    aggiorna_termina();                               /* anche il suo stato puo' cambiare */
    widget_cambiato(&tabella.base);
    aggiorna_riassunto_processi();

    /* la memoria: quello che sanno i processi, la parte condivisa, la libera; il resto
     * e' del kernel (il suo codice, le sue tabelle, le pile dei compiti...) */
    struct info_memoria m;
    if (stato_memoria(&m) < 0) return;
    unsigned long long sistema = 0, applicazioni = 0;
    for (int i = 0; i < tabella.quante; i++) {
        if (tabella.righe[i].uid == UID_SISTEMA) sistema += tabella.righe[i].memoria;
        else applicazioni += tabella.righe[i].memoria;
    }
    unsigned long long contati = m.libera + m.condivisa + sistema + applicazioni;
    struct grafico *g = &memoria.grafico;
    g->totale = m.totale;
    g->byte[KERNEL] = m.totale > contati ? m.totale - contati : 0;
    g->byte[SISTEMA] = sistema;
    g->byte[APPLICAZIONI] = applicazioni;
    g->byte[CONDIVISA] = m.condivisa;
    g->byte[LIBERA] = m.libera;

    char testo[64], *d = metti_testo(testo, "In uso ");
    d = metti_byte(d, m.totale - m.libera);
    d = metti_testo(d, " su ");
    metti_byte(d, m.totale);
    etichetta_testo(&memoria.riassunto, testo);
    pagina_grafico_cambiata(&memoria);
}

/* Rifa' i conti del disco, camminando nelle cartelle: quando si apre la pagina Disco e
 * poi ogni 5 secondi, finche' la si guarda (i file cambiano di rado, e camminare costa
 * piu' che chiedere i processi). */
#define OGNI_DISCO 5000

static unsigned long long peso(const char *cartella) {
    char percorso[MAX_PERCORSO];
    int lungo = (int)(metti_testo(percorso, cartella) - percorso);
    return peso_cartella(percorso, lungo, 0);
}

static void aggiorna_disco(struct finestra_tk *f, void *dati) {
    (void)f; (void)dati;
    unsigned long long totale = 0, libero = 0;
    for (int i = 0; i < (int)(sizeof partizioni / sizeof partizioni[0]); i++) {
        struct info_disco d;
        if (stato_disco(partizioni[i], &d) < 0) continue;
        totale += d.totale;
        libero += d.libero;
    }
    unsigned long long *b = disco.grafico.byte;
    unsigned long long app_sistema = peso("/system/apps");
    unsigned long long tutto_sistema = peso("/system");
    b[DISCO_SISTEMA] = tutto_sistema > app_sistema ? tutto_sistema - app_sistema : 0;
    b[DISCO_APPLICAZIONI] = app_sistema + peso("/data/apps");
    b[DISCO_UTENTI] = peso("/data/users");
    unsigned long long in_uso = totale - libero;
    unsigned long long contati = b[DISCO_SISTEMA] + b[DISCO_APPLICAZIONI] + b[DISCO_UTENTI];
    b[DISCO_ALTRO] = in_uso > contati ? in_uso - contati : 0;
    b[DISCO_LIBERO] = libero;
    disco.grafico.totale = totale;

    char testo[64], *d = metti_testo(testo, "In uso ");
    d = metti_byte(d, in_uso);
    d = metti_testo(d, " su ");
    metti_byte(d, totale);
    etichetta_testo(&disco.riassunto, testo);
    pagina_grafico_cambiata(&disco);
}

static void scelta_cambiata(struct tabella *t) {
    (void)t;
    etichetta_testo(&messaggio, "");
    aggiorna_termina();
}

static void premi_termina(struct pulsante_barra *b, void *dati) {
    (void)b; (void)dati;
    if (tabella.scelto < 0) return;
    if (termina_processo(tabella.scelto) < 0) {
        etichetta_testo(&messaggio, "Questo processo non si pu\x95 terminare");
        return;
    }
    etichetta_testo(&messaggio, "");
    aggiorna(&finestra, 0);
}

/* Le schede: si vede la pagina di quella scelta (la pillola la accende da sola). La X
 * per terminare c'e' solo con i processi; i conti del disco si fanno solo mentre lo si
 * guarda. */
static void mostra_pagina(struct pulsante_barra *b, void *dati) {
    (void)dati;
    int processi = b == &scheda_processi, su_disco = b == &scheda_disco;
    widget_nascondi(&pagina_processi.base, !processi);
    widget_nascondi(&memoria.colonna.base, b != &scheda_memoria);
    widget_nascondi(&disco.colonna.base, !su_disco);
    widget_nascondi(&azioni.base, !processi);
    if (su_disco) aggiorna_disco(&finestra, 0);
    finestra_tk_ogni(&finestra, su_disco ? OGNI_DISCO : 0, aggiorna_disco, 0);
    toolkit_ridisponi(&finestra);
}

int main(void) {
    /* la barra degli strumenti: a sinistra le schede, a destra (solo con i processi) la X
     * per terminare il processo scelto */
    barra_strumenti_prepara(&strumenti);
    pillola_prepara(&schede, 1);
    pulsante_barra_testo(&scheda_processi, "Processi", mostra_pagina, 0);
    pulsante_barra_testo(&scheda_memoria, "Memoria", mostra_pagina, 0);
    pulsante_barra_testo(&scheda_disco, "Disco", mostra_pagina, 0);
    pillola_aggiungi(&schede, &scheda_processi);
    pillola_aggiungi(&schede, &scheda_memoria);
    pillola_aggiungi(&schede, &scheda_disco);
    pulsante_barra_accendi(&scheda_processi, 1);
    spazio_prepara(&dopo_schede);
    pillola_prepara(&azioni, 0);
    pulsante_barra_prepara(&pulsante_termina, ICONA_X, premi_termina, 0);
    pulsante_termina.base.disabilitato = 1;
    pillola_aggiungi(&azioni, &pulsante_termina);
    barra_strumenti_aggiungi(&strumenti, &schede.base);
    barra_strumenti_aggiungi(&strumenti, &dopo_schede.base);
    barra_strumenti_aggiungi(&strumenti, &azioni.base);

    colonna_prepara(&colonna, SPAZIO_TRA);

    /* la pagina dei processi: la tabella, e sotto il messaggio (se terminare non riesce)
     * e il riassunto */
    colonna_prepara(&pagina_processi, SPAZIO_TRA);
    pagina_processi.base.elastico = 1;
    tabella = (struct tabella){ .base = { .tipo = &tipo_tabella, .elastico = 1, .largo = 1 },
                                .scelto = -1, .al_cambio = scelta_cambiata };
    barra_scorrimento_prepara(&tabella.barra);
    widget_aggiungi(&tabella.base, &tabella.barra.base);
    riga_prepara(&piede, 8);
    etichetta_prepara(&messaggio, "", FONT_TESTO);
    messaggio.colore = STATO_ERRORE;
    messaggio.base.elastico = 1;
    etichetta_prepara(&riassunto_processi, "", FONT_PICCOLO);
    riassunto_processi.colore = CONTENUTO_SECONDARIO;
    widget_aggiungi(&piede.base, &messaggio.base);
    widget_aggiungi(&piede.base, &riassunto_processi.base);
    widget_aggiungi(&pagina_processi.base, &tabella.base);
    widget_aggiungi(&pagina_processi.base, &piede.base);

    /* le pagine della memoria e del disco: il riassunto, la barra, la legenda */
    pagina_grafico_prepara(&memoria, PEZZI_MEMORIA, nomi_memoria, colori_memoria, LIBERA);
    pagina_grafico_prepara(&disco, PEZZI_DISCO, nomi_disco, colori_disco, DISCO_LIBERO);

    widget_aggiungi(&colonna.base, &pagina_processi.base);
    widget_aggiungi(&colonna.base, &memoria.colonna.base);
    widget_aggiungi(&colonna.base, &disco.colonna.base);

    finestra_tk_prepara(&finestra, "Attivit\x85", &colonna.base);
    finestra_tk_barra(&finestra, &strumenti.base);
    finestra_tk_dimensione(&finestra, 640, 460);
    aggiorna(&finestra, 0);
    finestra_tk_ogni(&finestra, 1000, aggiorna, 0);
    return toolkit_esegui(&finestra);
}
