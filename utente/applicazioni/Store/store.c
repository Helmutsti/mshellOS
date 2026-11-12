/* Store: le app installate, e l'installazione dei pacchetti .app.
 *
 * Due modi, a seconda di come parte:
 *   - SENZA ARGOMENTO (dall'isola): l'ELENCO delle app installate (ELENCO_APP), in due
 *     gruppi, "Le tue app" e "App del sistema", ogni riga con nome, autore e versione.
 *     Scegliendo una riga, a destra il DETTAGLIO: versione, autore, descrizione, i permessi
 *     che dichiara (se la persona li ha concessi non e' ancora scritto da nessuna parte: si
 *     mostra solo l'elenco), le deviazioni ("Cosa fa di diverso"), i ruoli. Sotto, "Svuota
 *     dati" per ogni app e "Togli app" solo per quelle installate dalla persona, ognuno con
 *     la sua domanda di conferma (nello stesso posto del dettaglio: UIKit non ha
 *     ancora le finestre di dialogo);
 *   - CON UN PERCORSO (lo fa partire il launcher quando si apre un file .app, APRI_FILE):
 *     la RICHIESTA DI INSTALLAZIONE. Si chiede al launcher di descrivere il pacchetto
 *     (DESCRIVI_PACCHETTO) e si mostra cosa c'e' dentro: nome, autore, versione,
 *     descrizione, i permessi che chiedera' al primo avvio, le deviazioni, i ruoli, gli
 *     avvisi e gli errori. Poi i pulsanti: "Installa" (app nuova), "Aggiorna" (una versione
 *     piu' nuova di un'app gia' installata), solo "Chiudi" se ci sono errori. Dopo
 *     l'installazione: "<nome> e' installata", con "Apri" e "Chiudi".
 *
 * Il lavoro vero lo fa il launcher (launcher.h): lo Store chiede e mostra. Puo' chiedere di
 * installare, togliere e svuotare perche' e' un'app del sistema con il permesso
 * "app.gestire" (manifest.toml).
 *
 * L'aspetto e' provvisorio, fatto con i widget di UIKit: quello vero arrivera' dal
 * disegno in Figma.
 *
 * Ogni passo importante lo scrive anche nella seriale ("store: ..."): lo guardano le prove
 * (test/prove/store.py). */
#include "uikit/uikit.h"
#include "launcher.h"
#include "testo437.h"

/* --- piccoli aiuti sui testi (UTF-8, prima di tradurli per il font) --- */

static void aggiungi(char *dove, const char *da, int massimo) { descrizione_aggiungi(dove, da, massimo); }

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

/* --- i nomi del sistema, spiegati (APPLICAZIONI.md, sezione 9) --- */

static const struct { const char *nome, *cosa; int di_sistema; } permessi_noti[] = {
    { "file.leggere", "Leggere tutti i tuoi file (la tua Home)", 0 },
    { "file.scrivere", "Leggere e modificare tutti i tuoi file", 0 },
    { "notifiche", "Mandarti notifiche", 0 },
    { "rete", "Usare la rete e internet", 0 },
    { "audio", "Riprodurre suoni", 0 },
    { "processi", "Vedere i programmi in esecuzione e fermarli", 1 },
    { "impostazioni.sistema", "Cambiare le impostazioni del sistema (tema, lingua, schermo)", 1 },
    { "spegnimento", "Spegnere e riavviare il computer", 1 },
    { "app.gestire", "Installare, aggiornare e togliere app", 1 },
};

static const struct { const char *nome, *cosa; } deviazioni_note[] = {
    { "senza_finestra", "Non mostra nessuna finestra: mentre lavora la trovi tra quelle attive in Attivit\xc3\xa0." },
    { "sottofondo", "Resta accesa anche dopo che chiudi la sua finestra: finch\xc3\xa9 \xc3\xa8 accesa vedrai un segno." },
    { "avvio_automatico", "Parte da sola quando entri: la puoi spegnere dalle Impostazioni." },
    { "schermo_intero", "Pu\xc3\xb2 coprire tutto lo schermo, barra compresa: c'\xc3\xa8 sempre un tasto per uscire." },
};

/* I ruoli e le loro funzioni obbligatorie: un'app fa da quel ruolo se le offre tutte. */
#define MAX_FUNZIONI_RUOLO 2
static const struct { const char *nome; const char *funzioni[MAX_FUNZIONI_RUOLO]; } ruoli[] = {
    { "Gestore dei file", { "file.apri_cartella", "file.mostra" } },
    { "Browser", { "web.apri", 0 } },
    { "Mail", { "mail.scrivi", 0 } },
    { "Immagini", { "immagine.apri", 0 } },
    { "Testo", { "testo.apri", "testo.nuovo" } },
    { "Terminale", { "terminale.apri", 0 } },
};

#define QUANTI(v) ((int)(sizeof(v) / sizeof((v)[0])))

static int dichiara_funzione(const struct manifesto *m, const char *f) {
    for (int i = 0; i < m->quante_funzioni && i < MAX_VOCI; i++)
        if (descrizione_uguali(m->funzioni[i], f)) return 1;
    return 0;
}

/* --- il documento: un widget che mostra testi uno sotto l'altro, andando a capo ---
 *
 * Fatto qui, per lo Store: titoli, sezioni, paragrafi, punti di un elenco, avvisi ed
 * errori (con la loro icona), e in cima la "testata" di un'app (il posto dell'icona, il
 * nome, autore e versione). Se non ci sta tutto, scorre con la rotella (la barra di
 * scorrimento di UIKit, come la tabella di Attivita'). I testi si danno in UTF-8: li
 * traduce lui per il font. */

enum { D_TESTATA, D_TITOLO, D_SEZIONE, D_TESTO, D_NOTA, D_PUNTO, D_AVVISO, D_ERRORE, D_FATTO };

#define MAX_VOCI_DOC  64
#define MAX_TESTO_DOC 400
#define DOC_ICONA     56               /* il posto dell'icona dell'app, nella testata */
#define DOC_DESTRA    SPAZIO_16        /* a destra resta lo spazio della barra di scorrimento */
#define DOC_RIENTRO   24               /* il testo di punti, avvisi ed errori */

struct voce_doc {
    int tipo;
    char testo[MAX_TESTO_DOC];         /* gia' nella code page 437 */
    char sotto[MAX_TESTO_DOC / 2];     /* la testata: la riga sotto il nome */
};

struct documento {
    struct widget base;
    struct voce_doc voci[MAX_VOCI_DOC];
    int quante;
    struct barra_scorrimento barra;
};

static int interlinea(const struct font *f) { return f->altezza + 4; }

/* Scrive 's' andando a capo tra le parole perche' stia in 'largo' pixel (una parola piu'
 * lunga si taglia). Con p == 0 misura soltanto. Quanto e' alto. */
static int a_capo(struct pennello *p, const struct font *f, int x, int y, int largo, const char *s) {
    char riga[MAX_TESTO_DOC];
    int alto = 0;
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int i = 0, buono = 0, l = 0;
        while (s[i]) {
            l += glifo_di(f, (unsigned char)s[i])->avanzo;
            if (l > largo) break;
            i++;
            if (s[i] == ' ' || !s[i]) buono = i;           /* qui si puo' andare a capo */
        }
        if (!s[i]) buono = i;                              /* il resto ci sta tutto */
        else if (!buono) buono = i ? i : 1;                /* una parola sola, troppo lunga */
        for (int k = 0; k < buono; k++) riga[k] = s[k];
        riga[buono] = 0;
        if (p) testo(p, f, x, y + alto, riga);
        alto += interlinea(f);
        s += buono;
    }
    return alto;
}

/* Mette in fila le voci: con p == 0 misura soltanto, se no disegna (con la cima del
 * contenuto in 'y0'). L'altezza di tutto. */
static int documento_impagina(struct documento *d, struct pennello *p, int y0) {
    const struct widget *w = &d->base;
    int x = w->x, largo = w->l - DOC_DESTRA, y = 0;
    if (largo < 40) largo = 40;
    for (int i = 0; i < d->quante; i++) {
        const struct voce_doc *v = &d->voci[i];
        int prima = i > 0;
        switch (v->tipo) {
        case D_TESTATA:
            if (p) {
                pennello_colore(p, CONTROLLO_SFONDO_SOPRA);   /* il posto dell'icona: arrivera' l'SVG */
                riempi_tondo(p, x, y0 + y, DOC_ICONA, DOC_ICONA, 14);
                pennello_colore(p, TESTO_SECONDARIO);
                icona(p, ICONA_SQUARE, 24, x + (DOC_ICONA - 24) / 2, y0 + y + (DOC_ICONA - 24) / 2);
                pennello_colore(p, TESTO_PRIMARIO);
                testo(p, FONT_TITOLO_NUOVO, x + DOC_ICONA + SPAZIO_16, y0 + y + 4, v->testo);
                pennello_colore(p, TESTO_SECONDARIO);
                testo(p, FONT_CORPO, x + DOC_ICONA + SPAZIO_16, y0 + y + 8 + FONT_TITOLO_NUOVO->altezza, v->sotto);
            }
            y += DOC_ICONA + SPAZIO_16;
            break;
        case D_TITOLO:
            if (p) pennello_colore(p, TESTO_PRIMARIO);
            y += a_capo(p, FONT_TITOLO_NUOVO, x, y0 + y, largo, v->testo) + SPAZIO_12;
            break;
        case D_SEZIONE:
            if (prima) y += SPAZIO_12;
            if (p) pennello_colore(p, TESTO_PRIMARIO);
            y += a_capo(p, FONT_SEZIONE_NUOVO, x, y0 + y, largo, v->testo) + SPAZIO_4;
            break;
        case D_TESTO:
        case D_NOTA:
            if (p) pennello_colore(p, v->tipo == D_TESTO ? TESTO_PRIMARIO : TESTO_SECONDARIO);
            y += a_capo(p, FONT_CORPO, x, y0 + y, largo, v->testo) + SPAZIO_4;
            break;
        case D_PUNTO:
            if (p) {
                pennello_colore(p, TESTO_SECONDARIO);
                riempi_tondo(p, x + 8, y0 + y + interlinea(FONT_CORPO) / 2 - 2, 5, 5, 2);
                pennello_colore(p, TESTO_PRIMARIO);
            }
            y += a_capo(p, FONT_CORPO, x + DOC_RIENTRO, y0 + y, largo - DOC_RIENTRO, v->testo) + SPAZIO_4;
            break;
        default: {                                         /* avviso, errore, fatto: con l'icona */
            int quale = v->tipo == D_AVVISO ? ICONA_TRIANGLE_ALERT : v->tipo == D_ERRORE ? ICONA_X : ICONA_CHECK;
            unsigned int colore = v->tipo == D_ERRORE ? STATO_ERRORE : TESTO_PRIMARIO;
            if (p) {
                pennello_colore(p, colore);
                icona(p, quale, 16, x, y0 + y + (interlinea(FONT_CORPO) - 16) / 2);
            }
            y += a_capo(p, FONT_CORPO, x + DOC_RIENTRO, y0 + y, largo - DOC_RIENTRO, v->testo) + SPAZIO_8;
            break;
        }
        }
    }
    return y;
}

static void documento_misure(struct documento *d) {
    scorrimento_misure(&d->barra.scorrimento, documento_impagina(d, 0, 0), d->base.a);
}

static void documento_misura(struct widget *w, int *l, int *a) { (void)w; *l = 240; *a = 120; }

static void documento_disponi(struct widget *w) {
    struct documento *d = (struct documento *)w;
    barra_scorrimento_posto(&d->barra, w->x, w->y, w->l, w->a);
    documento_misure(d);
}

static void documento_disegna(struct widget *w, struct pennello *p) {
    struct documento *d = (struct documento *)w;
    pennello_salva(p);
    pennello_ritaglia(p, w->x, w->y, w->l, w->a);
    documento_impagina(d, p, w->y - d->barra.scorrimento.posizione);
    pennello_ripristina(p);
}

static void documento_rotella(struct widget *w, int sx, int sy) {
    (void)sx;
    barra_scorrimento_rotella(&((struct documento *)w)->barra, sy);
}

static const struct tipo_widget tipo_documento = {
    documento_misura, documento_disponi, documento_disegna, 0, 0, documento_rotella, 0, 0, 0
};

static void documento_prepara(struct documento *d) {
    *d = (struct documento){ .base = { .tipo = &tipo_documento, .elastico = 1, .largo = 1 } };
    barra_scorrimento_prepara(&d->barra);
    widget_aggiungi(&d->base, &d->barra.base);
}

/* Da capo: niente voci, in cima. */
static void documento_svuota(struct documento *d) {
    d->quante = 0;
    d->barra.scorrimento.posizione = 0;
    widget_cambiato(&d->base);
}

static void voce(struct documento *d, int tipo, const char *testo_utf8) {
    if (d->quante >= MAX_VOCI_DOC) return;
    struct voce_doc *v = &d->voci[d->quante++];
    v->tipo = tipo;
    in_437(v->testo, testo_utf8, MAX_TESTO_DOC);
    v->sotto[0] = 0;
}

/* La testata di un'app: il posto dell'icona, il nome, "autore · versione". */
static void testata(struct documento *d, const struct manifesto *m) {
    char sotto[MAX_TESTO_DOC / 2] = "";
    aggiungi(sotto, m->autore[0] ? m->autore : "Autore sconosciuto", sizeof sotto);
    aggiungi(sotto, " \xc2\xb7 versione ", sizeof sotto);
    aggiungi(sotto, m->versione[0] ? m->versione : "?", sizeof sotto);
    voce(d, D_TESTATA, m->nome[0] ? m->nome : m->id);
    in_437(d->voci[d->quante - 1].sotto, sotto, (int)sizeof d->voci[0].sotto);
}

/* I permessi, le deviazioni e i ruoli dichiarati nel manifesto, ognuno nella sua sezione.
 * 'titolo_permessi' cambia tra la richiesta di installazione e il dettaglio. */
static void elenchi_del_manifesto(struct documento *d, const struct manifesto *m, const char *titolo_permessi) {
    char riga[MAX_TESTO_DOC];
    voce(d, D_SEZIONE, titolo_permessi);
    if (!m->quanti_permessi) voce(d, D_NOTA, "Nessun permesso.");
    for (int i = 0; i < m->quanti_permessi && i < MAX_VOCI; i++) {
        int k = 0;
        while (k < QUANTI(permessi_noti) && !descrizione_uguali(permessi_noti[k].nome, m->permessi[i])) k++;
        riga[0] = 0;
        aggiungi(riga, k < QUANTI(permessi_noti) ? permessi_noti[k].cosa : m->permessi[i], sizeof riga);
        if (k < QUANTI(permessi_noti) && permessi_noti[k].di_sistema) aggiungi(riga, " (permesso di sistema)", sizeof riga);
        voce(d, D_PUNTO, riga);
    }

    voce(d, D_SEZIONE, "Cosa fa di diverso");
    if (!m->quante_deviazioni) voce(d, D_NOTA, "Niente: segue lo standard.");
    for (int i = 0; i < m->quante_deviazioni && i < MAX_VOCI; i++) {
        int k = 0;
        while (k < QUANTI(deviazioni_note) && !descrizione_uguali(deviazioni_note[k].nome, m->deviazioni[i])) k++;
        voce(d, D_PUNTO, k < QUANTI(deviazioni_note) ? deviazioni_note[k].cosa : m->deviazioni[i]);
    }

    voce(d, D_SEZIONE, "Ruoli");
    int quanti = 0;
    for (int r = 0; r < QUANTI(ruoli); r++) {
        int tutte = 1;
        for (int f = 0; f < MAX_FUNZIONI_RUOLO && ruoli[r].funzioni[f]; f++)
            if (!dichiara_funzione(m, ruoli[r].funzioni[f])) tutte = 0;
        if (tutte) { voce(d, D_PUNTO, ruoli[r].nome); quanti++; }
    }
    if (!quanti) voce(d, D_NOTA, "Nessuno.");
}

/* --- la finestra --- */

static struct finestra_tk finestra;

static void ridisponi(void) {
    if (finestra.base.l > 0) uikit_ridisponi(&finestra);
}

static void chiudi_store(struct pulsante *b, void *dati) {
    (void)b; (void)dati;
    finestra_tk_chiudi(&finestra);
}

/* ================= LA RICHIESTA DI INSTALLAZIONE (con un file .app) ================= */

static const char *pacchetto;                  /* il percorso del file */
static struct resoconto resoconto;             /* quello che dice il launcher (e' grande) */
static struct manifesto app;                   /* l'app del pacchetto, per dopo */
static struct contenitore colonna_richiesta, pulsanti_richiesta;
static struct documento doc_richiesta;
static struct spazio spinta_richiesta;
static struct pulsante p_annulla, p_installa, p_aggiorna, p_apri, p_chiudi;

/* Il nome del file, senza le cartelle. */
static const char *nome_file(const char *percorso) {
    const char *nome = percorso;
    for (const char *c = percorso; *c; c++) if (*c == '/') nome = c + 1;
    return nome;
}

/* Quali pulsanti si vedono (gli altri si nascondono). */
static void mostra_pulsanti(int con_annulla, int con_installa, int con_aggiorna, int con_apri, int con_chiudi) {
    widget_nascondi(&p_annulla.base, !con_annulla);
    widget_nascondi(&p_installa.base, !con_installa);
    widget_nascondi(&p_aggiorna.base, !con_aggiorna);
    widget_nascondi(&p_apri.base, !con_apri);
    widget_nascondi(&p_chiudi.base, !con_chiudi);
    ridisponi();
}

/* Quando l'app e' gia' installata con la stessa versione (o una piu' nuova): il launcher lo
 * segna come errore sulla "versione" (non c'e' niente da aggiornare); lo Store invece lo
 * dice come un fatto, "e' gia' installata". */
static int gia_installata(const struct resoconto *r) {
    return r->gia_installata && r->versione_installata[0] && versione_valida(r->m.versione) &&
           confronta_versioni(r->m.versione, r->versione_installata) <= 0;
}

static int da_mostrare(const struct problema *p, int gia) {
    return !(gia && p->blocca && descrizione_uguali(p->voce, "versione"));
}

/* Il pacchetto non si legge (o non e' un pacchetto): un messaggio chiaro e "Chiudi". */
static void mostra_danneggiato(int si_apre) {
    char riga[MAX_TESTO_DOC] = "Il file \xc2\xab";
    aggiungi(riga, nome_file(pacchetto), sizeof riga);
    documento_svuota(&doc_richiesta);
    if (!si_apre) {
        aggiungi(riga, "\xc2\xbb non si riesce a leggere: forse \xc3\xa8 stato spostato o cancellato, o non hai il "
                       "permesso di aprirlo.", sizeof riga);
        voce(&doc_richiesta, D_TITOLO, "Questo file non si apre");
        voce(&doc_richiesta, D_TESTO, riga);
        registro("pacchetto illeggibile: ", pacchetto);
    } else {
        aggiungi(riga, "\xc2\xbb non \xc3\xa8 un'app che si pu\xc3\xb2 installare: \xc3\xa8 danneggiato, o non \xc3\xa8 un "
                       "pacchetto .app.", sizeof riga);
        voce(&doc_richiesta, D_TITOLO, "Questo pacchetto \xc3\xa8 danneggiato");
        voce(&doc_richiesta, D_TESTO, riga);
        for (int i = 0; i < resoconto.quanti; i++)
            if (resoconto.problemi[i].blocca) voce(&doc_richiesta, D_ERRORE, resoconto.problemi[i].testo);
        registro("pacchetto danneggiato: ", pacchetto);
    }
    mostra_pulsanti(0, 0, 0, 0, 1);
}

/* La richiesta: cosa c'e' nel pacchetto e cosa si puo' fare. */
static void mostra_richiesta(void) {
    struct resoconto *r = &resoconto;
    app = r->m;
    const char *nome = app.nome[0] ? app.nome : app.id;
    int gia = gia_installata(r), errori = 0;
    for (int i = 0; i < r->quanti; i++)
        if (r->problemi[i].blocca && da_mostrare(&r->problemi[i], gia)) errori++;
    int aggiornamento = !gia && r->gia_installata;

    char riga[MAX_TESTO_DOC] = "";
    documento_svuota(&doc_richiesta);
    aggiungi(riga, errori ? "" : gia ? "" : aggiornamento ? "Vuoi aggiornare " : "Vuoi installare ", sizeof riga);
    aggiungi(riga, nome, sizeof riga);
    aggiungi(riga, errori ? " non si pu\xc3\xb2 installare" : gia ? " \xc3\xa8 gi\xc3\xa0 installata" : "?", sizeof riga);
    voce(&doc_richiesta, D_TITOLO, riga);
    testata(&doc_richiesta, &app);
    if (app.descrizione[0]) voce(&doc_richiesta, D_TESTO, app.descrizione);
    if (aggiornamento && !errori) {
        riga[0] = 0;
        aggiungi(riga, "\xc3\x88 gi\xc3\xa0 installata la versione ", sizeof riga);
        aggiungi(riga, r->versione_installata[0] ? r->versione_installata : "?", sizeof riga);
        aggiungi(riga, ": l'aggiornamento la porta alla ", sizeof riga);
        aggiungi(riga, app.versione, sizeof riga);
        aggiungi(riga, ". I tuoi dati restano.", sizeof riga);
        voce(&doc_richiesta, D_NOTA, riga);
    }
    if (gia) {
        riga[0] = 0;
        aggiungi(riga, "Sul computer c'\xc3\xa8 gi\xc3\xa0 la versione ", sizeof riga);
        aggiungi(riga, r->versione_installata, sizeof riga);
        aggiungi(riga, ": non c'\xc3\xa8 niente da aggiornare.", sizeof riga);
        voce(&doc_richiesta, D_NOTA, riga);
    }
    if (errori) {
        voce(&doc_richiesta, D_SEZIONE, "Perch\xc3\xa9 non si pu\xc3\xb2 installare");
        for (int i = 0; i < r->quanti; i++)
            if (r->problemi[i].blocca && da_mostrare(&r->problemi[i], gia))
                voce(&doc_richiesta, D_ERRORE, r->problemi[i].testo);
    }
    elenchi_del_manifesto(&doc_richiesta, &app, gia ? "Permessi che chiede" : "Permessi che chieder\xc3\xa0 al primo avvio");
    /* gli avvisi (le deviazioni no: sono gia' in "Cosa fa di diverso") */
    int avvisi = 0;
    for (int i = 0; i < r->quanti; i++) {
        const struct problema *p = &r->problemi[i];
        if (p->blocca || descrizione_uguali(p->voce, "deviazioni")) continue;
        if (!avvisi++) voce(&doc_richiesta, D_SEZIONE, "Da sapere");
        voce(&doc_richiesta, D_AVVISO, p->testo);
    }

    if (errori) { mostra_pulsanti(0, 0, 0, 0, 1); registro("non si puo' installare: ", app.id); }
    else if (gia) { mostra_pulsanti(0, 0, 0, 1, 1); registro("gia' installata: ", app.id); }
    else if (aggiornamento) { mostra_pulsanti(1, 0, 1, 0, 0); registro("richiesta di aggiornamento: ", app.id); }
    else { mostra_pulsanti(1, 1, 0, 0, 0); registro("richiesta di installazione: ", app.id); }
}

static void descrivi(void) {
    int esito = launcher_pacchetto(DESCRIVI_PACCHETTO, pacchetto, &resoconto);
    if (esito == LAUNCHER_ERRORE && !resoconto.manifesto_letto && !resoconto.quanti) mostra_danneggiato(0);
    else if (!resoconto.manifesto_letto) mostra_danneggiato(1);
    else mostra_richiesta();
}

/* "Installa" o "Aggiorna": lo fa il launcher; poi il risultato. */
static void premi_installa(struct pulsante *b, void *dati) {
    (void)dati;
    int aggiornare = b == &p_aggiorna;
    int esito = launcher_pacchetto(aggiornare ? AGGIORNA_PACCHETTO : INSTALLA_PACCHETTO, pacchetto, &resoconto);
    const char *nome = app.nome[0] ? app.nome : app.id;
    char riga[MAX_TESTO_DOC] = "";
    documento_svuota(&doc_richiesta);
    if (esito == LAUNCHER_FATTO) {
        aggiungi(riga, nome, sizeof riga);
        aggiungi(riga, aggiornare ? " \xc3\xa8 aggiornata" : " \xc3\xa8 installata", sizeof riga);
        voce(&doc_richiesta, D_TITOLO, riga);
        testata(&doc_richiesta, &app);
        voce(&doc_richiesta, D_NOTA, aggiornare ? "La trovi nell'elenco delle tue app, con i tuoi dati di prima."
                                                : "La trovi nell'elenco delle tue app. La scrivania la mostrer\xc3\xa0 "
                                                  "dalla prossima volta che entri.");
        mostra_pulsanti(0, 0, 0, 1, 1);
        registro(aggiornare ? "aggiornata " : "installata ", app.id);
        return;
    }
    aggiungi(riga, nome, sizeof riga);
    aggiungi(riga, aggiornare ? " non \xc3\xa8 stata aggiornata" : " non \xc3\xa8 stata installata", sizeof riga);
    voce(&doc_richiesta, D_TITOLO, riga);
    if (esito == LAUNCHER_NON_PERMESSO)
        voce(&doc_richiesta, D_ERRORE, "Lo Store non ha il permesso di installare app.");
    else if (resoconto.quanti == 0)
        voce(&doc_richiesta, D_ERRORE, "Il sistema non ha risposto: riprova tra poco.");
    for (int i = 0; i < resoconto.quanti; i++)
        if (resoconto.problemi[i].blocca) voce(&doc_richiesta, D_ERRORE, resoconto.problemi[i].testo);
    mostra_pulsanti(0, 0, 0, 0, 1);
    registro("installazione non riuscita: ", app.id);
}

/* "Apri": il launcher fa partire l'app, e lo Store ha finito. */
static void premi_apri(struct pulsante *b, void *dati) {
    (void)b; (void)dati;
    registro("apro ", app.id);
    apri_applicazione(app.id);
    finestra_tk_chiudi(&finestra);
}

static int richiesta(const char *percorso) {
    pacchetto = percorso;
    colonna_prepara(&colonna_richiesta, SPAZIO_16);
    documento_prepara(&doc_richiesta);
    riga_prepara(&pulsanti_richiesta, SPAZIO_8);
    spazio_prepara(&spinta_richiesta);
    pulsante_prepara(&p_annulla, "Annulla", NESSUNA_ICONA, chiudi_store, 0);
    pulsante_prepara(&p_installa, "Installa", NESSUNA_ICONA, premi_installa, 0);
    pulsante_prepara(&p_aggiorna, "Aggiorna", NESSUNA_ICONA, premi_installa, 0);
    pulsante_prepara(&p_apri, "Apri", NESSUNA_ICONA, premi_apri, 0);
    pulsante_prepara(&p_chiudi, "Chiudi", NESSUNA_ICONA, chiudi_store, 0);
    pulsante_stile(&p_installa, PRIMARIO, MISURA_M);
    pulsante_stile(&p_aggiorna, PRIMARIO, MISURA_M);
    pulsante_stile(&p_apri, PRIMARIO, MISURA_M);
    widget_aggiungi(&pulsanti_richiesta.base, &spinta_richiesta.base);
    widget_aggiungi(&pulsanti_richiesta.base, &p_annulla.base);
    widget_aggiungi(&pulsanti_richiesta.base, &p_chiudi.base);
    widget_aggiungi(&pulsanti_richiesta.base, &p_installa.base);
    widget_aggiungi(&pulsanti_richiesta.base, &p_aggiorna.base);
    widget_aggiungi(&pulsanti_richiesta.base, &p_apri.base);
    widget_aggiungi(&colonna_richiesta.base, &doc_richiesta.base);
    widget_aggiungi(&colonna_richiesta.base, &pulsanti_richiesta.base);

    descrivi();
    finestra_tk_prepara(&finestra, "Store", &colonna_richiesta.base);
    finestra_tk_dimensione(&finestra, 560, 480);
    return uikit_esegui(&finestra);
}

/* ================= L'ELENCO DELLE APP INSTALLATE (senza argomento) ================= */

static struct elenco_app installate;           /* dal launcher */

/* --- l'elenco: un widget fatto qui, a righe ---
 *
 * Le righe sono di tre tipi: il titolo di un gruppo ("Le tue app", "App del sistema"), una
 * app (il nome sopra, "autore · versione" sotto, piu' piccolo), e la nota "Nessuna app" per un
 * gruppo vuoto. Si sceglie un'app con un clic; la riga sotto il puntatore si illumina, come
 * nella tabella di Attivita'. Se le righe non ci stanno, scorre. */

#define RIGA_GRUPPO 32
#define RIGA_APP    48
#define RIGA_NOTA   28
#define MAX_RIGHE   (MAX_APP_ELENCO + 4)
#define ELENCO_LARGO 240

enum { R_GRUPPO, R_APP, R_NOTA };

struct riga_elenco {
    int tipo;
    int app;                           /* R_APP: l'indice in 'installate' */
    char nome[MAX_NOME_APP];           /* code page 437 */
    char sotto[MAX_AUTORE + MAX_VERSIONE + 16];
};

struct elenco {
    struct widget base;
    struct riga_elenco righe[MAX_RIGHE];
    int quante;
    int scelta;                        /* l'app scelta (indice in 'installate'), o -1 */
    int sotto;                         /* la riga sotto il puntatore, o -1 */
    void (*al_cambio)(void);
    struct barra_scorrimento barra;
};

static int altezza_riga(const struct riga_elenco *r) {
    return r->tipo == R_GRUPPO ? RIGA_GRUPPO : r->tipo == R_APP ? RIGA_APP : RIGA_NOTA;
}

static int elenco_totale(const struct elenco *e) {
    int a = 0;
    for (int i = 0; i < e->quante; i++) a += altezza_riga(&e->righe[i]);
    return a;
}

static void elenco_misura(struct widget *w, int *l, int *a) { (void)w; *l = ELENCO_LARGO; *a = 200; }

static void elenco_disponi(struct widget *w) {
    struct elenco *e = (struct elenco *)w;
    barra_scorrimento_posto(&e->barra, w->x, w->y, w->l, w->a);
    scorrimento_misure(&e->barra.scorrimento, elenco_totale(e), w->a);
}

static void elenco_disegna(struct widget *w, struct pennello *p) {
    struct elenco *e = (struct elenco *)w;
    pennello_salva(p);
    pennello_ritaglia(p, w->x, w->y, w->l, w->a);
    int y = w->y - e->barra.scorrimento.posizione;
    for (int i = 0; i < e->quante; i++) {
        const struct riga_elenco *r = &e->righe[i];
        int a = altezza_riga(r);
        if (r->tipo == R_GRUPPO) {
            pennello_colore(p, TESTO_SECONDARIO);
            testo_centrato(p, FONT_ETICHETTA, w->x + SPAZIO_12, y + 4, a - 4, r->nome);
        } else if (r->tipo == R_NOTA) {
            pennello_colore(p, TESTO_SECONDARIO);
            testo_centrato(p, FONT_CORPO, w->x + SPAZIO_12, y, a, r->nome);
        } else {
            int scelta = r->app == e->scelta;
            if (scelta || i == e->sotto) {
                pennello_colore(p, scelta ? CONTROLLO_SFONDO_SOPRA : CONTROLLO_SFONDO_NUOVO);
                riempi_tondo(p, w->x, y + 2, w->l - SPAZIO_8, a - 4, RAGGIO_CONTROLLO_NUOVO);
            }
            pennello_colore(p, TESTO_PRIMARIO);
            testo(p, FONT_CORPO_MEDIO, w->x + SPAZIO_12, y + 6, r->nome);
            pennello_colore(p, TESTO_SECONDARIO);
            testo(p, FONT_PICCOLO_NUOVO, w->x + SPAZIO_12, y + 8 + FONT_CORPO_MEDIO->altezza, r->sotto);
        }
        y += a;
    }
    pennello_ripristina(p);
}

/* La riga in y (contando lo scorrimento), o -1. */
static int elenco_riga_in(const struct elenco *e, int y) {
    if (y < e->base.y || y >= e->base.y + e->base.a) return -1;
    int dentro = y - e->base.y + e->barra.scorrimento.posizione, cima = 0;
    for (int i = 0; i < e->quante; i++) {
        int a = altezza_riga(&e->righe[i]);
        if (dentro >= cima && dentro < cima + a) return i;
        cima += a;
    }
    return -1;
}

static void elenco_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    struct elenco *e = (struct elenco *)w;
    (void)x;
    if (!(pulsanti & PULSANTE_SINISTRO)) return;
    int i = elenco_riga_in(e, y);
    if (i < 0 || e->righe[i].tipo != R_APP || e->righe[i].app == e->scelta) return;
    e->scelta = e->righe[i].app;
    widget_cambiato(w);
    if (e->al_cambio) e->al_cambio();
}

static void elenco_movimento(struct widget *w, int x, int y) {
    struct elenco *e = (struct elenco *)w;
    (void)x;
    int i = elenco_riga_in(e, y);
    if (i >= 0 && e->righe[i].tipo != R_APP) i = -1;
    if (i == e->sotto) return;
    e->sotto = i;
    widget_cambiato(w);
}

static void elenco_sopra_cambiato(struct widget *w) {
    struct elenco *e = (struct elenco *)w;
    if (!w->sopra && e->sotto >= 0) { e->sotto = -1; widget_cambiato(w); }
}

static void elenco_rotella(struct widget *w, int sx, int sy) {
    struct elenco *e = (struct elenco *)w;
    (void)sx;
    barra_scorrimento_rotella(&e->barra, sy);
    if (e->sotto >= 0) { e->sotto = -1; widget_cambiato(w); }
}

static const struct tipo_widget tipo_elenco = {
    elenco_misura, elenco_disponi, elenco_disegna, elenco_puntatore, 0, elenco_rotella,
    elenco_sopra_cambiato, elenco_movimento, 0
};

static void elenco_riga(struct elenco *e, int tipo, int app, const char *nome_utf8) {
    if (e->quante >= MAX_RIGHE) return;
    struct riga_elenco *r = &e->righe[e->quante++];
    *r = (struct riga_elenco){ .tipo = tipo, .app = app };
    in_437(r->nome, nome_utf8, (int)sizeof r->nome);
    if (tipo == R_APP) {
        const struct manifesto *m = &installate.app[app].m;
        char sotto[sizeof r->sotto] = "";
        aggiungi(sotto, m->autore[0] ? m->autore : "Autore sconosciuto", sizeof sotto);
        aggiungi(sotto, " \xc2\xb7 ", sizeof sotto);
        aggiungi(sotto, m->versione, sizeof sotto);
        in_437(r->sotto, sotto, (int)sizeof r->sotto);
    }
}

/* Le righe dall'elenco delle app: prima le tue, poi quelle del sistema. */
static void elenco_riempi(struct elenco *e) {
    e->quante = 0;
    e->sotto = -1;
    for (int gruppo = 0; gruppo < 2; gruppo++) {          /* 0: le tue, 1: del sistema */
        elenco_riga(e, R_GRUPPO, -1, gruppo ? "App del sistema" : "Le tue app");
        int quante = 0;
        for (int i = 0; i < installate.quante; i++) {
            const struct manifesto *m = &installate.app[i].m;
            if (installate.app[i].di_sistema != gruppo) continue;
            elenco_riga(e, R_APP, i, m->nome[0] ? m->nome : m->id);
            quante++;
        }
        if (!quante) elenco_riga(e, R_NOTA, -1, gruppo ? "Nessuna" : "Nessuna app installata da te");
    }
    scorrimento_misure(&e->barra.scorrimento, elenco_totale(e), e->base.a);
    widget_cambiato(&e->base);
}

/* --- il dettaglio di un'app, e le azioni --- */

static struct contenitore riga_principale, colonna_dettaglio, azioni;
static struct elenco elenco;
static struct documento doc_dettaglio;
static struct spazio spinta_azioni;
/* le azioni sull'app scelta, e gli avvisi di UIKit che chiedono conferma */
static struct pulsante p_svuota, p_togli;
static struct avviso avviso;   /* uno solo: la finestra ne ha uno alla volta, lo si prepara a ogni domanda */

/* Cosa si vede a destra. */
enum { NIENTE, DETTAGLIO };

static int stato_destra = NIENTE;

static void mostra_azioni(int stato) {
    stato_destra = stato;
    const struct app_installata *a = elenco.scelta >= 0 ? &installate.app[elenco.scelta] : 0;
    widget_nascondi(&p_svuota.base, stato != DETTAGLIO);
    widget_nascondi(&p_togli.base, stato != DETTAGLIO || !a || a->di_sistema);   /* solo le app della persona */
    ridisponi();
}

static const char *nome_scelta(void) {
    const struct manifesto *m = &installate.app[elenco.scelta].m;
    return m->nome[0] ? m->nome : m->id;
}

/* Il dettaglio dell'app scelta, con in cima (se c'e') l'esito dell'ultima azione. */
static void mostra_dettaglio(int esito_tipo, const char *esito) {
    documento_svuota(&doc_dettaglio);
    if (elenco.scelta < 0) {
        if (esito && esito[0]) voce(&doc_dettaglio, esito_tipo, esito);
        voce(&doc_dettaglio, D_NOTA, installate.quante ? "Scegli un'app per vederne i dettagli."
                                                       : "Non si riesce a leggere l'elenco delle app.");
        mostra_azioni(NIENTE);
        return;
    }
    const struct app_installata *a = &installate.app[elenco.scelta];
    const struct manifesto *m = &a->m;
    testata(&doc_dettaglio, m);
    if (esito && esito[0]) voce(&doc_dettaglio, esito_tipo, esito);
    voce(&doc_dettaglio, D_NOTA, a->di_sistema ? "App del sistema" : "Installata da te");
    if (m->descrizione[0]) voce(&doc_dettaglio, D_TESTO, m->descrizione);
    elenchi_del_manifesto(&doc_dettaglio, m, "Permessi che chiede");
    mostra_azioni(DETTAGLIO);
}

static void scelta_cambiata(void) { mostra_dettaglio(0, 0); }

/* Rilegge le app dal launcher. */
static void leggi_elenco(void) {
    if (elenco_app_installate(&installate) != LAUNCHER_FATTO) installate.quante = 0;
    int tue = 0;
    for (int i = 0; i < installate.quante; i++) tue += !installate.app[i].di_sistema;
    char conto[48] = "";
    numero_in(conto, installate.quante);
    aggiungi(conto, " app, ", sizeof conto);
    char n[12];
    numero_in(n, tue);
    aggiungi(conto, n, sizeof conto);
    aggiungi(conto, " tue", sizeof conto);
    registro("elenco: ", conto);
    elenco_riempi(&elenco);
}

static void scelta_svuota(struct avviso *a, int scelta, void *dati);
static void scelta_togli(struct avviso *a, int scelta, void *dati);

/* Le domande di conferma: l'avviso di UIKit, aperto sopra la finestra. Il titolo porta il
 * nome dell'app scelta, quindi lo si prepara ogni volta prima di aprirlo. */
static void chiedi_svuota(struct pulsante *b, void *dati) {
    (void)b; (void)dati;
    char riga[MAX_TESTO_WIDGET] = "Cancellare i dati di ";
    aggiungi(riga, nome_scelta(), sizeof riga);
    aggiungi(riga, "?", sizeof riga);
    avviso_prepara(&avviso, riga, "L'app rester\xc3\xa0 installata ma ripartir\xc3\xa0 da zero.",
                   "Svuota dati", PERICOLO, 1, scelta_svuota, 0);
    finestra_tk_avviso(&finestra, &avviso);
    avviso_apri(&avviso);
}

static void chiedi_togli(struct pulsante *b, void *dati) {
    (void)b; (void)dati;
    char riga[MAX_TESTO_WIDGET] = "Togliere ";
    aggiungi(riga, nome_scelta(), sizeof riga);
    aggiungi(riga, "?", sizeof riga);
    avviso_prepara(&avviso, riga,
                   "L'app sparisce dal computer. Puoi togliere anche i suoi dati, oppure tenerli: "
                   "se un giorno la installi di nuovo, li ritrova.",
                   "Togli app e dati", PERICOLO, 1, scelta_togli, 0);
    avviso_alternativa(&avviso, "Togli solo l'app", SECONDARIO);
    finestra_tk_avviso(&finestra, &avviso);
    avviso_apri(&avviso);
}

static void scelta_svuota(struct avviso *a, int scelta, void *dati) {
    (void)a; (void)dati;
    if (scelta != AVVISO_CONFERMA) return;
    struct risposta_launcher risposta;
    const char *id = installate.app[elenco.scelta].m.id;
    int esito = svuota_dati_app(id, &risposta);
    registro(esito == LAUNCHER_FATTO ? "dati svuotati di " : "dati non svuotati di ", id);
    mostra_dettaglio(esito == LAUNCHER_FATTO ? D_FATTO : D_ERRORE,
                     esito == LAUNCHER_FATTO ? "Dati svuotati."
                     : risposta.messaggio[0] ? risposta.messaggio : "Non si \xc3\xa8 riusciti a svuotare i dati.");
}

/* Togli: conferma = "Togli app e dati", alternativa = "Togli solo l'app" (i dati restano). */
static void scelta_togli(struct avviso *a, int scelta, void *dati) {
    (void)a; (void)dati;
    if (scelta != AVVISO_CONFERMA && scelta != AVVISO_ALTERNATIVA) return;
    int tieni_dati = scelta == AVVISO_ALTERNATIVA;
    struct risposta_launcher risposta;
    char id[MAX_ID];
    descrizione_copia(id, installate.app[elenco.scelta].m.id, MAX_ID);
    int esito = togli_app(id, tieni_dati, &risposta);
    registro(esito == LAUNCHER_FATTO ? (tieni_dati ? "tolta (dati tenuti) " : "tolta ") : "non tolta ", id);
    if (esito != LAUNCHER_FATTO && esito != LAUNCHER_ERRORE) {   /* l'app c'e' ancora: si resta li' */
        mostra_dettaglio(D_ERRORE, risposta.messaggio[0] ? risposta.messaggio : "Non si \xc3\xa8 riusciti a togliere l'app.");
        return;
    }
    /* tolta (o forse tolta a meta'): si torna all'elenco, riletto */
    char messaggio[MAX_MESSAGGIO_LAUNCHER];
    descrizione_copia(messaggio, risposta.messaggio[0] ? risposta.messaggio : "Non si \xc3\xa8 riusciti a togliere l'app.",
                      MAX_MESSAGGIO_LAUNCHER);
    elenco.scelta = -1;
    leggi_elenco();
    mostra_dettaglio(esito == LAUNCHER_FATTO ? D_FATTO : D_ERRORE, messaggio);
}

static int mostra_elenco(void) {
    riga_prepara(&riga_principale, SPAZIO_24);
    elenco = (struct elenco){ .base = { .tipo = &tipo_elenco, .largo = 1 }, .scelta = -1, .sotto = -1,
                              .al_cambio = scelta_cambiata };
    barra_scorrimento_prepara(&elenco.barra);
    widget_aggiungi(&elenco.base, &elenco.barra.base);

    colonna_prepara(&colonna_dettaglio, SPAZIO_16);
    colonna_dettaglio.base.elastico = 1;
    documento_prepara(&doc_dettaglio);
    riga_prepara(&azioni, SPAZIO_8);
    spazio_prepara(&spinta_azioni);
    pulsante_prepara(&p_svuota, "Svuota dati", NESSUNA_ICONA, chiedi_svuota, 0);
    pulsante_prepara(&p_togli, "Togli app", ICONA_TRASH_2, chiedi_togli, 0);
    widget_aggiungi(&azioni.base, &spinta_azioni.base);
    widget_aggiungi(&azioni.base, &p_svuota.base);
    widget_aggiungi(&azioni.base, &p_togli.base);
    widget_aggiungi(&colonna_dettaglio.base, &doc_dettaglio.base);
    widget_aggiungi(&colonna_dettaglio.base, &azioni.base);

    widget_aggiungi(&riga_principale.base, &elenco.base);
    widget_aggiungi(&riga_principale.base, &colonna_dettaglio.base);

    leggi_elenco();
    mostra_dettaglio(0, 0);
    finestra_tk_prepara(&finestra, "Store", &riga_principale.base);
    finestra_tk_dimensione(&finestra, 760, 500);
    return uikit_esegui(&finestra);
}

int main(int quanti, char **argomenti) {
    if (quanti > 1 && argomenti[1][0]) return richiesta(argomenti[1]);
    return mostra_elenco();
}
