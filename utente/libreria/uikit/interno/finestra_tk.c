/* UIKit, il nucleo: la finestra dell'applicazione come widget, la radice dell'albero. Le sue
 * misure (testata, comandi, margini), le zone da ridisegnare, il disegno della testata e del
 * corpo, le opzioni (barra degli strumenti, aree, mini finestra, avviso), le chiamate "ogni
 * tanto", chi ha il puntatore sopra e chi ha il fuoco, gli ascoltatori dei tasti fisici.
 *
 * Qui NIENTE compositore: non si apre la finestra e non si ricevono notizie (quello e' in
 * ciclo.c). Cosi' questo file si compila anche nell'anteprima (strumenti/anteprima), che
 * disegna la finestra su un PNG senza nessun compositore. */
#include "uikit/uikit.h"
#include "uikit/interno/nucleo.h"

int testata_altezza(const struct finestra_tk *f) {
    return f->mini ? ALTEZZA_BARRA_MINI : TITOLO_ALTEZZA;
}

/* Il lato dei comandi (riduci, chiudi): S (32), o XS (24) nella mini finestra. */
static int comando_lato(const struct finestra_tk *f) { return f->mini ? ALTEZZA_XS : ALTEZZA_S; }

/* Lo spazio dei comandi a destra (e, uguale, quello vuoto a sinistra che centra il titolo):
 * riduci e chiudi con i margini (76), o nella mini finestra il solo chiudi (4 + 24 = 28). */
static int comandi_larghezza(const struct finestra_tk *f) {
    return f->mini ? TITOLO_MARGINE + ALTEZZA_XS : TITOLO_COMANDI_LARGHEZZA;
}

/* Il margine intorno al contenuto: FINESTRA_PADDING (24), SPAZIO_16 nella mini finestra,
 * CORPO_PADDING (8) nella finestra a aree (finestra_tk_aree). */
static int finestra_padding(const struct finestra_tk *f) {
    return f->senza_margine ? 0 : f->aree ? CORPO_PADDING : f->mini ? SPAZIO_16 : FINESTRA_PADDING;
}

struct avviso *finestra_tk_avviso_aperto(const struct finestra_tk *f) {
    return f->avviso && !f->avviso->base.nascosto ? f->avviso : 0;
}

int finestra_tk_bloccata(const struct finestra_tk *f) {
    struct avviso *v = finestra_tk_avviso_aperto(f);
    return v && v->bloccante;
}

struct finestra_tk *widget_finestra(struct widget *w) {
    while (w->genitore) w = w->genitore;
    return (struct finestra_tk *)w;
}

/* LE ZONE DA RIDISEGNARE della finestra (vedi in cima, "si ridisegna solo quello che e'
 * cambiato"): un rettangolo in piu', in coordinate della finestra. */
static void uikit_cambiata_zona(struct finestra_tk *f, int x, int y, int l, int a) {
    struct zona_finestra z = zona_dentro((struct zona_finestra){ x, y, l, a },
                                         (struct zona_finestra){ 0, 0, f->base.l, f->base.a });
    if (zona_vuota(z)) return;
    f->da_ridisegnare = 1;
    if (!f->tutta_cambiata) zone_aggiungi(f->cambiate, &f->quante_cambiate, MAX_ZONE_MOSTRA, z);
}

void uikit_cambiata_tutta(struct finestra_tk *f) {
    f->da_ridisegnare = 1;
    f->tutta_cambiata = 1;
}

void uikit_cambiata_testata(struct finestra_tk *f) {
    uikit_cambiata_zona(f, 0, 0, f->base.l, testata_altezza(f));
}

/* Se il widget non e' ancora in una finestra (lo si sta preparando, prima di
 * uikit_esegui), non c'e' niente da ridisegnare: la cima del suo albero non e' una
 * finestra, e non la si tocca. La zona si taglia sullo spazio del widget con il suo sbordo:
 * fuori di li' il widget non disegna. */
static void widget_segna_zona(struct widget *w, int x, int y, int l, int a) {
    struct finestra_tk *f = widget_finestra(w);
    if (f->base.tipo != &tipo_finestra_tk) return;
    struct zona_finestra z = zona_dentro((struct zona_finestra){ x, y, l, a },
                                         (struct zona_finestra){ w->x - WIDGET_SBORDO, w->y - WIDGET_SBORDO,
                                                                 w->l + 2 * WIDGET_SBORDO, w->a + 2 * WIDGET_SBORDO });
    uikit_cambiata_zona(f, z.x, z.y, z.l, z.a);
}

void widget_segna(struct widget *w) {
    widget_segna_zona(w, w->x - WIDGET_SBORDO, w->y - WIDGET_SBORDO, w->l + 2 * WIDGET_SBORDO,
                      w->a + 2 * WIDGET_SBORDO);
}

void widget_cambiato(struct widget *w) {
    if (widget_visibile(w)) widget_segna(w);
}

void widget_cambiato_zona(struct widget *w, int x, int y, int l, int a) {
    if (widget_visibile(w)) widget_segna_zona(w, x, y, l, a);
}

/* Dove stanno i due comandi nella testata (la finestra e' descritta in uikit.h): riduci e
 * chiudi, a destra. */
static int comando_x(const struct finestra_tk *f, int chiudi) {
    int lato = comando_lato(f);
    return f->base.l - TITOLO_MARGINE - lato - (chiudi ? 0 : lato + TITOLO_MARGINE);
}

/* centrati in altezza nella testata: 4 nella normale (4 + 32 + 4), 2 nella mini (2 + 24 + 2) */
static int comando_y(const struct finestra_tk *f) { return (testata_altezza(f) - comando_lato(f)) / 2; }

int su_comando(const struct finestra_tk *f, int chiudi, int x, int y) {
    if (f->mini && !chiudi) return 0;
    int cx = comando_x(f, chiudi), cy = comando_y(f), lato = comando_lato(f);
    return x >= cx && x < cx + lato && y >= cy && y < cy + lato;
}

/* La striscia della barra degli strumenti sotto la testata (0 se non c'e' o e' nascosta): nella
 * finestra a aree il padding del corpo e la barra, alta quanto le sue cose (il gap sotto e' il
 * margine del contenuto, finestra_padding); nelle altre STRUMENTI_ALTEZZA (48: la barra, alta
 * 32, con STRUMENTI_MARGINE_SOPRA sopra e sotto). */
static int finestra_tk_striscia(const struct finestra_tk *f) {
    if (!f->strumenti || f->strumenti->nascosto) return 0;
    if (!f->aree) return STRUMENTI_ALTEZZA;
    int l, a;
    widget_misura(f->strumenti, &l, &a);
    return CORPO_PADDING + a;
}

int finestra_tk_corpo(const struct finestra_tk *f) {
    return testata_altezza(f) + finestra_tk_striscia(f);
}

/* Il contenuto, piu' il padding e le barre; abbastanza larga per il titolo e i comandi, e
 * per quello che sta nella barra degli strumenti. Con una misura fissa, la misura e' di
 * tutta la finestra: la barra degli strumenti la prende al contenuto. */
static void finestra_tk_misura(struct widget *w, int *l, int *a) {
    struct finestra_tk *f = (struct finestra_tk *)w;
    if (f->l_fissa) { *l = f->l_fissa; *a = f->a_fissa; return; }
    int cl = 0, ca = 0;
    if (w->quanti_figli) widget_misura(w->figli[0], &cl, &ca);
    /* il titolo sta tra i due gruppi (lo spazio vuoto e i comandi), con un po' d'aria */
    int barra = 2 * (comandi_larghezza(f) + SPAZIO_8) +
                larghezza_testo_stile(f->mini ? FONT_ETICHETTA : FONT_TITOLO_FINESTRA, f->titolo);
    *l = 2 * finestra_padding(f) + cl;
    if (*l < barra) *l = barra;
    if (f->strumenti && !f->strumenti->nascosto) {
        int sl, sa;
        widget_misura(f->strumenti, &sl, &sa);
        sl += 2 * STRUMENTI_MARGINE_LATI;   /* e' a 8 dai bordi (CORPO_PADDING, a aree) */
        if (*l < sl) *l = sl;
    }
    *a = finestra_tk_corpo(f) + 2 * finestra_padding(f) + ca;
}

static void finestra_tk_disponi(struct widget *w) {
    struct finestra_tk *f = (struct finestra_tk *)w;
    if (!w->quanti_figli) return;
    if (f->strumenti) {                     /* tutta la larghezza, a 8 dai bordi e dalla testata */
        int lati = f->aree ? CORPO_PADDING : STRUMENTI_MARGINE_LATI;
        int sopra = f->aree ? CORPO_PADDING : STRUMENTI_MARGINE_SOPRA;
        f->strumenti->x = lati;
        f->strumenti->y = testata_altezza(f) + sopra;
        f->strumenti->l = w->l - 2 * lati;
        f->strumenti->a = f->aree ? finestra_tk_striscia(f) - sopra : ALTEZZA_S;
    }
    int corpo = finestra_tk_corpo(f);
    int margine = finestra_padding(f);
    struct widget *c = w->figli[0];
    c->x = margine;
    c->y = corpo + margine;
    c->l = w->l - 2 * margine;
    c->a = w->a - corpo - 2 * margine;
}

/* IL CONTENUTO della finestra, cioe' il suo fondo: il vetro (SUPERFICIE_VETRO, bianco o
 * grafite al 72%) che lascia vedere lo sfondo che il compositore ha sfocato qui sotto, con i
 * soli angoli in basso arrotondati. Quello che sta nel contenuto (la barra degli strumenti,
 * il widget dell'applicazione) lo disegnano i figli, dopo. */
static void finestra_tk_contenuto_disegna(struct finestra_tk *f, struct pennello *p) {
    int inizio = testata_altezza(f);
    struct tondo contenuto = { 0, inizio, f->base.l, f->base.a - inizio,
                               { 0, 0, RAGGIO_FINESTRA_NUOVO, RAGGIO_FINESTRA_NUOVO } };
    pennello_colore(p, SUPERFICIE_VETRO);
    riempi_forma(p, &contenuto);
}

/* LA TESTATA: il fondo (lo stesso vetro, soli angoli in alto arrotondati), il titolo centrato e
 * i comandi (sotto, nessuna linea: deciso dalla persona). Una finestra che non ha il
 * fuoco (non e' in primo piano) ha il titolo in TESTO_SECONDARIO e i comandi al 50%.
 *
 * Il VETRO e' lo stesso del contenuto: i due pezzi si toccano lungo il divisore, e le
 * coperture dei loro bordi dritti sono piene, quindi in mezzo non resta nessuna cucitura. */
static void finestra_tk_titolo_disegna(struct finestra_tk *f, struct pennello *p) {
    struct widget *w = &f->base;
    int alta = testata_altezza(f);
    struct tondo testata = { 0, 0, w->l, alta,
                             { RAGGIO_FINESTRA_NUOVO, RAGGIO_FINESTRA_NUOVO, 0, 0 } };
    pennello_colore(p, SUPERFICIE_VETRO);
    riempi_forma(p, &testata);
    /* il titolo, al centro della finestra: tra lo spazio vuoto a sinistra (largo quanto i
     * comandi) e i comandi non entra mai nei comandi, un titolo lungo si taglia. Nella mini
     * finestra e' piu' piccolo (l'etichetta, 12). */
    const struct font *font = f->mini ? FONT_ETICHETTA : FONT_TITOLO_FINESTRA;
    pennello_salva(p);
    int margine = comandi_larghezza(f) + SPAZIO_8;
    pennello_ritaglia(p, margine, 0, w->l - 2 * margine, alta);
    int attiva = f->attiva && !finestra_tk_bloccata(f);   /* sotto un avviso bloccante si spegne */
    pennello_colore(p, attiva ? TESTO_PRIMARIO : TESTO_SECONDARIO);
    testo_centrato(p, font, (w->l - larghezza_testo_stile(font, f->titolo)) / 2, 0, alta, f->titolo);
    pennello_ripristina(p);

    /* i comandi: pulsanti ghost tondi (S, o XS nella mini finestra, che ha solo chiudi).
     * Senza fondo a riposo; sotto il puntatore CONTROLLO_SFONDO, premuto
     * CONTROLLO_SFONDO_SOPRA (come ogni ghost di UIKit). */
    int lato = comando_lato(f), segno = f->mini ? ICONA_XS : ICONA_S, cy = comando_y(f);
    pennello_salva(p);
    if (!attiva) pennello_opacita(p, OPACITA_COMANDI_INATTIVI);
    for (int chiudi = f->mini ? 1 : 0; chiudi < 2; chiudi++) {
        int cx = comando_x(f, chiudi);
        int sopra = f->comando_sopra == 1 + chiudi;
        int premuto = sopra && (chiudi ? f->chiudi_premuto : f->riduci_premuto);
        if (sopra) {
            pennello_colore(p, premuto ? CONTROLLO_SFONDO_SOPRA : CONTROLLO_SFONDO_NUOVO);
            riempi_cerchio(p, cx + lato / 2, cy + lato / 2, lato / 2);
        }
        pennello_colore(p, COLORE_ICONA(COLORE_ICONA_PRIMARIO));
        icona(p, chiudi ? ICONA_X : ICONA_MINUS, segno, cx + (lato - segno) / 2, cy + (lato - segno) / 2);
    }
    pennello_ripristina(p);
}

/* La finestra: pulisce, disegna il contenuto e la testata (due pezzi separati, vedi sopra),
 * e infine il filo di luce interno del tema scuro (gli strati INTERNI della tabella
 * "finestra": nel chiaro non ce ne sono). Le ombre ESTERNE e la sfocatura dello sfondo non
 * sono qui: stanno fuori dalla finestra, le fa il compositore. Non c'e' nessun bordo.
 *
 * IL FONDO SCURO. Prima, sotto il vetro, c'era VETRO_FONDO (un blu scuro al 50%) perche' il
 * testo era bianco e sopra uno sfondo chiaro non si sarebbe letto. Ora il testo cambia col
 * tema: scuro sul vetro bianco (tema chiaro), chiaro sul vetro grafite (tema scuro). Il vetro
 * bianco al 72% sopra lo sfondo sfocato resta chiaro (il testo scuro si legge: sotto, il
 * colore dello sfondo conta per il 28%); il vetro grafite al 72% resta scuro. Un fondo scuro
 * in piu' renderebbe grigio il vetro chiaro, e non c'e' nel disegno: tolto in tutti e due i
 * temi. */
static void finestra_tk_disegna(struct widget *w, struct pennello *p) {
    struct finestra_tk *f = (struct finestra_tk *)w;
    pulisci(p);
    finestra_tk_contenuto_disegna(f, p);
    finestra_tk_titolo_disegna(f, p);
    struct tondo tutta = tondo(0, 0, w->l, w->a, RAGGIO_FINESTRA_NUOVO);
    ombre_strati_dentro(p, &tutta, OMBRA_TAB(finestra), OMBRA_N(finestra));
}

const struct tipo_widget tipo_finestra_tk = {
    finestra_tk_misura, finestra_tk_disponi, finestra_tk_disegna, 0, 0, 0, 0, 0, 0
};

void finestra_tk_prepara(struct finestra_tk *f, const char *titolo,
                         struct widget *contenuto) {
    *f = (struct finestra_tk){ .base = { .tipo = &tipo_finestra_tk } };
    copia_testo(f->titolo, titolo, MAX_TITOLO);
    widget_aggiungi(&f->base, contenuto);
}

void finestra_tk_barra(struct finestra_tk *f, struct widget *strumenti) {
    f->strumenti = strumenti;
    widget_aggiungi(&f->base, strumenti);
}

void finestra_tk_aree(struct finestra_tk *f) { f->aree = 1; }

void finestra_tk_bordo(struct finestra_tk *f) { f->senza_margine = 1; }

void finestra_tk_titolo(struct finestra_tk *f, const char *titolo) {
    copia_testo(f->titolo, titolo, MAX_TITOLO);
    uikit_cambiata_testata(f);
}

void finestra_tk_mini(struct finestra_tk *f) { f->mini = 1; }

void finestra_tk_avviso(struct finestra_tk *f, struct avviso *v) {
    f->avviso = v;
    v->base.genitore = &f->base;
}

void finestra_tk_chiudi(struct finestra_tk *f) { f->da_chiudere = 1; }

void finestra_tk_dimensione(struct finestra_tk *f, int l, int a) {
    f->l_fissa = l;
    f->a_fissa = a;
}

void finestra_tk_ogni(struct finestra_tk *f, unsigned int ms,
                      void (*funzione)(struct finestra_tk *f, void *dati), void *dati) {
    struct chiamata_tk *posto = 0;
    for (int i = 0; i < MAX_CHIAMATE; i++) {
        struct chiamata_tk *c = &f->chiamate[i];
        if (c->funzione == funzione && c->dati == dati) { posto = c; break; }
        if (!c->funzione && !posto) posto = c;
    }
    if (!posto) return;                         /* tutte prese: non si puo' */
    if (!ms) {
        if (posto->funzione == funzione && posto->dati == dati) posto->funzione = 0;
        return;
    }
    *posto = (struct chiamata_tk){ funzione, dati, ms, tempo() + ms };
}

void finestra_tk_guarda(struct finestra_tk *f, int d,
                        void (*funzione)(struct finestra_tk *f, void *dati), void *dati) {
    f->guardato = d;
    f->al_pronto = funzione;
    f->dati_pronto = dati;
}

struct chiamata_tk *uikit_prima_chiamata(struct finestra_tk *f) {
    struct chiamata_tk *prima = 0;
    for (int i = 0; i < MAX_CHIAMATE; i++) {
        struct chiamata_tk *c = &f->chiamate[i];
        if (c->funzione && (!prima || (int)(c->scadenza - prima->scadenza) < 0)) prima = c;
    }
    return prima;
}

void uikit_chiamate_scadute(struct finestra_tk *f) {
    unsigned int ora = tempo();
    for (int i = 0; i < MAX_CHIAMATE; i++) {
        struct chiamata_tk *c = &f->chiamate[i];
        if (!c->funzione || (int)(c->scadenza - ora) > 0) continue;
        c->scadenza = ora + c->ms;
        c->funzione(f, c->dati);
    }
}

void uikit_ridisponi(struct finestra_tk *f) {
    widget_disponi(&f->base);
    uikit_cambiata_tutta(f);
}

void uikit_disegna_tutto(struct finestra_tk *f, struct pennello *p) {
    widget_disegna(&f->base, p);
    struct avviso *v = finestra_tk_avviso_aperto(f);
    if (v) {
        avviso_sotto(v, p, f->base.l, f->base.a);
        widget_disegna(&v->base, p);
    }
}

void uikit_sotto(struct finestra_tk *f, struct widget *w) {
    if (w == f->sotto) return;
    struct widget *prima = f->sotto;
    /* un widget che segue il puntatore pezzo per pezzo (ha 'movimento': una tabella, una
     * griglia, un menu) accende le sue parti e ridisegna solo quelle: tutto il widget no */
    if (prima) { prima->sopra = 0; if (!prima->tipo->movimento) widget_cambiato(prima); }
    f->sotto = w;
    if (w) { w->sopra = 1; if (!w->tipo->movimento) widget_cambiato(w); }
    if (prima && prima->tipo->sopra_cambiato) prima->tipo->sopra_cambiato(prima);
    if (w && w->tipo->sopra_cambiato) w->tipo->sopra_cambiato(w);
}

void uikit_fuoco(struct finestra_tk *f, struct widget *w) {
    if (w == f->con_fuoco) return;
    if (f->con_fuoco) { f->con_fuoco->fuoco = 0; widget_cambiato(f->con_fuoco); }
    f->con_fuoco = w;
    if (w) { w->fuoco = 1; widget_cambiato(w); }
}

void finestra_tk_dai_fuoco(struct finestra_tk *f, struct widget *w) {
    if (w && !w->tipo->tasto) return;      /* non prende i tasti: il fuoco non e' suo */
    uikit_fuoco(f, w);
}

void finestra_tk_ascolta_tasti(struct finestra_tk *f, ascolta_tasti_tk funzione, void *dati) {
    for (int i = 0; i < MAX_ASCOLTATORI; i++)
        if (f->ascoltatori[i].funzione == funzione && f->ascoltatori[i].dati == dati) return;
    for (int i = 0; i < MAX_ASCOLTATORI; i++)
        if (!f->ascoltatori[i].funzione) { f->ascoltatori[i] = (struct ascoltatore_tk){ funzione, dati }; return; }
}

void finestra_tk_smetti_tasti(struct finestra_tk *f, ascolta_tasti_tk funzione, void *dati) {
    for (int i = 0; i < MAX_ASCOLTATORI; i++)
        if (f->ascoltatori[i].funzione == funzione && f->ascoltatori[i].dati == dati) {
            for (int k = i; k < MAX_ASCOLTATORI - 1; k++) f->ascoltatori[k] = f->ascoltatori[k + 1];
            f->ascoltatori[MAX_ASCOLTATORI - 1] = (struct ascoltatore_tk){ 0, 0 };
            return;
        }
}
