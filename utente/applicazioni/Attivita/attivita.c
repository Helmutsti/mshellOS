/* Attivita': il task manager. Nella barra degli strumenti tre schede, per tre pagine:
 *   - Processi: i programmi che girano (PID, nome, utente, stato, memoria), aggiornati ogni
 *     secondo, ad albero: ognuno sotto chi l'ha lanciato. Se ne sceglie uno con un clic: nella
 *     barra compaiono "Riavvia" (solo per le app) e "Termina processo". Sotto la tabella,
 *     quanti sono e quanta memoria usano, e l'interruttore "Mostra i servizi di sistema".
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
 * UIKit: la finestra a aree, le schede nella barra degli strumenti, i pulsanti, il pannello
 * chiaro incassato (la pagina con il fondo), la barra di utilizzo con la legenda, l'avviso del
 * riavvio, l'interruttore. La tabella dei processi e' quella di UIKit (tabella.h), con le righe
 * ad albero (la colonna Processo); non si ordina per colonna: l'ordine e' quello dell'albero.
 *
 * LA SPAZIATURA (la regola: un solo livello con il padding, uguale in x e in y). Il corpo della
 * finestra ha 8 intorno e 8 tra la barra degli strumenti e il pannello. Processi: il pannello
 * ha 8 di padding, le righe hanno 8 dentro perche' hanno un fondo (e l'intestazione e il piede
 * si allineano a loro): dal bordo del pannello al primo testo 8 + 8 = 16 in x, e in y 8 + 8 (il
 * testo alto 16 al centro dell'intestazione alta 32) = 16. Memoria e Disco: il pannello ha 16
 * di padding, i figli niente: 16 e 16. */
#include "uikit/uikit.h"
#include "corekit/sistema/launcher.h"     /* Riavvia: riaprire l'app */

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

/* --- la tabella dei processi: la tabella di UIKit (tabella.h), con le righe ad albero ---
 *
 * Le colonne del disegno "Attivita' - Processi": PID 48, Processo (prende quello che avanza, in
 * TESTO_PRIMARIO, ad albero), Utente 120, Stato 120, Memoria 100 (a destra). Nessuna si ordina.
 *
 * L'ALBERO: ogni processo sta sotto il suo padre (chi l'ha lanciato), i figli in ordine di PID.
 * Un clic sulla freccia apre o chiude il ramo (i rami chiusi si ricordano per PID). Con
 * l'interruttore spento non si vedono i servizi del sistema: i processi con il PID minore di
 * quello della scrivania (avvio, eventi, compositore, launcher); chi ha il padre nascosto sale
 * al primo livello. Un clic su una riga la sceglie. */

static const struct colonna_tk colonne[] = {
    { "PID", 48, 0, 0, 0 },
    { "Processo", 0, 0, 1, 0 },
    { "Utente", 120, 0, 0, 0 },
    { "Stato", 120, 0, 0, 0 },
    { "Memoria", 100, 1, 0, 0 },
};
enum { COL_PID, COL_PROCESSO, COL_UTENTE, COL_STATO, COL_MEMORIA };

static struct tabella_tk tabella;
static struct info_processo processi[MAX_PROCESSI];
static int quanti_processi;
static int scelto = -1;                /* il PID del processo scelto, o -1 */

/* le righe: quale processo (indice in processi), il livello e il ramo */
static int riga_processo[MAX_PROCESSI], riga_livello[MAX_PROCESSI], riga_ramo[MAX_PROCESSI];
static int quante_righe;
static int servizi_visibili;           /* l'interruttore "Mostra i servizi di sistema" */
static int rami_chiusi[MAX_PROCESSI];  /* i PID dei rami chiusi */
static int quanti_chiusi;

static int ramo_chiuso(int pid) {
    for (int i = 0; i < quanti_chiusi; i++) if (rami_chiusi[i] == pid) return 1;
    return 0;
}

static void apri_chiudi_ramo(int pid) {
    for (int i = 0; i < quanti_chiusi; i++)
        if (rami_chiusi[i] == pid) { rami_chiusi[i] = rami_chiusi[--quanti_chiusi]; return; }
    if (quanti_chiusi < MAX_PROCESSI) rami_chiusi[quanti_chiusi++] = pid;
}

static int stesso_testo(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

/* Il PID della scrivania (dal nome del programma), o 0 se non c'e'. */
static int pid_scrivania(void) {
    for (int i = 0; i < quanti_processi; i++)
        if (stesso_testo(processi[i].nome, "scrivania")) return processi[i].numero;
    return 0;
}

/* Rifa' le righe: quali processi si vedono, messi in fila ad albero (tabella_tk_albero). I
 * processi arrivano gia' in ordine di PID, quindi anche i figli di uno stesso padre. La riga
 * scelta segue il suo processo; se non si vede piu' (e' finito, o il suo ramo si e' chiuso),
 * non e' piu' scelto. */
static void fai_righe(void) {
    int primo = servizi_visibili ? 0 : pid_scrivania();
    int visti[MAX_PROCESSI], genitore[MAX_PROCESSI], quanti = 0;
    unsigned char chiuso[MAX_PROCESSI];
    for (int i = 0; i < quanti_processi; i++)
        if (processi[i].numero >= primo) visti[quanti++] = i;
    for (int j = 0; j < quanti; j++) {
        genitore[j] = -1;
        for (int k = 0; k < quanti; k++)
            if (processi[visti[k]].numero == processi[visti[j]].padre) genitore[j] = k;
        chiuso[j] = (unsigned char)ramo_chiuso(processi[visti[j]].numero);
    }
    quante_righe = tabella_tk_albero(quanti, genitore, chiuso, riga_processo, riga_livello, riga_ramo);
    int riga = -1;
    for (int k = 0; k < quante_righe; k++) {
        riga_processo[k] = visti[riga_processo[k]];
        if (processi[riga_processo[k]].numero == scelto) riga = k;
    }
    if (riga < 0) scelto = -1;
    tabella_tk_scegli(&tabella, riga);
}

static void dati_riga(struct tabella_tk *t, int k, struct riga_tk *r, void *dati) {
    (void)t; (void)dati;
    const struct info_processo *pr = &processi[riga_processo[k]];
    r->livello = riga_livello[k];
    r->ramo = riga_ramo[k];
    metti_numero(r->testi[COL_PID], (unsigned int)pr->numero);
    copia_testo(r->testi[COL_PROCESSO], pr->nome, MAX_CELLA);
    copia_testo(r->testi[COL_UTENTE], nome_utente(pr->uid), MAX_CELLA);
    copia_testo(r->testi[COL_STATO], nome_stato(pr->stato), MAX_CELLA);
    testo_byte(r->testi[COL_MEMORIA], pr->memoria);
}

/* LE RIGHE CHE SI VEDONO, come erano all'ultimo aggiornamento: ogni secondo si ridisegnano
 * solo le righe il cui testo e' cambiato davvero (di solito nessuna, o la memoria di un
 * processo), non tutta la tabella. */
static struct riga_tk righe_viste[MAX_PROCESSI];

static int stessa_riga(const struct riga_tk *a, const struct riga_tk *b) {
    for (int c = 0; c < MAX_COLONNE_TABELLA; c++)
        for (int i = 0; i < MAX_CELLA; i++) {
            if (a->testi[c][i] != b->testi[c][i]) return 0;
            if (!a->testi[c][i]) break;
        }
    return a->oggetto == b->oggetto && a->livello == b->livello && a->ramo == b->ramo;
}

/* Dopo aver rifatto le righe: quelle cambiate si ridisegnano; se e' cambiato quante sono,
 * tutta la tabella. */
static void righe_cambiate(void) {
    int prima = tabella.righe;
    if (quante_righe != prima) {
        int sopra = tabella.sopra;
        tabella_tk_aggiorna(&tabella, quante_righe);
        tabella.sopra = sopra < quante_righe ? sopra : -1;   /* la riga sotto il puntatore resta accesa */
    }
    for (int k = 0; k < quante_righe; k++) {
        struct riga_tk r = { .oggetto = -1 };
        dati_riga(&tabella, k, &r, 0);
        if (k >= prima || !stessa_riga(&r, &righe_viste[k])) {
            tabella_tk_riga_cambiata(&tabella, k);
            righe_viste[k] = r;
        }
    }
}

/* --- le pagine Memoria e Disco: il riepilogo, la barra di utilizzo e la legenda di UIKit --- */

struct pagina_grafico {
    struct pagina pagina;              /* il pannello: padding 16, SPAZIO_16 tra riepilogo e legenda */
    struct contenitore riepilogo;      /* il titolo e la barra, a SPAZIO_8 */
    struct paragrafo titolo;
    struct barra_utilizzo barra;
    struct legenda legenda;
    struct grafico grafico;
    struct grafico visto;              /* com'era quando lo si e' disegnato l'ultima volta */
};

static void pagina_grafico_prepara(struct pagina_grafico *pg, int quanti, const char *const *nomi,
                                   const int *toni, int resto) {
    pagina_prepara(&pg->pagina, SPAZIO_16, SPAZIO_16, SPAZIO_16, SPAZIO_16);
    pg->pagina.fondo = 1;
    pg->pagina.base.nascosto = 1;
    pg->grafico = (struct grafico){ .quanti = quanti, .nomi = nomi, .toni = toni, .resto = resto };
    colonna_prepara(&pg->riepilogo, SPAZIO_8);
    paragrafo_prepara(&pg->titolo, "", FONT_SEZIONE_NUOVO, INTERLINEA_SEZIONE, COLORE_ICONA_PRIMARIO);
    barra_utilizzo_prepara(&pg->barra, &pg->grafico);
    legenda_prepara(&pg->legenda, &pg->grafico);
    widget_aggiungi(&pg->riepilogo.base, &pg->titolo.base);
    widget_aggiungi(&pg->riepilogo.base, &pg->barra.base);
    pagina_aggiungi(&pg->pagina, &pg->riepilogo.base);
    pagina_aggiungi(&pg->pagina, &pg->legenda.base);
}

/* Il grafico e' stato riletto: la barra e la legenda si ridisegnano solo se quello che
 * mostrano e' cambiato (i numeri scritti nella legenda, i pezzi della barra). */
static void pagina_grafico_cambiata(struct pagina_grafico *pg) {
    if (!grafico_uguale(&pg->grafico, &pg->visto, pg->barra.base.l)) {
        widget_cambiato(&pg->barra.base);
        widget_cambiato(&pg->legenda.base);
    }
    pg->visto = pg->grafico;
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
static struct pagina area;                    /* il corpo sotto la barra: uno dei pannelli */
static struct pagina pagina_tabella, piede;
static struct contenitore riga_piede;
static struct paragrafo messaggio, riassunto_processi, etichetta_servizi;
static struct contenitore servizi;            /* l'etichetta e l'interruttore, a destra nel piede */
static struct interruttore interruttore_servizi;
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
    for (int i = 0; i < quanti_processi; i++)
        if (processi[i].numero == scelto) return &processi[i];
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

/* Sotto la tabella: "8 processi \xFA 34,6 MB in uso" (\xFA e' il punto a mezza altezza),
 * contando solo le righe che si vedono. */
static void aggiorna_riassunto_processi(void) {
    unsigned long long in_uso = 0;
    for (int k = 0; k < quante_righe; k++) in_uso += processi[riga_processo[k]].memoria;
    char testo[MAX_TESTO_WIDGET], *d = metti_numero(testo, (unsigned int)quante_righe);
    d = metti_testo(d, quante_righe == 1 ? " processo \xFA " : " processi \xFA ");
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
    int n = elenco_processi(processi, MAX_PROCESSI);
    quanti_processi = n < 0 ? 0 : n;
    for (int i = 0; i < quanti_processi; i++)
        if (processi[i].stato == STATO_IN_ESECUZIONE) mio_uid = (int)processi[i].uid;
    fai_righe();
    righe_cambiate();
    aggiorna_azioni();
    aggiorna_riassunto_processi();

    /* la memoria: quello che sanno i processi, la condivisa, la libera; il resto e' del kernel */
    struct info_memoria m;
    if (stato_memoria(&m) < 0) return;
    unsigned long long sistema = 0, applicazioni = 0;
    for (int i = 0; i < quanti_processi; i++) {
        if (processi[i].uid == UID_SISTEMA) sistema += processi[i].memoria;
        else applicazioni += processi[i].memoria;
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

/* Un clic su una riga la sceglie (sul vuoto, o col tasto destro, non cambia niente). */
static void al_clic_tabella_processi(struct tabella_tk *t, int k, int come, int x, int y, void *dati) {
    (void)x; (void)y; (void)dati;
    if (k < 0 || come == CLIC_DESTRO) return;
    scelto = processi[riga_processo[k]].numero;
    tabella_tk_scegli(t, k);
    scrivi_messaggio("");
    aggiorna_azioni();
}

/* Le righe sono da rifare (un ramo aperto o chiuso, l'interruttore). */
static void righe_da_rifare(void) {
    fai_righe();
    righe_cambiate();
    aggiorna_azioni();
    aggiorna_riassunto_processi();
}

/* Un clic sulla freccia di un ramo: lo apre o lo chiude. */
static void al_ramo_processi(struct tabella_tk *t, int k, void *dati) {
    (void)t; (void)dati;
    apri_chiudi_ramo(processi[riga_processo[k]].numero);
    righe_da_rifare();
}

/* L'interruttore "Mostra i servizi di sistema". */
static void al_cambio_servizi(struct interruttore *i, void *dati) {
    (void)dati;
    servizi_visibili = i->acceso;
    righe_da_rifare();
}

static void premi_termina(struct pulsante *b, void *dati) {
    (void)b; (void)dati;
    if (scelto < 0) return;
    if (termina_processo(scelto) < 0) {
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
    da_riavviare = scelto;
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

    /* sotto la barra, uno dei pannelli: quello dei processi (padding 8: la tabella e, sotto, il
     * piede alto 32 con 8 ai lati come le righe), oppure le pagine Memoria e Disco */
    pagina_prepara(&area, SPAZIO_0, SPAZIO_0, SPAZIO_0, SPAZIO_0);
    pagina_prepara(&pagina_tabella, SPAZIO_0, SPAZIO_8, SPAZIO_8, SPAZIO_8);
    pagina_tabella.fondo = 1;
    tabella_tk_prepara(&tabella, colonne, (int)(sizeof colonne / sizeof colonne[0]), dati_riga,
                       al_clic_tabella_processi, 0);
    tabella.colonna_albero = COL_PROCESSO;
    tabella.al_ramo = al_ramo_processi;
    pagina_aggiungi(&pagina_tabella, &tabella.base);

    /* il piede: alto ALTEZZA_S (32), 8 ai lati come le righe; dentro, centrati in altezza, il
     * conteggio (prende lo spazio), il messaggio e, a destra, "Mostra i servizi di sistema" con
     * l'interruttore (alto 24: e' lui a dare l'altezza alla riga, 4 sopra e 4 sotto) */
    pagina_prepara(&piede, SPAZIO_0, TABELLA_DENTRO, (ALTEZZA_S - INTERRUTTORE_ALTO) / 2,
                   (ALTEZZA_S - INTERRUTTORE_ALTO) / 2);
    piede.base.elastico = 0;
    riga_prepara(&riga_piede, SPAZIO_16);
    paragrafo_prepara(&riassunto_processi, "", FONT_PICCOLO_NUOVO, INTERLINEA_PICCOLO, COLORE_ICONA_SECONDARIO);
    riassunto_processi.base.largo = 0;
    riassunto_processi.base.elastico = 1;
    paragrafo_prepara(&messaggio, "", FONT_PICCOLO_NUOVO, INTERLINEA_PICCOLO, COLORE_ICONA_PRIMARIO);
    messaggio.base.largo = 0;
    riga_prepara(&servizi, SPAZIO_8);
    servizi.base.largo = 0;
    paragrafo_prepara(&etichetta_servizi, "Mostra i servizi di sistema", FONT_CORPO, INTERLINEA_CORPO,
                      COLORE_ICONA_PRIMARIO);
    etichetta_servizi.base.largo = 0;
    interruttore_prepara(&interruttore_servizi, 0, al_cambio_servizi, 0);
    widget_aggiungi(&servizi.base, &etichetta_servizi.base);
    widget_aggiungi(&servizi.base, &interruttore_servizi.base);
    widget_aggiungi(&riga_piede.base, &riassunto_processi.base);
    widget_aggiungi(&riga_piede.base, &messaggio.base);
    widget_aggiungi(&riga_piede.base, &servizi.base);
    pagina_aggiungi(&piede, &riga_piede.base);

    pagina_grafico_prepara(&memoria, PEZZI_MEMORIA, nomi_memoria, toni_memoria, LIBERA);
    pagina_grafico_prepara(&disco, PEZZI_DISCO, nomi_disco, toni_disco, DISCO_LIBERO);

    pagina_aggiungi(&pagina_tabella, &piede.base);
    pagina_aggiungi(&area, &pagina_tabella.base);
    pagina_aggiungi(&area, &memoria.pagina.base);
    pagina_aggiungi(&area, &disco.pagina.base);

    finestra_tk_prepara(&finestra, "Attivit\x85", &area.base);
    finestra_tk_barra(&finestra, &strumenti.base);
    finestra_tk_aree(&finestra);
    finestra_tk_dimensione(&finestra, 768, 526);   /* il disegno, schermata Processi */
    avviso_prepara(&avviso_riavvia, "Riavviare?",
                   "Il processo viene chiuso e aperto di nuovo. I dati non salvati andranno persi.",
                   "Riavvia", PERICOLO, 1, risposta_riavvia, 0);
    finestra_tk_avviso(&finestra, &avviso_riavvia);
    aggiorna(&finestra, 0);
    finestra_tk_ogni(&finestra, 1000, aggiorna, 0);
    return uikit_esegui(&finestra);
}
