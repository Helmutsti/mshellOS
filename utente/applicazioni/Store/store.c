/* Store: le app installate, e l'installazione dei pacchetti .app (APPLICAZIONI.md, sezione 11).
 *
 * Due modi, a seconda di come parte:
 *   - SENZA ARGOMENTO (dall'isola): l'ELENCO delle app installate (ELENCO_APP), in due sezioni,
 *     "Le tue app" e "App del sistema". Un clic su una riga apre il DETTAGLIO dell'app al posto
 *     dell'elenco (nella barra degli strumenti, "App installate" per tornare): nome, autore,
 *     versione, descrizione, i permessi, "Cosa fa di diverso", i ruoli, e le azioni "Svuota dati"
 *     (ogni app) e "Togli app" (solo quelle della persona), con l'avviso di conferma di UIKit.
 *   - CON UN PERCORSO (lo fa partire il launcher quando si apre un file .app, APRI_FILE): la
 *     RICHIESTA DI INSTALLAZIONE o di AGGIORNAMENTO, oppure perche' non si puo' installare.
 *
 * Il lavoro vero lo fa il launcher (launcher.h): lo Store chiede e mostra. Puo' chiedere di
 * installare, togliere e svuotare perche' e' un'app del sistema con il permesso "app.gestire".
 *
 * L'ASPETTO e' quello della pagina "App - Store" del file Figma, fatto con i componenti di UIKit:
 * la pagina (l'area dei contenuti chiara, che scorre), il paragrafo, la riga dell'app, la riga di
 * permesso con l'etichetta di stato, il messaggio in linea, l'icona dell'app, lo stato vuoto, i
 * pulsanti, l'avviso. Dove il disegno non dice niente, le scelte sono scritte vicino al codice.
 *
 * LA SPAZIATURA (un solo livello con il margine, uguale in x e in y): l'elenco ha 16 ai lati e 24
 * sopra; le righe dell'app hanno 8 di margine interno (hanno un fondo) e i titoli delle sezioni si
 * allineano a loro: 16 + 8 = 24 in x, 24 in y. Il dettaglio e la richiesta: 24 e 24.
 *
 * Ogni passo importante lo scrive anche nella seriale ("store: ..."): lo guardano le prove
 * (test/prove/store.py). */
#include "uikit/uikit.h"
#include "launcher.h"
#include "testo437.h"

#define QUANTI(v) ((int)(sizeof(v) / sizeof((v)[0])))

/* --- piccoli aiuti sui testi (UTF-8; i componenti li vogliono nella code page 437) --- */

static void aggiungi(char *dove, const char *da, int massimo) { descrizione_aggiungi(dove, da, massimo); }

/* Traduce sul posto da UTF-8 alla code page 437 (il testo non si allunga mai). */
static void in_437_qui(char *s, int massimo) { in_437(s, s, massimo); }

static void registro(const char *a, const char *b) {
    scrivi_testo("store: ");
    scrivi_testo(a);
    scrivi_testo(b);
    scrivi_testo("\n");
}

static void numero_in(char *dove, int n) {
    char c[12];
    int i = 11;
    c[i] = 0;
    do { c[--i] = (char)('0' + n % 10); n /= 10; } while (n);
    descrizione_copia(dove, &c[i], 12);
}

static const char *nome_di(const struct manifesto *m) { return m->nome[0] ? m->nome : m->id; }

/* "<a><nome><b>", per esempio "Togliere \"Note\"?". Le virgolette basse del disegno il font
 * non le ha: diventano dritte. */
static void con_nome(char *dove, int massimo, const char *a, const char *nome, const char *b) {
    dove[0] = 0;
    aggiungi(dove, a, massimo);
    aggiungi(dove, nome, massimo);
    aggiungi(dove, b, massimo);
}

/* --- i nomi del sistema, spiegati (APPLICAZIONI.md, sezione 9; i testi dal disegno) --- */

static const struct { const char *nome, *titolo, *cosa; int simbolo; } permessi_noti[] = {
    { "file.leggere", "Leggere i tuoi file", "Leggere tutti i file della tua Home", ICONA_FOLDER },
    { "file.scrivere", "Modificare i tuoi file", "Leggere e modificare tutti i file della tua Home", ICONA_FOLDER },
    { "notifiche", "Notifiche", "Mandare notifiche", ICONA_BELL },
    { "rete", "Rete", "Usare la rete e internet", ICONA_GLOBE },
    { "audio", "Audio", "Riprodurre suoni", ICONA_VOLUME_2 },
    { "processi", "Processi", "Vedere i programmi in esecuzione e fermarli", ICONA_ACTIVITY },
    { "impostazioni.sistema", "Impostazioni del sistema", "Cambiare tema, lingua e schermo per tutti", ICONA_SETTINGS },
    { "spegnimento", "Spegnimento", "Spegnere e riavviare il computer", ICONA_POWER },
    { "app.gestire", "Gestire le app", "Installare, aggiornare e togliere app", ICONA_PACKAGE },
};

static const struct { const char *nome, *titolo, *cosa; int simbolo; } deviazioni_note[] = {
    { "senza_finestra", "Non ha una finestra", "Lavora senza mostrare niente: mentre \xc3\xa8 accesa la trovi in Attivit\xc3\xa0.", ICONA_EYE_OFF },
    { "sottofondo", "Resta accesa in sottofondo", "Continua a lavorare anche quando chiudi la finestra. Finch\xc3\xa9 \xc3\xa8 accesa vedrai un segno nella barra.", ICONA_LAYERS },
    { "avvio_automatico", "Parte da sola quando entri", "Si avvia appena entri nel computer. Puoi spegnere questa scelta nelle Impostazioni.", ICONA_POWER },
    { "schermo_intero", "Pu\xc3\xb2 coprire tutto lo schermo", "Anche la barra: c'\xc3\xa8 sempre un tasto per uscire.", ICONA_SCAN },
};

/* I ruoli e le loro funzioni obbligatorie: un'app fa da quel ruolo se le offre tutte. */
#define MAX_FUNZIONI_RUOLO 2
static const struct { const char *nome; const char *funzioni[MAX_FUNZIONI_RUOLO]; const char *cosa; } ruoli[] = {
    { "File", { "file.apri_cartella", "file.mostra" }, "Apre le cartelle e mostra i file quando un'altra app lo chiede." },
    { "Browser", { "web.apri", 0 }, "Apre le pagine web quando un'altra app lo chiede." },
    { "Mail", { "mail.scrivi", 0 }, "Pu\xc3\xb2 fare da app per la posta: quando un'altra app vuole mandare una mail, il sistema pu\xc3\xb2 passarla a lei." },
    { "Immagini", { "immagine.apri", 0 }, "Apre le immagini quando un'altra app lo chiede." },
    { "Testo", { "testo.apri", "testo.nuovo" }, "Pu\xc3\xb2 aprire e creare testi quando un'altra app lo chiede." },
    { "Terminale", { "terminale.apri", 0 }, "Apre un terminale in una cartella quando un'altra app lo chiede." },
};

static int dichiara(const char (*voci)[MAX_VOCE], int quante, const char *v) {
    for (int i = 0; i < quante && i < MAX_VOCI; i++)
        if (descrizione_uguali(voci[i], v)) return 1;
    return 0;
}

static int fa_ruolo(const struct manifesto *m, int r) {
    for (int f = 0; f < MAX_FUNZIONI_RUOLO && ruoli[r].funzioni[f]; f++)
        if (!dichiara(m->funzioni, m->quante_funzioni, ruoli[r].funzioni[f])) return 0;
    return 1;
}

/* --- la finestra e l'area dei contenuti --- */

static struct finestra_tk finestra;
static struct pagina area;                     /* l'area dei contenuti, chiara, sotto le barre */

static void ridisponi(void) {
    if (finestra.base.l > 0) uikit_ridisponi(&finestra);
}

static void chiudi_store(struct pulsante *b, void *dati) {
    (void)b; (void)dati;
    finestra_tk_chiudi(&finestra);
}

/* --- le sezioni con le righe di permesso: permessi, cosa fa di diverso, ruoli, cosa cambia ---
 *
 * Dal disegno: una colonna a SPAZIO_8 con l'intestazione (il titolo in semigrassetto 14 alto 20,
 * e a destra un eventuale pulsante), la spiegazione facoltativa (FONT_PICCOLO_NUOVO, 16) e le
 * righe di permesso, a SPAZIO_16 l'una dall'altra. */

#define MAX_RIGHE_SEZIONE (MAX_VOCI + 4)

struct sezione {
    struct contenitore colonna, testa, righe;
    struct paragrafo titolo, spiegazione;
    int quante;
    struct riga_permesso riga[MAX_RIGHE_SEZIONE];
};

static struct sezione sez_permessi, sez_diverso, sez_ruoli, sez_cambia;

static void sezione_prepara(struct sezione *s, const char *titolo, const char *spiegazione, struct widget *pulsante) {
    colonna_prepara(&s->colonna, SPAZIO_8);
    riga_prepara(&s->testa, SPAZIO_8);
    colonna_prepara(&s->righe, SPAZIO_16);
    paragrafo_prepara(&s->titolo, titolo, FONT_TITOLO_FINESTRA, INTERLINEA_CORPO, TESTO_PRIMARIO);
    in_437_qui(s->titolo.testo, PARAGRAFO_TESTO);
    s->titolo.base.elastico = 1;
    widget_aggiungi(&s->testa.base, &s->titolo.base);
    if (pulsante) widget_aggiungi(&s->testa.base, pulsante);
    widget_aggiungi(&s->colonna.base, &s->testa.base);
    if (spiegazione) {
        paragrafo_prepara(&s->spiegazione, spiegazione, FONT_PICCOLO_NUOVO, INTERLINEA_PICCOLO, TESTO_SECONDARIO);
        in_437_qui(s->spiegazione.testo, PARAGRAFO_TESTO);
        widget_aggiungi(&s->colonna.base, &s->spiegazione.base);
    }
    widget_aggiungi(&s->colonna.base, &s->righe.base);
    s->quante = 0;
}

static void sezione_riga(struct sezione *s, int simbolo, const char *nome, const char *cosa,
                         const char *stato, int tipo_stato) {
    if (s->quante >= MAX_RIGHE_SEZIONE) return;
    struct riga_permesso *r = &s->riga[s->quante++];
    riga_permesso_prepara(r, simbolo, nome, cosa, stato, tipo_stato);
    in_437_qui(r->nome, MAX_TESTO_WIDGET);
    in_437_qui(r->spiegazione, (int)sizeof r->spiegazione);
    in_437_qui(r->stato, MAX_TESTO_WIDGET);
    widget_aggiungi(&s->righe.base, &r->base);
}

/* I permessi del manifesto. 'stato': l'etichetta (nel dettaglio "Concesso"), o 0. */
static void righe_permessi(struct sezione *s, const struct manifesto *m, const char *stato) {
    for (int i = 0; i < m->quanti_permessi && i < MAX_VOCI; i++) {
        int k = 0;
        while (k < QUANTI(permessi_noti) && !descrizione_uguali(permessi_noti[k].nome, m->permessi[i])) k++;
        if (k < QUANTI(permessi_noti))
            sezione_riga(s, permessi_noti[k].simbolo, permessi_noti[k].titolo, permessi_noti[k].cosa, stato, ETICHETTA_SUCCESSO);
        else
            sezione_riga(s, ICONA_LOCK, m->permessi[i], "Un permesso che il sistema non conosce", stato, ETICHETTA_SUCCESSO);
    }
    if (!m->quanti_permessi) {
        char cosa[160];
        con_nome(cosa, sizeof cosa, "", nome_di(m), " non chiede permessi.");
        sezione_riga(s, ICONA_CIRCLE_CHECK, "Nessuno", cosa, 0, 0);
    }
}

static void righe_deviazioni(struct sezione *s, const struct manifesto *m) {
    for (int i = 0; i < m->quante_deviazioni && i < MAX_VOCI; i++) {
        int k = 0;
        while (k < QUANTI(deviazioni_note) && !descrizione_uguali(deviazioni_note[k].nome, m->deviazioni[i])) k++;
        if (k < QUANTI(deviazioni_note))
            sezione_riga(s, deviazioni_note[k].simbolo, deviazioni_note[k].titolo, deviazioni_note[k].cosa, 0, 0);
        else
            sezione_riga(s, ICONA_TRIANGLE_ALERT, m->deviazioni[i], "Una differenza che il sistema non conosce", 0, 0);
    }
    if (!m->quante_deviazioni) {
        char cosa[200];
        con_nome(cosa, sizeof cosa, "", nome_di(m), " si comporta come ogni app: ha una finestra e si ferma quando la chiudi.");
        sezione_riga(s, ICONA_CIRCLE_CHECK, "Nessuna", cosa, 0, 0);
    }
}

/* I ruoli; 0 se non ne ha (allora la sezione non si mostra: il disegno non ha il caso). */
static int righe_ruoli(struct sezione *s, const struct manifesto *m) {
    int quanti = 0;
    for (int r = 0; r < QUANTI(ruoli); r++)
        if (fa_ruolo(m, r)) { sezione_riga(s, ICONA_BADGE_CHECK, ruoli[r].nome, ruoli[r].cosa, 0, 0); quanti++; }
    return quanti;
}

/* --- l'intestazione dell'app: icona 64, nome (titolo 24/32), autore e versione, descrizione;
 * a destra, nel dettaglio, le azioni. In riga a SPAZIO_16, centrati in altezza. --- */

static struct contenitore testa_app, testi_app, azioni_app;
static struct icona_app icona_grande;
static struct paragrafo p_nome, p_autore, p_descrizione;

static void intestazione_app(const struct manifesto *m, const char *sotto, int con_azioni) {
    riga_prepara(&testa_app, SPAZIO_16);
    colonna_prepara(&testi_app, SPAZIO_4);
    testi_app.base.elastico = 1;
    icona_app_prepara(&icona_grande, 64);
    paragrafo_prepara(&p_nome, nome_di(m), FONT_TITOLO_NUOVO, INTERLINEA_TITOLO, TESTO_PRIMARIO);
    in_437_qui(p_nome.testo, PARAGRAFO_TESTO);
    paragrafo_prepara(&p_autore, sotto, FONT_CORPO, INTERLINEA_CORPO, TESTO_SECONDARIO);
    in_437_qui(p_autore.testo, PARAGRAFO_TESTO);
    paragrafo_prepara(&p_descrizione, m->descrizione, FONT_CORPO, INTERLINEA_CORPO, TESTO_PRIMARIO);
    in_437_qui(p_descrizione.testo, PARAGRAFO_TESTO);
    widget_aggiungi(&testi_app.base, &p_nome.base);
    widget_aggiungi(&testi_app.base, &p_autore.base);
    if (m->descrizione[0]) widget_aggiungi(&testi_app.base, &p_descrizione.base);
    widget_aggiungi(&testa_app.base, &icona_grande.base);
    widget_aggiungi(&testa_app.base, &testi_app.base);
    if (con_azioni) widget_aggiungi(&testa_app.base, &azioni_app.base);
}

/* "Mario Rossi \xc2\xb7 versione 1.0.0" */
static void autore_versione(char *dove, int massimo, const struct manifesto *m, const char *dopo) {
    dove[0] = 0;
    aggiungi(dove, m->autore[0] ? m->autore : "Autore sconosciuto", massimo);
    aggiungi(dove, " \xc2\xb7 versione ", massimo);
    aggiungi(dove, m->versione[0] ? m->versione : "?", massimo);
    if (dopo) aggiungi(dove, dopo, massimo);
}

/* ================= LA RICHIESTA DI INSTALLAZIONE (con un file .app) ================= */

static const char *pacchetto;                  /* il percorso del file */
static struct resoconto resoconto;             /* quello che dice il launcher (e' grande) */
static struct manifesto app;                   /* l'app del pacchetto, per dopo */
static struct elenco_app installate;           /* dal launcher: per l'aggiornamento, e per l'elenco */
static struct pagina pg_richiesta, pg_azioni;
static struct contenitore riga_azioni;
static struct spazio spinta;
static struct pulsante p_annulla, p_installa, p_aggiorna, p_apri, p_chiudi;
static struct messaggio avvisi[4], errore;
static struct stato_vuoto esito;
static char testo_apri[MAX_TESTO_WIDGET];

/* Il nome del file, senza le cartelle. */
static const char *nome_file(const char *percorso) {
    const char *nome = percorso;
    for (const char *c = percorso; *c; c++) if (*c == '/') nome = c + 1;
    return nome;
}

static void mostra_pulsanti(int con_annulla, int con_installa, int con_aggiorna, int con_apri, int con_chiudi) {
    widget_nascondi(&p_annulla.base, !con_annulla);
    widget_nascondi(&p_installa.base, !con_installa);
    widget_nascondi(&p_aggiorna.base, !con_aggiorna);
    widget_nascondi(&p_apri.base, !con_apri);
    widget_nascondi(&p_chiudi.base, !con_chiudi);
}

static int gia_installata(const struct resoconto *r) {
    return r->gia_installata && r->versione_installata[0] && versione_valida(r->m.versione) &&
           confronta_versioni(r->m.versione, r->versione_installata) <= 0;
}

static int da_mostrare(const struct problema *p, int gia) {
    return !(gia && p->blocca && descrizione_uguali(p->voce, "versione"));
}

/* 1 se 'pezzo' sta dentro 's'. */
static int contiene(const char *s, const char *pezzo) {
    for (; *s; s++) {
        int i = 0;
        while (pezzo[i] && s[i] == pezzo[i]) i++;
        if (!pezzo[i]) return 1;
    }
    return 0;
}

/* Il titolo del messaggio "non si puo' installare": i tre casi del disegno, e uno generico. */
static const char *titolo_errore(const struct problema *p) {
    if (descrizione_uguali(p->voce, "processore")) return "Non si pu\xc3\xb2 installare. \xc3\x88 fatta per un altro processore";
    if (descrizione_uguali(p->voce, "sistema_minimo")) return "Non si pu\xc3\xb2 installare. Serve un sistema pi\xc3\xb9 recente";
    if (descrizione_uguali(p->voce, "id") && contiene(p->testo, "app del sistema"))
        return "Non si pu\xc3\xb2 installare. Il nome \xc3\xa8 gi\xc3\xa0 di un'app del sistema";
    return "Non si pu\xc3\xb2 installare";
}

static void messaggio_utf8(struct messaggio *m, int tipo, const char *titolo, const char *testo) {
    messaggio_prepara(m, tipo, titolo, testo);
    in_437_qui(m->titolo, (int)sizeof m->titolo);
    in_437_qui(m->spiegazione, MESSAGGIO_TESTO);
}

/* Lo stato vuoto con testi in UTF-8, in mezzo alla pagina. */
static void mostra_esito(int simbolo, const char *titolo, const char *spiegazione) {
    stato_vuoto_prepara(&esito, simbolo, titolo, spiegazione);
    in_437_qui(esito.titolo, MAX_TESTO_WIDGET);
    in_437_qui(esito.spiegazione, (int)sizeof esito.spiegazione);
    pagina_svuota(&pg_richiesta);
    pg_richiesta.margine_x = pg_richiesta.margine_sopra = pg_richiesta.margine_sotto = SPAZIO_0;
    pagina_aggiungi(&pg_richiesta, &esito.base);
}

/* Il pacchetto non si legge (o non e' un pacchetto): lo stato vuoto e "Chiudi". */
static void mostra_illeggibile(int si_apre) {
    char riga[200];
    con_nome(riga, sizeof riga, "Il file \"", nome_file(pacchetto), "\" \xc3\xa8 danneggiato o non \xc3\xa8 un pacchetto di app.");
    mostra_esito(ICONA_TRIANGLE_ALERT, "Impossibile leggere il pacchetto", riga);
    stato_vuoto_colore_icona(&esito, COLORE_ICONA_PRIMARIO);   /* un errore: come nel disegno */
    mostra_pulsanti(0, 0, 0, 0, 1);
    registro(si_apre ? "pacchetto danneggiato: " : "pacchetto illeggibile: ", pacchetto);
}

/* La sezione "Cosa cambia" dell'aggiornamento: la versione, i permessi nuovi, le differenze
 * nuove (confrontando con l'app installata, dall'elenco del launcher). */
static void cosa_cambia(const struct resoconto *r) {
    const struct manifesto *prima = 0;
    if (elenco_app_installate(&installate) == LAUNCHER_FATTO)
        for (int i = 0; i < installate.quante; i++)
            if (descrizione_uguali(installate.app[i].m.id, r->m.id)) prima = &installate.app[i].m;
    sezione_prepara(&sez_cambia, "Cosa cambia", "I tuoi dati e i permessi che hai gi\xc3\xa0 dato restano come sono.", 0);
    char riga[200];
    con_nome(riga, sizeof riga, "Da ", r->versione_installata, " a ");
    aggiungi(riga, r->m.versione, sizeof riga);
    sezione_riga(&sez_cambia, ICONA_ARROW_RIGHT, "Versione", riga, 0, 0);
    for (int i = 0; i < r->m.quanti_permessi && i < MAX_VOCI; i++) {
        if (prima && dichiara(prima->permessi, prima->quanti_permessi, r->m.permessi[i])) continue;
        int k = 0;
        while (k < QUANTI(permessi_noti) && !descrizione_uguali(permessi_noti[k].nome, r->m.permessi[i])) k++;
        char titolo[96], cosa[200];
        con_nome(titolo, sizeof titolo, "Nuovo permesso: ", k < QUANTI(permessi_noti) ? permessi_noti[k].titolo : r->m.permessi[i], "");
        con_nome(cosa, sizeof cosa, "", k < QUANTI(permessi_noti) ? permessi_noti[k].cosa : "Un permesso nuovo", ". Te lo chieder\xc3\xa0 al prossimo avvio.");
        sezione_riga(&sez_cambia, k < QUANTI(permessi_noti) ? permessi_noti[k].simbolo : ICONA_LOCK, titolo, cosa,
                     "Nuovo", ETICHETTA_INFORMAZIONE);
    }
    int nuove = 0;
    for (int i = 0; i < r->m.quante_deviazioni && i < MAX_VOCI; i++) {
        if (prima && dichiara(prima->deviazioni, prima->quante_deviazioni, r->m.deviazioni[i])) continue;
        int k = 0;
        while (k < QUANTI(deviazioni_note) && !descrizione_uguali(deviazioni_note[k].nome, r->m.deviazioni[i])) k++;
        char titolo[96];
        con_nome(titolo, sizeof titolo, "Nuovo in \"Cosa fa di diverso\": ", k < QUANTI(deviazioni_note) ? deviazioni_note[k].titolo : r->m.deviazioni[i], "");
        sezione_riga(&sez_cambia, k < QUANTI(deviazioni_note) ? deviazioni_note[k].simbolo : ICONA_TRIANGLE_ALERT, titolo,
                     k < QUANTI(deviazioni_note) ? deviazioni_note[k].cosa : "", "Nuovo", ETICHETTA_INFORMAZIONE);
        nuove++;
    }
    if (!nuove) {
        con_nome(riga, sizeof riga, "", nome_di(&r->m), r->m.quante_deviazioni ? " continua a fare le stesse cose di prima."
                                                                              : " continua a comportarsi come ogni app.");
        sezione_riga(&sez_cambia, ICONA_CIRCLE_CHECK, "Niente di nuovo in \"Cosa fa di diverso\"", riga, 0, 0);
    }
}

/* La richiesta: cosa c'e' nel pacchetto e cosa si puo' fare. */
static void mostra_richiesta(void) {
    struct resoconto *r = &resoconto;
    app = r->m;
    const char *nome = nome_di(&app);
    int gia = gia_installata(r), errori = 0;
    for (int i = 0; i < r->quanti; i++)
        if (r->problemi[i].blocca && da_mostrare(&r->problemi[i], gia)) errori++;
    int aggiornamento = !gia && r->gia_installata && !errori;
    char riga[200];

    con_nome(riga, sizeof riga, aggiornamento ? "Aggiorna \"" : "Installa \"", nome, "\"");
    in_437_qui(riga, sizeof riga);
    finestra_tk_titolo(&finestra, riga);

    pagina_svuota(&pg_richiesta);
    if (aggiornamento) {
        con_nome(riga, sizeof riga, app.autore[0] ? app.autore : "Autore sconosciuto", " \xc2\xb7 da ", r->versione_installata);
        aggiungi(riga, " a ", sizeof riga);
        aggiungi(riga, app.versione, sizeof riga);
    } else {
        autore_versione(riga, sizeof riga, &app, 0);
    }
    intestazione_app(&app, riga, 0);
    pagina_aggiungi(&pg_richiesta, &testa_app.base);

    if (errori) {                                  /* i motivi, uno per messaggio */
        int n = 0;
        for (int i = 0; i < r->quanti && n < QUANTI(avvisi); i++) {
            const struct problema *p = &r->problemi[i];
            if (!p->blocca || !da_mostrare(p, gia)) continue;
            messaggio_utf8(&avvisi[n], MESSAGGIO_ERRORE, titolo_errore(p), p->testo);
            pagina_aggiungi(&pg_richiesta, &avvisi[n++].base);
        }
        mostra_pulsanti(0, 0, 0, 0, 1);
        registro("non si puo' installare: ", app.id);
        return;
    }
    if (gia) {                                     /* non e' nel disegno: lo si dice come informazione */
        con_nome(riga, sizeof riga, "Sul computer c'\xc3\xa8 gi\xc3\xa0 la versione ", r->versione_installata,
                 ": non c'\xc3\xa8 niente da aggiornare.");
        char titolo[120];
        con_nome(titolo, sizeof titolo, "", nome, " \xc3\xa8 gi\xc3\xa0 installata");
        messaggio_utf8(&avvisi[0], MESSAGGIO_INFORMAZIONE, titolo, riga);
        pagina_aggiungi(&pg_richiesta, &avvisi[0].base);
        con_nome(testo_apri, MAX_TESTO_WIDGET, "Apri ", nome, "");
        in_437_qui(testo_apri, MAX_TESTO_WIDGET);
        copia_testo(p_apri.testo, testo_apri, MAX_TESTO_WIDGET);
        mostra_pulsanti(0, 0, 0, 1, 1);
        registro("gia' installata: ", app.id);
        return;
    }
    if (aggiornamento) {
        cosa_cambia(r);
        pagina_aggiungi(&pg_richiesta, &sez_cambia.colonna.base);
        mostra_pulsanti(1, 0, 1, 0, 0);
        registro("richiesta di aggiornamento: ", app.id);
        return;
    }

    /* gli avvisi: le icone mancanti con i testi del disegno, gli altri con la frase del
     * launcher (le deviazioni no: sono gia' in "Cosa fa di diverso") */
    int n = 0, icone_dette = 0;
    for (int i = 0; i < r->quanti && n < QUANTI(avvisi); i++) {
        const struct problema *p = &r->problemi[i];
        if (p->blocca || descrizione_uguali(p->voce, "deviazioni")) continue;
        if (descrizione_uguali(p->voce, "icone") && r->svg_presente[0] != r->svg_presente[1]) {
            if (icone_dette++) continue;
            int manca_scuro = !r->svg_presente[1];
            messaggio_utf8(&avvisi[n], MESSAGGIO_AVVISO,
                           manca_scuro ? "Manca l'icona per il tema scuro" : "Manca l'icona per il tema chiaro",
                           manca_scuro ? "L'app si installa lo stesso. In tema scuro vedrai l'icona chiara."
                                       : "L'app si installa lo stesso. In tema chiaro vedrai l'icona scura.");
        } else {
            messaggio_utf8(&avvisi[n], MESSAGGIO_AVVISO, "Da sapere", p->testo);
        }
        pagina_aggiungi(&pg_richiesta, &avvisi[n++].base);
    }
    sezione_prepara(&sez_permessi, "Permessi che chieder\xc3\xa0",
                    "Non li concedi adesso: te li chieder\xc3\xa0 l'app al primo avvio. Potrai cambiarli quando vuoi nelle Impostazioni.", 0);
    righe_permessi(&sez_permessi, &app, 0);
    pagina_aggiungi(&pg_richiesta, &sez_permessi.colonna.base);
    char spiega[200];
    con_nome(spiega, sizeof spiega, "Di solito un'app ha una finestra e si ferma quando la chiudi. ", nome, " fa anche queste cose:");
    sezione_prepara(&sez_diverso, "Cosa fa di diverso", app.quante_deviazioni ? spiega : 0, 0);
    righe_deviazioni(&sez_diverso, &app);
    pagina_aggiungi(&pg_richiesta, &sez_diverso.colonna.base);
    sezione_prepara(&sez_ruoli, "Ruoli", 0, 0);
    if (righe_ruoli(&sez_ruoli, &app)) pagina_aggiungi(&pg_richiesta, &sez_ruoli.colonna.base);
    mostra_pulsanti(1, 1, 0, 0, 0);
    registro("richiesta di installazione: ", app.id);
}

/* "Installa" o "Aggiorna": lo fa il launcher; poi il risultato. */
static void premi_installa(struct pulsante *b, void *dati) {
    (void)dati;
    int aggiornare = b == &p_aggiorna;
    int esito_launcher = launcher_pacchetto(aggiornare ? AGGIORNA_PACCHETTO : INSTALLA_PACCHETTO, pacchetto, &resoconto);
    const char *nome = nome_di(&app);
    char titolo[120];
    if (esito_launcher == LAUNCHER_FATTO) {
        con_nome(titolo, sizeof titolo, "", nome, aggiornare ? " \xc3\xa8 aggiornata" : " \xc3\xa8 installata");
        mostra_esito(ICONA_CIRCLE_CHECK, titolo,
                     aggiornare ? "La trovi tra le tue app, con i tuoi dati di prima."
                                : "La trovi tra le tue app. Al primo avvio ti chieder\xc3\xa0 i permessi.");
        con_nome(testo_apri, MAX_TESTO_WIDGET, "Apri ", nome, "");
        in_437_qui(testo_apri, MAX_TESTO_WIDGET);
        copia_testo(p_apri.testo, testo_apri, MAX_TESTO_WIDGET);
        mostra_pulsanti(0, 0, 0, 1, 1);
        registro(aggiornare ? "aggiornata " : "installata ", app.id);
        ridisponi();
        return;
    }
    /* non riuscita: il motivo in cima alla richiesta (il disegno non ha questo caso) */
    con_nome(titolo, sizeof titolo, "", nome, aggiornare ? " non \xc3\xa8 stata aggiornata" : " non \xc3\xa8 stata installata");
    const char *perche = esito_launcher == LAUNCHER_NON_PERMESSO ? "Lo Store non ha il permesso di installare app."
                       : "Il sistema non ha risposto: riprova tra poco.";
    for (int i = 0; i < resoconto.quanti; i++)
        if (resoconto.problemi[i].blocca) { perche = resoconto.problemi[i].testo; break; }
    messaggio_utf8(&errore, MESSAGGIO_ERRORE, titolo, perche);
    pagina_svuota(&pg_richiesta);
    pagina_aggiungi(&pg_richiesta, &testa_app.base);
    pagina_aggiungi(&pg_richiesta, &errore.base);
    mostra_pulsanti(0, 0, 0, 0, 1);
    registro("installazione non riuscita: ", app.id);
    ridisponi();
}

/* "Apri <nome>": il launcher fa partire l'app, e lo Store ha finito. */
static void premi_apri(struct pulsante *b, void *dati) {
    (void)b; (void)dati;
    registro("apro ", app.id);
    apri_applicazione(app.id);
    finestra_tk_chiudi(&finestra);
}

static int richiesta(const char *percorso) {
    pacchetto = percorso;
    pagina_prepara(&area, SPAZIO_0, SPAZIO_0, SPAZIO_0, SPAZIO_0);
    area.fondo = 1;
    pagina_prepara(&pg_richiesta, SPAZIO_24, SPAZIO_24, SPAZIO_24, SPAZIO_24);
    pg_richiesta.scorre = 1;
    /* le azioni in fondo, a destra: 24 ai lati e sotto (sopra c'e' gia' il margine della pagina) */
    pagina_prepara(&pg_azioni, SPAZIO_0, SPAZIO_24, SPAZIO_0, SPAZIO_24);
    pg_azioni.base.elastico = 0;
    riga_prepara(&riga_azioni, SPAZIO_8);
    spazio_prepara(&spinta);
    pulsante_prepara(&p_annulla, "Annulla", NESSUNA_ICONA, chiudi_store, 0);
    pulsante_prepara(&p_chiudi, "Chiudi", NESSUNA_ICONA, chiudi_store, 0);
    pulsante_prepara(&p_installa, "Installa", ICONA_DOWNLOAD, premi_installa, 0);
    pulsante_prepara(&p_aggiorna, "Aggiorna", ICONA_DOWNLOAD, premi_installa, 0);
    pulsante_prepara(&p_apri, "Apri", NESSUNA_ICONA, premi_apri, 0);
    pulsante_stile(&p_installa, PRIMARIO, MISURA_M);
    pulsante_stile(&p_aggiorna, PRIMARIO, MISURA_M);
    pulsante_stile(&p_apri, PRIMARIO, MISURA_M);
    widget_aggiungi(&riga_azioni.base, &spinta.base);
    widget_aggiungi(&riga_azioni.base, &p_annulla.base);
    widget_aggiungi(&riga_azioni.base, &p_chiudi.base);
    widget_aggiungi(&riga_azioni.base, &p_installa.base);
    widget_aggiungi(&riga_azioni.base, &p_aggiorna.base);
    widget_aggiungi(&riga_azioni.base, &p_apri.base);
    pagina_aggiungi(&pg_azioni, &riga_azioni.base);
    pagina_aggiungi(&area, &pg_richiesta.base);
    pagina_aggiungi(&area, &pg_azioni.base);
    finestra_tk_prepara(&finestra, "Store", &area.base);
    finestra_tk_bordo(&finestra);

    /* la misura dipende da cosa si mostra (il disegno: 600 per la richiesta, 560 se non si
     * puo' installare, 480 per il pacchetto illeggibile); poi non cambia */
    int esito_launcher = launcher_pacchetto(DESCRIVI_PACCHETTO, pacchetto, &resoconto);
    if (esito_launcher == LAUNCHER_ERRORE && !resoconto.manifesto_letto && !resoconto.quanti) {
        mostra_illeggibile(0);
        finestra_tk_dimensione(&finestra, 480, 257);
    } else if (!resoconto.manifesto_letto) {
        mostra_illeggibile(1);
        finestra_tk_dimensione(&finestra, 480, 257);
    } else {
        mostra_richiesta();
        int errori = !p_chiudi.base.nascosto && p_apri.base.nascosto;
        if (errori) finestra_tk_dimensione(&finestra, 560, 345);
        else finestra_tk_dimensione(&finestra, 600, 600);   /* nel disegno 900: non ci sta nello schermo, scorre */
    }
    return uikit_esegui(&finestra);
}

/* ================= L'ELENCO DELLE APP INSTALLATE (senza argomento) ================= */

static struct barra_strumenti strumenti;
static struct pulsante p_indietro, p_gestisci, p_svuota, p_togli;
static struct pagina pg_elenco, pg_dettaglio;
static struct messaggio esito_elenco, esito_dettaglio;
static struct avviso avviso;                   /* uno solo: lo si prepara a ogni domanda */
static int scelta = -1;                        /* l'app del dettaglio (indice in 'installate') */

/* Una sezione dell'elenco: il titolo (e la spiegazione) con 8 di margine, come il testo delle
 * righe; le righe (a SPAZIO_4) in blocchi da 15 (un widget ha al massimo 16 figli). */
#define RIGHE_BLOCCO 15
#define MAX_BLOCCHI  6

struct sezione_elenco {
    struct contenitore colonna, righe;
    struct pagina testa;                       /* margine 8 ai lati: si allinea al testo delle righe */
    struct paragrafo titolo, spiegazione;
};

static struct sezione_elenco sez_tue, sez_sistema;
static struct stato_vuoto vuoto_tue;
static struct riga_app righe[MAX_APP_ELENCO];
static struct contenitore blocchi[MAX_BLOCCHI];
static int blocchi_usati;

static void sezione_elenco_prepara(struct sezione_elenco *s, const char *titolo, const char *spiegazione) {
    colonna_prepara(&s->colonna, SPAZIO_8);
    colonna_prepara(&s->righe, SPAZIO_4);
    pagina_prepara(&s->testa, SPAZIO_8, SPAZIO_8, SPAZIO_0, SPAZIO_0);
    s->testa.base.elastico = 0;
    paragrafo_prepara(&s->titolo, titolo, FONT_TITOLO_FINESTRA, INTERLINEA_CORPO, TESTO_PRIMARIO);
    pagina_aggiungi(&s->testa, &s->titolo.base);
    if (spiegazione) {
        paragrafo_prepara(&s->spiegazione, spiegazione, FONT_PICCOLO_NUOVO, INTERLINEA_PICCOLO, TESTO_SECONDARIO);
        pagina_aggiungi(&s->testa, &s->spiegazione.base);
    }
    widget_aggiungi(&s->colonna.base, &s->testa.base);
    widget_aggiungi(&s->colonna.base, &s->righe.base);
}

static void apri_dettaglio(struct riga_app *r, void *dati);

static void sezione_elenco_riga(struct sezione_elenco *s, int i, int *in_blocco) {
    if (*in_blocco == 0 || *in_blocco == RIGHE_BLOCCO) {
        if (blocchi_usati >= MAX_BLOCCHI) return;
        colonna_prepara(&blocchi[blocchi_usati], SPAZIO_4);
        widget_aggiungi(&s->righe.base, &blocchi[blocchi_usati++].base);
        *in_blocco = 0;
    }
    const struct manifesto *m = &installate.app[i].m;
    char dettagli[MAX_TESTO_WIDGET * 2];
    autore_versione(dettagli, sizeof dettagli, m, 0);
    riga_app_prepara(&righe[i], nome_di(m), dettagli, apri_dettaglio, 0);
    in_437_qui(righe[i].nome, MAX_TESTO_WIDGET);
    in_437_qui(righe[i].dettagli, (int)sizeof righe[i].dettagli);
    widget_aggiungi(&blocchi[blocchi_usati - 1].base, &righe[i].base);
    (*in_blocco)++;
}

/* Rilegge le app dal launcher e rifa' l'elenco; 'esito' (o 0) e' il messaggio in cima. */
static void mostra_elenco_app(struct messaggio *esito_in_cima) {
    if (elenco_app_installate(&installate) != LAUNCHER_FATTO) installate.quante = 0;
    int tue = 0;
    for (int i = 0; i < installate.quante; i++) tue += !installate.app[i].di_sistema;
    char conto[48] = "", n[12];
    numero_in(conto, installate.quante);
    aggiungi(conto, " app, ", sizeof conto);
    numero_in(n, tue);
    aggiungi(conto, n, sizeof conto);
    aggiungi(conto, " tue", sizeof conto);
    registro("elenco: ", conto);

    pagina_svuota(&pg_elenco);
    blocchi_usati = 0;
    if (esito_in_cima) pagina_aggiungi(&pg_elenco, &esito_in_cima->base);
    sezione_elenco_prepara(&sez_tue, "Le tue app", 0);
    sezione_elenco_prepara(&sez_sistema, "App del sistema", "Fanno parte di mshell e si aggiornano con il sistema.");
    int in_blocco = 0;
    for (int i = 0; i < installate.quante; i++)
        if (!installate.app[i].di_sistema) sezione_elenco_riga(&sez_tue, i, &in_blocco);
    if (!tue) {
        stato_vuoto_prepara(&vuoto_tue, ICONA_PACKAGE, "Non hai ancora installato app",
                            "Per installarne una, apri il suo file .app, per esempio da Files.");
        vuoto_tue.base.elastico = 0;
        widget_aggiungi(&sez_tue.righe.base, &vuoto_tue.base);
    }
    in_blocco = 0;
    for (int i = 0; i < installate.quante; i++)
        if (installate.app[i].di_sistema) sezione_elenco_riga(&sez_sistema, i, &in_blocco);
    pagina_aggiungi(&pg_elenco, &sez_tue.colonna.base);
    pagina_aggiungi(&pg_elenco, &sez_sistema.colonna.base);

    scelta = -1;
    widget_nascondi(&strumenti.base, 1);
    widget_nascondi(&pg_dettaglio.base, 1);
    widget_nascondi(&pg_elenco.base, 0);
    uikit_sotto(&finestra, 0);
    ridisponi();
}

/* Il dettaglio dell'app 'scelta', con in cima (se c'e') l'esito dell'ultima azione. */
static void mostra_dettaglio(struct messaggio *esito_in_cima) {
    const struct app_installata *a = &installate.app[scelta];
    const struct manifesto *m = &a->m;
    char riga[200];
    autore_versione(riga, sizeof riga, m, a->di_sistema ? " \xc2\xb7 app del sistema" : 0);
    widget_nascondi(&p_togli.base, a->di_sistema);   /* le app del sistema non si tolgono */
    intestazione_app(m, riga, 1);

    pagina_svuota(&pg_dettaglio);
    pagina_aggiungi(&pg_dettaglio, &testa_app.base);
    if (esito_in_cima) pagina_aggiungi(&pg_dettaglio, &esito_in_cima->base);
    /* se i permessi siano concessi non e' ancora scritto da nessuna parte: si mostra lo stato
     * di partenza del disegno, "Concesso" */
    sezione_prepara(&sez_permessi, "Permessi",
                    a->di_sistema ? "Fa parte del sistema: si aggiorna con mshell e non si pu\xc3\xb2 togliere. I permessi si cambiano nelle Impostazioni."
                                  : "Li hai scelti al primo avvio. Puoi cambiarli nelle Impostazioni.",
                    &p_gestisci.base);
    righe_permessi(&sez_permessi, m, "Concesso");
    pagina_aggiungi(&pg_dettaglio, &sez_permessi.colonna.base);
    sezione_prepara(&sez_diverso, "Cosa fa di diverso", 0, 0);
    righe_deviazioni(&sez_diverso, m);
    pagina_aggiungi(&pg_dettaglio, &sez_diverso.colonna.base);
    sezione_prepara(&sez_ruoli, "Ruoli", 0, 0);
    if (righe_ruoli(&sez_ruoli, m)) pagina_aggiungi(&pg_dettaglio, &sez_ruoli.colonna.base);

    widget_nascondi(&strumenti.base, 0);
    widget_nascondi(&pg_elenco.base, 1);
    widget_nascondi(&pg_dettaglio.base, 0);
    uikit_sotto(&finestra, 0);
    ridisponi();
}

static void apri_dettaglio(struct riga_app *r, void *dati) {
    (void)dati;
    scelta = (int)(r - righe);
    mostra_dettaglio(0);
}

static void torna_elenco(struct pulsante *b, void *dati) {
    (void)b; (void)dati;
    mostra_elenco_app(0);
}

/* "Gestisci in Impostazioni": apre le Impostazioni (che oggi non hanno ancora una pagina per i
 * permessi delle app). */
static void apri_impostazioni(struct pulsante *b, void *dati) {
    (void)b; (void)dati;
    registro("apro ", "impostazioni");
    apri_applicazione("impostazioni");
}

static void scelta_svuota(struct avviso *a, int risposta, void *dati);
static void scelta_togli(struct avviso *a, int risposta, void *dati);

static void prepara_avviso(const char *titolo, const char *messaggio, const char *conferma, al_risposta_avviso al_risposta) {
    avviso_prepara(&avviso, titolo, messaggio, conferma, PERICOLO, 1, al_risposta, 0);
    in_437_qui(avviso.titolo, MAX_TESTO_WIDGET);
    in_437_qui(avviso.messaggio, (int)sizeof avviso.messaggio);
    finestra_tk_avviso(&finestra, &avviso);
}

static void chiedi_svuota(struct pulsante *b, void *dati) {
    (void)b; (void)dati;
    char titolo[MAX_TESTO_WIDGET];
    con_nome(titolo, sizeof titolo, "Cancellare i dati di \"", nome_di(&installate.app[scelta].m), "\"?");
    prepara_avviso(titolo, "L'app rester\xc3\xa0 installata ma ripartir\xc3\xa0 da zero: impostazioni, salvataggi e "
                           "tutto quello che ha conservato per te.", "Svuota dati", scelta_svuota);
    avviso_apri(&avviso);
}

static void chiedi_togli(struct pulsante *b, void *dati) {
    (void)b; (void)dati;
    char titolo[MAX_TESTO_WIDGET];
    con_nome(titolo, sizeof titolo, "Togliere \"", nome_di(&installate.app[scelta].m), "\"?");
    prepara_avviso(titolo, "Puoi togliere anche i dati che l'app ha salvato per te, oppure tenerli: se la "
                           "reinstalli, li ritrover\xc3\xa0.", "Togli app e dati", scelta_togli);
    avviso_alternativa(&avviso, "Togli solo l'app", SECONDARIO);
    avviso_apri(&avviso);
}

static void scelta_svuota(struct avviso *a, int risposta, void *dati) {
    (void)a; (void)dati;
    if (risposta != AVVISO_CONFERMA) return;
    struct risposta_launcher r;
    const struct manifesto *m = &installate.app[scelta].m;
    int fatto = svuota_dati_app(m->id, &r) == LAUNCHER_FATTO;
    registro(fatto ? "dati svuotati di " : "dati non svuotati di ", m->id);
    char testo[200];
    if (fatto) {
        con_nome(testo, sizeof testo, "Al prossimo avvio ", nome_di(m), " ripartir\xc3\xa0 da zero.");
        messaggio_utf8(&esito_dettaglio, MESSAGGIO_INFORMAZIONE, "Dati svuotati", testo);
    } else {
        messaggio_utf8(&esito_dettaglio, MESSAGGIO_ERRORE, "I dati non sono stati svuotati",
                       r.messaggio[0] ? r.messaggio : "Non si \xc3\xa8 riusciti a svuotare i dati.");
    }
    mostra_dettaglio(&esito_dettaglio);
}

/* Togli: conferma = "Togli app e dati", alternativa = "Togli solo l'app" (i dati restano). */
static void scelta_togli(struct avviso *a, int risposta, void *dati) {
    (void)a; (void)dati;
    if (risposta != AVVISO_CONFERMA && risposta != AVVISO_ALTERNATIVA) return;
    int tieni_dati = risposta == AVVISO_ALTERNATIVA;
    struct risposta_launcher r;
    char id[MAX_ID], nome[MAX_NOME_APP];
    descrizione_copia(id, installate.app[scelta].m.id, MAX_ID);
    descrizione_copia(nome, nome_di(&installate.app[scelta].m), MAX_NOME_APP);
    int esito_launcher = togli_app(id, tieni_dati, &r);
    registro(esito_launcher == LAUNCHER_FATTO ? (tieni_dati ? "tolta (dati tenuti) " : "tolta ") : "non tolta ", id);
    char titolo[120];
    if (esito_launcher != LAUNCHER_FATTO && esito_launcher != LAUNCHER_ERRORE) {   /* c'e' ancora: si resta li' */
        con_nome(titolo, sizeof titolo, "\"", nome, "\" non \xc3\xa8 stata tolta");
        messaggio_utf8(&esito_dettaglio, MESSAGGIO_ERRORE, titolo,
                       r.messaggio[0] ? r.messaggio : "Non si \xc3\xa8 riusciti a togliere l'app.");
        mostra_dettaglio(&esito_dettaglio);
        return;
    }
    if (esito_launcher == LAUNCHER_FATTO) {
        con_nome(titolo, sizeof titolo, "\"", nome, "\" \xc3\xa8 stata tolta");
        messaggio_utf8(&esito_elenco, MESSAGGIO_INFORMAZIONE, titolo,
                       tieni_dati ? "I dati che aveva salvato per te restano: se la reinstalli, li ritrover\xc3\xa0."
                                  : "Anche i dati che aveva salvato per te sono stati cancellati.");
    } else {
        con_nome(titolo, sizeof titolo, "\"", nome, "\" forse non \xc3\xa8 stata tolta del tutto");
        messaggio_utf8(&esito_elenco, MESSAGGIO_ERRORE, titolo,
                       r.messaggio[0] ? r.messaggio : "Non si \xc3\xa8 riusciti a togliere l'app.");
    }
    mostra_elenco_app(&esito_elenco);
}

static int elenco(void) {
    /* la barra degli strumenti (solo nel dettaglio): "App installate", ghost S con la freccia */
    barra_strumenti_prepara(&strumenti);
    pulsante_prepara(&p_indietro, "App installate", ICONA_CHEVRON_LEFT, torna_elenco, 0);
    pulsante_stile(&p_indietro, GHOST, MISURA_S);
    barra_strumenti_aggiungi(&strumenti, &p_indietro.base);
    /* le azioni del dettaglio: Secondario M; "Gestisci in Impostazioni": ghost S con la sua icona */
    riga_prepara(&azioni_app, SPAZIO_8);
    pulsante_prepara(&p_svuota, "Svuota dati", NESSUNA_ICONA, chiedi_svuota, 0);
    pulsante_prepara(&p_togli, "Togli app", NESSUNA_ICONA, chiedi_togli, 0);
    widget_aggiungi(&azioni_app.base, &p_svuota.base);
    widget_aggiungi(&azioni_app.base, &p_togli.base);
    pulsante_prepara(&p_gestisci, "Gestisci in Impostazioni", ICONA_EXTERNAL_LINK, apri_impostazioni, 0);
    pulsante_stile(&p_gestisci, GHOST, MISURA_S);

    pagina_prepara(&area, SPAZIO_0, SPAZIO_0, SPAZIO_0, SPAZIO_0);
    area.fondo = 1;
    /* l'elenco: 16 ai lati (le righe hanno 8 dentro) e 24 sopra e sotto; il dettaglio: 24 */
    pagina_prepara(&pg_elenco, SPAZIO_24, SPAZIO_16, SPAZIO_24, SPAZIO_24);
    pg_elenco.scorre = 1;
    pagina_prepara(&pg_dettaglio, SPAZIO_24, SPAZIO_24, SPAZIO_24, SPAZIO_24);
    pg_dettaglio.scorre = 1;
    pagina_aggiungi(&area, &pg_elenco.base);
    pagina_aggiungi(&area, &pg_dettaglio.base);

    finestra_tk_prepara(&finestra, "Store", &area.base);
    finestra_tk_barra(&finestra, &strumenti.base);
    finestra_tk_bordo(&finestra);
    finestra_tk_dimensione(&finestra, 760, 600);   /* nel disegno 640: in basso uscirebbe dallo schermo */
    mostra_elenco_app(0);
    return uikit_esegui(&finestra);
}

int main(int quanti, char **argomenti) {
    if (quanti > 1 && argomenti[1][0]) return richiesta(argomenti[1]);
    return elenco();
}
