/* La scrivania: la "faccia" del sistema, come GNOME Shell o il Dock e la barra del Mac.
 *
 * E' un programma del sistema (uid 0), sorvegliato da init come il compositore. Per il
 * compositore e' un'applicazione come le altre, ma siccome e' del sistema puo' chiedere
 * delle SUPERFICI SPECIALI (vedi protocollo.h):
 *   - lo SFONDO, grande come lo schermo, sotto tutte le finestre;
 *   - l'ISOLA, un PANNELLO in alto al centro, sopra tutte le finestre: il pulsante del
 *     menu, le applicazioni installate (con un puntino sotto quelle aperte) e l'ora.
 *
 * LE APPLICAZIONI INSTALLATE le legge da /Applicazioni: una cartella per applicazione,
 * con la sua descrizione (nome, programma, icona, colore). QUALI SONO APERTE glielo dice
 * il compositore, che a un pannello racconta ogni finestra che compare o sparisce, con il
 * programma a cui appartiene.
 *
 * NON FA PARTIRE NIENTE: per ora mostra soltanto. Far partire le applicazioni sara' il
 * compito di un servizio a parte, il launcher, a cui la scrivania chiedera' "apri questa".
 *
 * Disegna con la libreria di disegno e lo stile del sistema, come il disegno Figma
 * "mshell - interfaccia" (Isola, Voce dell'isola, Pulsante applicazioni, Orologio). */
#include "finestra.h"
#include "stile.h"

/* --- le applicazioni installate --- */

#define MAX_APP 16

struct app {
    char nome[32];
    char programma[MAX_PERCORSO];   /* il percorso completo del programma */
    int icona;
    unsigned int colore;            /* 0: neutra (di vetro) */
};

static struct app app[MAX_APP];
static int quante_app;

static const struct { const char *nome; int icona; } nomi_icone[] = {
    { "save", ICONA_SAVE }, { "check", ICONA_CHECK }, { "minus", ICONA_MINUS },
    { "plus", ICONA_PLUS }, { "x", ICONA_X }, { "user", ICONA_USER },
    { "triangle-alert", ICONA_TRIANGLE_ALERT }, { "layout-grid", ICONA_LAYOUT_GRID },
    { "settings", ICONA_SETTINGS }, { "square", ICONA_SQUARE },
};
static const struct { const char *nome; unsigned int colore; } nomi_colori[] = {
    { "neutro", 0 }, { "blu", BLU_NORMALE }, { "verde", VERDE_NORMALE },
    { "rosso", ROSSO_NORMALE }, { "arancio", ARANCIO_NORMALE },
};

static int uguali(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static void copia(char *dove, const char *da, int massimo) {
    int i = 0;
    for (; i < massimo - 1 && da[i]; i++) dove[i] = da[i];
    dove[i] = 0;
}

static void aggiungi(char *dove, const char *da, int massimo) {
    int n = lunghezza(dove);
    copia(dove + n, da, massimo - n);
}

/* Una riga "chiave = valore" della descrizione: si tolgono gli spazi intorno. */
static void leggi_riga(struct app *a, const char *cartella, char *riga) {
    while (*riga == ' ' || *riga == '\t') riga++;
    if (!*riga || *riga == '#') return;           /* vuota, o un commento */
    char *uguale = riga;
    while (*uguale && *uguale != '=') uguale++;
    if (!*uguale) return;
    char *fine = uguale;
    *uguale = 0;
    while (fine > riga && (fine[-1] == ' ' || fine[-1] == '\t')) *--fine = 0;
    char *valore = uguale + 1;
    while (*valore == ' ' || *valore == '\t') valore++;
    for (int n = lunghezza(valore); n > 0 && (valore[n - 1] == ' ' || valore[n - 1] == '\r'); n--)
        valore[n - 1] = 0;

    if (uguali(riga, "nome")) copia(a->nome, valore, sizeof a->nome);
    else if (uguali(riga, "programma")) {
        copia(a->programma, "/Applicazioni/", MAX_PERCORSO);
        aggiungi(a->programma, cartella, MAX_PERCORSO);
        aggiungi(a->programma, "/", MAX_PERCORSO);
        aggiungi(a->programma, valore, MAX_PERCORSO);
    } else if (uguali(riga, "icona")) {
        for (unsigned int i = 0; i < sizeof nomi_icone / sizeof nomi_icone[0]; i++)
            if (uguali(valore, nomi_icone[i].nome)) a->icona = nomi_icone[i].icona;
    } else if (uguali(riga, "colore")) {
        for (unsigned int i = 0; i < sizeof nomi_colori / sizeof nomi_colori[0]; i++)
            if (uguali(valore, nomi_colori[i].nome)) a->colore = nomi_colori[i].colore;
    }
}

static void leggi_applicazioni(void) {
    int d = apri("/Applicazioni", APRI_LEGGERE);
    struct voce_cartella v;
    while (d >= 0 && quante_app < MAX_APP && leggi(d, &v, sizeof v) == sizeof v) {
        if (v.tipo != TIPO_CARTELLA) continue;
        char percorso[MAX_PERCORSO] = "/Applicazioni/";
        aggiungi(percorso, v.nome, MAX_PERCORSO);
        aggiungi(percorso, "/descrizione", MAX_PERCORSO);
        char testo[512];
        int f = apri(percorso, APRI_LEGGERE);
        int n = f >= 0 ? leggi(f, testo, sizeof testo - 1) : -1;
        if (f >= 0) chiudi(f);
        if (n <= 0) continue;                     /* senza descrizione non e' un'applicazione */
        testo[n] = 0;

        struct app *a = &app[quante_app];
        *a = (struct app){ .icona = ICONA_SQUARE };
        copia(a->nome, v.nome, sizeof a->nome);
        for (char *riga = testo; *riga;) {        /* riga per riga */
            char *dopo = riga;
            while (*dopo && *dopo != '\n') dopo++;
            int ultima = !*dopo;
            *dopo = 0;
            leggi_riga(a, v.nome, riga);
            if (ultima) break;
            riga = dopo + 1;
        }
        if (a->programma[0]) quante_app++;
    }
    if (d >= 0) chiudi(d);
}

/* --- le finestre aperte: le racconta il compositore --- */

#define MAX_APERTE 32

static struct { unsigned int id; char programma[MAX_PERCORSO]; } aperte[MAX_APERTE];
static int quante_aperte;

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
static int isola_l, spazio_ora;

static int calcola_isola(void) {
    spazio_ora = larghezza_ora();
    int voci = quante_app ? quante_app * VOCE_LATO + (quante_app - 1) * VOCI_SPAZIO + ISOLA_SPAZIO : 0;
    return 2 * ISOLA_MARGINE + VOCE_LATO + ISOLA_SPAZIO + voci + 1 + ISOLA_SPAZIO + spazio_ora;
}

/* Un tondo come "Icona app" del disegno: colorato (con ombra e luce) o di vetro. */
static void voce(struct pennello *p, int x, int y, unsigned int colore, int icona_voce) {
    struct tondo t = tondo(x, y, VOCE_LATO, VOCE_LATO, VOCE_LATO / 2);
    if (colore) {
        pennello_colore(p, VOCE_OMBRA);
        ombra_forma(p, &t, 0, 2, 3);
        pennello_colore(p, colore);
        riempi_forma(p, &t);
        pennello_colore(p, PULSANTE_BORDO_COLORATO);
        contorna_forma(p, &t, 256);
        pennello_colore(p, VOCE_LUCE);
        luce_sopra(p, &t);
    } else {
        pennello_colore(p, VETRO_SOPRA);
        riempi_forma(p, &t);
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
    voce(&p, x, y, BLU_NORMALE, ICONA_LAYOUT_GRID);          /* il pulsante del menu */
    x += VOCE_LATO + ISOLA_SPAZIO;

    for (int i = 0; i < quante_app; i++) {                   /* le applicazioni */
        voce(&p, x, y, app[i].colore, app[i].icona);
        if (e_aperta(&app[i])) {                              /* il puntino: e' aperta */
            pennello_colore(&p, CONTENUTO_PRINCIPALE);
            riempi_tondo(&p, x + VOCE_LATO / 2 - 2, y + VOCE_LATO + 2, 4, 4, 2);
        }
        x += VOCE_LATO + (i + 1 < quante_app ? VOCI_SPAZIO : ISOLA_SPAZIO);
    }

    pennello_colore(&p, VETRO_BORDO);                        /* il separatore */
    riempi_rettangolo(&p, x, (ISOLA_ALTEZZA - 22) / 2, 1, 22);
    x += 1 + ISOLA_SPAZIO;

    struct data_ora d;
    char ora[32];
    data_ora(&d);
    scrivi_ora(ora, &d);
    pennello_colore(&p, CONTENUTO_PRINCIPALE);
    testo(&p, FONT_BARRA, x + (spazio_ora - larghezza_testo(FONT_BARRA, ora)) / 2,
          (ISOLA_ALTEZZA - FONT_BARRA->altezza) / 2, ora);
    return finestra_mostra(&isola) >= 0;
}

/* --- lo sfondo --- */

static struct finestra sfondo;

/* Lo sfondo del disegno, ingrandito da quello piccolo (320x200) di grafica_dati.h: ogni
 * pixel nuovo e' una media pesata dei quattro vicini di quello piccolo. E' una sfumatura
 * morbida: ingrandendola non si perde niente. */
static int disegna_sfondo(int l, int a) {
    unsigned int *pixel = finestra_tela(&sfondo);
    if (!pixel) return 0;
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
    return finestra_mostra(&sfondo) >= 0;
}

/* --- il ciclo --- */

int main(void) {
    int schermo_l, schermo_a;
    if (finestra_schermo(&schermo_l, &schermo_a) < 0) {
        scrivi_testo("scrivania: non trovo il compositore\n");
        return 1;
    }
    leggi_applicazioni();

    if (finestra_apri_come(&sfondo, schermo_l, schermo_a, "sfondo", RUOLO_SFONDO, 0, 0, 0) < 0 ||
        !disegna_sfondo(schermo_l, schermo_a))
        return 1;
    isola_l = calcola_isola();
    if (finestra_apri_come(&isola, isola_l, ISOLA_ALTEZZA, "isola", RUOLO_PANNELLO,
                           (schermo_l - isola_l) / 2, ISOLA_DALL_ALTO, ISOLA_ALTEZZA / 2) < 0 ||
        !disegna_isola())
        return 1;

    /* Si aspettano le notizie del compositore (le finestre che compaiono e spariscono, e i
     * suoi "ci sei?", a cui risponde la libreria) e l'inizio del minuto dopo, per l'ora. */
    for (;;) {
        struct data_ora d;
        data_ora(&d);
        struct da_guardare g[2] = { { sfondo.collegamento, 0 }, { isola.collegamento, 0 } };
        int pronti = aspetta(g, 2, (60 - (int)d.secondi) * 1000);
        if (pronti < 0) return 1;
        int ridisegna = pronti == 0;                 /* tempo scaduto: e' cambiato il minuto */
        struct notizia n;
        if (g[0].pronto && !finestra_ricevi(&sfondo, &n)) return 1;   /* il compositore ha chiuso */
        if (g[1].pronto) {
            if (!finestra_ricevi(&isola, &n)) return 1;
            ridisegna |= notizia(&n);
        }
        if (ridisegna) {
            if (!disegna_isola()) return 1;
            /* mentre aspettava una tela libera, la libreria ha messo da parte le notizie */
            for (int i = 0; i < isola.quante; i++) notizia(&isola.messe_da_parte[i]);
            if (isola.quante) { isola.quante = 0; if (!disegna_isola()) return 1; }
        }
    }
}
