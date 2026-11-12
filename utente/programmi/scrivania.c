/* La scrivania: la "faccia" del sistema, come GNOME Shell o il Dock del Mac.
 *
 * E' un programma del sistema (uid 0), sorvegliato da init come il compositore. Per il
 * compositore e' un'applicazione come le altre, ma siccome e' del sistema puo' chiedere
 * delle SUPERFICI SPECIALI (vedi gkit.h):
 *   - lo SFONDO, grande come lo schermo, sotto tutte le finestre, con le ICONE della
 *     scrivania in alto a sinistra (oggi solo "Shell", provvisoria: vedi piu' sotto);
 *   - la USERBAR, un PANNELLO (una barra) sul bordo sinistro, centrato in altezza, sopra
 *     tutte le finestre: il pulsante App e le applicazioni del sistema, con un segno a
 *     sinistra di quelle aperte;
 *   - il VASSOIO DELLE APP, un altro PANNELLO (di quelli che si aprono sopra il resto,
 *     come un menu), a destra del pulsante App: tutte le applicazioni installate, in una
 *     griglia. C'e' solo mentre e' aperto: si apre e si chiude con il pulsante App.
 * Il disegno e' quello della pagina "Scrivania" del file Figma (userbar e vassoio:
 * uikit/userbar.h). L'ora e la data, che stavano nella vecchia isola, nel disegno non ci
 * sono: per ora la scrivania non le mostra (STATO.md, "Da confermare").
 *
 * LE APPLICAZIONI INSTALLATE le legge da /system/apps (quelle del sistema) e poi da
 * /data/apps (quelle installate dalla persona): una cartella per applicazione, con il suo
 * manifest.toml (descrizione.h). Le legge una volta, all'avvio: un'app installata dopo si
 * vede quando la scrivania riparte. Nella userbar ci sono quelle del sistema, nel vassoio
 * tutte. QUALI SONO APERTE glielo dice il compositore, che a un pannello racconta ogni
 * finestra che compare o sparisce, con il programma a cui appartiene.
 *
 * LE ICONE sono quelle vere delle app: la PNG piccola del pacchetto (il manifesto le elenca
 * per tema e per misura), caricata con immagine_carica di GKit. Si prende quella del tema in
 * uso con la misura piu' vicina a quella del posto (USERBAR_PNG_BARRA e USERBAR_PNG_VASSOIO, in userbar.h); se
 * nel tema non ce n'e' nessuna, quella dell'altro tema (APPLICAZIONI.md, "Icone mancanti").
 * Gli SVG non si sanno ancora disegnare: un'app che ha solo l'SVG ha il simbolo "pacchetto".
 *
 * IL CLIC SU UN'APPLICAZIONE (nella userbar o nel vassoio):
 *   - se e' chiusa, la scrivania chiede al launcher (il servizio che fa partire le
 *     applicazioni) "apri questa": e' lui a farla partire, la scrivania non fa partire
 *     niente da sola;
 *   - se e' aperta, ma ridotta o dietro le altre, chiede al compositore di riportare
 *     davanti la sua finestra (PORTA_DAVANTI);
 *   - se e' gia' in primo piano, niente, come il Dock del Mac.
 * Un clic nel vassoio lo chiude. Il vassoio si chiude anche premendo sullo sfondo o quando
 * un'altra finestra prende il fuoco. */
#include "gkit/gkit.h"
#include "uikit/userbar.h"
#include "descrizione.h"
#include "launcher.h"
#include "testo437.h"

/* --- le applicazioni installate --- */

#define MAX_APP 16

struct app {
    char cartella[MAX_NOME_FILE];   /* il nome della sua cartella: lo si chiede al launcher */
    char programma[MAX_PERCORSO_PROGRAMMA];   /* il percorso completo del programma */
    char nome[MAX_NOME_APP];        /* quello che si vede (code page 437, per i font) */
    int di_sistema;                 /* sta in /system/apps: e' fissata nella userbar */
    struct immagine png[2];         /* le icone caricate (al massimo due misure) */
};

static struct app app[MAX_APP];
static int quante_app;

/* Le voci della userbar (le app fissate) e del vassoio (tutte), per userbar.h. */
static struct voce_app voci_barra[MAX_APP], voci_vassoio[MAX_APP];
static int fissata[MAX_APP];        /* la voce 1..n della userbar -> l'app (indice in app) */
static int quante_fissate;

static int uguali(const char *a, const char *b) { return descrizione_uguali(a, b); }
static void copia(char *dove, const char *da, int massimo) { descrizione_copia(dove, da, massimo); }
static void aggiungi(char *dove, const char *da, int massimo) { descrizione_aggiungi(dove, da, massimo); }

/* Gia' nell'elenco? (un'applicazione di /data/apps con lo stesso nome di una del sistema
 * e' la stessa: vale quella del sistema, come per il launcher) */
static int gia_letta(const char *cartella) {
    for (int i = 0; i < quante_app; i++)
        if (uguali(app[i].cartella, cartella)) return 1;
    return 0;
}

/* La PNG del manifesto da usare a 'lato' pixel: quella del tema in uso con la misura piu'
 * vicina; se il tema non ne ha, la piu' vicina dell'altro. Il suo percorso dentro la
 * cartella dell'app, o 0 se l'app non ha PNG. */
static const char *png_per(const struct manifesto *m, int lato) {
    int tema = TEMA_SCURO_IN_USO ? TEMA_SCURO : TEMA_CHIARO;
    for (int giro = 0; giro < 2; giro++) {
        const struct icone_tema *t = &m->icone[giro ? 1 - tema : tema];
        int scelta = -1, distanza = 0;
        for (int i = 0; i < t->quante_png; i++) {
            int d = t->png_misura[i] > lato ? t->png_misura[i] - lato : lato - t->png_misura[i];
            if (scelta < 0 || d < distanza) { scelta = i; distanza = d; }
        }
        if (scelta >= 0) return t->png[scelta];
    }
    return 0;
}

/* Carica la PNG 'dentro' (un percorso nella cartella 'cartella_app') in uno dei posti
 * liberi di a->png; se e' gia' stata caricata (la stessa misura per userbar e vassoio)
 * torna quella. 0 se non c'e' o non si legge. */
static const struct immagine *carica_icona(struct app *a, const char *cartella_app, const char *dentro,
                                           const char **gia, int *quante) {
    if (!dentro) return 0;
    for (int i = 0; i < *quante; i++)
        if (uguali(gia[i], dentro)) return a->png[i].pixel ? &a->png[i] : 0;
    if (*quante >= 2) return 0;
    char percorso[MAX_PERCORSO];
    copia(percorso, cartella_app, sizeof percorso);
    aggiungi(percorso, "/", sizeof percorso);
    aggiungi(percorso, dentro, sizeof percorso);
    struct immagine *im = &a->png[*quante];
    gia[(*quante)++] = dentro;
    return immagine_carica(percorso, im) == 0 ? im : 0;
}

/* Le applicazioni installate di una delle cartelle delle applicazioni: le sue cartelle
 * che hanno un manifest. Prima quelle del sistema, poi quelle della persona, ognuna in
 * ordine alfabetico. */
static void leggi_applicazioni_in(const char *cartelle) {
    int d = apri(cartelle, APRI_LEGGERE);
    struct voce_cartella v;
    while (d >= 0 && quante_app < MAX_APP && leggi(d, &v, sizeof v) == sizeof v) {
        struct descrizione desc;
        if (v.tipo != TIPO_CARTELLA || gia_letta(v.nome) || leggi_descrizione(v.nome, &desc) < 0)
            continue;
        int i = quante_app++;
        struct app *a = &app[i];
        *a = (struct app){ .di_sistema = desc.di_sistema };
        copia(a->cartella, v.nome, sizeof a->cartella);
        copia(a->programma, desc.programma, sizeof a->programma);
        in_437(a->nome, desc.nome, sizeof a->nome);
        voci_barra[i] = voci_vassoio[i] = (struct voce_app){ .icona = ICONA_PACKAGE, .nome = a->nome };

        /* le icone: il manifesto intero (descrizione non le ha) */
        char cartella_app[MAX_PERCORSO];
        copia(cartella_app, cartelle, sizeof cartella_app);
        aggiungi(cartella_app, "/", sizeof cartella_app);
        aggiungi(cartella_app, v.nome, sizeof cartella_app);
        static struct manifesto m;
        if (manifesto_dell_app(cartella_app, &m) < 0) continue;
        const char *gia[2];
        int quante = 0;
        voci_barra[i].immagine = carica_icona(a, cartella_app, png_per(&m, USERBAR_PNG_BARRA), gia, &quante);
        voci_vassoio[i].immagine = carica_icona(a, cartella_app, png_per(&m, USERBAR_PNG_VASSOIO), gia, &quante);
    }
    if (d >= 0) chiudi(d);
}

static void leggi_applicazioni(void) {
    for (int i = 0; i < QUANTE_CARTELLE_APPLICAZIONI; i++)
        leggi_applicazioni_in(cartelle_applicazioni[i]);
    /* nella userbar quelle del sistema, nel loro ordine */
    for (int i = 0; i < quante_app; i++)
        if (app[i].di_sistema) {
            voci_barra[quante_fissate] = voci_barra[i];
            fissata[quante_fissate++] = i;
        }
}

/* --- le finestre aperte: le racconta il compositore --- */

#define MAX_APERTE 32

static struct { unsigned int id; char programma[MAX_PERCORSO_PROGRAMMA]; } aperte[MAX_APERTE];
static int quante_aperte;
static unsigned int attiva;   /* la finestra che ha il fuoco (0 nessuna) */

/* La finestra piu' recente dell'applicazione, o 0 se non ne ha. */
static unsigned int finestra_di(const struct app *a) {
    for (int i = quante_aperte - 1; i >= 0; i--)
        if (uguali(aperte[i].programma, a->programma)) return aperte[i].id;
    return 0;
}

/* L'applicazione ha la finestra con il fuoco? */
static int in_primo_piano(const struct app *a) {
    for (int i = 0; i < quante_aperte; i++)
        if (aperte[i].id == attiva && uguali(aperte[i].programma, a->programma)) return 1;
    return 0;
}

static int stato_app(const struct app *a) {
    return in_primo_piano(a) ? APP_ATTIVA : finestra_di(a) ? APP_APERTA : APP_CHIUSA;
}

/* Una notizia dal compositore sulle finestre: 1 se la userbar va ridisegnata. Se cambia la
 * finestra con il fuoco, *fuoco_cambiato diventa 1. */
static int notizia_finestre(const struct notizia *n, int *fuoco_cambiato) {
    if (n->tipo == FINESTRA_APERTA && quante_aperte < MAX_APERTE) {
        aperte[quante_aperte].id = n->numero;
        copia(aperte[quante_aperte].programma, n->testo, MAX_PERCORSO_PROGRAMMA);
        quante_aperte++;
        return 1;
    }
    if (n->tipo == FINESTRA_ATTIVA) {
        if (attiva != n->numero) *fuoco_cambiato = 1;
        attiva = n->numero;
        return 1;
    }
    if (n->tipo == FINESTRA_CHIUSA) {
        for (int i = 0; i < quante_aperte; i++)
            if (aperte[i].id == n->numero) {
                aperte[i] = aperte[--quante_aperte];
                return 1;
            }
    }
    return 0;
}

/* --- i pannelli: la userbar e il vassoio --- */

static struct finestra barra, vassoio;
static int barra_x, barra_y;
static int vassoio_aperto, vassoio_x, vassoio_y;

/* Il puntatore su un pannello, come su dei pulsanti: la voce sotto e quella su cui si e'
 * premuto. Scatta quando si LASCIA, sulla stessa voce su cui si era premuto. */
struct puntatore_pannello {
    int sopra, premuta;         /* -1 nessuna */
    unsigned int pulsanti_prima;
};

static struct puntatore_pannello su_barra = { -1, -1, 0 }, su_vassoio = { -1, -1, 0 };

/* Una notizia del puntatore per il pannello 'pp', con 'voce' quella sotto il punto. Torna
 * la voce cliccata (premuta e lasciata sopra), o -1; *cambiato a 1 se va ridisegnato. */
static int puntatore_pannello(struct puntatore_pannello *pp, const struct notizia *n, int voce,
                              int *cambiato) {
    int giu = n->pulsanti & PULSANTE_SINISTRO, prima = pp->pulsanti_prima & PULSANTE_SINISTRO;
    int sopra_prima = pp->sopra, premuta_prima = pp->premuta, cliccata = -1;
    pp->pulsanti_prima = n->pulsanti;
    pp->sopra = voce;
    if (giu && !prima) pp->premuta = voce;
    if (!giu && prima) {
        if (pp->premuta >= 0 && voce == pp->premuta) cliccata = voce;
        pp->premuta = -1;
    }
    if (pp->sopra != sopra_prima || pp->premuta != premuta_prima) *cambiato = 1;
    return cliccata;
}

static int disegna_barra(void) {
    unsigned int *pixel = finestra_tela(&barra);
    if (!pixel) return 0;
    struct tela t = { pixel, barra.larghezza, barra.altezza };
    struct pennello p;
    pennello_prepara(&p, &t);
    pulisci(&p);
    int stati[MAX_APP];
    for (int i = 0; i < quante_fissate; i++) stati[i] = stato_app(&app[fissata[i]]);
    /* mentre il vassoio e' aperto il pulsante App resta premuto */
    userbar_disegna(&p, 0, 0, voci_barra, stati, quante_fissate, su_barra.sopra,
                    vassoio_aperto ? 0 : su_barra.premuta);
    return finestra_mostra(&barra) >= 0;
}

static int disegna_vassoio(void) {
    unsigned int *pixel = finestra_tela(&vassoio);
    if (!pixel) return 0;
    struct tela t = { pixel, vassoio.larghezza, vassoio.altezza };
    struct pennello p;
    pennello_prepara(&p, &t);
    pulisci(&p);
    vassoio_disegna(&p, 0, 0, voci_vassoio, quante_app, su_vassoio.sopra, su_vassoio.premuta);
    return finestra_mostra(&vassoio) >= 0;
}

static int schermo_l, schermo_a;

static void apri_vassoio(void) {
    if (vassoio_aperto) return;
    int l, a;
    vassoio_misura(quante_app, &l, &a);
    vassoio_posto(barra_x, barra_y, quante_app, &vassoio_x, &vassoio_y);
    /* centrato sul pulsante App, ma dentro lo schermo */
    if (vassoio_y + a > schermo_a - USERBAR_DAL_BORDO) vassoio_y = schermo_a - USERBAR_DAL_BORDO - a;
    if (vassoio_y < USERBAR_DAL_BORDO) vassoio_y = USERBAR_DAL_BORDO;
    su_vassoio = (struct puntatore_pannello){ -1, -1, 0 };
    if (finestra_apri_pannello(&vassoio, l, a, "vassoio", PANNELLO_SOPRA, vassoio_x, vassoio_y,
                               VASSOIO_RAGGIO) < 0)
        return;
    vassoio_aperto = 1;
    if (!disegna_vassoio()) { finestra_chiudi(&vassoio); vassoio_aperto = 0; }
}

static void chiudi_vassoio(void) {
    if (!vassoio_aperto) return;
    finestra_chiudi(&vassoio);
    vassoio_aperto = 0;
}

/* Il clic su un'applicazione: aprirla, riportarla davanti, o niente (vedi in cima). */
static void clic_su(const struct app *a) {
    unsigned int finestra = finestra_di(a);
    if (!finestra) { apri_applicazione(a->cartella); return; }
    if (in_primo_piano(a)) return;
    struct porta_davanti m = { .tipo = PORTA_DAVANTI, .numero = finestra };
    manda(barra.collegamento, &m, sizeof m);
}

/* Le notizie del puntatore per la userbar: dove e' (PUNTATORE) o che e' uscito (USCITO,
 * come un puntatore fuori da tutto: nessuna voce resta accesa). 1 se va ridisegnata. Vale
 * anche per quelle messe da parte mentre la barra si ridisegnava: un USCITO perso
 * lascerebbe accesa l'ultima voce toccata. */
static int notizia_barra(const struct notizia *n) {
    if (n->tipo != PUNTATORE && n->tipo != USCITO) return 0;
    int voce = n->tipo == PUNTATORE ? userbar_voce_sotto(0, 0, quante_fissate, n->x, n->y) : -1;
    int cambiato = 0;
    int cliccata = puntatore_pannello(&su_barra, n->tipo == PUNTATORE ? n : &(struct notizia){ .tipo = USCITO },
                                      voce, &cambiato);
    if (cliccata == 0) {                     /* il pulsante App: apre o chiude il vassoio */
        if (vassoio_aperto) chiudi_vassoio();
        else apri_vassoio();
        cambiato = 1;
    } else if (cliccata > 0) {
        if (vassoio_aperto) { chiudi_vassoio(); cambiato = 1; }
        clic_su(&app[fissata[cliccata - 1]]);
    }
    return cambiato;
}

/* Le notizie del puntatore per il vassoio. 1 se la userbar va ridisegnata (il vassoio si
 * e' chiuso); il vassoio si ridisegna da solo. */
static int notizia_vassoio(const struct notizia *n) {
    if (!vassoio_aperto || (n->tipo != PUNTATORE && n->tipo != USCITO)) return 0;
    int voce = n->tipo == PUNTATORE ? vassoio_voce_sotto(0, 0, quante_app, n->x, n->y) : -1;
    int cambiato = 0;
    int cliccata = puntatore_pannello(&su_vassoio, n->tipo == PUNTATORE ? n : &(struct notizia){ .tipo = USCITO },
                                      voce, &cambiato);
    if (cliccata >= 0) {
        chiudi_vassoio();
        clic_su(&app[cliccata]);
        return 1;
    }
    if (cambiato && !disegna_vassoio()) { chiudi_vassoio(); return 1; }
    return 0;
}

/* --- lo sfondo --- */

static struct finestra sfondo;

/* Lo sfondo del disegno, ingrandito da quello piccolo (320x200, il materiale sfondo_piccolo:
 * /system/resources/wallpapers/default.image): ogni pixel nuovo e' una media pesata dei
 * quattro vicini di quello piccolo. E' una sfumatura morbida: ingrandendola non si perde
 * niente. Se il file manca (GKit lo scrive nel registro), lo sfondo e' grigio. */
#define SFONDO_DI_RISERVA 0xFF505058u
static void prepara_sfondo(unsigned int *pixel, int l, int a) {
    const struct immagine *piccolo = &sfondo_piccolo;
    int pl = piccolo->l, pa = piccolo->a;
    if (pl < 1 || pa < 1) {
        for (int i = 0; i < l * a; i++) pixel[i] = SFONDO_DI_RISERVA;
        return;
    }
    const unsigned int *sp = piccolo->pixel;
    for (int y = 0; y < a; y++) {
        int fy = y * ((pa - 1) << 8) / (a - 1);   /* in 256esimi di pixel piccolo */
        int iy = fy >> 8, ty = fy & 255, iy2 = iy + 1 < pa ? iy + 1 : iy;
        for (int x = 0; x < l; x++) {
            int fx = x * ((pl - 1) << 8) / (l - 1);
            int ix = fx >> 8, tx = fx & 255, ix2 = ix + 1 < pl ? ix + 1 : ix;
            unsigned int c1 = sp[iy * pl + ix], c2 = sp[iy * pl + ix2];
            unsigned int c3 = sp[iy2 * pl + ix], c4 = sp[iy2 * pl + ix2];
            unsigned int px = 0xFF000000u;   /* opaco */
            for (int k = 0; k < 24; k += 8) {
                int su = ((c1 >> k) & 0xFF) * (256 - tx) + ((c2 >> k) & 0xFF) * tx;
                int giu = ((c3 >> k) & 0xFF) * (256 - tx) + ((c4 >> k) & 0xFF) * tx;
                px |= (unsigned int)((su * (256 - ty) + giu * ty) >> 16) << k;
            }
            pixel[y * l + x] = px;
        }
    }
}

/* --- le icone sullo sfondo --- */

/* LE ICONE DELLA SCRIVANIA, in colonna in alto a sinistra, a destra della userbar: l'elemento
 * di un file di Files (elemento_file.h, con l'oggetto del suo tipo: oggetti.h). Sotto il
 * puntatore si vela appena, premere la sceglie (vetro chiaro), premere altrove sullo sfondo
 * la lascia; il DOPPIO CLIC la apre.
 * Oggi c'e' solo "Shell", PROVVISORIA (STATO.md): apre il terminale con la shell di prova,
 * un programma normale che il launcher fa partire come la persona (APRI_PROGRAMMA). Le icone
 * sono un elenco perche' un giorno saranno quelle della cartella Scrivania della persona. */
struct icona_scrivania {
    const char *nome;
    int oggetto;                    /* OGGETTO_... */
    const char *programma;          /* cosa si apre con il doppio clic (in /system/bin) */
};

static const struct icona_scrivania icone[] = {
    { "Shell", OGGETTO_BIN, "/system/bin/terminale" },
};
#define QUANTE_ICONE ((int)(sizeof icone / sizeof icone[0]))

#define ICONE_X (USERBAR_DAL_BORDO + USERBAR_LARGHEZZA + SPAZIO_16)   /* dopo la userbar */
#define ICONE_Y SPAZIO_16

static int icona_y[QUANTE_ICONE], icona_alta[QUANTE_ICONE];
static int icona_sopra = -1, icona_scelta = -1;   /* -1: nessuna */
static unsigned int *sfondo_pulito;  /* lo sfondo senza le icone: lo si ricopia a ogni disegno */

static void sistema_icone(void) {
    int y = ICONE_Y;
    for (int i = 0; i < QUANTE_ICONE; i++) {
        icona_y[i] = y;
        icona_alta[i] = elemento_altezza(icone[i].nome);
        y += icona_alta[i] + ELEMENTO_TRA;
    }
}

static int icona_sotto(int x, int y) {
    for (int i = 0; i < QUANTE_ICONE; i++)
        if (x >= ICONE_X && x < ICONE_X + ELEMENTO_LARGHEZZA && y >= icona_y[i] && y < icona_y[i] + icona_alta[i])
            return i;
    return -1;
}

/* Lo sfondo con sopra le icone, nel loro stato. Si ricopia tutto lo sfondo (cosi' tutte e
 * due le immagini della finestra sono giuste), ma al compositore si dice solo la colonna
 * delle icone, l'unica parte che cambia ('tutto' la prima volta). */
static int disegna_sfondo(int tutto) {
    unsigned int *pixel = finestra_tela(&sfondo);
    if (!pixel) return 0;
    for (int i = 0; i < schermo_l * schermo_a; i++) pixel[i] = sfondo_pulito[i];
    struct tela t = { pixel, schermo_l, schermo_a };
    struct pennello p;
    pennello_prepara(&p, &t);
    for (int i = 0; i < QUANTE_ICONE; i++) {
        int stato = i == icona_scelta ? ELEMENTO_STATO_SCELTO : i == icona_sopra ? ELEMENTO_STATO_SOPRA
                                                                                   : ELEMENTO_NORMALE;
        elemento_disegna_oggetto(&p, ICONE_X, icona_y[i], icona_alta[i], icone[i].oggetto, icone[i].nome, stato);
    }
    struct zona_finestra colonna = { ICONE_X, ICONE_Y, ELEMENTO_LARGHEZZA,
                                     icona_y[QUANTE_ICONE - 1] + icona_alta[QUANTE_ICONE - 1] - ICONE_Y };
    return finestra_mostra_zone(&sfondo, &colonna, tutto ? 0 : 1) >= 0;
}

/* Il puntatore sullo sfondo: illumina, sceglie e apre le icone; premere li' chiude anche il
 * vassoio (come un clic fuori da un menu). Restituisce 1 se il vassoio si e' chiuso (la
 * userbar va ridisegnata); *icone_cambiate a 1 se le icone vanno ridisegnate. */
static int notizia_sfondo(const struct notizia *n, int *icone_cambiate) {
    static unsigned int pulsanti_prima, clic_prima;
    if (n->tipo != PUNTATORE && n->tipo != USCITO) return 0;
    int sopra_prima = icona_sopra, scelta_prima = icona_scelta;
    icona_sopra = n->tipo == PUNTATORE ? icona_sotto(n->x, n->y) : -1;
    int premuto = n->tipo == PUNTATORE && (n->pulsanti & ~pulsanti_prima) != 0;   /* un pulsante qualsiasi */
    int appena = premuto && (n->pulsanti & ~pulsanti_prima & PULSANTE_SINISTRO);
    pulsanti_prima = n->tipo == PUNTATORE ? n->pulsanti : 0;
    if (appena) {
        unsigned int adesso = tempo();
        if (icona_sopra >= 0 && icona_sopra == icona_scelta && (int)(adesso - clic_prima) < DOPPIO_CLIC) {
            apri_programma(icone[icona_sopra].programma);
            clic_prima = 0;
        } else {
            clic_prima = adesso;
        }
        icona_scelta = icona_sopra;
    }
    if (icona_sopra != sopra_prima || icona_scelta != scelta_prima) *icone_cambiate = 1;
    if (!premuto || !vassoio_aperto) return 0;
    chiudi_vassoio();
    return 1;
}

/* --- il ciclo --- */

int main(void) {
    if (finestra_schermo(&schermo_l, &schermo_a) < 0) {
        scrivi_testo("scrivania: non trovo il compositore\n");
        return 1;
    }
    leggi_applicazioni();

    sistema_icone();
    sfondo_pulito = malloc((unsigned long)schermo_l * schermo_a * 4);
    if (!sfondo_pulito) return 1;
    prepara_sfondo(sfondo_pulito, schermo_l, schermo_a);
    if (finestra_apri_come(&sfondo, schermo_l, schermo_a, "sfondo", RUOLO_SFONDO, 0, 0, 0) < 0 ||
        !disegna_sfondo(1))
        return 1;

    userbar_posto(schermo_a, quante_fissate, &barra_x, &barra_y);
    if (finestra_apri_pannello(&barra, USERBAR_LARGHEZZA, userbar_altezza(quante_fissate), "userbar",
                               PANNELLO_BARRA, barra_x, barra_y, USERBAR_RAGGIO) < 0 ||
        !disegna_barra())
        return 1;

    /* Si aspettano le notizie del compositore: le finestre che compaiono e spariscono, il
     * puntatore sui pannelli e sullo sfondo, e i suoi "ci sei?", a cui risponde la libreria. */
    for (;;) {
        struct da_guardare g[3] = { { sfondo.collegamento, 0 }, { barra.collegamento, 0 },
                                    { vassoio.collegamento, 0 } };
        if (aspetta(g, vassoio_aperto ? 3 : 2, -1) < 0) return 1;
        struct notizia n;
        int ridisegna = 0, fuoco_cambiato = 0;
        if (g[0].pronto) {
            if (!finestra_ricevi(&sfondo, &n)) return 1;   /* il compositore ha chiuso */
            int icone_cambiate = 0;
            ridisegna |= notizia_sfondo(&n, &icone_cambiate);
            if (icone_cambiate) {
                if (!disegna_sfondo(0)) return 1;
                /* le notizie arrivate mentre si aspettava un'immagine libera */
                int ancora = 0;
                for (int i = 0; i < sfondo.quante; i++)
                    ridisegna |= notizia_sfondo(&sfondo.messe_da_parte[i], &ancora);
                sfondo.quante = 0;
                if (ancora && !disegna_sfondo(0)) return 1;
            }
        }
        if (g[1].pronto) {
            if (!finestra_ricevi(&barra, &n)) return 1;
            ridisegna |= notizia_barra(&n);
            ridisegna |= notizia_finestre(&n, &fuoco_cambiato);
        }
        if (vassoio_aperto && g[2].pronto) {
            /* del vassoio conta solo il puntatore: le notizie sulle finestre arrivano
             * uguali alla userbar */
            if (finestra_ricevi(&vassoio, &n)) ridisegna |= notizia_vassoio(&n);
            else { chiudi_vassoio(); ridisegna = 1; }   /* il compositore l'ha chiuso */
            /* quelle messe da parte mentre il vassoio si ridisegnava */
            for (int i = 0; vassoio_aperto && i < vassoio.quante; i++)
                ridisegna |= notizia_vassoio(&vassoio.messe_da_parte[i]);
            if (vassoio_aperto) vassoio.quante = 0;
        }
        if (fuoco_cambiato && vassoio_aperto) { chiudi_vassoio(); ridisegna = 1; }  /* un'altra finestra davanti */
        if (ridisegna) {
            if (!disegna_barra()) return 1;
            /* mentre aspettava una tela libera, la libreria ha messo da parte le notizie */
            int ancora = 0;
            fuoco_cambiato = 0;
            for (int i = 0; i < barra.quante; i++) {
                ancora |= notizia_barra(&barra.messe_da_parte[i]);
                ancora |= notizia_finestre(&barra.messe_da_parte[i], &fuoco_cambiato);
            }
            barra.quante = 0;
            if (fuoco_cambiato && vassoio_aperto) { chiudi_vassoio(); ancora = 1; }
            if (ancora && !disegna_barra()) return 1;
        }
        sfondo.quante = 0;   /* (gia' servite sopra, dopo il disegno delle icone) */
    }
}
