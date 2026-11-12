/* Files: il gestore dei file, come il Finder del Mac o Nautilus di GNOME. Mostra il
 * contenuto di una cartella e permette di girare per il disco, come nel disegno Figma
 * ("File e cartelle"):
 *
 *   +--------------------------------------------------------------------+
 *   |                         Home                              (-) (x)  | barra del titolo
 *   | [<][>]  ^ home                              [::][=]  [ Cerca in.. ]| barra degli strumenti
 *   +--------------+-----------------------------------------------------+
 *   | Scorciatoie  |  [cartella] [cartella] [file] ...                   |
 *   |  Home        |                                                     |
 *   |  Scrivania   |   il contenuto: a icone o a elenco                  |
 *   |  Apps        |                                                     |
 *   | Filtri       |                                                     |
 *   |  Immagini    |                                                     |
 *   |  Testi       |                                                     |
 *   |  Programmi   |                                                     |
 *   +--------------+-----------------------------------------------------+
 *
 *   - LA BARRA LATERALE: le scorciatoie (la casa, la sua Scrivania, le applicazioni del
 *     sistema) e i filtri, che mostrano della cartella solo le immagini, i testi o i
 *     programmi (riconosciuti dall'estensione del nome; i programmi dal permesso di
 *     esecuzione). Un filtro acceso si spegne con un altro clic.
 *   - LA BARRA DEGLI STRUMENTI: indietro e avanti (la storia delle cartelle viste), il
 *     percorso (un clic su un nome torna a quella cartella), icone o elenco, la ricerca
 *     (mostra solo i file il cui nome contiene quello che si scrive).
 *   - IL CONTENUTO: un clic sceglie un file, il doppio clic su una cartella la apre (gli
 *     altri file per ora non si aprono, tranne i pacchetti .app, che si chiedono al
 *     launcher: lui fa partire lo Store, o dice che le app vanno installate prima).
 *     Nella vista a elenco le colonne Nome, Tipo, Dimensione, Modificato. Si scorre con la
 *     rotella o con la barra di scorrimento del toolkit.
 *   - IL TASTO DESTRO apre un menu dove si e' cliccato: su un file Apri, Rinomina,
 *     Duplica, Informazioni, Elimina; sul vuoto Nuova cartella, Ordina per, Mostra come
 *     icone o elenco. Rinominare (anche la cartella appena creata) si fa li', nel nome.
 *   - GLI STATI: una cartella vuota, una che non si puo' leggere (un errore del disco, un
 *     percorso troppo lungo) e una che non si ha il permesso di aprire si dicono al
 *     centro, con un'icona e una spiegazione.
 *
 * I file li legge e li cambia la libreria (apri, leggi, info, crea_cartella, rinomina,
 * cancella), cioe' Linux: Files gira come la persona (uid 1000) e puo' quello che puo'
 * lei. Nella partizione data (la sua casa) scrive; /system e' in sola lettura, e le
 * cartelle del sistema non le puo' aprire. Quando qualcosa non riesce lo dice in basso,
 * in un avviso che sparisce alla prossima azione.
 *
 * LA CARTELLA DI PARTENZA e' quella che il launcher passa come argomento (vedi
 * launcher.h: aprire una cartella, anche dalla scrivania, vuol dire far partire Files su
 * di lei); senza, la casa (/home).
 *
 * LA TASTIERA. Per ora arrivano alle applicazioni solo i caratteri, Invio e "cancella"
 * (eventi.c non manda Ctrl, F2, Canc ed Esc): Invio apre la cartella scelta e conferma
 * il nome nuovo. Le altre scorciatoie scritte nei menu (F2, Ctrl+D...) non arrivano. */
#include "toolkit.h"
#include "elemento_file.h"
#include "launcher.h"     /* aprire un pacchetto .app: lo chiede al launcher */

#define MAX_ELEMENTI 256
#define MAX_LUNGO    256     /* un percorso in Files: anche piu' lungo di quelli che si aprono */
#define MAX_STORIA    32
#define CASA         "/home"

/* --- i testi e i numeri --- */

static char *metti_testo(char *d, const char *s) {
    while (*s) *d++ = *s++;
    *d = 0;
    return d;
}

static char *metti_numero(char *d, unsigned long long n) {
    char cifre[24];
    int k = 0;
    do { cifre[k++] = (char)('0' + n % 10); n /= 10; } while (n);
    while (k) *d++ = cifre[--k];
    *d = 0;
    return d;
}

static char *metti_due_cifre(char *d, unsigned int n) {
    *d++ = (char)('0' + n / 10 % 10);
    *d++ = (char)('0' + n % 10);
    *d = 0;
    return d;
}

static int uguali(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static char minuscola(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 'a' - 'A') : c; }

/* Confronto senza badare a maiuscole e minuscole, come il Finder. */
static int confronta_nomi(const char *a, const char *b) {
    for (;; a++, b++) {
        char x = minuscola(*a), y = minuscola(*b);
        if (x != y || !x) return (unsigned char)x - (unsigned char)y;
    }
}

/* 'ago' sta dentro 'pagliaio' (senza badare a maiuscole e minuscole)? */
static int contiene(const char *pagliaio, const char *ago) {
    if (!*ago) return 1;
    for (; *pagliaio; pagliaio++) {
        int i = 0;
        while (ago[i] && minuscola(pagliaio[i]) == minuscola(ago[i])) i++;
        if (!ago[i]) return 1;
    }
    return 0;
}

/* --- i file di una cartella --- */

struct elemento {
    char nome[MAX_NOME_FILE];
    unsigned int tipo;               /* TIPO_... (di un collegamento: quello a cui porta) */
    int genere;                      /* FILE_CARTELLA, FILE_TESTO...: l'icona e il "Tipo" */
    unsigned int permessi;
    unsigned long long dimensione;
    long long modificato;
    int contenuti;                   /* per le cartelle: quante voci hanno (-1: non si sa) */
};

static struct elemento elementi[MAX_ELEMENTI];
static int quanti_elementi;
static int visibili[MAX_ELEMENTI];   /* quelli che si vedono (filtro, ricerca), in ordine */
static int quanti_visibili;

static char qui[MAX_LUNGO] = CASA;   /* la cartella che si guarda */
static int errore_qui;               /* 0, o l'ERRORE_... per cui non la si puo' leggere */
static unsigned int proprietario_qui;

enum { PER_NOME, PER_TIPO, PER_DIMENSIONE, PER_DATA };
static int ordine = PER_NOME;
enum { A_ICONE, A_ELENCO };
static int vista = A_ICONE;
static int filtro = -1;              /* FILE_IMMAGINE, FILE_TESTO, FILE_PROGRAMMA, o -1 */

static const char *const nomi_generi[TIPI_FILE] = {
    "cartella", "cartella", "file", "immagine", "testo", "programma"
};

static const char *const estensioni_immagini[] = { "png", "jpg", "jpeg", "gif", "bmp", "webp", 0 };
static const char *const estensioni_testi[] = {
    "txt", "md", "c", "h", "py", "sh", "json", "csv", "log", "ini", "conf", "xml", "html",
    "css", "js", 0
};

static int ha_estensione(const char *nome, const char *const *elenco) {
    const char *punto = 0;
    for (const char *c = nome; *c; c++) if (*c == '.') punto = c;
    if (!punto || punto == nome) return 0;
    for (int i = 0; elenco[i]; i++)
        if (!confronta_nomi(punto + 1, elenco[i])) return 1;
    return 0;
}

/* Che file e': una cartella, un programma (un file che si puo' eseguire), un'immagine o un
 * testo (dall'estensione del nome), o un file qualunque. */
static int genere_di(const struct elemento *e) {
    if (e->tipo == TIPO_CARTELLA) return FILE_CARTELLA;
    if (e->tipo == TIPO_FILE && (e->permessi & 0111)) return FILE_PROGRAMMA;
    if (ha_estensione(e->nome, estensioni_immagini)) return FILE_IMMAGINE;
    if (ha_estensione(e->nome, estensioni_testi)) return FILE_TESTO;
    return FILE_FILE;
}

/* Il percorso di un file della cartella in cui si e', in 'dove' (MAX_LUNGO). */
static void percorso_di(char *dove, const char *cartella, const char *nome) {
    char *d = metti_testo(dove, cartella);
    if (d - dove + 1 + lunghezza(nome) >= MAX_LUNGO) { *dove = 0; return; }
    if (d[-1] != '/') *d++ = '/';
    metti_testo(d, nome);
}

/* Quante voci ha una cartella (-1 se non la si puo' aprire). */
static int conta_voci(const char *percorso) {
    int d = apri(percorso, APRI_LEGGERE);
    if (d < 0) return -1;
    int n = 0;
    struct voce_cartella v;
    while (leggi(d, &v, sizeof v) == sizeof v) n++;
    chiudi(d);
    return n;
}

/* Rilegge la cartella 'qui' dal disco. */
static void leggi_cartella(void) {
    quanti_elementi = 0;
    struct info_file i;
    errore_qui = info(qui, &i);
    proprietario_qui = errore_qui ? 0 : i.proprietario;
    if (!errore_qui && i.tipo != TIPO_CARTELLA) errore_qui = ERRORE_NON_CARTELLA;
    int d = errore_qui ? -1 : apri(qui, APRI_LEGGERE);
    if (d < 0) {
        if (!errore_qui) errore_qui = d;
        return;
    }
    struct voce_cartella v;
    while (quanti_elementi < MAX_ELEMENTI && leggi(d, &v, sizeof v) == sizeof v) {
        struct elemento *e = &elementi[quanti_elementi++];
        v.nome[MAX_NOME_FILE - 1] = 0;
        copia_testo(e->nome, v.nome, MAX_NOME_FILE);
        e->tipo = v.tipo;
        e->permessi = v.permessi;
        e->dimensione = v.dimensione;
        e->modificato = v.modificato;
        e->contenuti = -1;
        char percorso[MAX_LUNGO];
        percorso_di(percorso, qui, e->nome);
        if (e->tipo == TIPO_COLLEGAMENTO && percorso[0] && info(percorso, &i) == 0) {
            e->tipo = i.tipo;                /* un collegamento vale per quello a cui porta */
            e->permessi = i.permessi;
            e->dimensione = i.dimensione;
        }
        if (e->tipo == TIPO_CARTELLA && percorso[0]) e->contenuti = conta_voci(percorso);
        e->genere = genere_di(e);
    }
    chiudi(d);
}

/* L'ordine: prima le cartelle, poi gli altri; dentro, per nome, per tipo (e poi nome),
 * per dimensione o per data (dal piu' grande e dal piu' recente, come il Finder). */
static int prima_di(const struct elemento *a, const struct elemento *b) {
    int ca = a->genere == FILE_CARTELLA, cb = b->genere == FILE_CARTELLA;
    if (ca != cb) return ca;
    if (ordine == PER_TIPO) {
        int t = confronta_nomi(nomi_generi[a->genere], nomi_generi[b->genere]);
        if (t) return t < 0;
    } else if (ordine == PER_DIMENSIONE) {
        unsigned long long da = ca ? (unsigned long long)(a->contenuti + 1) : a->dimensione;
        unsigned long long db = ca ? (unsigned long long)(b->contenuti + 1) : b->dimensione;
        if (da != db) return da > db;
    } else if (ordine == PER_DATA) {
        if (a->modificato != b->modificato) return a->modificato > b->modificato;
    }
    return confronta_nomi(a->nome, b->nome) < 0;
}

static struct campo ricerca;

/* Chi si vede, e in che ordine: il filtro, la ricerca, poi l'ordine (per inserimento:
 * i file di una cartella sono pochi). */
static void scegli_visibili(void) {
    quanti_visibili = 0;
    for (int i = 0; i < quanti_elementi; i++) {
        const struct elemento *e = &elementi[i];
        if (filtro >= 0 && e->genere != filtro) continue;
        if (!contiene(e->nome, ricerca.testo)) continue;
        int k = quanti_visibili++;
        while (k > 0 && prima_di(e, &elementi[visibili[k - 1]])) {
            visibili[k] = visibili[k - 1];
            k--;
        }
        visibili[k] = i;
    }
}

/* --- le date e le dimensioni, come le scrive il disegno --- */

static const char *const mesi[] = { "gen", "feb", "mar", "apr", "mag", "giu",
                                    "lug", "ago", "set", "ott", "nov", "dic" };

/* I giorni dal 1/1/1970 di una data (l'algoritmo "days from civil" di Howard Hinnant:
 * l'anno comincia a marzo, cosi' il giorno in piu' dei bisestili cade in fondo). */
static long long giorni_da_data(long long a, unsigned int m, unsigned int g) {
    a -= m <= 2;
    long long era = (a >= 0 ? a : a - 399) / 400;
    unsigned int anno_era = (unsigned int)(a - era * 400);
    unsigned int giorno_anno = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + g - 1;
    unsigned int giorno_era = anno_era * 365 + anno_era / 4 - anno_era / 100 + giorno_anno;
    return era * 146097 + (long long)giorno_era - 719468;
}

/* Il contrario: dai giorni dal 1/1/1970 alla data. */
static void data_da_giorni(long long z, long long *a, unsigned int *m, unsigned int *g) {
    z += 719468;
    long long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned int giorno_era = (unsigned int)(z - era * 146097);
    unsigned int anno_era = (giorno_era - giorno_era / 1460 + giorno_era / 36524 - giorno_era / 146096) / 365;
    unsigned int giorno_anno = giorno_era - (365 * anno_era + anno_era / 4 - anno_era / 100);
    unsigned int mp = (5 * giorno_anno + 2) / 153;
    *g = giorno_anno - (153 * mp + 2) / 5 + 1;
    *m = mp < 10 ? mp + 3 : mp - 9;
    *a = (long long)anno_era + era * 400 + (*m <= 2);
}

/* "oggi, 10:24", "ieri, 18:02", "3 nov, 09:15", "3 nov 2025". L'orologio del PC segna
 * l'ora di casa e Linux la prende per quella di Greenwich (libreria.c, data_ora): i
 * secondi dei file vanno letti allo stesso modo, senza fusi. */
static void scrivi_data(char *d, long long secondi) {
    struct data_ora ora;
    data_ora(&ora);
    long long oggi = giorni_da_data(ora.anno, ora.mese, ora.giorno);
    long long giorno = secondi >= 0 ? secondi / 86400 : (secondi - 86399) / 86400;
    unsigned int nel_giorno = (unsigned int)(secondi - giorno * 86400);
    long long a;
    unsigned int m, g;
    data_da_giorni(giorno, &a, &m, &g);
    if (giorno == oggi || giorno == oggi - 1) {
        d = metti_testo(d, giorno == oggi ? "oggi, " : "ieri, ");
    } else {
        d = metti_numero(d, g);
        d = metti_testo(d, " ");
        d = metti_testo(d, mesi[(m + 11) % 12]);
        if (a != (long long)ora.anno) {
            d = metti_testo(d, " ");
            metti_numero(d, (unsigned long long)(a > 0 ? a : 0));
            return;
        }
        d = metti_testo(d, ", ");
    }
    d = metti_due_cifre(d, nel_giorno / 3600);
    d = metti_testo(d, ":");
    metti_due_cifre(d, nel_giorno / 60 % 60);
}

/* "512 byte", "2 KB", "3,2 MB" */
static void scrivi_byte(char *d, unsigned long long byte) {
    if (byte < 1024) { metti_testo(metti_numero(d, byte), " byte"); return; }
    if (byte < 1024 * 1024) { metti_testo(metti_numero(d, (byte + 1023) / 1024), " KB"); return; }
    const char *unita = " MB";
    unsigned long long decimi = byte * 10 / (1024 * 1024);
    if (decimi >= 10240) { decimi = byte * 10 / (1024ULL * 1024 * 1024); unita = " GB"; }
    d = metti_numero(d, decimi / 10);
    *d++ = ',';
    d = metti_numero(d, decimi % 10);
    metti_testo(d, unita);
}

/* Nella colonna Dimensione: per una cartella quante cose ha dentro. */
static void scrivi_dimensione(char *d, const struct elemento *e) {
    if (e->genere != FILE_CARTELLA) { scrivi_byte(d, e->dimensione); return; }
    if (e->contenuti < 0) metti_testo(d, "--");
    else if (e->contenuti == 0) metti_testo(d, "vuota");
    else metti_testo(metti_numero(d, (unsigned long long)e->contenuti),
                     e->contenuti == 1 ? " elemento" : " elementi");
}

/* "rwxr-xr-x" */
static void scrivi_permessi(char *d, unsigned int permessi) {
    const char *lettere = "rwx";
    for (int i = 8; i >= 0; i--) *d++ = permessi & (1u << i) ? lettere[(8 - i) % 3] : '-';
    *d = 0;
}

/* --- la finestra e i suoi pezzi --- */

static struct finestra_tk finestra;
static struct barra_strumenti strumenti;
static struct pillola frecce, viste;
static struct pulsante_barra indietro, avanti, a_icone, a_elenco;
static struct percorso percorso;
static struct contenitore corpo;

static void vai(const char *dove, int nella_storia);
static void aggiorna_contenuto(void);
static void chiudi_menu(void);

/* LA STORIA: le cartelle viste, per "indietro" e "avanti", come in un browser. */
static char storia[MAX_STORIA][MAX_LUNGO];
static int quante_storia, posto_storia = -1;

static void aggiorna_frecce(void) {
    widget_disabilita(&indietro.base, posto_storia <= 0);
    widget_disabilita(&avanti.base, posto_storia >= quante_storia - 1);
}

/* L'AVVISO: quando qualcosa non riesce (una cartella non vuota da eliminare, un disco in
 * sola lettura), una riga in basso nel contenuto, fino alla prossima azione. */
static char avviso[96];

static void avvisa(const char *testo) {
    copia_testo(avviso, testo, sizeof avviso);
}

/* Cosa dire per un errore della libreria, quando si cambia qualcosa. */
static void avvisa_errore(const char *cosa, int errore) {
    char t[96], *d = metti_testo(t, cosa);
    switch (errore) {
    case ERRORE_SOLA_LETTURA: metti_testo(d, ": qui il disco \x8A in sola lettura"); break;
    case ERRORE_PERMESSO:     metti_testo(d, ": non hai il permesso di farlo qui"); break;
    case ERRORE_ESISTE:       metti_testo(d, ": c'\x8A gi\x85 un file con questo nome"); break;
    case ERRORE_NON_VUOTA:    metti_testo(d, ": la cartella non \x8A vuota"); break;
    case ERRORE_NOME:         metti_testo(d, ": il nome non va bene o \x8A troppo lungo"); break;
    case ERRORE_PIENO:        metti_testo(d, ": il disco \x8A pieno"); break;
    case ERRORE_NON_TROVATO:  metti_testo(d, ": non c'\x8A pi\x97"); break;
    default:                  metti_testo(d, ": il disco non ha risposto"); break;
    }
    avvisa(t);
}

/* --- la barra laterale --- */

/* Le misure vengono dal disegno Figma di Files ("Disegno - Files"), ridotte ai token: una
 * colonna larga 200 con 8 di margine ai lati e 12 in cima; i titoli delle sezioni (8 sopra,
 * il testo "etichetta" alto 16, 4 sotto) e le voci S da 32, a 4 l'una dall'altra (due
 * fondi arrotondati non si toccano mai). Dentro la voce: 12, il simbolo da 16, 8, il nome. */
#define LATERALE_LARGHEZZA 200
#define LATERALE_MARGINE   SPAZIO_8    /* ai lati delle voci */
#define LATERALE_CIMA      SPAZIO_12   /* sopra la prima */
#define LATERALE_TRA       SPAZIO_4    /* tra una voce e l'altra */
#define LATERALE_TITOLO    (SPAZIO_8 + INTERLINEA_ETICHETTA + SPAZIO_4)   /* 28 */
#define LATERALE_RIGA      ALTEZZA_S
#define LATERALE_DENTRO    PADDING_S   /* nella voce, prima del simbolo */
#define LATERALE_GAP       GAP_M       /* tra simbolo e nome */
#define LATERALE_ICONA     ICONA_S

enum { TITOLO, SCORCIATOIA, FILTRO };

static const struct riga_laterale {
    int tipo;
    const char *testo;
    int icona;
    const char *percorso;            /* una scorciatoia: la cartella */
    int genere;                      /* un filtro: i file che lascia vedere */
} righe_laterali[] = {
    { TITOLO,      "Scorciatoie", 0,             0,                  0 },
    { SCORCIATOIA, "Home",        ICONA_HOUSE,   CASA,               0 },
    { SCORCIATOIA, "Scrivania",   ICONA_FOLDER,  CASA "/Scrivania",  0 },
    { SCORCIATOIA, "Apps",        ICONA_FOLDER,  "/system/apps",     0 },
    { TITOLO,      "Filtri",      0,             0,                  0 },
    { FILTRO,      "Immagini",    ICONA_IMAGE,   0,                  FILE_IMMAGINE },
    { FILTRO,      "Testi",       ICONA_FILE,    0,                  FILE_TESTO },
    { FILTRO,      "Programmi",   ICONA_TERMINAL, 0,                 FILE_PROGRAMMA },
};
#define RIGHE_LATERALI ((int)(sizeof righe_laterali / sizeof righe_laterali[0]))

struct laterale {
    struct widget base;
    int sopra;                       /* la riga sotto il puntatore, o -1 */
    int premuta;                     /* quella su cui si e' premuto, o -1 */
};

static struct laterale laterale;

/* Dove comincia la riga i, e quanto e' alta. */
static int riga_laterale_y(const struct widget *w, int i, int *alta) {
    int y = w->y + LATERALE_CIMA;
    for (int k = 0; k < i; k++)
        y += (righe_laterali[k].tipo == TITOLO ? LATERALE_TITOLO : LATERALE_RIGA) + LATERALE_TRA;
    *alta = righe_laterali[i].tipo == TITOLO ? LATERALE_TITOLO : LATERALE_RIGA;
    return y;
}

/* La riga (non un titolo) nel punto (x, y), o -1. */
static int riga_laterale_in(const struct widget *w, int x, int y) {
    if (x < w->x + LATERALE_MARGINE || x >= w->x + w->l - LATERALE_MARGINE) return -1;
    for (int i = 0; i < RIGHE_LATERALI; i++) {
        int alta, ry = riga_laterale_y(w, i, &alta);
        if (righe_laterali[i].tipo != TITOLO && y >= ry && y < ry + alta) return i;
    }
    return -1;
}

static int riga_accesa(int i) {
    const struct riga_laterale *r = &righe_laterali[i];
    if (r->tipo == SCORCIATOIA) return uguali(qui, r->percorso);
    if (r->tipo == FILTRO) return filtro == r->genere;
    return 0;
}

static void laterale_misura(struct widget *w, int *l, int *a) {
    (void)w;
    *l = LATERALE_LARGHEZZA;
    *a = 0;
}

static void laterale_disegna(struct widget *w, struct pennello *p) {
    struct laterale *lt = (struct laterale *)w;
    /* niente fondo proprio (sotto c'e' il vetro della finestra, come per il contenuto): la
     * divide dal contenuto una linea di un pixel, come il titolo dal corpo */
    pennello_colore(p, BORDO_SEPARATORE);
    riempi_rettangolo(p, w->x + w->l - 1, w->y, 1, w->a);
    for (int i = 0; i < RIGHE_LATERALI; i++) {
        const struct riga_laterale *r = &righe_laterali[i];
        int alta, y = riga_laterale_y(w, i, &alta);
        int x = w->x + LATERALE_MARGINE, largo = w->l - 2 * LATERALE_MARGINE;
        if (r->tipo == TITOLO) {
            pennello_colore(p, TESTO_SECONDARIO);
            testo(p, FONT_ETICHETTA, x + LATERALE_DENTRO,
                  y + SPAZIO_8 + (INTERLINEA_ETICHETTA - FONT_ETICHETTA->altezza) / 2, r->testo);
            continue;
        }
        /* accesa (la cartella in cui si e', il filtro acceso) come una riga scelta, sotto il
         * puntatore come una riga "sopra": piatte, con il fondo dei controlli */
        if (riga_accesa(i) || lt->sopra == i) {
            pennello_colore(p, riga_accesa(i) ? CONTROLLO_SFONDO_SOPRA : CONTROLLO_SFONDO_NUOVO);
            riempi_tondo(p, x, y, largo, alta, RAGGIO_CONTROLLO_NUOVO);
        }
        pennello_colore(p, TESTO_PRIMARIO);
        icona(p, r->icona, LATERALE_ICONA, x + LATERALE_DENTRO, y + (alta - LATERALE_ICONA) / 2);
        testo_centrato(p, FONT_CORPO, x + LATERALE_DENTRO + LATERALE_ICONA + LATERALE_GAP, y, alta, r->testo);
    }
}

/* Come un pulsante: scatta quando si lascia, sulla stessa riga su cui si e' premuto. */
static void laterale_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    struct laterale *lt = (struct laterale *)w;
    int riga = riga_laterale_in(w, x, y);
    if (pulsanti & PULSANTE_SINISTRO) {
        if (lt->premuta < 0) lt->premuta = riga;
        return;
    }
    int premuta = lt->premuta;
    lt->premuta = -1;
    if (pulsanti || premuta < 0 || riga != premuta) return;
    const struct riga_laterale *r = &righe_laterali[riga];
    avvisa("");
    if (r->tipo == SCORCIATOIA) {
        vai(r->percorso, 1);
    } else {
        filtro = filtro == r->genere ? -1 : r->genere;   /* un altro clic lo spegne */
        aggiorna_contenuto();
    }
    widget_cambiato(w);
}

static void laterale_movimento(struct widget *w, int x, int y) {
    struct laterale *lt = (struct laterale *)w;
    int riga = riga_laterale_in(w, x, y);
    if (riga != lt->sopra) { lt->sopra = riga; widget_cambiato(w); }
}

static void laterale_sopra(struct widget *w) {
    struct laterale *lt = (struct laterale *)w;
    if (!w->sopra && lt->sopra >= 0) { lt->sopra = -1; widget_cambiato(w); }
}

static const struct tipo_widget tipo_laterale = {
    laterale_misura, 0, laterale_disegna, laterale_puntatore, 0, 0, laterale_sopra, laterale_movimento, 0
};

/* --- il contenuto: i file, a icone o a elenco --- */

/* LA VISTA A ICONE: la "Griglia" del file Figma (Componenti complessi). Ogni file e' un
 * TILE largo 96 e alto 116 (fisso: due righe di nome), senza fondo proprio:
 *
 *     +----------------+   4 sopra
 *     |   +--------+   |   il riquadro dell'icona, 64x64, raggio 12: si colora sotto il
 *     |   | icona  |   |   puntatore (CONTROLLO_SFONDO) e se scelto (CONTROLLO_SFONDO_SOPRA)
 *     |   +--------+   |   4
 *     |  nome del file |   il nome, "corpo" (14/20), centrato, al massimo su due righe; se
 *     |  lungo...      |   scelto ha un fondo suo (AZIONE_SFONDO, raggio 6, testo AZIONE_TESTO)
 *     +----------------+
 *
 * Tra un tile e l'altro 8; intorno alla griglia 16. L'icona del file resta l'immagine di
 * grafica_dati.h (icone_file), centrata nel riquadro. */
#define GRIGLIA_MARGINE   SPAZIO_16
#define GRIGLIA_TRA       SPAZIO_8
#define TILE_ALTEZZA      116
#define TILE_CIMA         SPAZIO_4    /* sopra il riquadro */
#define TILE_RIQUADRO     64
#define TILE_SPAZIO       SPAZIO_4    /* tra riquadro e nome */
#define TILE_RIGHE        2
#define TILE_TESTO_DENTRO SPAZIO_4    /* il fondo del nome scelto: a sinistra e a destra del testo */
#define TILE_NOME_LARGO   (TILE_LARGHEZZA - 2 * TILE_TESTO_DENTRO)

/* LA VISTA A ELENCO: la "Tabella" del file Figma. L'intestazione di colonna (alta 32, testo
 * "etichetta" in TESTO_SECONDARIO), una linea, poi le righe da 40, a 4 l'una dall'altra, con
 * 8 di margine dentro e 12 in ogni cella. Colonne: Nome (il resto), Tipo 120, Dimensione 100
 * (allineata a destra), Modificato 140. Righe piatte: solo quella sotto il puntatore
 * (CONTROLLO_SFONDO) e quella scelta (CONTROLLO_SFONDO_SOPRA) hanno un fondo, raggio 12. */
#define ELENCO_MARGINE     SPAZIO_8    /* intorno alla tabella */
#define ELENCO_TESTATA     ALTEZZA_S
#define ELENCO_LINEA       1
#define ELENCO_SOPRA_RIGHE SPAZIO_4    /* tra la linea e la prima riga */
#define RIGA_ALTA          ALTEZZA_M
#define RIGA_TRA           SPAZIO_4
#define RIGA_DENTRO        SPAZIO_8    /* a sinistra e a destra della riga */
#define CELLA_DENTRO       SPAZIO_12   /* a sinistra e a destra di ogni cella */
#define NOME_GAP           GAP_M       /* tra l'icona piccola e il nome */
#define COLONNA_TIPO       120
#define COLONNA_DIMENSIONE 100
#define COLONNA_MODIFICATO 140

/* Lo stato della cartella (vuota, errore...) al centro: icona 48, 12, il titolo (corpo
 * medio), 12, la spiegazione (piccolo), su una colonna larga 280. */
#define STATO_LARGHEZZA   280
#define STATO_ICONA       ICONA_OGGETTO
#define STATO_TRA         SPAZIO_12

/* Il campo del nome nuovo (rinomina): come un campo di testo del toolkit, incassato e con il
 * fuoco. Nell'elenco e' un campo S (alto 32, raggio 10); nel tile prende il posto del nome
 * (alto 24, raggio 6 come il fondo del nome scelto). */
#define NOME_CAMPO_DENTRO SPAZIO_8     /* nel campo, a sinistra e a destra del testo */
#define NOME_CAMPO_TILE   (INTERLINEA_CORPO + SPAZIO_4)   /* 24 */
#define CURSORE_SPAZIO    1            /* tra l'ultima lettera e il cursore */
#define CURSORE_LARGO     2

struct contenuto {
    struct widget base;
    struct barra_scorrimento barra;  /* sua figlia: sa anche dove si e' scorso */
    int scelto;                      /* l'elemento scelto (indice in elementi), o -1 */
    int sopra;                       /* quello sotto il puntatore (indice in elementi), o -1 */
    int rinomina;                    /* quello di cui si sta cambiando il nome, o -1 */
    char nome_nuovo[MAX_NOME_FILE];
    /* la vista a icone: dove sta ogni tile visibile (nel contenuto, prima di scorrere:
     * rispetto all'angolo del widget) */
    int ex[MAX_ELEMENTI], ey[MAX_ELEMENTI];
    unsigned int clic_prima;         /* tempo() della pressione di prima, per il doppio clic */
    int clic_su;                     /* e su quale elemento */
};

static struct contenuto contenuto;

/* Dove cominciano le righe (sotto l'intestazione e la linea), sullo schermo. */
static int righe_y(const struct widget *w) {
    return w->y + ELENCO_MARGINE + ELENCO_TESTATA + ELENCO_LINEA + ELENCO_SOPRA_RIGHE;
}

/* La riga k dell'elenco: dove comincia, nelle righe (prima di scorrere). */
static int riga_elenco_y(int k) { return k * (RIGA_ALTA + RIGA_TRA); }

/* Rifa' il posto dei tile (vista a icone) e l'altezza di tutto, per lo scorrimento. */
static void contenuto_impagina(struct contenuto *c) {
    struct widget *w = &c->base;
    if (vista == A_ICONE) {
        int largo = w->l - 2 * GRIGLIA_MARGINE;
        int per_riga = (largo + GRIGLIA_TRA) / (TILE_LARGHEZZA + GRIGLIA_TRA);
        if (per_riga < 1) per_riga = 1;
        for (int k = 0; k < quanti_visibili; k++) {
            c->ex[k] = GRIGLIA_MARGINE + k % per_riga * (TILE_LARGHEZZA + GRIGLIA_TRA);
            c->ey[k] = GRIGLIA_MARGINE + k / per_riga * (TILE_ALTEZZA + GRIGLIA_TRA);
        }
        int righe = (quanti_visibili + per_riga - 1) / per_riga;
        int totale = 2 * GRIGLIA_MARGINE + (righe ? righe * (TILE_ALTEZZA + GRIGLIA_TRA) - GRIGLIA_TRA : 0);
        scorrimento_misure(&c->barra.scorrimento, totale, w->a);
    } else {
        int totale = (quanti_visibili ? riga_elenco_y(quanti_visibili) - RIGA_TRA : 0) + ELENCO_MARGINE;
        scorrimento_misure(&c->barra.scorrimento, totale, w->y + w->a - righe_y(w));
    }
}

/* L'elemento visibile nel punto (x, y) dello schermo: il suo posto in 'visibili', o -1. */
static int visibile_in(const struct contenuto *c, int x, int y) {
    const struct widget *w = &c->base;
    int scorso = c->barra.scorrimento.posizione;
    if (vista == A_ICONE) {
        for (int k = 0; k < quanti_visibili; k++) {
            int ex = w->x + c->ex[k], ey = w->y + c->ey[k] - scorso;
            if (x >= ex && x < ex + TILE_LARGHEZZA && y >= ey && y < ey + TILE_ALTEZZA) return k;
        }
        return -1;
    }
    if (y < righe_y(w) || x < w->x + ELENCO_MARGINE || x >= w->x + w->l - ELENCO_MARGINE) return -1;
    int dentro = y - righe_y(w) + scorso;
    int k = dentro / (RIGA_ALTA + RIGA_TRA);
    if (dentro - riga_elenco_y(k) >= RIGA_ALTA) return -1;
    return k < quanti_visibili ? k : -1;
}

/* Il posto in 'visibili' di un elemento, o -1 se non si vede. */
static int posto_visibile(int elemento) {
    for (int k = 0; k < quanti_visibili; k++) if (visibili[k] == elemento) return k;
    return -1;
}

static void contenuto_misura(struct widget *w, int *l, int *a) {
    (void)w;
    *l = 0;
    *a = 0;
}

static void contenuto_disponi(struct widget *w) {
    struct contenuto *c = (struct contenuto *)w;
    barra_scorrimento_posto(&c->barra, w->x, w->y, w->l, w->a);
    contenuto_impagina(c);
}

/* Il campo del nome nuovo (rinomina), largo 'l' e alto 'a', raggio 'raggio': incassato con il
 * fuoco come campo_disegna del toolkit (ombre "focus", SUPERFICIE_INCASSATA, bordo
 * BORDO_FOCUS), con dentro il testo e il cursore in fondo. Il testo centrato se 'centrato';
 * se e' piu' lungo del campo, si vede la fine (dove si scrive). */
static void campo_nome(struct pennello *p, int x, int y, int l, int a, int raggio,
                       const char *testo_nome, int centrato) {
    const struct font *f = FONT_CORPO;
    struct tondo t = tondo(x, y, l, a, raggio);
    ombre_strati_fuori(p, &t, OMBRA_TAB(focus), OMBRA_N(focus));
    pennello_colore(p, SUPERFICIE_INCASSATA);
    riempi_forma(p, &t);
    ombre_strati_dentro(p, &t, OMBRA_TAB(focus), OMBRA_N(focus));
    pennello_colore(p, BORDO_FOCUS);
    contorna_forma(p, &t, BORDO_FOCUS_SPESSORE);
    int dentro = a < ALTEZZA_S ? TILE_TESTO_DENTRO : NOME_CAMPO_DENTRO;
    int largo = larghezza_testo(f, testo_nome) + CURSORE_SPAZIO + CURSORE_LARGO;   /* il cursore compreso */
    int spazio = l - 2 * dentro;
    int tx = centrato && largo < spazio ? x + (l - largo) / 2 : x + dentro;
    if (largo > spazio) tx = x + l - dentro - largo;
    pennello_salva(p);
    pennello_ritaglia(p, x + 2, y + 1, l - 4, a - 2);
    pennello_colore(p, TESTO_PRIMARIO);
    int fine = testo_centrato(p, f, tx, y, a, testo_nome);
    riempi_rettangolo(p, fine + CURSORE_SPAZIO, y + (a - f->altezza) / 2, CURSORE_LARGO, f->altezza);
    pennello_ripristina(p);
}

/* Un testo allineato a destra, che finisce in x. */
static void testo_a_destra(struct pennello *p, const struct font *f, int x, int y, int a, const char *s) {
    testo_centrato(p, f, x - larghezza_testo(f, s), y, a, s);
}

/* Un testo tagliato con "..." se e' piu' largo di 'largo'. */
static void testo_tagliato(struct pennello *p, const struct font *f, int x, int y, int a, int largo,
                           const char *s) {
    char riga[1][MAX_RIGA_NOME];
    if (larghezza_testo(f, s) <= largo) { testo_centrato(p, f, x, y, a, s); return; }
    testo_a_capo(f, s, largo, riga, 1);
    testo_centrato(p, f, x, y, a, riga[0]);
}

/* Lo stato della cartella al centro: l'icona, il titolo, la spiegazione (su piu' righe). */
static void disegna_stato(struct pennello *p, const struct widget *w, int icona_stato,
                          const char *titolo, const char *spiegazione) {
    char righe_t[3][MAX_RIGA_NOME], righe_s[3][MAX_RIGA_NOME];
    int nt = testo_a_capo(FONT_CORPO_MEDIO, titolo, STATO_LARGHEZZA, righe_t, 3);
    int ns = testo_a_capo(FONT_PICCOLO_NUOVO, spiegazione, STATO_LARGHEZZA, righe_s, 3);
    int alto = STATO_ICONA + STATO_TRA + nt * INTERLINEA_CORPO + (ns ? STATO_TRA + ns * INTERLINEA_PICCOLO : 0);
    int y = w->y + (w->a - alto) / 2, centro = w->x + w->l / 2;
    pennello_colore(p, TESTO_SECONDARIO);
    icona(p, icona_stato, STATO_ICONA, centro - STATO_ICONA / 2, y);
    y += STATO_ICONA + STATO_TRA;
    pennello_colore(p, TESTO_PRIMARIO);
    for (int i = 0; i < nt; i++, y += INTERLINEA_CORPO)
        testo_centrato(p, FONT_CORPO_MEDIO, centro - larghezza_testo(FONT_CORPO_MEDIO, righe_t[i]) / 2, y,
                       INTERLINEA_CORPO, righe_t[i]);
    y += STATO_TRA;
    pennello_colore(p, TESTO_SECONDARIO);
    for (int i = 0; i < ns; i++, y += INTERLINEA_PICCOLO)
        testo_centrato(p, FONT_PICCOLO_NUOVO, centro - larghezza_testo(FONT_PICCOLO_NUOVO, righe_s[i]) / 2, y,
                       INTERLINEA_PICCOLO, righe_s[i]);
}

/* Perche' la cartella non si vede: vuota, permesso negato, errore. */
static void disegna_stato_cartella(struct pennello *p, const struct widget *w) {
    if (errore_qui == ERRORE_PERMESSO) {
        disegna_stato(p, w, ICONA_LOCK, "Non hai il permesso di aprire questa cartella",
                      proprietario_qui == UID_SISTEMA
                          ? "Appartiene al sistema: solo il sistema pu\x95 leggerla."
                          : "Appartiene a un altro utente: solo lui pu\x95 leggerla.");
    } else if (errore_qui) {
        const char *perche = errore_qui == ERRORE_NOME ? "Il suo percorso \x8A troppo lungo per aprirla."
                           : errore_qui == ERRORE_NON_TROVATO ? "Non c'\x8A pi\x97: forse \x8A stata spostata."
                           : errore_qui == ERRORE_NON_CARTELLA ? "Non \x8A una cartella."
                           : "Il disco non ha risposto. Riprova tra poco.";
        disegna_stato(p, w, ICONA_TRIANGLE_ALERT, "Impossibile leggere la cartella", perche);
    } else if (!quanti_elementi) {
        disegna_stato(p, w, ICONA_FOLDER, "Questa cartella \x8A vuota",
                      "Usa il tasto destro per creare una nuova cartella.");
    } else {                                 /* ci sono file, ma il filtro o la ricerca li nascondono */
        disegna_stato(p, w, ICONA_SEARCH, "Nessun file da mostrare",
                      "Nessun file di questa cartella corrisponde al filtro o alla ricerca.");
    }
}

enum { NORMALE, SOPRA, SCELTO };

/* Come si vede un elemento: scelto (anche quello che si sta rinominando), sotto il
 * puntatore, o normale. */
static int stato_elemento(const struct contenuto *c, int e) {
    return e == c->scelto || e == c->rinomina ? SCELTO : e == c->sopra ? SOPRA : NORMALE;
}

/* Un tile con l'angolo in (x, y). */
static void disegna_tile(struct pennello *p, struct contenuto *c, int x, int y, int e) {
    const struct elemento *el = &elementi[e];
    int stato = stato_elemento(c, e);
    int rx = x + (TILE_LARGHEZZA - TILE_RIQUADRO) / 2, ry = y + TILE_CIMA;
    if (stato != NORMALE) {
        pennello_colore(p, stato == SCELTO ? CONTROLLO_SFONDO_SOPRA : CONTROLLO_SFONDO_NUOVO);
        riempi_tondo(p, rx, ry, TILE_RIQUADRO, TILE_RIQUADRO, RAGGIO_CONTROLLO_NUOVO);
    }
    const struct immagine *im = icone_file[el->genere][0];
    disegna_immagine(p, im, rx + (TILE_RIQUADRO - im->l) / 2, ry + (TILE_RIQUADRO - im->a) / 2);
    int ny = ry + TILE_RIQUADRO + TILE_SPAZIO;
    if (e == c->rinomina) {                  /* rinomina: al posto del nome, il campo */
        campo_nome(p, x, ny, TILE_LARGHEZZA, NOME_CAMPO_TILE, RAGGIO_CASELLA, c->nome_nuovo, 1);
        return;
    }
    char righe[TILE_RIGHE][MAX_RIGA_NOME];
    int n = testo_a_capo(FONT_CORPO, el->nome, TILE_NOME_LARGO, righe, TILE_RIGHE);
    if (stato == SCELTO && n) {              /* il nome scelto ha un fondo suo, largo quanto il testo */
        int largo = 0;
        for (int i = 0; i < n; i++) {
            int l = larghezza_testo(FONT_CORPO, righe[i]);
            if (l > largo) largo = l;
        }
        largo += 2 * TILE_TESTO_DENTRO;
        pennello_colore(p, AZIONE_SFONDO);
        riempi_tondo(p, x + (TILE_LARGHEZZA - largo) / 2, ny, largo, n * INTERLINEA_CORPO, RAGGIO_CASELLA);
    }
    pennello_colore(p, stato == SCELTO ? AZIONE_TESTO : TESTO_PRIMARIO);
    for (int i = 0; i < n; i++)
        testo_centrato(p, FONT_CORPO, x + (TILE_LARGHEZZA - larghezza_testo(FONT_CORPO, righe[i])) / 2,
                       ny + i * INTERLINEA_CORPO, INTERLINEA_CORPO, righe[i]);
}

static void disegna_icone(struct pennello *p, struct contenuto *c) {
    struct widget *w = &c->base;
    int scorso = c->barra.scorrimento.posizione;
    for (int k = 0; k < quanti_visibili; k++) {
        int x = w->x + c->ex[k], y = w->y + c->ey[k] - scorso;
        if (y + TILE_ALTEZZA + SPAZIO_4 <= w->y || y - SPAZIO_4 >= w->y + w->a) continue;   /* fuori dalla vista */
        disegna_tile(p, c, x, y, visibili[k]);
    }
}

static void disegna_elenco(struct pennello *p, struct contenuto *c) {
    struct widget *w = &c->base;
    int sinistra = w->x + ELENCO_MARGINE, destra = w->x + w->l - ELENCO_MARGINE;
    /* le colonne, da destra: Modificato, Dimensione, Tipo; il nome prende il resto */
    int modificato_x = destra - RIGA_DENTRO - COLONNA_MODIFICATO;
    int dimensione_x = modificato_x - COLONNA_DIMENSIONE;
    int tipo_x = dimensione_x - COLONNA_TIPO;
    int icona_x = sinistra + RIGA_DENTRO + CELLA_DENTRO;
    int nome_x = icona_x + icone_file[0][1]->l + NOME_GAP;
    int nome_largo = tipo_x - CELLA_DENTRO - nome_x;

    /* l'intestazione e la linea restano ferme */
    int ty = w->y + ELENCO_MARGINE;
    pennello_colore(p, TESTO_SECONDARIO);
    testo_centrato(p, FONT_ETICHETTA, icona_x, ty, ELENCO_TESTATA, "Nome");
    testo_centrato(p, FONT_ETICHETTA, tipo_x + CELLA_DENTRO, ty, ELENCO_TESTATA, "Tipo");
    testo_a_destra(p, FONT_ETICHETTA, modificato_x - CELLA_DENTRO, ty, ELENCO_TESTATA, "Dimensione");
    testo_centrato(p, FONT_ETICHETTA, modificato_x + CELLA_DENTRO, ty, ELENCO_TESTATA, "Modificato");
    int linea = ty + ELENCO_TESTATA;
    pennello_colore(p, BORDO_SEPARATORE);
    riempi_rettangolo(p, sinistra, linea, destra - sinistra, ELENCO_LINEA);

    pennello_salva(p);
    int sopra = righe_y(w);
    pennello_ritaglia(p, w->x, linea + ELENCO_LINEA, w->l, w->y + w->a - (linea + ELENCO_LINEA));
    int scorso = c->barra.scorrimento.posizione;
    for (int k = 0; k < quanti_visibili; k++) {
        int y = sopra + riga_elenco_y(k) - scorso;
        if (y + RIGA_ALTA + SPAZIO_4 <= linea || y - SPAZIO_4 >= w->y + w->a) continue;
        int e = visibili[k];
        const struct elemento *el = &elementi[e];
        int stato = stato_elemento(c, e);
        if (stato != NORMALE) {
            pennello_colore(p, stato == SCELTO ? CONTROLLO_SFONDO_SOPRA : CONTROLLO_SFONDO_NUOVO);
            riempi_tondo(p, sinistra, y, destra - sinistra, RIGA_ALTA, RAGGIO_CONTROLLO_NUOVO);
        }
        const struct immagine *im = icone_file[el->genere][1];
        disegna_immagine(p, im, icona_x, y + (RIGA_ALTA - im->a) / 2);
        if (e == c->rinomina) {
            campo_nome(p, nome_x - NOME_CAMPO_DENTRO, y + (RIGA_ALTA - ALTEZZA_S) / 2,
                       nome_largo + 2 * NOME_CAMPO_DENTRO, ALTEZZA_S, RAGGIO_CAMPO_S, c->nome_nuovo, 0);
        } else {
            pennello_colore(p, TESTO_PRIMARIO);
            testo_tagliato(p, FONT_CORPO, nome_x, y, RIGA_ALTA, nome_largo, el->nome);
        }
        char dimensione[24], data[32];
        scrivi_dimensione(dimensione, el);
        scrivi_data(data, el->modificato);
        pennello_colore(p, TESTO_SECONDARIO);
        testo_tagliato(p, FONT_CORPO, tipo_x + CELLA_DENTRO, y, RIGA_ALTA, COLONNA_TIPO - 2 * CELLA_DENTRO,
                       nomi_generi[el->genere]);
        testo_a_destra(p, FONT_CORPO, modificato_x - CELLA_DENTRO, y, RIGA_ALTA, dimensione);
        testo_tagliato(p, FONT_CORPO, modificato_x + CELLA_DENTRO, y, RIGA_ALTA,
                       COLONNA_MODIFICATO - 2 * CELLA_DENTRO, data);
    }
    pennello_ripristina(p);
}

/* L'AVVISO in basso: una pillola che sta sopra il contenuto (livello sovrapposto), con i
 * colori del suggerimento: il triangolo e il testo. */
#define AVVISO_ALTEZZA ALTEZZA_S
#define AVVISO_DENTRO  PADDING_S
#define AVVISO_ICONA   ICONA_S
#define AVVISO_GAP     GAP_M
#define AVVISO_BASSO   SPAZIO_16    /* dal fondo del contenuto */

static void disegna_avviso(struct pennello *p, const struct widget *w) {
    int largo = 2 * AVVISO_DENTRO + AVVISO_ICONA + AVVISO_GAP + larghezza_testo(FONT_CORPO, avviso);
    int x = w->x + (w->l - largo) / 2, y = w->y + w->a - AVVISO_ALTEZZA - AVVISO_BASSO;
    struct tondo t = tondo(x, y, largo, AVVISO_ALTEZZA, AVVISO_ALTEZZA / 2);
    ombre_strati_fuori(p, &t, OMBRA_TAB(sovrapposto), OMBRA_N(sovrapposto));
    pennello_colore(p, SUGGERIMENTO_SFONDO);
    riempi_forma(p, &t);
    pennello_colore(p, SUGGERIMENTO_TESTO_SECONDARIO);
    icona(p, ICONA_TRIANGLE_ALERT, AVVISO_ICONA, x + AVVISO_DENTRO, y + (AVVISO_ALTEZZA - AVVISO_ICONA) / 2);
    pennello_colore(p, SUGGERIMENTO_TESTO);
    testo_centrato(p, FONT_CORPO, x + AVVISO_DENTRO + AVVISO_ICONA + AVVISO_GAP, y, AVVISO_ALTEZZA, avviso);
}

static void contenuto_disegna(struct widget *w, struct pennello *p) {
    struct contenuto *c = (struct contenuto *)w;
    pennello_salva(p);
    pennello_ritaglia(p, w->x, w->y, w->l, w->a);
    if (errore_qui || !quanti_visibili) disegna_stato_cartella(p, w);
    else if (vista == A_ICONE) disegna_icone(p, c);
    else disegna_elenco(p, c);
    if (avviso[0]) disegna_avviso(p, w);
    pennello_ripristina(p);
}

/* --- cambiare i file: rinominare, creare, duplicare, eliminare --- */

/* L'elemento non e' piu' scelto (o un altro lo e'). */
static void scegli(int e) {
    if (contenuto.scelto == e) return;
    contenuto.scelto = e;
    widget_cambiato(&contenuto.base);
}

/* Rilegge la cartella e sceglie di nuovo il file che si chiama 'nome' (se c'e'). */
static void rileggi(const char *nome) {
    leggi_cartella();
    contenuto.scelto = contenuto.sopra = contenuto.rinomina = -1;
    if (nome)
        for (int i = 0; i < quanti_elementi; i++)
            if (uguali(elementi[i].nome, nome)) contenuto.scelto = i;
    aggiorna_contenuto();
}

/* Un nome che non c'e' ancora nella cartella: 'base', poi "base 2", "base 3"... prima
 * dell'estensione ("foto copia.png", "foto copia 2.png"). */
static void nome_libero(char *dove, const char *base, const char *estensione) {
    for (int n = 1; n < 1000; n++) {
        char *d = metti_testo(dove, base);
        if (n > 1) { *d++ = ' '; d = metti_numero(d, (unsigned long long)n); }
        metti_testo(d, estensione);
        char percorso[MAX_LUNGO];
        struct info_file i;
        percorso_di(percorso, qui, dove);
        if (!percorso[0] || info(percorso, &i) == ERRORE_NON_TROVATO) return;
    }
}

/* Comincia a rinominare l'elemento: il nome diventa un campo, con il nome di adesso. */
static void comincia_rinomina(int e) {
    if (e < 0) return;
    contenuto.rinomina = e;
    contenuto.scelto = e;
    copia_testo(contenuto.nome_nuovo, elementi[e].nome, MAX_NOME_FILE);
    toolkit_fuoco(&finestra, &contenuto.base);      /* i tasti vanno al campo */
    widget_cambiato(&contenuto.base);
}

/* Finisce di rinominare: se il nome e' cambiato (e va bene) il file si rinomina. */
static void finisci_rinomina(void) {
    int e = contenuto.rinomina;
    if (e < 0) return;
    contenuto.rinomina = -1;
    widget_cambiato(&contenuto.base);
    const char *nuovo = contenuto.nome_nuovo;
    if (uguali(nuovo, elementi[e].nome)) return;
    int va_bene = nuovo[0] && !uguali(nuovo, ".") && !uguali(nuovo, "..");
    for (const char *k = nuovo; *k; k++) if (*k == '/') va_bene = 0;
    if (!va_bene) { avvisa("Rinomina: il nome non va bene"); return; }
    char da[MAX_LUNGO], a[MAX_LUNGO];
    struct info_file i;
    percorso_di(da, qui, elementi[e].nome);
    percorso_di(a, qui, nuovo);
    /* rinomina sostituirebbe un file che c'e' gia' con quel nome: prima si guarda */
    int errore = !a[0] ? ERRORE_NOME : info(a, &i) == 0 ? ERRORE_ESISTE : rinomina(da, a);
    if (errore < 0) { avvisa_errore("Rinomina", errore); return; }
    char nome[MAX_NOME_FILE];
    copia_testo(nome, nuovo, MAX_NOME_FILE);
    rileggi(nome);
}

static void nuova_cartella(void) {
    char nome[MAX_NOME_FILE], percorso[MAX_LUNGO];
    nome_libero(nome, "Nuova cartella", "");
    percorso_di(percorso, qui, nome);
    int errore = percorso[0] ? crea_cartella(percorso) : ERRORE_NOME;
    if (errore < 0) { avvisa_errore("Nuova cartella", errore); return; }
    rileggi(nome);
    comincia_rinomina(contenuto.scelto);     /* come il Finder: si scrive subito il nome */
}

/* Duplica un file: una copia accanto, "nome copia.est". Le cartelle per ora no (bisognerebbe
 * copiare tutto quello che hanno dentro). */
static void duplica(int e) {
    if (e < 0 || elementi[e].tipo != TIPO_FILE) return;
    const char *nome = elementi[e].nome;
    char base[MAX_NOME_FILE], estensione[MAX_NOME_FILE] = "", copia[MAX_NOME_FILE];
    copia_testo(base, nome, MAX_NOME_FILE);
    int punto = -1;
    for (int i = 1; base[i]; i++) if (base[i] == '.') punto = i;
    if (punto > 0) { copia_testo(estensione, base + punto, MAX_NOME_FILE); base[punto] = 0; }
    metti_testo(base + lunghezza(base), " copia");
    nome_libero(copia, base, estensione);
    char da[MAX_LUNGO], a[MAX_LUNGO];
    percorso_di(da, qui, nome);
    percorso_di(a, qui, copia);
    int errore = 0;
    int f = a[0] ? apri(da, APRI_LEGGERE) : ERRORE_NOME;
    int g = f < 0 ? f : apri(a, APRI_SCRIVERE | APRI_CREARE | APRI_SVUOTARE);
    if (f < 0 || g < 0) errore = f < 0 ? f : g;
    static char blocco[4096];
    int n;
    while (!errore && (n = leggi(f, blocco, sizeof blocco)) > 0)
        if (scrivi_in(g, blocco, (unsigned int)n) != n) errore = ERRORE_PIENO;
    if (f >= 0) chiudi(f);
    if (g >= 0) chiudi(g);
    if (g >= 0 && errore) cancella(a);           /* una copia a meta' non serve */
    if (errore < 0) { avvisa_errore("Duplica", errore); return; }
    rileggi(copia);
}

/* Elimina un file, o una cartella vuota (quelle piene per ora no: niente cestino, e
 * cancellare tutto quello che hanno dentro e' troppo facile da sbagliare). */
static void elimina(int e) {
    if (e < 0) return;
    char percorso[MAX_LUNGO];
    percorso_di(percorso, qui, elementi[e].nome);
    int errore = percorso[0] ? cancella(percorso) : ERRORE_NOME;
    if (errore == ERRORE_NON_VUOTA) {
        avvisa("Per ora si possono eliminare solo le cartelle vuote");
        return;
    }
    if (errore < 0) { avvisa_errore("Elimina", errore); return; }
    rileggi(0);
}

/* Un pacchetto .app (un'app da installare, vedi pacchetto.h)? */
static const char *const estensioni_pacchetti[] = { "app", 0 };

static int e_pacchetto(int e) {
    return e >= 0 && elementi[e].tipo == TIPO_FILE && ha_estensione(elementi[e].nome, estensioni_pacchetti);
}

/* Apre un elemento: una cartella ci si entra; un pacchetto .app lo si chiede al launcher,
 * che fa partire lo Store (o dice che le app vanno installate prima: launcher.h); gli
 * altri file per ora no. */
static void apri_elemento(int e) {
    if (e < 0 || (elementi[e].tipo != TIPO_CARTELLA && !e_pacchetto(e))) return;
    char percorso[MAX_LUNGO];
    percorso_di(percorso, qui, elementi[e].nome);
    if (!percorso[0] || (e_pacchetto(e) && lunghezza(percorso) >= MAX_PERCORSO)) {
        avvisa("Il percorso \x8A troppo lungo");
        return;
    }
    if (elementi[e].tipo == TIPO_CARTELLA) { vai(percorso, 1); return; }
    struct risposta_launcher risposta;
    int esito = apri_file(percorso, &risposta);
    if (esito == LAUNCHER_SERVE_STORE) avvisa("Le app vanno installate prima di aprirle");
    else if (esito < 0) avvisa("Non si riesce ad aprire il pacchetto");
}

/* --- il menu del tasto destro e la scheda delle informazioni --- */

enum {
    NIENTE, APRI, RINOMINA, DUPLICA, INFORMAZIONI, ELIMINA,
    NUOVA_CARTELLA, ORDINA, MOSTRA_ICONE, MOSTRA_ELENCO,
    ORDINA_NOME, ORDINA_TIPO, ORDINA_DIMENSIONE, ORDINA_DATA,
    SEPARATORE
};

struct voce_menu {
    int azione;                      /* SEPARATORE: una linea */
    const char *testo;
    int icona;                       /* NESSUNA_ICONA: niente (e niente posto per lei) */
    const char *scorciatoia;         /* 0: niente */
};

static const struct voce_menu menu_file[] = {
    { APRI,         "Apri",         ICONA_FOLDER_OPEN, "Invio" },
    { SEPARATORE,   0, 0, 0 },
    { RINOMINA,     "Rinomina",     ICONA_PENCIL,      "F2" },
    { DUPLICA,      "Duplica",      ICONA_COPY,        "Ctrl+D" },
    { INFORMAZIONI, "Informazioni", ICONA_INFO,        "Ctrl+I" },
    { SEPARATORE,   0, 0, 0 },
    { ELIMINA,      "Elimina",      ICONA_TRASH_2,     "Canc" },
};
static const struct voce_menu menu_cartella[] = {
    { NUOVA_CARTELLA, "Nuova cartella",     ICONA_FOLDER_PLUS,   "Ctrl+Maiusc+N" },
    { SEPARATORE,     0, 0, 0 },
    { ORDINA,         "Ordina per",         ICONA_ARROW_UP_DOWN, 0 },
    { SEPARATORE,     0, 0, 0 },
    { MOSTRA_ICONE,   "Mostra come icone",  ICONA_LAYOUT_GRID,   0 },
    { MOSTRA_ELENCO,  "Mostra come elenco", ICONA_LIST,          0 },
};
static const struct voce_menu menu_ordina[] = {
    { ORDINA_NOME,       "Nome",             NESSUNA_ICONA, 0 },
    { ORDINA_TIPO,       "Tipo",             NESSUNA_ICONA, 0 },
    { ORDINA_DIMENSIONE, "Dimensione",       NESSUNA_ICONA, 0 },
    { ORDINA_DATA,       "Data di modifica", NESSUNA_ICONA, 0 },
};
#define QUANTE(v) ((int)(sizeof v / sizeof v[0]))

/* Un pannello aperto: le sue voci e dove sta. */
struct pannello {
    const struct voce_menu *voci;
    int quante;
    int x, y;                        /* nella finestra */
};

enum { CHIUSO, MENU, SCHEDA };

struct menu {
    struct widget base;              /* aperto, copre tutta la finestra: un clic fuori lo chiude */
    int stato;                       /* CHIUSO, MENU, SCHEDA (le informazioni) */
    struct pannello pannelli[2];     /* il menu e il sottomenu (Ordina per) */
    int quanti;                      /* 1, o 2 con il sottomenu aperto */
    int sopra;                       /* la voce sotto il puntatore: pannello * 100 + voce, o -1 */
    int premuta;                     /* quella su cui si e' premuto, o -1 */
    int bersaglio;                   /* l'elemento su cui si e' aperto il menu (o -1) */
};

static struct menu menu;

/* IL MENU: il "Menu" del file Figma (Componenti complessi), livello sovrapposto. Un pannello
 * largo 240, raggio 20, fondo SUPERFICIE_ELEVATA con l'ombra "sovrapposto"; 8 sopra e sotto;
 * le voci M (alte 40, larghe 224: rientrate di 8 ai lati, raggio 12, concentrico) una sotto
 * l'altra; i separatori sono una linea BORDO_SEPARATORE da bordo a bordo, con 4 sopra e sotto.
 * Nella voce: 12, il simbolo (18), 8, il testo ("corpo"), la scorciatoia ("piccolo",
 * TESTO_SECONDARIO) o la freccia del sottomenu o la spunta (16) a destra, 12. Sotto il
 * puntatore la voce ha il fondo CONTROLLO_SFONDO; spenta e' tutta TESTO_DISABILITATO. */
#define MENU_LARGO         240
#define MENU_PADDING       SPAZIO_8     /* sopra, sotto, e a sinistra e a destra delle voci */
#define VOCE_ALTA          ALTEZZA_M
#define VOCE_DENTRO        PADDING_S    /* 12 */
#define VOCE_GAP           GAP_M        /* 8 */
#define VOCE_SIMBOLO       ICONA_M      /* 18 */
#define VOCE_DESTRA        ICONA_S      /* 16: la freccia del sottomenu, la spunta */
#define SEPARA_ALTO        (SPAZIO_4 + 1 + SPAZIO_4)
#define SOTTOMENU_STACCO   SPAZIO_4     /* tra il menu e il sottomenu */
#define MENU_BORDO         SPAZIO_4     /* il menu non va piu' vicino di cosi' al bordo della finestra */

static int alta_voce(const struct voce_menu *v) { return v->azione == SEPARATORE ? SEPARA_ALTO : VOCE_ALTA; }

static int altezza_pannello(const struct pannello *pn) {
    int a = 2 * MENU_PADDING;
    for (int i = 0; i < pn->quante; i++) a += alta_voce(&pn->voci[i]);
    return a;
}

/* Dove comincia la voce i del pannello (y), e quanto e' alta. */
static int voce_y(const struct pannello *pn, int i, int *alta) {
    int y = pn->y + MENU_PADDING;
    for (int k = 0; k < i; k++) y += alta_voce(&pn->voci[k]);
    *alta = alta_voce(&pn->voci[i]);
    return y;
}

/* La voce si puo' scegliere? Apri solo le cartelle (gli altri file non hanno ancora chi
 * li apra), Duplica solo i file. */
static int voce_attiva(int azione) {
    int e = menu.bersaglio;
    if (azione == APRI) return e >= 0 && (elementi[e].tipo == TIPO_CARTELLA || e_pacchetto(e));
    if (azione == DUPLICA) return e >= 0 && elementi[e].tipo == TIPO_FILE;
    if (azione == NUOVA_CARTELLA) return !errore_qui;
    return 1;
}

/* La spunta: la vista e l'ordine di adesso. */
static int voce_spuntata(int azione) {
    return (azione == MOSTRA_ICONE && vista == A_ICONE) || (azione == MOSTRA_ELENCO && vista == A_ELENCO) ||
           (azione == ORDINA_NOME && ordine == PER_NOME) || (azione == ORDINA_TIPO && ordine == PER_TIPO) ||
           (azione == ORDINA_DIMENSIONE && ordine == PER_DIMENSIONE) || (azione == ORDINA_DATA && ordine == PER_DATA);
}

/* Il fondo dei pannelli (il menu, la scheda delle informazioni): sovrapposto, cioe' l'ombra a
 * strati fuori, il fondo pieno SUPERFICIE_ELEVATA, poi gli strati interni (il filo di luce
 * del tema scuro). E' opaco: il contenuto di sotto non si vede, e non serve sfocarlo. */
static void fondo_pannello(struct pennello *p, int x, int y, int l, int a) {
    struct tondo t = tondo(x, y, l, a, RAGGIO_PANNELLO);
    ombre_strati_fuori(p, &t, OMBRA_TAB(sovrapposto), OMBRA_N(sovrapposto));
    pennello_colore(p, SUPERFICIE_ELEVATA);
    riempi_forma(p, &t);
    ombre_strati_dentro(p, &t, OMBRA_TAB(sovrapposto), OMBRA_N(sovrapposto));
}

static void disegna_pannello(struct pennello *p, const struct pannello *pn, int numero) {
    fondo_pannello(p, pn->x, pn->y, MENU_LARGO, altezza_pannello(pn));
    int x = pn->x + MENU_PADDING, largo = MENU_LARGO - 2 * MENU_PADDING;
    for (int i = 0; i < pn->quante; i++) {
        const struct voce_menu *v = &pn->voci[i];
        int alta, y = voce_y(pn, i, &alta);
        if (v->azione == SEPARATORE) {
            pennello_colore(p, BORDO_SEPARATORE);
            riempi_rettangolo(p, pn->x, y + SPAZIO_4, MENU_LARGO, 1);
            continue;
        }
        int attiva = voce_attiva(v->azione);
        int accesa = attiva && (menu.sopra == numero * 100 + i ||
                                (v->azione == ORDINA && menu.quanti > 1));
        if (accesa) {
            pennello_colore(p, CONTROLLO_SFONDO_NUOVO);
            riempi_tondo(p, x, y, largo, alta, RAGGIO_CONTROLLO_NUOVO);
        }
        int tx = x + VOCE_DENTRO;
        pennello_colore(p, attiva ? TESTO_PRIMARIO : TESTO_DISABILITATO);
        if (v->icona != NESSUNA_ICONA) {
            icona(p, v->icona, VOCE_SIMBOLO, tx, y + (alta - VOCE_SIMBOLO) / 2);
            tx += VOCE_SIMBOLO + VOCE_GAP;
        }
        testo_centrato(p, FONT_CORPO, tx, y, alta, v->testo);
        int destra = x + largo - VOCE_DENTRO;
        if (v->azione == ORDINA)
            icona(p, ICONA_CHEVRON_RIGHT, VOCE_DESTRA, destra - VOCE_DESTRA, y + (alta - VOCE_DESTRA) / 2);
        if (voce_spuntata(v->azione))
            icona(p, ICONA_CHECK, VOCE_DESTRA, destra - VOCE_DESTRA, y + (alta - VOCE_DESTRA) / 2);
        if (v->scorciatoia) {
            pennello_colore(p, attiva ? TESTO_SECONDARIO : TESTO_DISABILITATO);
            testo_a_destra(p, FONT_PICCOLO_NUOVO, destra, y, alta, v->scorciatoia);
        }
    }
}

/* LA SCHEDA DELLE INFORMAZIONI: un pannello al centro della finestra, come il menu
 * (sovrapposto, raggio 20), con 24 di margine: l'icona, il nome (sezione) e sotto, a due
 * colonne, quello che si sa del file (titoli in TESTO_SECONDARIO, valori a destra). Un clic
 * ovunque la chiude. */
#define SCHEDA_LARGHEZZA 320
#define SCHEDA_DENTRO    SPAZIO_24
#define SCHEDA_RIGA      (ALTEZZA_S - SPAZIO_8)   /* 24: una riga di "corpo" con un po' d'aria */
#define SCHEDA_TITOLI    90                     /* la colonna dei titoli */

static const char *const titoli_scheda[] = { "Tipo", "Dimensione", "Modificato", "Permessi", "Dove" };
#define RIGHE_SCHEDA 5

static int altezza_scheda(void) {
    return 2 * SCHEDA_DENTRO + icone_file[0][0]->a + SPAZIO_8 + INTERLINEA_SEZIONE + SPAZIO_16 +
           RIGHE_SCHEDA * (SCHEDA_RIGA);
}

static void disegna_scheda(struct pennello *p, const struct widget *w) {
    int e = menu.bersaglio;
    if (e < 0) return;
    const struct elemento *el = &elementi[e];
    int alta = altezza_scheda();
    int x = w->x + (w->l - SCHEDA_LARGHEZZA) / 2;
    int corpo_y = finestra_tk_corpo(&finestra);
    int y = corpo_y + (w->a - corpo_y - alta) / 2;
    fondo_pannello(p, x, y, SCHEDA_LARGHEZZA, alta);
    const struct immagine *im = icone_file[el->genere][0];
    disegna_immagine(p, im, x + (SCHEDA_LARGHEZZA - im->l) / 2, y + SCHEDA_DENTRO);
    int ny = y + SCHEDA_DENTRO + im->a + SPAZIO_8;
    int largo = SCHEDA_LARGHEZZA - 2 * SCHEDA_DENTRO;
    char nome[1][MAX_RIGA_NOME];
    testo_a_capo(FONT_SEZIONE_NUOVO, el->nome, largo, nome, 1);
    pennello_colore(p, TESTO_PRIMARIO);
    testo_centrato(p, FONT_SEZIONE_NUOVO, x + (SCHEDA_LARGHEZZA - larghezza_testo(FONT_SEZIONE_NUOVO, nome[0])) / 2,
                   ny, INTERLINEA_SEZIONE, nome[0]);
    char valori[RIGHE_SCHEDA][MAX_LUNGO];
    copia_testo(valori[0], nomi_generi[el->genere], MAX_LUNGO);
    scrivi_dimensione(valori[1], el);
    scrivi_data(valori[2], el->modificato);
    scrivi_permessi(valori[3], el->permessi);
    copia_testo(valori[4], qui, MAX_LUNGO);
    int ry = ny + INTERLINEA_SEZIONE + SPAZIO_16;
    for (int i = 0; i < RIGHE_SCHEDA; i++, ry += SCHEDA_RIGA) {
        pennello_colore(p, TESTO_SECONDARIO);
        testo_centrato(p, FONT_CORPO, x + SCHEDA_DENTRO, ry, SCHEDA_RIGA, titoli_scheda[i]);
        pennello_colore(p, TESTO_PRIMARIO);
        char riga[1][MAX_RIGA_NOME];
        testo_a_capo(FONT_CORPO, valori[i], largo - SCHEDA_TITOLI, riga, 1);
        testo_a_destra(p, FONT_CORPO, x + SCHEDA_LARGHEZZA - SCHEDA_DENTRO, ry, SCHEDA_RIGA, riga[0]);
    }
}

static void menu_disegna(struct widget *w, struct pennello *p) {
    struct menu *m = (struct menu *)w;
    if (m->stato == SCHEDA) { disegna_scheda(p, w); return; }
    for (int i = 0; i < m->quanti; i++) disegna_pannello(p, &m->pannelli[i], i);
}

/* La voce nel punto (x, y): pannello * 100 + voce (anche un separatore), o -1 se il punto
 * e' fuori dai pannelli; -2 se e' dentro un pannello ma non su una voce. */
static int voce_in(const struct menu *m, int x, int y) {
    for (int n = m->quanti - 1; n >= 0; n--) {
        const struct pannello *pn = &m->pannelli[n];
        if (x < pn->x || x >= pn->x + MENU_LARGO || y < pn->y || y >= pn->y + altezza_pannello(pn)) continue;
        for (int i = 0; i < pn->quante; i++) {
            int alta, vy = voce_y(pn, i, &alta);
            if (y >= vy && y < vy + alta && x >= pn->x + MENU_PADDING && x < pn->x + MENU_LARGO - MENU_PADDING)
                return pn->voci[i].azione == SEPARATORE ? -2 : n * 100 + i;
        }
        return -2;
    }
    return -1;
}

static const struct voce_menu *voce_di(const struct menu *m, int id) {
    return id < 0 ? 0 : &m->pannelli[id / 100].voci[id % 100];
}

/* Apre il sottomenu "Ordina per" accanto alla voce 'i' del menu: a destra, o a sinistra
 * se a destra non ci sta. */
static void apri_sottomenu(struct menu *m, int i) {
    const struct pannello *pn = &m->pannelli[0];
    int alta, vy = voce_y(pn, i, &alta);
    struct pannello *sotto = &m->pannelli[1];
    *sotto = (struct pannello){ menu_ordina, QUANTE(menu_ordina), pn->x + MENU_LARGO + SOTTOMENU_STACCO, vy - MENU_PADDING };
    if (sotto->x + MENU_LARGO > m->base.l - MENU_BORDO) sotto->x = pn->x - MENU_LARGO - SOTTOMENU_STACCO;
    if (sotto->x < MENU_BORDO) sotto->x = MENU_BORDO;
    int a = altezza_pannello(sotto);
    if (sotto->y + a > m->base.a - MENU_BORDO) sotto->y = m->base.a - MENU_BORDO - a;
    m->quanti = 2;
}

static void cambia_vista(int nuova);

static void esegui_voce(int azione) {
    int e = menu.bersaglio;
    chiudi_menu();
    switch (azione) {
    case APRI:          apri_elemento(e); break;
    case RINOMINA:      comincia_rinomina(e); break;
    case DUPLICA:       duplica(e); break;
    case INFORMAZIONI:
        menu.stato = SCHEDA;
        menu.bersaglio = e;
        menu.base.nascosto = 0;
        break;
    case ELIMINA:       elimina(e); break;
    case NUOVA_CARTELLA: nuova_cartella(); break;
    case MOSTRA_ICONE:  cambia_vista(A_ICONE); break;
    case MOSTRA_ELENCO: cambia_vista(A_ELENCO); break;
    case ORDINA_NOME: case ORDINA_TIPO: case ORDINA_DIMENSIONE: case ORDINA_DATA:
        ordine = azione == ORDINA_NOME ? PER_NOME : azione == ORDINA_TIPO ? PER_TIPO
               : azione == ORDINA_DIMENSIONE ? PER_DIMENSIONE : PER_DATA;
        aggiorna_contenuto();
        break;
    }
    widget_cambiato(&contenuto.base);
}

/* Premuto fuori dai pannelli: il menu si chiude (e il clic finisce li', come sul Mac).
 * Lasciato sulla stessa voce su cui si e' premuto: si fa quello che dice. */
static void menu_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    struct menu *m = (struct menu *)w;
    if (m->stato == SCHEDA) {
        if (pulsanti) chiudi_menu();
        return;
    }
    int id = voce_in(m, x, y);
    if (pulsanti & (PULSANTE_SINISTRO | PULSANTE_DESTRO)) {
        if (m->premuta == -1) m->premuta = id;
        if (id == -1) chiudi_menu();
        return;
    }
    int premuta = m->premuta;
    m->premuta = -1;
    if (id < 0 || id != premuta) return;
    const struct voce_menu *v = voce_di(m, id);
    if (!voce_attiva(v->azione)) return;
    if (v->azione == ORDINA) { apri_sottomenu(m, id % 100); widget_cambiato(w); return; }
    esegui_voce(v->azione);
}

/* Il puntatore sopra le voci: si accendono; sopra "Ordina per" si apre il sottomenu,
 * sopra un'altra voce del menu si chiude. */
static void menu_movimento(struct widget *w, int x, int y) {
    struct menu *m = (struct menu *)w;
    if (m->stato != MENU) return;
    int id = voce_in(m, x, y);
    if (id < -1) id = -1;
    if (id == m->sopra) return;
    m->sopra = id;
    const struct voce_menu *v = voce_di(m, id);
    if (v && id / 100 == 0) {
        if (v->azione == ORDINA) apri_sottomenu(m, id % 100);
        else m->quanti = 1;
    }
    widget_cambiato(w);
}

static const struct tipo_widget tipo_menu = { 0, 0, menu_disegna, menu_puntatore, 0, 0, 0, menu_movimento, 0 };

/* Apre un menu con l'angolo nel punto (x, y) della finestra (dove si e' cliccato),
 * spostandolo quanto basta perche' stia tutto nella finestra. */
static void apri_menu(const struct voce_menu *voci, int quante, int x, int y, int bersaglio) {
    struct menu *m = &menu;
    m->stato = MENU;
    m->quanti = 1;
    m->sopra = m->premuta = -1;
    m->bersaglio = bersaglio;
    m->base.x = m->base.y = 0;
    m->base.l = finestra.base.l;
    m->base.a = finestra.base.a;
    struct pannello *pn = &m->pannelli[0];
    *pn = (struct pannello){ voci, quante, x, y };
    int a = altezza_pannello(pn);
    if (pn->x + MENU_LARGO > m->base.l - MENU_BORDO) pn->x = m->base.l - MENU_BORDO - MENU_LARGO;
    if (pn->y + a > m->base.a - MENU_BORDO) pn->y = m->base.a - MENU_BORDO - a;
    if (pn->x < MENU_BORDO) pn->x = MENU_BORDO;
    if (pn->y < TITOLO_ALTEZZA) pn->y = TITOLO_ALTEZZA;
    widget_nascondi(&m->base, 0);
}

static void chiudi_menu(void) {
    if (menu.stato == CHIUSO) return;
    menu.stato = CHIUSO;
    menu.quanti = 0;
    widget_nascondi(&menu.base, 1);
    /* il puntatore e' ancora li': il toolkit deve ritrovare il widget sotto di lui */
    toolkit_sotto(&finestra, 0);
}

/* --- il contenuto: il puntatore, la tastiera, la rotella --- */

static void contenuto_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    struct contenuto *c = (struct contenuto *)w;
    unsigned int adesso = tempo();
    int giu = pulsanti & (PULSANTE_SINISTRO | PULSANTE_DESTRO);
    static unsigned int prima;
    int appena = giu && !(prima & (PULSANTE_SINISTRO | PULSANTE_DESTRO));
    prima = pulsanti;
    if (!appena) return;
    int k = visibile_in(c, x, y), e = k >= 0 ? visibili[k] : -1;
    if (avviso[0]) { avvisa(""); widget_cambiato(w); }
    if (c->rinomina >= 0 && e != c->rinomina) finisci_rinomina();   /* clic altrove: va bene cosi' */
    if (c->rinomina >= 0) return;                                   /* clic nel campo */
    /* rinominare puo' aver riletto la cartella: l'elemento si ritrova dal punto */
    k = visibile_in(c, x, y);
    e = k >= 0 ? visibili[k] : -1;
    scegli(e);
    if (pulsanti & PULSANTE_DESTRO) {
        if (e >= 0) apri_menu(menu_file, QUANTE(menu_file), x, y, e);
        else if (!errore_qui) apri_menu(menu_cartella, QUANTE(menu_cartella), x, y, -1);
        c->clic_su = -1;
        return;
    }
    if (e >= 0 && e == c->clic_su && (int)(adesso - c->clic_prima) < DOPPIO_CLIC) {
        c->clic_su = -1;
        apri_elemento(e);
        return;
    }
    c->clic_su = e;
    c->clic_prima = adesso;
}

/* I tasti: mentre si rinomina, le lettere vanno nel nome (Invio conferma); se no Invio
 * apre la cartella scelta. */
static void contenuto_tasto(struct widget *w, unsigned int tasto) {
    struct contenuto *c = (struct contenuto *)w;
    if (c->rinomina >= 0) {
        int n = lunghezza(c->nome_nuovo);
        if (tasto == '\n') { finisci_rinomina(); return; }
        if (tasto == '\b') { if (n) c->nome_nuovo[n - 1] = 0; }
        else if (tasto >= ' ' && tasto != 0x7F && tasto != '/' && n < MAX_PERCORSO - 8) {
            c->nome_nuovo[n] = (char)tasto;
            c->nome_nuovo[n + 1] = 0;
        } else return;
        widget_cambiato(w);
        return;
    }
    if (tasto == '\n') apri_elemento(c->scelto);
}

static void contenuto_rotella(struct widget *w, int sx, int sy) {
    (void)sx;
    barra_scorrimento_rotella(&((struct contenuto *)w)->barra, sy);
}

/* Il puntatore sopra un elemento lo illumina appena. */
static void contenuto_movimento(struct widget *w, int x, int y) {
    struct contenuto *c = (struct contenuto *)w;
    int k = visibile_in(c, x, y), e = k >= 0 ? visibili[k] : -1;
    if (e != c->sopra) { c->sopra = e; widget_cambiato(w); }
}

static void contenuto_sopra(struct widget *w) {
    struct contenuto *c = (struct contenuto *)w;
    if (!w->sopra && c->sopra >= 0) { c->sopra = -1; widget_cambiato(w); }
}

static const struct tipo_widget tipo_contenuto = {
    contenuto_misura, contenuto_disponi, contenuto_disegna, contenuto_puntatore, contenuto_tasto,
    contenuto_rotella, contenuto_sopra, contenuto_movimento, 0
};

/* --- andare in una cartella --- */

/* Il filtro o la ricerca sono cambiati, o la cartella e' stata riletta: si rifa' chi si
 * vede e dove. */
static void aggiorna_contenuto(void) {
    scegli_visibili();
    if (contenuto.scelto >= 0 && posto_visibile(contenuto.scelto) < 0) contenuto.scelto = -1;
    contenuto.sopra = -1;
    contenuto_impagina(&contenuto);
    widget_cambiato(&contenuto.base);
    widget_cambiato(&laterale.base);
}

/* Il percorso nella barra: dentro la casa, la casetta e "home", poi le cartelle; fuori,
 * "Disco" (la radice) e le cartelle. Se sono troppe, si tengono le ultime. */
static int primo_nome;   /* quanti nomi del percorso intero non si vedono, all'inizio */
static int dentro_casa(const char *p) {
    int n = lunghezza(CASA);
    for (int i = 0; i < n; i++) if (p[i] != CASA[i]) return 0;
    return p[n] == 0 || p[n] == '/';
}

/* I nomi del percorso 'p' in 'nomi' (puntatori dentro 'copia'): quanti sono. */
static int nomi_del_percorso(const char *p, char *copia, const char **nomi, int massimo) {
    int casa = dentro_casa(p), n = 0;
    copia_testo(copia, casa ? p + lunghezza(CASA) : p, MAX_LUNGO);
    nomi[n++] = casa ? "home" : "Disco";
    for (char *c = copia; *c && n < massimo;) {
        while (*c == '/') *c++ = 0;
        if (!*c) break;
        nomi[n++] = c;
        while (*c && *c != '/') c++;
    }
    return n;
}

static void aggiorna_percorso(void) {
    char copia[MAX_LUNGO];
    const char *nomi[MAX_LUNGO / 2];
    int n = nomi_del_percorso(qui, copia, nomi, MAX_LUNGO / 2);
    primo_nome = n > MAX_NOMI_PERCORSO ? n - MAX_NOMI_PERCORSO : 0;
    percorso_casa(&percorso, dentro_casa(qui));
    percorso_metti(&percorso, nomi + primo_nome, n - primo_nome);

    /* il titolo della finestra e il suggerimento della ricerca: il nome della cartella */
    const char *ultimo = n == 1 ? (dentro_casa(qui) ? "Home" : "Disco") : nomi[n - 1];
    char titolo[MAX_TITOLO];
    copia_testo(titolo, ultimo, MAX_TITOLO);
    if (lunghezza(ultimo) > MAX_TITOLO - 1) metti_testo(titolo + MAX_TITOLO - 4, "...");   /* troppo lungo */
    finestra_tk_titolo(&finestra, titolo);
    char cerca[MAX_TESTO_WIDGET], *d = metti_testo(cerca, "Cerca in ");
    copia_testo(d, ultimo, MAX_TESTO_WIDGET - 10);
    copia_testo(ricerca.suggerimento, cerca, MAX_TESTO_WIDGET);
    widget_cambiato(&ricerca.base);
}

/* Un clic su un nome del percorso: si torna a quella cartella. */
static void clic_percorso(struct percorso *p, int indice, void *dati) {
    (void)p; (void)dati;
    char copia[MAX_LUNGO], dove[MAX_LUNGO];
    const char *nomi[MAX_LUNGO / 2];
    int n = nomi_del_percorso(qui, copia, nomi, MAX_LUNGO / 2);
    indice += primo_nome;
    if (indice >= n - 1) return;
    char *d = metti_testo(dove, dentro_casa(qui) ? CASA : "");
    for (int i = 1; i <= indice; i++) { *d++ = '/'; d = metti_testo(d, nomi[i]); }
    if (!dove[0]) metti_testo(dove, "/");
    avvisa("");
    vai(dove, 1);
}

/* Va nella cartella 'dove' (e se 'nella_storia', la mette nella storia). Il filtro e la
 * ricerca ricominciano da capo; si torna in cima. */
static void vai(const char *dove, int nella_storia) {
    chiudi_menu();
    if (contenuto.rinomina >= 0) finisci_rinomina();
    copia_testo(qui, dove, MAX_LUNGO);
    int n = lunghezza(qui);
    while (n > 1 && qui[n - 1] == '/') qui[--n] = 0;   /* senza la barra in fondo */
    if (nella_storia) {
        if (posto_storia >= 0 && uguali(storia[posto_storia], qui)) nella_storia = 0;
    }
    if (nella_storia) {
        if (posto_storia == MAX_STORIA - 1) {            /* piena: si dimentica la piu' vecchia */
            for (int i = 1; i < MAX_STORIA; i++) copia_testo(storia[i - 1], storia[i], MAX_LUNGO);
            posto_storia--;
        }
        posto_storia++;
        copia_testo(storia[posto_storia], qui, MAX_LUNGO);
        quante_storia = posto_storia + 1;                 /* l'"avanti" di prima non vale piu' */
    }
    filtro = -1;
    ricerca.testo[0] = 0;
    widget_cambiato(&ricerca.base);
    leggi_cartella();
    contenuto.scelto = contenuto.sopra = contenuto.rinomina = contenuto.clic_su = -1;
    contenuto.barra.scorrimento.posizione = 0;
    aggiorna_contenuto();
    aggiorna_percorso();
    aggiorna_frecce();
    toolkit_ridisponi(&finestra);
}

static void premi_indietro(struct pulsante_barra *b, void *dati) {
    (void)b; (void)dati;
    if (posto_storia <= 0) return;
    posto_storia--;
    avvisa("");
    vai(storia[posto_storia], 0);
}

static void premi_avanti(struct pulsante_barra *b, void *dati) {
    (void)b; (void)dati;
    if (posto_storia >= quante_storia - 1) return;
    posto_storia++;
    avvisa("");
    vai(storia[posto_storia], 0);
}

/* Icone o elenco (dalla pillola o dal menu): si accende il pulsante giusto e si rifa'
 * il posto dei file, tornando in cima. */
static void cambia_vista(int nuova) {
    vista = nuova;
    pulsante_barra_accendi(nuova == A_ICONE ? &a_icone : &a_elenco, 1);
    contenuto.barra.scorrimento.posizione = 0;
    contenuto_disponi(&contenuto.base);
    widget_cambiato(&contenuto.base);
}

static void premi_vista(struct pulsante_barra *b, void *dati) {
    (void)dati;
    cambia_vista(b == &a_icone ? A_ICONE : A_ELENCO);
}

static void ricerca_cambiata(struct campo *c, void *dati) {
    (void)c; (void)dati;
    contenuto.barra.scorrimento.posizione = 0;
    aggiorna_contenuto();
}

int main(int quanti, char **argomenti) {
    /* la barra degli strumenti: indietro e avanti, il percorso, icone o elenco, la ricerca */
    barra_strumenti_prepara(&strumenti);
    pillola_prepara(&frecce, 0);
    pulsante_barra_prepara(&indietro, ICONA_ARROW_LEFT, premi_indietro, 0);
    pulsante_barra_prepara(&avanti, ICONA_ARROW_RIGHT, premi_avanti, 0);
    pillola_aggiungi(&frecce, &indietro);
    pillola_aggiungi(&frecce, &avanti);
    percorso_prepara(&percorso, clic_percorso, 0);
    pillola_prepara(&viste, 1);
    pulsante_barra_prepara(&a_icone, ICONA_LAYOUT_GRID, premi_vista, 0);
    pulsante_barra_prepara(&a_elenco, ICONA_LIST, premi_vista, 0);
    pillola_aggiungi(&viste, &a_icone);
    pillola_aggiungi(&viste, &a_elenco);
    pulsante_barra_accendi(&a_icone, 1);
    ricerca_prepara(&ricerca, "Cerca in Home", ricerca_cambiata, 0);
    barra_strumenti_aggiungi(&strumenti, &frecce.base);
    barra_strumenti_aggiungi(&strumenti, &percorso.base);
    barra_strumenti_aggiungi(&strumenti, &viste.base);
    barra_strumenti_aggiungi(&strumenti, &ricerca.base);

    /* il corpo: la barra laterale e il contenuto, attaccati, fino ai bordi */
    riga_prepara(&corpo, 0);
    laterale = (struct laterale){ .base = { .tipo = &tipo_laterale, .largo = 1 }, .sopra = -1, .premuta = -1 };
    contenuto = (struct contenuto){ .base = { .tipo = &tipo_contenuto, .elastico = 1, .largo = 1 },
                                    .scelto = -1, .sopra = -1, .rinomina = -1, .clic_su = -1 };
    barra_scorrimento_prepara(&contenuto.barra);
    widget_aggiungi(&contenuto.base, &contenuto.barra.base);
    widget_aggiungi(&corpo.base, &laterale.base);
    widget_aggiungi(&corpo.base, &contenuto.base);

    finestra_tk_prepara(&finestra, "Files", &corpo.base);
    finestra_tk_barra(&finestra, &strumenti.base);
    finestra_tk_bordo(&finestra);
    finestra_tk_dimensione(&finestra, 800, 480);

    /* il menu, sopra tutto (l'ultimo figlio della finestra): nascosto finche' non serve */
    menu = (struct menu){ .base = { .tipo = &tipo_menu, .nascosto = 1 }, .sopra = -1, .premuta = -1,
                          .bersaglio = -1 };
    widget_aggiungi(&finestra.base, &menu.base);

    vai(quanti > 1 && argomenti[1][0] == '/' ? argomenti[1] : CASA, 1);
    return toolkit_esegui(&finestra);
}
