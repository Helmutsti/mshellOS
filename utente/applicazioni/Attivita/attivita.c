/* Attivita': il task manager. Nella barra degli strumenti tre schede, per tre pagine:
 *   - Processi: i programmi che girano (numero, nome, utente, stato, memoria), aggiornati
 *     ogni secondo. Se ne sceglie uno con un clic e lo si puo' terminare con la X a
 *     destra nella barra. Sotto la tabella, quanti sono e quanta memoria usano.
 *   - Memoria: com'e' usata la RAM, come la pagina "Archiviazione" del Mac: una barra
 *     divisa in pezzi, e sotto la legenda con quanto pesa ogni pezzo.
 *   - Disco: lo stesso per i file: quanto pesano il sistema, le applicazioni, gli utenti,
 *     e quanto e' libero.
 *
 * Tutto quello che mostra lo chiede alla libreria (elenco_processi, stato_memoria,
 * stato_disco, le cartelle), che lo legge da Linux (/proc, statvfs). Terminare segue la
 * regola di Unix: Attivita' gira come utente "applicazioni", quindi puo' terminare solo
 * le applicazioni; i servizi del sistema no.
 *
 * L'ASPETTO e' quello del nuovo disegno ("Flusso"): solo i colori semantici del tema in
 * uso (Chiaro o Scuro, stile.h), i font Plex. La tabella segue i componenti "Tabella",
 * "Riga di tabella" e "Intestazione di colonna" del file Figma; la barra e la legenda la
 * pagina "Disegno - Attivita'" (una barra sottile in scala di grigi, pallini di 8). */
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
 * Dal componente "Tabella" del Figma. Non ha un fondo suo (sta sul vetro della finestra):
 *   - l'INTESTAZIONE, alta ALTEZZA_S (32), con i titoli delle colonne in FONT_ETICHETTA e
 *     TESTO_SECONDARIO, e sotto una linea di 1 pixel BORDO_SEPARATORE da bordo a bordo;
 *   - le RIGHE, alte ALTEZZA_M (40), a SPAZIO_4 l'una dall'altra (e dalla linea). Una riga
 *     e' un rettangolo arrotondato (RAGGIO_CONTROLLO_NUOVO) che si vede solo sotto il
 *     puntatore (CONTROLLO_SFONDO_NUOVO) o se e' scelta (CONTROLLO_SFONDO_SOPRA): e' piatta,
 *     niente ombre. Il testo e' FONT_CORPO: il nome in TESTO_PRIMARIO, il resto in
 *     TESTO_SECONDARIO.
 *   - le COLONNE stanno a SPAZIO_8 dai bordi, e ogni cella ha SPAZIO_12 di margine ai lati;
 *     la memoria e' allineata a destra.
 *
 * Le righe che non ci stanno si vedono SCORRENDO: con la rotella sopra la tabella, o
 * trascinando la barra di scorrimento del toolkit, che sta sopra le righe, sul bordo
 * destro. L'intestazione resta ferma; le righe scorrono sotto di lei, pixel per pixel. */

#define TABELLA_TESTATA   ALTEZZA_S
#define TABELLA_LINEA     1
#define TABELLA_RIGA      ALTEZZA_M
#define TABELLA_TRA       SPAZIO_4                  /* tra una riga e l'altra */
#define TABELLA_PASSO     (TABELLA_RIGA + TABELLA_TRA)
#define TABELLA_LATI      SPAZIO_8                  /* dai bordi della tabella alle colonne */
#define CELLA_MARGINE     SPAZIO_12                 /* dentro ogni cella, a sinistra e a destra */
#define COLONNE           5

struct tabella {
    struct widget base;
    struct info_processo righe[MAX_PROCESSI];
    int quante;
    int scelto;                        /* il numero del processo scelto, o -1 */
    int sotto;                         /* la riga sotto il puntatore (indice), o -1 */
    void (*al_cambio)(struct tabella *t);
    struct barra_scorrimento barra;    /* sua figlia: sa anche dove si e' scorso */
};

/* Le righe stanno sotto l'intestazione e la sua linea, a partire da TABELLA_TRA. */
#define RIGHE_SOPRA (TABELLA_TESTATA + TABELLA_LINEA)

static int tabella_vista(const struct widget *w) { return w->a - RIGHE_SOPRA; }

/* Quanto sono alte tutte le righe (l'ultima senza lo spazio dopo, ma con quello prima della
 * prima), e quanto se ne vede: lo deve sapere lo scorrimento. */
static void tabella_misure(struct tabella *t) {
    scorrimento_misure(&t->barra.scorrimento, t->quante * TABELLA_PASSO, tabella_vista(&t->base));
}

/* Le celle: dove comincia ognuna (dal bordo sinistro delle colonne) e quanto e' larga. Le
 * prime tre hanno una larghezza fissa (quelle del disegno, il Numero un po' piu' largo
 * perche' il suo titolo ci stia con i margini); lo Stato prende quello che avanza, la
 * Memoria (allineata a destra) sta in fondo. */
static const char *const titoli[COLONNE] = { "Numero", "Nome", "Utente", "Stato", "Memoria" };
static const int larghezze[COLONNE - 1] = { 76, 156, 110, 0 };
#define MEMORIA_LARGA 96

static void cella(const struct widget *w, int i, int *x, int *l) {
    int inizio = w->x + TABELLA_LATI, fine = w->x + w->l - TABELLA_LATI;
    if (i == COLONNE - 1) { *x = fine - MEMORIA_LARGA; *l = MEMORIA_LARGA; return; }
    *x = inizio;
    for (int k = 0; k < i; k++) *x += larghezze[k];
    *l = larghezze[i] ? larghezze[i] : fine - MEMORIA_LARGA - *x;
}

/* Un testo in una cella, ritagliato nella cella (meno i suoi margini); a destra se e'
 * l'ultima colonna. */
static void testo_cella(struct pennello *p, const struct widget *w, int i, const struct font *f,
                        int y, int a, const char *s) {
    int x, l;
    cella(w, i, &x, &l);
    pennello_salva(p);
    pennello_ritaglia(p, x + CELLA_MARGINE, y, l - 2 * CELLA_MARGINE, a);
    int tx = i == COLONNE - 1 ? x + l - CELLA_MARGINE - larghezza_testo(f, s) : x + CELLA_MARGINE;
    testo_centrato(p, f, tx, y, a, s);
    pennello_ripristina(p);
}

static void tabella_misura(struct widget *w, int *l, int *a) {
    (void)w;
    *l = 560;
    *a = RIGHE_SOPRA + 4 * TABELLA_PASSO;
}

static void tabella_disegna(struct widget *w, struct pennello *p) {
    struct tabella *t = (struct tabella *)w;

    /* l'intestazione e la sua linea */
    pennello_colore(p, TESTO_SECONDARIO);
    for (int i = 0; i < COLONNE; i++)
        testo_cella(p, w, i, FONT_ETICHETTA, w->y, TABELLA_TESTATA, titoli[i]);
    pennello_colore(p, BORDO_SEPARATORE);
    riempi_rettangolo(p, w->x, w->y + TABELLA_TESTATA, w->l, TABELLA_LINEA);

    /* le righe: spostate in su di quanto si e' scorso, e ritagliate nella vista */
    pennello_salva(p);
    pennello_ritaglia(p, w->x, w->y + RIGHE_SOPRA, w->l, tabella_vista(w));
    int scorso = t->barra.scorrimento.posizione;
    for (int r = 0; r < t->quante; r++) {
        const struct info_processo *pr = &t->righe[r];
        int y = w->y + RIGHE_SOPRA + TABELLA_TRA + r * TABELLA_PASSO - scorso;
        if (y + TABELLA_RIGA <= w->y + RIGHE_SOPRA || y >= w->y + w->a) continue;   /* fuori */
        int scelta = pr->numero == t->scelto;
        if (scelta || r == t->sotto) {
            pennello_colore(p, scelta ? CONTROLLO_SFONDO_SOPRA : CONTROLLO_SFONDO_NUOVO);
            riempi_tondo(p, w->x, y, w->l, TABELLA_RIGA, RAGGIO_CONTROLLO_NUOVO);
        }
        char numero[12], memoria[16];
        metti_numero(numero, (unsigned int)pr->numero);
        metti_byte(memoria, pr->memoria);
        pennello_colore(p, TESTO_SECONDARIO);
        testo_cella(p, w, 0, FONT_CORPO, y, TABELLA_RIGA, numero);
        testo_cella(p, w, 2, FONT_CORPO, y, TABELLA_RIGA, nome_utente(pr->uid));
        testo_cella(p, w, 3, FONT_CORPO, y, TABELLA_RIGA, nome_stato(pr->stato));
        testo_cella(p, w, 4, FONT_CORPO, y, TABELLA_RIGA, memoria);
        pennello_colore(p, TESTO_PRIMARIO);
        testo_cella(p, w, 1, FONT_CORPO, y, TABELLA_RIGA, pr->nome);
    }
    pennello_ripristina(p);
}

/* La riga che si vede in (x, y), contando quanto si e' scorso; -1 se li' non ce n'e' una
 * (l'intestazione, lo spazio tra due righe, sotto l'ultima). */
static int tabella_riga_in(const struct tabella *t, int y) {
    const struct widget *w = &t->base;
    if (y < w->y + RIGHE_SOPRA || y >= w->y + RIGHE_SOPRA + tabella_vista(w)) return -1;
    int nel_contenuto = y - w->y - RIGHE_SOPRA - TABELLA_TRA + t->barra.scorrimento.posizione;
    if (nel_contenuto < 0) return -1;
    int r = nel_contenuto / TABELLA_PASSO;
    if (r >= t->quante || nel_contenuto % TABELLA_PASSO >= TABELLA_RIGA) return -1;
    return r;
}

/* Un clic su una riga la sceglie. */
static void tabella_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    struct tabella *t = (struct tabella *)w;
    (void)x;
    if (!(pulsanti & PULSANTE_SINISTRO)) return;
    int r = tabella_riga_in(t, y);
    if (r < 0 || t->righe[r].numero == t->scelto) return;
    t->scelto = t->righe[r].numero;
    widget_cambiato(w);
    if (t->al_cambio) t->al_cambio(t);
}

/* La riga sotto il puntatore si illumina (lo stato "Sopra" della riga di tabella). */
static void tabella_movimento(struct widget *w, int x, int y) {
    struct tabella *t = (struct tabella *)w;
    (void)x;
    int r = tabella_riga_in(t, y);
    if (r == t->sotto) return;
    t->sotto = r;
    widget_cambiato(w);
}

static void tabella_sopra_cambiato(struct widget *w) {
    struct tabella *t = (struct tabella *)w;
    if (!w->sopra && t->sotto >= 0) { t->sotto = -1; widget_cambiato(w); }
}

/* La barra di scorrimento sta sopra le righe (sotto l'intestazione, fino al fondo). */
static void tabella_disponi(struct widget *w) {
    struct tabella *t = (struct tabella *)w;
    barra_scorrimento_posto(&t->barra, w->x, w->y + RIGHE_SOPRA, w->l, tabella_vista(w));
    tabella_misure(t);
}

static void tabella_rotella(struct widget *w, int sx, int sy) {
    struct tabella *t = (struct tabella *)w;
    (void)sx;
    barra_scorrimento_rotella(&t->barra, sy);
    if (t->sotto >= 0) { t->sotto = -1; widget_cambiato(w); }   /* si riaccende muovendosi */
}

static const struct tipo_widget tipo_tabella = {
    tabella_misura, tabella_disponi, tabella_disegna, tabella_puntatore, 0, tabella_rotella,
    tabella_sopra_cambiato, tabella_movimento, 0
};

/* --- i grafici: la barra a pezzi e la legenda, per la memoria e per il disco ---
 *
 * Un grafico e' un elenco di pezzi (nome, tono, quanti byte). La barra e la legenda
 * sono due widget che leggono lo stesso grafico: la pagina Memoria e la pagina Disco ne
 * hanno uno ciascuna, con gli stessi widget.
 *
 * I TONI. Il disegno e' in scala di grigi: i pezzi si distinguono per quanto sono forti,
 * dal piu' forte (il primo) al piu' leggero. Sono colori semantici del tema, che nel
 * chiaro vanno dallo scuro al chiaro e nello scuro dal chiaro allo scuro, cioe' sempre
 * dal piu' al meno contrastato sul vetro. Il "resto" (la parte libera) non e' un pezzo:
 * e' il fondo della barra, INCASSATO come ogni binario (SUPERFICIE_INCASSATA con l'ombra
 * "incassato"), e cosi' e' anche il suo pallino nella legenda. */

#define MAX_PEZZI 6
#define NESSUNO   (-1)

static unsigned int tono(int i) {
    switch (i) {
    case 0:  return TESTO_PRIMARIO;
    case 1:  return TESTO_SECONDARIO;
    case 2:  return SCORRIMENTO_CURSORE_SOPRA;
    case 3:  return TESTO_DISABILITATO;
    default: return BORDO_CONTROLLO;
    }
}

struct grafico {
    int quanti;
    const char *const *nomi;
    const int *toni;                   /* il tono (vedi sopra) di ogni pezzo */
    unsigned long long byte[MAX_PEZZI];
    /* la barra intera, in byte; 0: la somma dei pezzi (la barra e' piena di loro) */
    unsigned long long totale;
    /* il pezzo che e' "il resto" (la memoria libera): non si disegna sopra la barra, ne
     * e' il fondo. NESSUNO se non c'e'. */
    int resto;
};

/* Un pezzo incassato (il fondo della barra, il pallino del resto). */
static void incassato(struct pennello *p, const struct tondo *t) {
    ombre_strati_fuori(p, t, OMBRA_TAB(incassato), OMBRA_N(incassato));
    pennello_colore(p, SUPERFICIE_INCASSATA);
    riempi_forma(p, t);
    ombre_strati_dentro(p, t, OMBRA_TAB(incassato), OMBRA_N(incassato));
}

#define BARRA_SPESSORE 10              /* la barra del disegno: una pillola alta 10 */

struct barra { struct widget base; struct grafico *grafico; };

static void barra_misura(struct widget *w, int *l, int *a) {
    (void)w;
    *l = 400;
    *a = BARRA_SPESSORE;
}

/* Ogni pezzo e' la barra intera ritagliata nella sua striscia: cosi' gli angoli tondi
 * restano solo ai due capi. La striscia di un pezzo va da dove finisce la somma dei
 * pezzi prima di lui a dove finisce la somma con lui, in proporzione al totale. */
static void barra_disegna(struct widget *w, struct pennello *p) {
    const struct grafico *g = ((struct barra *)w)->grafico;
    struct tondo t = tondo(w->x, w->y, w->l, w->a, w->a / 2);
    incassato(p, &t);
    unsigned long long totale = g->totale;
    if (!totale)
        for (int i = 0; i < g->quanti; i++) totale += g->byte[i];
    if (!totale) return;
    int x = w->x;
    unsigned long long somma = 0;
    for (int i = 0; i < g->quanti; i++) {
        if (i == g->resto) continue;
        somma += g->byte[i];
        int fine = w->x + (int)(somma * (unsigned int)w->l / totale);
        if (fine <= x) continue;
        pennello_salva(p);
        pennello_ritaglia(p, x, w->y, fine - x, w->a);
        pennello_colore(p, tono(g->toni[i]));
        riempi_forma(p, &t);
        pennello_ripristina(p);
        x = fine;
    }
}

static const struct tipo_widget tipo_barra = { barra_misura, 0, barra_disegna, 0, 0, 0, 0, 0, 0 };

/* La legenda: una voce per pezzo, alta INTERLINEA_CORPO, a SPAZIO_12 l'una dall'altra. Il
 * pallino di 8, il nome in FONT_CORPO e TESTO_PRIMARIO a 18 dal bordo, quanto pesa in
 * FONT_PICCOLO_NUOVO e TESTO_SECONDARIO, allineato a destra. */
#define LEGENDA_VOCE    INTERLINEA_CORPO
#define LEGENDA_PASSO   (LEGENDA_VOCE + SPAZIO_12)
#define LEGENDA_PUNTO   8
#define LEGENDA_TESTO   18

struct legenda { struct widget base; struct grafico *grafico; };

static void legenda_misura(struct widget *w, int *l, int *a) {
    int n = ((struct legenda *)w)->grafico->quanti;
    *l = 400;
    *a = n ? n * LEGENDA_PASSO - SPAZIO_12 : 0;
}

static void legenda_disegna(struct widget *w, struct pennello *p) {
    const struct grafico *g = ((struct legenda *)w)->grafico;
    for (int i = 0; i < g->quanti; i++) {
        int y = w->y + i * LEGENDA_PASSO;
        struct tondo punto = tondo(w->x, y + (LEGENDA_VOCE - LEGENDA_PUNTO) / 2,
                                   LEGENDA_PUNTO, LEGENDA_PUNTO, LEGENDA_PUNTO / 2);
        if (i == g->resto) {
            incassato(p, &punto);
        } else {
            pennello_colore(p, tono(g->toni[i]));
            riempi_forma(p, &punto);
        }
        char quanto[16];
        metti_byte(quanto, g->byte[i]);
        pennello_colore(p, TESTO_PRIMARIO);
        testo_centrato(p, FONT_CORPO, w->x + LEGENDA_TESTO, y, LEGENDA_VOCE, g->nomi[i]);
        pennello_colore(p, TESTO_SECONDARIO);
        testo_centrato(p, FONT_PICCOLO_NUOVO, w->x + w->l - larghezza_testo(FONT_PICCOLO_NUOVO, quanto),
                       y, LEGENDA_VOCE, quanto);
    }
}

static const struct tipo_widget tipo_legenda = { legenda_misura, 0, legenda_disegna, 0, 0, 0, 0, 0, 0 };

/* Una pagina con un grafico: il riassunto in alto (FONT_SEZIONE_NUOVO), la barra, la legenda. */
struct pagina_grafico {
    struct contenitore colonna;
    struct etichetta riassunto;
    struct barra barra;
    struct legenda legenda;
    struct grafico grafico;
};

static void pagina_grafico_prepara(struct pagina_grafico *pg, int quanti, const char *const *nomi,
                                   const int *toni, int resto) {
    colonna_prepara(&pg->colonna, SPAZIO_12);
    pg->colonna.base.elastico = 1;
    pg->colonna.base.nascosto = 1;
    pg->grafico = (struct grafico){ .quanti = quanti, .nomi = nomi, .toni = toni, .resto = resto };
    etichetta_prepara(&pg->riassunto, "", FONT_SEZIONE_NUOVO);
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
 * la libera (il resto della barra). Dal piu' forte al piu' leggero, come nel disegno. */
enum { KERNEL, SISTEMA, APPLICAZIONI, CONDIVISA, LIBERA, PEZZI_MEMORIA };
static const char *const nomi_memoria[PEZZI_MEMORIA] = {
    "Kernel", "Servizi del sistema", "Applicazioni", "Memoria condivisa (finestre)", "Libera"
};
static const int toni_memoria[PEZZI_MEMORIA] = { 0, 1, 2, 3, 0 };   /* Libera: il resto */

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
 *     memoria, ma e' il tono piu' leggero: non sono file di qualcuno;
 *   - Libero: il fondo della barra (come per la memoria). */
enum { DISCO_SISTEMA, DISCO_APPLICAZIONI, DISCO_UTENTI, DISCO_ALTRO, DISCO_LIBERO, PEZZI_DISCO };
static const char *const nomi_disco[PEZZI_DISCO] = {
    "Sistema", "Applicazioni", "Utenti (home)", "Altro (file system)", "Libero"
};
static const int toni_disco[PEZZI_DISCO] = { 0, 1, 2, 4, 0 };       /* Libero: il resto */
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
static struct pillola schede;
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
    if (tabella.sotto >= tabella.quante) tabella.sotto = -1;
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
    widget_nascondi(&pulsante_termina.base, !processi);
    if (su_disco) aggiorna_disco(&finestra, 0);
    finestra_tk_ogni(&finestra, su_disco ? OGNI_DISCO : 0, aggiorna_disco, 0);
    toolkit_ridisponi(&finestra);
}

int main(void) {
    /* la barra degli strumenti: a sinistra le schede (un gruppo a scelta), a destra (solo
     * con i processi) la X per terminare il processo scelto, un pulsante solo icona */
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
    pulsante_barra_prepara(&pulsante_termina, ICONA_X, premi_termina, 0);
    pulsante_termina.base.disabilitato = 1;
    barra_strumenti_aggiungi(&strumenti, &schede.base);
    barra_strumenti_aggiungi(&strumenti, &dopo_schede.base);
    barra_strumenti_aggiungi(&strumenti, &pulsante_termina.base);

    colonna_prepara(&colonna, SPAZIO_0);

    /* la pagina dei processi: la tabella, e sotto il messaggio (se terminare non riesce)
     * e il riassunto */
    colonna_prepara(&pagina_processi, SPAZIO_12);
    pagina_processi.base.elastico = 1;
    tabella = (struct tabella){ .base = { .tipo = &tipo_tabella, .elastico = 1, .largo = 1 },
                                .scelto = -1, .sotto = -1, .al_cambio = scelta_cambiata };
    barra_scorrimento_prepara(&tabella.barra);
    widget_aggiungi(&tabella.base, &tabella.barra.base);
    riga_prepara(&piede, SPAZIO_8);
    etichetta_prepara(&messaggio, "", FONT_PICCOLO_NUOVO);    /* TESTO_PRIMARIO: si nota */
    messaggio.base.elastico = 1;
    etichetta_prepara(&riassunto_processi, "", FONT_PICCOLO_NUOVO);
    riassunto_processi.colore = TESTO_SECONDARIO;
    widget_aggiungi(&piede.base, &messaggio.base);
    widget_aggiungi(&piede.base, &riassunto_processi.base);
    widget_aggiungi(&pagina_processi.base, &tabella.base);
    widget_aggiungi(&pagina_processi.base, &piede.base);

    /* le pagine della memoria e del disco: il riassunto, la barra, la legenda */
    pagina_grafico_prepara(&memoria, PEZZI_MEMORIA, nomi_memoria, toni_memoria, LIBERA);
    pagina_grafico_prepara(&disco, PEZZI_DISCO, nomi_disco, toni_disco, DISCO_LIBERO);

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
