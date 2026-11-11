/* Attivita': il task manager. Due pagine:
 *   - Processi: i programmi che girano (numero, nome, utente, stato, memoria), aggiornati
 *     ogni secondo. Se ne sceglie uno con un clic e lo si puo' terminare.
 *   - Memoria: com'e' usata la RAM, come la pagina "Archiviazione" del Mac: una barra
 *     divisa in pezzi colorati, e sotto la legenda con quanto pesa ogni pezzo.
 *
 * Tutto quello che mostra lo chiede al kernel (elenco_processi, memoria): e' come /proc
 * in Linux, ma con chiamate di sistema invece di file. Terminare segue la regola di Unix:
 * Attivita' gira come utente "applicazioni", quindi puo' terminare solo le applicazioni;
 * i servizi del sistema no. */
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
            pennello_colore(p, BLU_NORMALE);
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
    tabella_misura, tabella_disponi, tabella_disegna, tabella_puntatore, 0, tabella_rotella, 0
};

/* --- la memoria: la barra a pezzi e la legenda --- */

enum { KERNEL, SISTEMA, APPLICAZIONI, CONDIVISA, LIBERA, PEZZI };

static const char *nomi_pezzi[PEZZI] = {
    "Kernel", "Servizi del sistema", "Applicazioni", "Memoria condivisa (finestre)", "Libera"
};
static const unsigned int colori_pezzi[PEZZI] = {
    OPACO(0x94A3B8), BLU_NORMALE, VERDE_SOPRA, ARANCIO_SOPRA, VETRO_SUPERFICIE
};

struct grafico {                       /* la barra e la legenda leggono gli stessi numeri */
    unsigned long long byte[PEZZI];
    unsigned long long totale;
};
static struct grafico grafico;

struct barra { struct widget base; };

static void barra_misura(struct widget *w, int *l, int *a) {
    (void)w;
    *l = 400;
    *a = 28;
}

/* Ogni pezzo e' la barra intera ritagliata nella sua striscia: cosi' gli angoli tondi
 * restano solo ai due capi. */
static void barra_disegna(struct widget *w, struct pennello *p) {
    struct tondo t = tondo(w->x, w->y, w->l, w->a, RAGGIO_MEDIO);
    pennello_colore(p, colori_pezzi[LIBERA]);
    riempi_forma(p, &t);
    if (!grafico.totale) return;
    int x = w->x;
    unsigned long long somma = 0, totale = grafico.totale;
    for (int i = 0; i < LIBERA; i++) {
        somma += grafico.byte[i];
        int fine = w->x + (int)(somma * (unsigned int)w->l / totale);
        if (fine <= x) continue;
        pennello_salva(p);
        pennello_ritaglia(p, x, w->y, fine - x, w->a);
        pennello_colore(p, colori_pezzi[i]);
        riempi_forma(p, &t);
        pennello_ripristina(p);
        x = fine;
    }
    pennello_colore(p, CAMPO_BORDO);
    contorna_forma(p, &t, 256);
}

static const struct tipo_widget tipo_barra = { barra_misura, 0, barra_disegna, 0, 0, 0, 0 };

#define LEGENDA_RIGA 30

struct legenda { struct widget base; };

static void legenda_misura(struct widget *w, int *l, int *a) {
    (void)w;
    *l = 400;
    *a = PEZZI * LEGENDA_RIGA;
}

static void legenda_disegna(struct widget *w, struct pennello *p) {
    for (int i = 0; i < PEZZI; i++) {
        int y = w->y + i * LEGENDA_RIGA;
        pennello_colore(p, colori_pezzi[i]);
        riempi_tondo(p, w->x, y + (LEGENDA_RIGA - 12) / 2, 12, 12, 6);
        if (i == LIBERA) {
            pennello_colore(p, CONTROLLO_BORDO);
            contorna_tondo(p, w->x, y + (LEGENDA_RIGA - 12) / 2, 12, 12, 6, 256);
        }
        char quanto[16];
        metti_byte(quanto, grafico.byte[i]);
        pennello_colore(p, CONTENUTO_PRINCIPALE);
        testo_centrato(p, FONT_TESTO, w->x + 24, y, LEGENDA_RIGA, nomi_pezzi[i]);
        pennello_colore(p, CONTENUTO_SECONDARIO);
        testo_centrato(p, FONT_TESTO, w->x + w->l - larghezza_testo(FONT_TESTO, quanto), y,
                       LEGENDA_RIGA, quanto);
    }
}

static const struct tipo_widget tipo_legenda = { legenda_misura, 0, legenda_disegna, 0, 0, 0, 0 };

/* --- la finestra --- */

static struct finestra_tk finestra;
static struct contenitore colonna, schede, pagina_processi, azioni, pagina_memoria;
static struct pulsante scheda_processi, scheda_memoria, pulsante_termina;
static struct spazio dopo_schede;
static struct etichetta messaggio, riassunto;
static struct tabella tabella;
static struct barra barra;
static struct legenda legenda;

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

/* "Termina processo" si accende solo se il processo scelto si puo' terminare davvero. */
static void aggiorna_termina(void) {
    int si_puo = 0;
    for (int i = 0; i < tabella.quante; i++)
        if (tabella.righe[i].numero == tabella.scelto) si_puo = si_puo_terminare(&tabella.righe[i]);
    if (pulsante_termina.base.disabilitato == !si_puo) return;   /* niente di nuovo */
    widget_disabilita(&pulsante_termina.base, !si_puo);
}

/* Rilegge processi e memoria dal kernel. */
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
    grafico.totale = m.totale;
    grafico.byte[KERNEL] = m.totale > contati ? m.totale - contati : 0;
    grafico.byte[SISTEMA] = sistema;
    grafico.byte[APPLICAZIONI] = applicazioni;
    grafico.byte[CONDIVISA] = m.condivisa;
    grafico.byte[LIBERA] = m.libera;

    char testo[64], *d = metti_testo(testo, "In uso ");
    d = metti_byte(d, m.totale - m.libera);
    d = metti_testo(d, " su ");
    metti_byte(d, m.totale);
    etichetta_testo(&riassunto, testo);
    widget_cambiato(&barra.base);
}

static void scelta_cambiata(struct tabella *t) {
    (void)t;
    etichetta_testo(&messaggio, "");
    aggiorna_termina();
}

static void premi_termina(struct pulsante *p, void *dati) {
    (void)p; (void)dati;
    if (tabella.scelto < 0) return;
    if (termina_processo(tabella.scelto) < 0) {
        etichetta_testo(&messaggio, "Questo processo non si pu\x95 terminare");
        return;
    }
    etichetta_testo(&messaggio, "");
    aggiorna(&finestra, 0);
}

/* Le due schede: quella scelta e' blu, e si vede la sua pagina. */
static void mostra_pagina(struct pulsante *p, void *dati) {
    (void)dati;
    int memoria_scelta = p == &scheda_memoria;
    widget_nascondi(&pagina_processi.base, memoria_scelta);
    widget_nascondi(&pagina_memoria.base, !memoria_scelta);
    pulsante_stile(&scheda_processi, memoria_scelta ? NEUTRO : BLU, MEDIO);
    pulsante_stile(&scheda_memoria, memoria_scelta ? BLU : NEUTRO, MEDIO);
    toolkit_ridisponi(&finestra);
}

int main(void) {
    colonna_prepara(&colonna, SPAZIO_TRA);

    riga_prepara(&schede, 8);
    pulsante_prepara(&scheda_processi, "Processi", NESSUNA_ICONA, mostra_pagina, 0);
    pulsante_prepara(&scheda_memoria, "Memoria", NESSUNA_ICONA, mostra_pagina, 0);
    pulsante_stile(&scheda_processi, BLU, MEDIO);
    spazio_prepara(&dopo_schede);
    widget_aggiungi(&schede.base, &scheda_processi.base);
    widget_aggiungi(&schede.base, &scheda_memoria.base);
    widget_aggiungi(&schede.base, &dopo_schede.base);

    /* la pagina dei processi: la tabella, e sotto il messaggio e "Termina processo" */
    colonna_prepara(&pagina_processi, SPAZIO_TRA);
    pagina_processi.base.elastico = 1;
    tabella = (struct tabella){ .base = { .tipo = &tipo_tabella, .elastico = 1, .largo = 1 },
                                .scelto = -1, .al_cambio = scelta_cambiata };
    barra_scorrimento_prepara(&tabella.barra);
    widget_aggiungi(&tabella.base, &tabella.barra.base);
    riga_prepara(&azioni, 8);
    etichetta_prepara(&messaggio, "", FONT_TESTO);
    messaggio.colore = STATO_ERRORE;
    messaggio.base.elastico = 1;
    pulsante_prepara(&pulsante_termina, "Termina processo", ICONA_X, premi_termina, 0);
    pulsante_stile(&pulsante_termina, ROSSO, MEDIO);
    pulsante_termina.base.disabilitato = 1;
    widget_aggiungi(&azioni.base, &messaggio.base);
    widget_aggiungi(&azioni.base, &pulsante_termina.base);
    widget_aggiungi(&pagina_processi.base, &tabella.base);
    widget_aggiungi(&pagina_processi.base, &azioni.base);

    /* la pagina della memoria: il riassunto, la barra, la legenda */
    colonna_prepara(&pagina_memoria, SPAZIO_TRA + 4);
    pagina_memoria.base.elastico = 1;
    pagina_memoria.base.nascosto = 1;
    etichetta_prepara(&riassunto, "", FONT_TITOLO);
    riassunto.base.largo = 1;
    barra = (struct barra){ .base = { .tipo = &tipo_barra, .largo = 1 } };
    legenda = (struct legenda){ .base = { .tipo = &tipo_legenda, .largo = 1 } };
    widget_aggiungi(&pagina_memoria.base, &riassunto.base);
    widget_aggiungi(&pagina_memoria.base, &barra.base);
    widget_aggiungi(&pagina_memoria.base, &legenda.base);

    widget_aggiungi(&colonna.base, &schede.base);
    widget_aggiungi(&colonna.base, &pagina_processi.base);
    widget_aggiungi(&colonna.base, &pagina_memoria.base);

    finestra_tk_prepara(&finestra, "Attivit\x85", &colonna.base);
    finestra_tk_dimensione(&finestra, 640, 460);
    aggiorna(&finestra, 0);
    finestra_tk_ogni(&finestra, 1000, aggiorna, 0);
    return toolkit_esegui(&finestra);
}
