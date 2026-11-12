/* Attivita': il task manager. Nella barra degli strumenti tre schede, per tre pagine:
 *   - Processi: i programmi che girano (numero, nome, utente, stato, memoria), aggiornati
 *     ogni secondo. Se ne sceglie uno con un clic: nella barra compaiono "Riavvia" (solo per
 *     le app) e "Termina processo". Sotto la tabella, quanti sono e quanta memoria usano.
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
 * L'ASPETTO e' quello della pagina "App - Attivita'" del file Figma, fatto con i componenti di
 * UIKit: le schede nella barra degli strumenti, i pulsanti, l'area dei contenuti chiara (la
 * pagina), la barra di utilizzo con la legenda, l'avviso del riavvio. La tabella dei processi e'
 * ancora disegnata qui: il suo componente arriva in UIKit con il lavoro su Files.
 *
 * LA SPAZIATURA (la regola: un solo livello con il margine, uguale in x e in y). Processi: la
 * pagina della tabella ha 16 di margine, le righe hanno 8 di margine interno perche' hanno un
 * fondo (e l'intestazione si allinea a loro): dal bordo dell'area al primo testo 16 + 8 = 24 in
 * x, e in y 16 + 8 (il testo alto 16 al centro dell'intestazione alta 32) = 24. Il piede: 24 ai
 * lati e sotto. Memoria e Disco: la pagina ha 24 di margine, i figli niente: 24 e 24. */
#include "uikit/uikit.h"
#include "launcher.h"     /* Riavvia: riaprire l'app */

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
 * DA PASSARE A UIKIT: tabella (intestazione di colonna con la freccia dell'ordinamento, riga
 * di tabella nella variante "Processo"). I componenti li fa il lavoro su Files; finche' non
 * ci sono, la tabella e' disegnata qui, con le misure del disegno "Attivita' - Processi":
 *   - l'INTESTAZIONE, alta ALTEZZA_S (32), con i titoli in FONT_ETICHETTA e TESTO_SECONDARIO;
 *     sotto una linea di 1 pixel BORDO_SEPARATORE da bordo a bordo. (La freccia "crescente"
 *     sulla colonna Numero e il clic per ordinare arriveranno con il componente: l'elenco e'
 *     gia' in ordine di numero.)
 *   - le RIGHE, alte ALTEZZA_M (40), a SPAZIO_4 l'una dall'altra (e dalla linea), raggio
 *     RAGGIO_CONTROLLO_NUOVO, con SPAZIO_8 di margine interno ai lati (hanno un fondo: sotto il
 *     puntatore CONTROLLO_SFONDO, scelta CONTROLLO_SFONDO_SOPRA). Testo FONT_CORPO: il nome in
 *     TESTO_PRIMARIO, il resto in TESTO_SECONDARIO.
 *   - le COLONNE: Numero 96, Processo quello che avanza, Utente 120, Stato 120, Memoria 100 (a
 *     destra), a SPAZIO_24 l'una dall'altra; l'intestazione ha lo stesso margine delle righe.
 * Le righe che non ci stanno si vedono scorrendo (rotella, o la barra di scorrimento di UIKit). */

#define TABELLA_TESTATA   ALTEZZA_S
#define TABELLA_LINEA     1
#define TABELLA_RIGA      ALTEZZA_M
#define TABELLA_TRA       SPAZIO_4
#define TABELLA_PASSO     (TABELLA_RIGA + TABELLA_TRA)
#define TABELLA_LATI      SPAZIO_8                  /* il margine interno delle righe */
#define TABELLA_COLONNE_TRA SPAZIO_24
#define COLONNE           5

struct tabella {
    struct widget base;
    struct info_processo righe[MAX_PROCESSI];
    int quante;
    int scelto;                        /* il numero del processo scelto, o -1 */
    int sotto;                         /* la riga sotto il puntatore (indice), o -1 */
    void (*al_cambio)(struct tabella *t);
    struct barra_scorrimento barra;
};

#define RIGHE_SOPRA (TABELLA_TESTATA + TABELLA_LINEA)

static int tabella_vista(const struct widget *w) { return w->a - RIGHE_SOPRA; }

static void tabella_misure(struct tabella *t) {
    scorrimento_misure(&t->barra.scorrimento, t->quante * TABELLA_PASSO, tabella_vista(&t->base));
}

static const char *const titoli[COLONNE] = { "Numero", "Processo", "Utente", "Stato", "Memoria" };
static const int larghezze[COLONNE] = { 96, 0, 120, 120, 100 };   /* 0: quello che avanza */

static void cella(const struct widget *w, int i, int *x, int *l) {
    int inizio = w->x + TABELLA_LATI, fine = w->x + w->l - TABELLA_LATI;
    int fisse = 0;
    for (int k = 0; k < COLONNE; k++) fisse += larghezze[k];
    int resto = fine - inizio - fisse - (COLONNE - 1) * TABELLA_COLONNE_TRA;
    *x = inizio;
    for (int k = 0; k < i; k++) *x += (larghezze[k] ? larghezze[k] : resto) + TABELLA_COLONNE_TRA;
    *l = larghezze[i] ? larghezze[i] : resto;
}

static void testo_cella(struct pennello *p, const struct widget *w, int i, const struct font *f,
                        int y, int a, const char *s) {
    int x, l;
    cella(w, i, &x, &l);
    pennello_salva(p);
    pennello_ritaglia(p, x, y, l, a);
    int tx = i == COLONNE - 1 ? x + l - larghezza_testo(f, s) : x;
    testo_centrato(p, f, tx, y, a, s);
    pennello_ripristina(p);
}

static void tabella_misura(struct widget *w, int *l, int *a) {
    (void)w;
    *l = 0;
    *a = RIGHE_SOPRA + 4 * TABELLA_PASSO;
}

static void tabella_disegna(struct widget *w, struct pennello *p) {
    struct tabella *t = (struct tabella *)w;
    pennello_colore(p, TESTO_SECONDARIO);
    for (int i = 0; i < COLONNE; i++)
        testo_cella(p, w, i, FONT_ETICHETTA, w->y, TABELLA_TESTATA, titoli[i]);
    pennello_colore(p, BORDO_SEPARATORE);
    riempi_rettangolo(p, w->x, w->y + TABELLA_TESTATA, w->l, TABELLA_LINEA);

    pennello_salva(p);
    pennello_ritaglia(p, w->x, w->y + RIGHE_SOPRA, w->l, tabella_vista(w));
    int scorso = t->barra.scorrimento.posizione;
    for (int r = 0; r < t->quante; r++) {
        const struct info_processo *pr = &t->righe[r];
        int y = w->y + RIGHE_SOPRA + TABELLA_TRA + r * TABELLA_PASSO - scorso;
        if (y + TABELLA_RIGA <= w->y + RIGHE_SOPRA || y >= w->y + w->a) continue;
        int scelta = pr->numero == t->scelto;
        if (scelta || r == t->sotto) {
            pennello_colore(p, scelta ? CONTROLLO_SFONDO_SOPRA : CONTROLLO_SFONDO_NUOVO);
            riempi_tondo(p, w->x, y, w->l, TABELLA_RIGA, RAGGIO_CONTROLLO_NUOVO);
        }
        char numero[12], memoria[24];
        metti_numero(numero, (unsigned int)pr->numero);
        testo_byte(memoria, pr->memoria);
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

static int tabella_riga_in(const struct tabella *t, int y) {
    const struct widget *w = &t->base;
    if (y < w->y + RIGHE_SOPRA || y >= w->y + RIGHE_SOPRA + tabella_vista(w)) return -1;
    int nel_contenuto = y - w->y - RIGHE_SOPRA - TABELLA_TRA + t->barra.scorrimento.posizione;
    if (nel_contenuto < 0) return -1;
    int r = nel_contenuto / TABELLA_PASSO;
    if (r >= t->quante || nel_contenuto % TABELLA_PASSO >= TABELLA_RIGA) return -1;
    return r;
}

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

static void tabella_disponi(struct widget *w) {
    struct tabella *t = (struct tabella *)w;
    barra_scorrimento_posto(&t->barra, w->x, w->y + RIGHE_SOPRA, w->l, tabella_vista(w));
    tabella_misure(t);
}

static void tabella_rotella(struct widget *w, int sx, int sy) {
    struct tabella *t = (struct tabella *)w;
    (void)sx;
    barra_scorrimento_rotella(&t->barra, sy);
    if (t->sotto >= 0) { t->sotto = -1; widget_cambiato(w); }
}

static const struct tipo_widget tipo_tabella = {
    tabella_misura, tabella_disponi, tabella_disegna, tabella_puntatore, 0, tabella_rotella,
    tabella_sopra_cambiato, tabella_movimento, 0
};

/* --- le pagine Memoria e Disco: il riepilogo, la barra di utilizzo e la legenda di UIKit --- */

struct pagina_grafico {
    struct pagina pagina;              /* margine 24, SPAZIO_16 tra riepilogo e legenda */
    struct contenitore riepilogo;      /* il titolo e la barra, a SPAZIO_8 */
    struct paragrafo titolo;
    struct barra_utilizzo barra;
    struct legenda legenda;
    struct grafico grafico;
};

static void pagina_grafico_prepara(struct pagina_grafico *pg, int quanti, const char *const *nomi,
                                   const int *toni, int resto) {
    pagina_prepara(&pg->pagina, SPAZIO_16, SPAZIO_24, SPAZIO_24, SPAZIO_24);
    pg->pagina.base.nascosto = 1;
    pg->grafico = (struct grafico){ .quanti = quanti, .nomi = nomi, .toni = toni, .resto = resto };
    colonna_prepara(&pg->riepilogo, SPAZIO_8);
    paragrafo_prepara(&pg->titolo, "", FONT_SEZIONE_NUOVO, INTERLINEA_SEZIONE, TESTO_PRIMARIO);
    barra_utilizzo_prepara(&pg->barra, &pg->grafico);
    legenda_prepara(&pg->legenda, &pg->grafico);
    widget_aggiungi(&pg->riepilogo.base, &pg->titolo.base);
    widget_aggiungi(&pg->riepilogo.base, &pg->barra.base);
    pagina_aggiungi(&pg->pagina, &pg->riepilogo.base);
    pagina_aggiungi(&pg->pagina, &pg->legenda.base);
}

static void pagina_grafico_cambiata(struct pagina_grafico *pg) {
    widget_cambiato(&pg->barra.base);
    widget_cambiato(&pg->legenda.base);
}

/* "In uso 312,4 MB su 2 GB" */
static void riepilogo(struct pagina_grafico *pg, unsigned long long in_uso, unsigned long long totale) {
    char testo[64], *d = metti_testo(testo, "In uso ");
    d = testo_byte(d, in_uso);
    d = metti_testo(d, " su ");
    testo_byte(d, totale);
    paragrafo_testo(&pg->titolo, testo);
}

/* La memoria: il kernel, i servizi del sistema, le applicazioni, la parte condivisa, e la
 * libera (il fondo della barra). Dal piu' forte al piu' leggero, come nel disegno. */
enum { KERNEL, SISTEMA, APPLICAZIONI, CONDIVISA, LIBERA, PEZZI_MEMORIA };
static const char *const nomi_memoria[PEZZI_MEMORIA] = {
    "Kernel", "Servizi del sistema", "Applicazioni", "Memoria condivisa (finestre)", "Libera"
};
static const int toni_memoria[PEZZI_MEMORIA] = { 0, 1, 2, 3, 0 };

/* Il disco: system ("/", sola lettura) e data (/data) come una cosa sola. Totale e libero li
 * dice Linux (statvfs); sistema, applicazioni e utenti si contano camminando nelle cartelle;
 * "Altro" e' il resto di quello che e' in uso (le tabelle di ext4, i file che Attivita' non
 * puo' vedere). Non c'e' nel disegno: e' una domanda aperta (STATO.md, "Da confermare"). */
enum { DISCO_SISTEMA, DISCO_APPLICAZIONI, DISCO_UTENTI, DISCO_ALTRO, DISCO_LIBERO, PEZZI_DISCO };
static const char *const nomi_disco[PEZZI_DISCO] = {
    "Sistema", "Applicazioni", "Utenti (home)", "Altro (file system)", "Libero"
};
static const int toni_disco[PEZZI_DISCO] = { 0, 1, 2, 3, 0 };
static const char *const partizioni[] = { "/system", "/data" };

/* Quanto pesano i file di una cartella, contando anche quelli delle cartelle dentro.
 * 'percorso' e' lungo 'lungo' lettere, in un posto di MAX_PERCORSO. Una cartella che non si
 * puo' aprire conta 0. Oltre PROFONDITA_DISCO cartelle una dentro l'altra non si scende. */
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
        if (lungo + 1 + n >= MAX_PERCORSO) continue;
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
static struct schede schede;
static struct pulsante pulsante_riavvia, pulsante_termina;
static struct avviso avviso_riavvia;
static char id_da_riavviare[MAX_NOME_FILE];   /* l'app da riavviare (il nome del processo e' il suo id) */
static int da_riavviare = -1;
static struct pagina area;                    /* l'area dei contenuti, chiara */
static struct pagina pagina_tabella, piede;
static struct contenitore riga_piede;
static struct paragrafo messaggio, riassunto_processi;
static struct tabella tabella;
static struct pagina_grafico memoria, disco;

enum { SCHEDA_PROCESSI, SCHEDA_MEMORIA, SCHEDA_DISCO };

/* CHI SIAMO: mentre il kernel scrive l'elenco, il processo "in esecuzione" e' quello che l'ha
 * chiesto, cioe' noi. -1: non si sa. */
static int mio_uid = -1;

static int si_puo_terminare(const struct info_processo *pr) {
    if (pr->numero <= 1 || pr->stato == STATO_FINITO || mio_uid < 0) return 0;
    return (unsigned int)mio_uid == UID_SISTEMA || pr->uid == (unsigned int)mio_uid;
}

static const struct info_processo *processo_scelto(void) {
    for (int i = 0; i < tabella.quante; i++)
        if (tabella.righe[i].numero == tabella.scelto) return &tabella.righe[i];
    return 0;
}

/* Le azioni nella barra: si vedono solo con un processo scelto, nella pagina dei processi
 * (nel disegno, senza una riga scelta le schede prendono tutta la barra). "Termina processo"
 * si accende se il processo si puo' terminare; "Riavvia" solo per le app (i processi
 * dell'utente "applicazioni", il cui nome e' l'id dell'app). */
static void aggiorna_azioni(void) {
    const struct info_processo *pr = processo_scelto();
    int vedi = pr && schede.scelta == SCHEDA_PROCESSI;
    int termina = pr && si_puo_terminare(pr), riavvia = termina && pr->uid == UID_APPLICAZIONI;
    int cambia = pulsante_termina.base.nascosto == vedi;
    if (cambia) {
        widget_nascondi(&pulsante_termina.base, !vedi);
        widget_nascondi(&pulsante_riavvia.base, !vedi);
    }
    if (pulsante_termina.base.disabilitato == termina) widget_disabilita(&pulsante_termina.base, !termina);
    if (pulsante_riavvia.base.disabilitato == riavvia) widget_disabilita(&pulsante_riavvia.base, !riavvia);
    if (cambia && finestra.base.l) uikit_ridisponi(&finestra);
}

/* Sotto la tabella: "8 processi \xFA 34,6 MB in uso" (\xFA e' il punto a mezza altezza). */
static void aggiorna_riassunto_processi(void) {
    unsigned long long in_uso = 0;
    for (int i = 0; i < tabella.quante; i++) in_uso += tabella.righe[i].memoria;
    char testo[MAX_TESTO_WIDGET], *d = metti_numero(testo, (unsigned int)tabella.quante);
    d = metti_testo(d, tabella.quante == 1 ? " processo \xFA " : " processi \xFA ");
    d = testo_byte(d, in_uso);
    metti_testo(d, " in uso");
    int prima = larghezza_testo(riassunto_processi.font, riassunto_processi.testo);
    paragrafo_testo(&riassunto_processi, testo);
    if (larghezza_testo(riassunto_processi.font, testo) != prima && finestra.base.l) uikit_ridisponi(&finestra);
}

static void scrivi_messaggio(const char *testo) { paragrafo_testo(&messaggio, testo); }

/* Rilegge processi e memoria dal kernel (ogni secondo). */
static void aggiorna(struct finestra_tk *f, void *dati) {
    (void)f; (void)dati;
    int n = elenco_processi(tabella.righe, MAX_PROCESSI);
    tabella.quante = n < 0 ? 0 : n;
    tabella_misure(&tabella);
    if (tabella.sotto >= tabella.quante) tabella.sotto = -1;
    for (int i = 0; i < tabella.quante; i++)
        if (tabella.righe[i].stato == STATO_IN_ESECUZIONE) mio_uid = (int)tabella.righe[i].uid;
    if (!processo_scelto()) tabella.scelto = -1;   /* e' finito: non e' piu' scelto */
    aggiorna_azioni();
    widget_cambiato(&tabella.base);
    aggiorna_riassunto_processi();

    /* la memoria: quello che sanno i processi, la condivisa, la libera; il resto e' del kernel */
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
    riepilogo(&memoria, m.totale - m.libera, m.totale);
    pagina_grafico_cambiata(&memoria);
}

/* I conti del disco, camminando nelle cartelle: quando si apre la pagina Disco e poi ogni 5
 * secondi, finche' la si guarda. */
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
    riepilogo(&disco, in_uso, totale);
    pagina_grafico_cambiata(&disco);
}

static void scelta_cambiata(struct tabella *t) {
    (void)t;
    scrivi_messaggio("");
    aggiorna_azioni();
}

static void premi_termina(struct pulsante *b, void *dati) {
    (void)b; (void)dati;
    if (tabella.scelto < 0) return;
    if (termina_processo(tabella.scelto) < 0) {
        scrivi_messaggio("Questo processo non si pu\x95 terminare");
        return;
    }
    scrivi_messaggio("");
    aggiorna(&finestra, 0);
}

/* Riavvia: l'avviso (bloccante) chiede conferma; se si conferma, si termina il processo e si
 * apre di nuovo la stessa app con il launcher. */
static void risposta_riavvia(struct avviso *a, int scelta, void *dati) {
    (void)a; (void)dati;
    if (scelta != AVVISO_CONFERMA || da_riavviare < 0) return;
    int chiuso = termina_processo(da_riavviare) == 0;
    if (!chiuso) scrivi_messaggio("Questo processo non si pu\x95 riavviare");
    else if (apri_applicazione(id_da_riavviare) < 0) scrivi_messaggio("Non si riesce a riaprire l'app");
    else scrivi_messaggio("");
    aggiorna(&finestra, 0);
}

static void premi_riavvia(struct pulsante *b, void *dati) {
    (void)b; (void)dati;
    const struct info_processo *pr = processo_scelto();
    if (!pr) return;
    da_riavviare = tabella.scelto;
    copia_testo(id_da_riavviare, pr->nome, sizeof id_da_riavviare);
    char titolo[MAX_TESTO_WIDGET];
    char *d = metti_testo(titolo, "Riavviare \"");
    d = metti_testo(d, id_da_riavviare);
    metti_testo(d, "\"?");
    copia_testo(avviso_riavvia.titolo, titolo, MAX_TESTO_WIDGET);
    avviso_apri(&avviso_riavvia);
}

/* Le schede: si vede la pagina di quella scelta; i conti del disco si fanno solo mentre lo
 * si guarda. */
static void mostra_pagina(struct schede *s, int scelta, void *dati) {
    (void)s; (void)dati;
    widget_nascondi(&pagina_tabella.base, scelta != SCHEDA_PROCESSI);
    widget_nascondi(&piede.base, scelta != SCHEDA_PROCESSI);
    widget_nascondi(&memoria.pagina.base, scelta != SCHEDA_MEMORIA);
    widget_nascondi(&disco.pagina.base, scelta != SCHEDA_DISCO);
    if (scelta == SCHEDA_DISCO) aggiorna_disco(&finestra, 0);
    finestra_tk_ogni(&finestra, scelta == SCHEDA_DISCO ? OGNI_DISCO : 0, aggiorna_disco, 0);
    aggiorna_azioni();
    uikit_ridisponi(&finestra);
}

int main(void) {
    /* la barra degli strumenti: le schede (prendono lo spazio che avanza), poi, con un processo
     * scelto, "Riavvia" (Secondario S) e "Termina processo" (Pericolo S) */
    barra_strumenti_prepara(&strumenti);
    schede_prepara(&schede, mostra_pagina, 0);
    schede_aggiungi(&schede, "Processi");
    schede_aggiungi(&schede, "Memoria");
    schede_aggiungi(&schede, "Disco");
    pulsante_prepara(&pulsante_riavvia, "Riavvia", ICONA_ROTATE_CCW, premi_riavvia, 0);
    pulsante_stile(&pulsante_riavvia, SECONDARIO, MISURA_S);
    pulsante_prepara(&pulsante_termina, "Termina processo", ICONA_X, premi_termina, 0);
    pulsante_stile(&pulsante_termina, PERICOLO, MISURA_S);
    pulsante_riavvia.base.nascosto = pulsante_termina.base.nascosto = 1;
    barra_strumenti_aggiungi(&strumenti, &schede.base);
    barra_strumenti_aggiungi(&strumenti, &pulsante_riavvia.base);
    barra_strumenti_aggiungi(&strumenti, &pulsante_termina.base);

    /* l'area dei contenuti: la pagina della tabella (margine 16, sotto niente: il piede ha il
     * suo) e il piede (24 ai lati e sotto, 16 sopra), oppure le pagine Memoria e Disco */
    pagina_prepara(&area, SPAZIO_0, SPAZIO_0, SPAZIO_0, SPAZIO_0);
    area.fondo = 1;
    pagina_prepara(&pagina_tabella, SPAZIO_0, SPAZIO_16, SPAZIO_16, SPAZIO_0);
    tabella = (struct tabella){ .base = { .tipo = &tipo_tabella, .elastico = 1, .largo = 1 },
                                .scelto = -1, .sotto = -1, .al_cambio = scelta_cambiata };
    barra_scorrimento_prepara(&tabella.barra);
    widget_aggiungi(&tabella.base, &tabella.barra.base);
    pagina_aggiungi(&pagina_tabella, &tabella.base);

    pagina_prepara(&piede, SPAZIO_0, SPAZIO_24, SPAZIO_16, SPAZIO_24);
    piede.base.elastico = 0;
    riga_prepara(&riga_piede, SPAZIO_16);
    paragrafo_prepara(&riassunto_processi, "", FONT_PICCOLO_NUOVO, INTERLINEA_PICCOLO, TESTO_SECONDARIO);
    riassunto_processi.base.largo = 0;
    paragrafo_prepara(&messaggio, "", FONT_PICCOLO_NUOVO, INTERLINEA_PICCOLO, TESTO_PRIMARIO);
    messaggio.base.largo = 0;
    messaggio.base.elastico = 1;
    messaggio.allineamento = PARAGRAFO_DESTRA;
    widget_aggiungi(&riga_piede.base, &riassunto_processi.base);
    widget_aggiungi(&riga_piede.base, &messaggio.base);
    pagina_aggiungi(&piede, &riga_piede.base);

    pagina_grafico_prepara(&memoria, PEZZI_MEMORIA, nomi_memoria, toni_memoria, LIBERA);
    pagina_grafico_prepara(&disco, PEZZI_DISCO, nomi_disco, toni_disco, DISCO_LIBERO);

    pagina_aggiungi(&area, &pagina_tabella.base);
    pagina_aggiungi(&area, &piede.base);
    pagina_aggiungi(&area, &memoria.pagina.base);
    pagina_aggiungi(&area, &disco.pagina.base);

    finestra_tk_prepara(&finestra, "Attivit\x85", &area.base);
    finestra_tk_barra(&finestra, &strumenti.base);
    finestra_tk_bordo(&finestra);
    finestra_tk_dimensione(&finestra, 768, 547);
    avviso_prepara(&avviso_riavvia, "Riavviare?",
                   "Il processo viene chiuso e aperto di nuovo. I dati non salvati andranno persi.",
                   "Riavvia", PERICOLO, 1, risposta_riavvia, 0);
    finestra_tk_avviso(&finestra, &avviso_riavvia);
    aggiorna(&finestra, 0);
    finestra_tk_ogni(&finestra, 1000, aggiorna, 0);
    return uikit_esegui(&finestra);
}
