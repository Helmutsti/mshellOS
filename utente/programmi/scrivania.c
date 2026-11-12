/* La scrivania: la "faccia" del sistema, come GNOME Shell o il Dock e la barra del Mac.
 *
 * E' un programma del sistema (uid 0), sorvegliato da init come il compositore. Per il
 * compositore e' un'applicazione come le altre, ma siccome e' del sistema puo' chiedere
 * delle SUPERFICI SPECIALI (vedi gkit.h):
 *   - lo SFONDO, grande come lo schermo, sotto tutte le finestre;
 *   - l'ISOLA, un PANNELLO in alto al centro, sopra tutte le finestre: il pulsante del
 *     menu, le applicazioni installate (con un puntino sotto quelle aperte) e l'ora.
 *
 * LE APPLICAZIONI INSTALLATE le legge da /system/apps (quelle del sistema) e poi da
 * /data/apps (quelle installate dalla persona): una cartella per applicazione, con il suo
 * manifest.toml (descrizione.h). Le legge una volta, all'avvio: un'app installata dopo si
 * vede quando la scrivania riparte. QUALI SONO APERTE glielo dice
 * il compositore, che a un pannello racconta ogni finestra che compare o sparisce, con il
 * programma a cui appartiene.
 *
 * IL CLIC SU UN'APPLICAZIONE DELL'ISOLA:
 *   - se e' chiusa, la scrivania chiede al launcher (il servizio che fa partire le
 *     applicazioni) "apri questa": e' lui a farla partire, la scrivania non fa partire
 *     niente da sola;
 *   - se e' aperta, ma ridotta o dietro le altre, chiede al compositore di riportare
 *     davanti la sua finestra (PORTA_DAVANTI);
 *   - se e' gia' in primo piano, niente, come il Dock del Mac.
 * Sotto le applicazioni aperte c'e' un puntino; sotto quella in primo piano, una
 * lineetta.
 *
 * LA CARTELLA HOME, in alto a sinistra sullo sfondo, come una cartella del Finder sulla
 * scrivania del Mac: l'icona grande e il nome ("Elemento file" del disegno, lo stesso di
 * Files: elemento_file.h). Sotto il puntatore si vela appena, un clic la sceglie (vetro
 * chiaro), un clic altrove sullo sfondo la lascia. Il DOPPIO CLIC la apre: aprire una
 * cartella vuol dire far partire Files su di lei, quindi la scrivania chiede al launcher
 * "apri files, con argomento /home" (launcher.h).
 *
 * Disegna con la libreria di disegno e lo stile del sistema, come il disegno Figma
 * "mshell - interfaccia" (Isola, Voce dell'isola, Pulsante applicazioni, Orologio). */
#include "gkit/gkit.h"
#include "uikit/stile.h"
#include "descrizione.h"
#include "launcher.h"
#include "uikit/elemento_file.h"

/* --- le applicazioni installate --- */

#define MAX_APP 16

struct app {
    char cartella[MAX_NOME_FILE];   /* il nome della sua cartella: lo si chiede al launcher */
    char programma[MAX_PERCORSO];   /* il percorso completo del programma */
    int icona;
    unsigned int colore;            /* 0: neutra (di vetro) */
};

static struct app app[MAX_APP];
static int quante_app;

/* LE ICONE DELL'ISOLA: TEMPORANEE. Ogni app ha le sue icone SVG nel pacchetto
 * (resources/icona-chiara.svg e icona-scura.svg, vedi il manifesto), ma la scrivania non
 * sa ancora disegnare gli SVG: per ora mostra le icone Lucide di sempre, in un tondo
 * colorato, scelte da questa tabella in base all'id dell'app. Un'app che non e' qui ha il
 * quadrato su vetro. Quando la scrivania disegnera' gli SVG, la tabella sparisce. */
static const struct { const char *id; int icona; unsigned int colore; } icone_temporanee[] = {
    { "attivita", ICONA_ACTIVITY, VERDE_NORMALE },
    { "contatore", ICONA_PLUS, BLU_NORMALE },
    { "doom", ICONA_TRIANGLE_ALERT, ROSSO_NORMALE },
    { "esempio", ICONA_SQUARE, ARANCIO_NORMALE },
    { "files", ICONA_FOLDER, BLU_NORMALE },
    { "impostazioni", ICONA_SETTINGS, 0 },
    { "store", ICONA_SAVE, 0 },        /* fra le icone Lucide di oggi non c'e' "package" */
};

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
        struct app *a = &app[quante_app++];
        *a = (struct app){ .icona = ICONA_SQUARE };
        copia(a->cartella, v.nome, sizeof a->cartella);
        copia(a->programma, desc.programma, sizeof a->programma);
        for (unsigned int i = 0; i < sizeof icone_temporanee / sizeof icone_temporanee[0]; i++)
            if (uguali(desc.id, icone_temporanee[i].id)) {
                a->icona = icone_temporanee[i].icona;
                a->colore = icone_temporanee[i].colore;
            }
    }
    if (d >= 0) chiudi(d);
}

static void leggi_applicazioni(void) {
    for (int i = 0; i < QUANTE_CARTELLE_APPLICAZIONI; i++)
        leggi_applicazioni_in(cartelle_applicazioni[i]);
}

/* --- le finestre aperte: le racconta il compositore --- */

#define MAX_APERTE 32

static struct { unsigned int id; char programma[MAX_PERCORSO]; } aperte[MAX_APERTE];
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

static int e_aperta(const struct app *a) {
    for (int i = 0; i < quante_aperte; i++)
        if (uguali(aperte[i].programma, a->programma)) return 1;
    return 0;
}

/* Una notizia dal compositore: 1 se l'isola va ridisegnata. */
static int notizia(const struct notizia *n) {
    if (n->tipo == FINESTRA_APERTA && quante_aperte < MAX_APERTE) {
        aperte[quante_aperte].id = n->numero;
        copia(aperte[quante_aperte].programma, n->testo, MAX_PERCORSO);
        quante_aperte++;
        return 1;
    }
    if (n->tipo == FINESTRA_ATTIVA) {
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

/* --- l'ora --- */

static const char *const giorni[] = { "dom", "lun", "mar", "mer", "gio", "ven", "sab" };
static const char *const mesi[] = { "gen", "feb", "mar", "apr", "mag", "giu",
                                    "lug", "ago", "set", "ott", "nov", "dic" };

static void due_cifre(char *dove, unsigned int n) {
    dove[0] = '0' + n / 10 % 10;
    dove[1] = '0' + n % 10;
    dove[2] = 0;
}

/* "mar 10 nov  13:58", come nel disegno */
static void scrivi_ora(char *dove, const struct data_ora *d) {
    char cifre[3];
    dove[0] = 0;
    aggiungi(dove, giorni[d->giorno_settimana % 7], 32);
    aggiungi(dove, " ", 32);
    due_cifre(cifre, d->giorno);
    aggiungi(dove, cifre[0] == '0' ? cifre + 1 : cifre, 32);
    aggiungi(dove, " ", 32);
    aggiungi(dove, mesi[(d->mese + 11) % 12], 32);
    aggiungi(dove, "  ", 32);
    due_cifre(cifre, d->ore);
    aggiungi(dove, cifre, 32);
    aggiungi(dove, ":", 32);
    due_cifre(cifre, d->minuti);
    aggiungi(dove, cifre, 32);
}

/* Lo spazio per l'ora: quello della data piu' larga possibile, cosi' l'isola non cambia
 * misura quando cambia l'ora. */
static int larghezza_ora(void) {
    const struct font *f = FONT_BARRA;
    int cifra = 0, giorno = 0, mese = 0;
    for (char c = '0'; c <= '9'; c++) {
        char s[2] = { c, 0 };
        int l = larghezza_testo(f, s);
        if (l > cifra) cifra = l;
    }
    for (int i = 0; i < 7; i++) if (larghezza_testo(f, giorni[i]) > giorno) giorno = larghezza_testo(f, giorni[i]);
    for (int i = 0; i < 12; i++) if (larghezza_testo(f, mesi[i]) > mese) mese = larghezza_testo(f, mesi[i]);
    return giorno + mese + 6 * cifra + larghezza_testo(f, "    :");
}

/* --- l'isola --- */

#define ISOLA_ALTEZZA  52
#define ISOLA_MARGINE  10      /* intorno al contenuto */
#define ISOLA_SPAZIO   14      /* tra un gruppo e l'altro */
#define VOCE_LATO      32      /* i tondi delle applicazioni e del menu */
#define VOCI_SPAZIO     6      /* tra un'applicazione e l'altra */
#define VOCE_ICONA     16
#define VOCE_OMBRA     VELATO(0x050A24, 64)    /* 25%, spostata di 2, sfocata 3 */
#define VOCE_LUCE      VELATO(0xFFFFFF, 77)    /* la riga di luce in alto, 30% */
#define ISOLA_DALL_ALTO 12

static struct finestra isola;
static int voce_sopra = -1;     /* l'applicazione con il puntatore sopra, o -1 */
static int voce_premuta = -1;   /* quella su cui si e' premuto (finche' non si lascia), o -1 */
static int isola_l, spazio_ora;
static char ora_mostrata[32];   /* il testo dell'ora che si vede adesso nell'isola */

static int calcola_isola(void) {
    spazio_ora = larghezza_ora();
    int voci = quante_app ? quante_app * VOCE_LATO + (quante_app - 1) * VOCI_SPAZIO + ISOLA_SPAZIO : 0;
    return 2 * ISOLA_MARGINE + VOCE_LATO + ISOLA_SPAZIO + voci + 1 + ISOLA_SPAZIO + spazio_ora;
}

/* Gli stati di una voce, come quelli di un pulsante */
#define VOCE_NORMALE 0
#define VOCE_SOPRA   1   /* il puntatore e' sopra */
#define VOCE_PREMUTA 2   /* premuta, e il puntatore e' ancora sopra */

/* Il colore di una voce colorata nei tre stati: normale, sopra, premuta. */
static const unsigned int colori_voce[][3] = {
    { BLU_NORMALE,     BLU_SOPRA,     BLU_PREMUTO     },
    { VERDE_NORMALE,   VERDE_SOPRA,   VERDE_PREMUTO   },
    { ROSSO_NORMALE,   ROSSO_SOPRA,   ROSSO_PREMUTO   },
    { ARANCIO_NORMALE, ARANCIO_SOPRA, ARANCIO_PREMUTO },
};

static unsigned int colore_voce(unsigned int colore, int stato) {
    for (unsigned int i = 0; i < sizeof colori_voce / sizeof colori_voce[0]; i++)
        if (colori_voce[i][0] == colore) return colori_voce[i][stato];
    return colore;
}

/* Un tondo come "Icona app" del disegno: colorato (con ombra e luce) o di vetro. Premuta,
 * la voce "scende": spariscono l'ombra e la luce, come in un pulsante. */
static void voce(struct pennello *p, int x, int y, unsigned int colore, int icona_voce, int stato) {
    struct tondo t = tondo(x, y, VOCE_LATO, VOCE_LATO, VOCE_LATO / 2);
    if (colore) {
        if (stato != VOCE_PREMUTA) {
            pennello_colore(p, VOCE_OMBRA);
            ombra_forma(p, &t, 0, 2, 3);
        }
        pennello_colore(p, colore_voce(colore, stato));
        riempi_forma(p, &t);
        pennello_colore(p, PULSANTE_BORDO_COLORATO);
        contorna_forma(p, &t, 256);
        if (stato != VOCE_PREMUTA) {
            pennello_colore(p, VOCE_LUCE);
            luce_sopra(p, &t);
        }
    } else {
        /* di vetro: sotto il puntatore uno strato di vetro in piu', premuta piu' scura */
        pennello_colore(p, stato == VOCE_PREMUTA ? VETRO_SUPERFICIE : VETRO_SOPRA);
        riempi_forma(p, &t);
        if (stato == VOCE_SOPRA) {
            pennello_colore(p, VETRO_PREMUTO);
            riempi_forma(p, &t);
        }
        pennello_colore(p, VETRO_BORDO);
        contorna_forma(p, &t, 256);
    }
    pennello_colore(p, CONTENUTO_PRINCIPALE);
    icona(p, icona_voce, VOCE_ICONA, x + (VOCE_LATO - VOCE_ICONA) / 2, y + (VOCE_LATO - VOCE_ICONA) / 2);
}

static int disegna_isola(void) {
    unsigned int *pixel = finestra_tela(&isola);
    if (!pixel) return 0;
    struct tela t = { pixel, isola_l, ISOLA_ALTEZZA };
    struct pennello p;
    pennello_prepara(&p, &t);
    pulisci(&p);

    struct tondo forma = tondo(0, 0, isola_l, ISOLA_ALTEZZA, ISOLA_ALTEZZA / 2);
    pennello_colore(&p, VETRO_FONDO);
    riempi_forma(&p, &forma);
    pennello_colore(&p, VETRO_SUPERFICIE);
    riempi_forma(&p, &forma);
    pennello_colore(&p, VETRO_BORDO);
    contorna_forma(&p, &forma, 256);

    int x = ISOLA_MARGINE, y = ISOLA_MARGINE;
    voce(&p, x, y, BLU_NORMALE, ICONA_LAYOUT_GRID, VOCE_NORMALE);          /* il pulsante del menu */
    x += VOCE_LATO + ISOLA_SPAZIO;

    for (int i = 0; i < quante_app; i++) {                   /* le applicazioni */
        int stato = voce_sopra != i ? VOCE_NORMALE : voce_premuta == i ? VOCE_PREMUTA : VOCE_SOPRA;
        voce(&p, x, y, app[i].colore, app[i].icona, stato);
        if (in_primo_piano(&app[i])) {                       /* la lineetta: in primo piano */
            pennello_colore(&p, CONTENUTO_PRINCIPALE);
            riempi_tondo(&p, x + VOCE_LATO / 2 - 7, y + VOCE_LATO + 2, 14, 4, 2);
        } else if (e_aperta(&app[i])) {                       /* il puntino: e' aperta */
            pennello_colore(&p, CONTENUTO_PRINCIPALE);
            riempi_tondo(&p, x + VOCE_LATO / 2 - 2, y + VOCE_LATO + 2, 4, 4, 2);
        }
        x += VOCE_LATO + (i + 1 < quante_app ? VOCI_SPAZIO : ISOLA_SPAZIO);
    }

    pennello_colore(&p, VETRO_BORDO);                        /* il separatore */
    riempi_rettangolo(&p, x, (ISOLA_ALTEZZA - 22) / 2, 1, 22);
    x += 1 + ISOLA_SPAZIO;

    struct data_ora d;
    data_ora(&d);
    scrivi_ora(ora_mostrata, &d);
    pennello_colore(&p, CONTENUTO_PRINCIPALE);
    testo(&p, FONT_BARRA, x + (spazio_ora - larghezza_testo(FONT_BARRA, ora_mostrata)) / 2,
          (ISOLA_ALTEZZA - FONT_BARRA->altezza) / 2, ora_mostrata);
    return finestra_mostra(&isola) >= 0;
}

/* Quale applicazione c'e' nel punto (x, y) dell'isola: il suo indice, o -1. */
static int app_sotto(int x, int y) {
    if (y < ISOLA_MARGINE || y >= ISOLA_MARGINE + VOCE_LATO) return -1;
    int inizio = ISOLA_MARGINE + VOCE_LATO + ISOLA_SPAZIO;
    for (int i = 0; i < quante_app; i++) {
        int vx = inizio + i * (VOCE_LATO + VOCI_SPAZIO);
        if (x >= vx && x < vx + VOCE_LATO) return i;
    }
    return -1;
}

/* Il clic su un'applicazione: aprirla, riportarla davanti, o niente (vedi in cima). */
static void clic_su(const struct app *a) {
    unsigned int finestra = finestra_di(a);
    if (!finestra) { apri_applicazione(a->cartella); return; }
    if (in_primo_piano(a)) return;
    struct porta_davanti m = { .tipo = PORTA_DAVANTI, .numero = finestra };
    manda(isola.collegamento, &m, sizeof m);
}

/* Il puntatore sull'isola. Come un pulsante: scatta quando si LASCIA, sulla stessa voce
 * su cui si era premuto. Allora si chiede al launcher di aprire quell'applicazione. */
static int puntatore(const struct notizia *n) {
    static unsigned int pulsanti_prima;
    int giu = n->pulsanti & PULSANTE_SINISTRO, prima = pulsanti_prima & PULSANTE_SINISTRO;
    int sopra_prima = voce_sopra, premuta_prima = voce_premuta;
    pulsanti_prima = n->pulsanti;
    voce_sopra = app_sotto(n->x, n->y);
    if (giu && !prima) voce_premuta = voce_sopra;
    if (!giu && prima) {
        if (voce_premuta >= 0 && voce_sopra == voce_premuta) clic_su(&app[voce_premuta]);
        voce_premuta = -1;
    }
    return voce_sopra != sopra_prima || voce_premuta != premuta_prima;   /* da ridisegnare */
}

/* Le notizie del puntatore: dove e' (PUNTATORE) o che e' uscito dall'isola (USCITO, come
 * un puntatore fuori da tutto: nessuna voce resta accesa). Le altre non lo riguardano.
 * Vale anche per quelle messe da parte mentre l'isola si ridisegnava: un USCITO perso
 * lascerebbe accesa l'ultima voce toccata. */
static int notizia_puntatore(const struct notizia *n) {
    if (n->tipo == PUNTATORE) return puntatore(n);
    if (n->tipo == USCITO) return puntatore(&(struct notizia){ .tipo = PUNTATORE, .x = -1, .y = -1 });
    return 0;
}

/* --- lo sfondo --- */

static struct finestra sfondo;
static int sfondo_l, sfondo_a;
static unsigned int *sfondo_pulito;   /* lo sfondo senza niente sopra, per ridisegnare la Home */

/* Lo sfondo del disegno, ingrandito da quello piccolo (320x200) di materiali_dati.h: ogni
 * pixel nuovo e' una media pesata dei quattro vicini di quello piccolo. E' una sfumatura
 * morbida: ingrandendola non si perde niente. */
static void prepara_sfondo(unsigned int *pixel, int l, int a) {
    for (int y = 0; y < a; y++) {
        int fy = y * ((SFONDO_A - 1) << 8) / (a - 1);   /* in 256esimi di pixel piccolo */
        int iy = fy >> 8, ty = fy & 255, iy2 = iy + 1 < SFONDO_A ? iy + 1 : iy;
        for (int x = 0; x < l; x++) {
            int fx = x * ((SFONDO_L - 1) << 8) / (l - 1);
            int ix = fx >> 8, tx = fx & 255, ix2 = ix + 1 < SFONDO_L ? ix + 1 : ix;
            unsigned int c1 = sfondo_piccolo[iy * SFONDO_L + ix], c2 = sfondo_piccolo[iy * SFONDO_L + ix2];
            unsigned int c3 = sfondo_piccolo[iy2 * SFONDO_L + ix], c4 = sfondo_piccolo[iy2 * SFONDO_L + ix2];
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

/* --- la cartella Home sullo sfondo --- */

#define HOME_X 16            /* dove sta, come nel disegno (in alto a sinistra) */
#define HOME_Y 16

static int home_stato = ELEMENTO_NORMALE;   /* normale, sotto il puntatore, scelta */
static int home_alta;

/* Lo sfondo, con sopra la Home nel suo stato. Lo sfondo vero si calcola una volta sola
 * (sfondo_pulito): ogni volta lo si ricopia e si disegna la Home. */
static int disegna_sfondo(void) {
    unsigned int *pixel = finestra_tela(&sfondo);
    if (!pixel) return 0;
    int quanti = sfondo_l * sfondo_a;
    if (sfondo_pulito) for (int i = 0; i < quanti; i++) pixel[i] = sfondo_pulito[i];
    else prepara_sfondo(pixel, sfondo_l, sfondo_a);
    struct tela t = { pixel, sfondo_l, sfondo_a };
    struct pennello p;
    pennello_prepara(&p, &t);
    elemento_disegna(&p, HOME_X, HOME_Y, home_alta, icone_file[FILE_HOME][0], "Home", home_stato);
    return finestra_mostra(&sfondo) >= 0;
}

static int sulla_home(int x, int y) {
    return x >= HOME_X && x < HOME_X + ELEMENTO_LARGHEZZA && y >= HOME_Y && y < HOME_Y + home_alta;
}

/* Il puntatore sullo sfondo: 1 se la Home va ridisegnata. Si sceglie premendo (come nel
 * Finder); due pressioni vicine sulla Home la aprono in Files. */
static int puntatore_sfondo(const struct notizia *n) {
    static unsigned int pulsanti_prima, clic_prima;
    static int scelta;
    int prima = home_stato;
    int giu = n->pulsanti & PULSANTE_SINISTRO, appena = giu && !(pulsanti_prima & PULSANTE_SINISTRO);
    pulsanti_prima = n->pulsanti;
    int sopra = n->tipo == PUNTATORE && sulla_home(n->x, n->y);
    if (appena) {
        unsigned int adesso = tempo();
        if (sopra && scelta && (int)(adesso - clic_prima) < DOPPIO_CLIC) {
            apri_applicazione_con("files", "/home");
            clic_prima = 0;
        } else {
            clic_prima = adesso;
        }
        scelta = sopra;
    }
    home_stato = scelta ? ELEMENTO_STATO_SCELTO : sopra ? ELEMENTO_STATO_SOPRA : ELEMENTO_NORMALE;
    return home_stato != prima;
}

static int notizia_sfondo(const struct notizia *n) {
    if (n->tipo == PUNTATORE) return puntatore_sfondo(n);
    if (n->tipo == USCITO) return puntatore_sfondo(&(struct notizia){ .tipo = USCITO });
    return 0;
}

/* --- il ciclo --- */

int main(void) {
    int schermo_l, schermo_a;
    if (finestra_schermo(&schermo_l, &schermo_a) < 0) {
        scrivi_testo("scrivania: non trovo il compositore\n");
        return 1;
    }
    leggi_applicazioni();

    sfondo_l = schermo_l;
    sfondo_a = schermo_a;
    home_alta = elemento_altezza("Home");
    sfondo_pulito = malloc((unsigned long)schermo_l * schermo_a * 4);
    if (sfondo_pulito) prepara_sfondo(sfondo_pulito, schermo_l, schermo_a);
    if (finestra_apri_come(&sfondo, schermo_l, schermo_a, "sfondo", RUOLO_SFONDO, 0, 0, 0) < 0 ||
        !disegna_sfondo())
        return 1;
    isola_l = calcola_isola();
    if (finestra_apri_come(&isola, isola_l, ISOLA_ALTEZZA, "isola", RUOLO_PANNELLO,
                           (schermo_l - isola_l) / 2, ISOLA_DALL_ALTO, ISOLA_ALTEZZA / 2) < 0 ||
        !disegna_isola())
        return 1;

    /* Si aspettano le notizie del compositore (le finestre che compaiono e spariscono, e i
     * suoi "ci sei?", a cui risponde la libreria) e l'inizio del minuto dopo, per l'ora.
     *
     * L'ORA. Non basta ridisegnare quando l'attesa scade: il compositore manda un "ci
     * sei?" ogni secondo, quindi l'attesa finisce quasi sempre prima, per una notizia, e
     * il cambio di minuto passerebbe inosservato. Allora, comunque ci si sia svegliati,
     * si riscrive l'ora e la si confronta con quella che si vede: se e' cambiata (un
     * minuto nuovo, o a mezzanotte un giorno nuovo) si ridisegna. Il tempo massimo
     * dell'attesa (fino al minuto dopo) serve solo se nessuno scrive. */
    for (;;) {
        struct data_ora d;
        data_ora(&d);
        struct da_guardare g[2] = { { sfondo.collegamento, 0 }, { isola.collegamento, 0 } };
        int pronti = aspetta(g, 2, (60 - (int)d.secondi) * 1000);
        if (pronti < 0) return 1;
        char ora[32];
        data_ora(&d);
        scrivi_ora(ora, &d);
        int ridisegna = !uguali(ora, ora_mostrata);  /* e' cambiato il minuto */
        struct notizia n;
        if (g[0].pronto) {
            if (!finestra_ricevi(&sfondo, &n)) return 1;   /* il compositore ha chiuso */
            if (notizia_sfondo(&n)) {
                if (!disegna_sfondo()) return 1;
                /* le notizie arrivate mentre si aspettava una tela libera */
                int ancora = 0;
                for (int i = 0; i < sfondo.quante; i++) ancora |= notizia_sfondo(&sfondo.messe_da_parte[i]);
                sfondo.quante = 0;
                if (ancora && !disegna_sfondo()) return 1;
            }
        }
        if (g[1].pronto) {
            if (!finestra_ricevi(&isola, &n)) return 1;
            ridisegna |= notizia_puntatore(&n);
            ridisegna |= notizia(&n);
        }
        if (ridisegna) {
            if (!disegna_isola()) return 1;
            /* mentre aspettava una tela libera, la libreria ha messo da parte le notizie */
            for (int i = 0; i < isola.quante; i++) {
                notizia_puntatore(&isola.messe_da_parte[i]);
                notizia(&isola.messe_da_parte[i]);
            }
            if (isola.quante) { isola.quante = 0; if (!disegna_isola()) return 1; }
        }
    }
}
