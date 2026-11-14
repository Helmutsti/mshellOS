/* Files: il gestore dei file, come il Finder del Mac o Nautilus di GNOME. Mostra il
 * contenuto di una cartella e permette di girare per il disco, come nel disegno Figma
 * (pagina "App - Files"). E' fatto solo di componenti di UIKit:
 *
 *   +--------------------------------------------------------------------+
 *   |                         Documenti                         (-) (x)  | barra del titolo
 *   |               [<][>] [::][=]  Home / Documenti      (Cerca in ...) | barra degli strumenti
 *   | Scorciatoie   +---------------------------------------------------+|
 *   |  Home         | Nome ^                     Tipo        Dimensione ||
 *   |  Scrivania    | [] Lavoro                  cartella     6 elementi||  il pannello chiaro
 *   | Filtri        | ...                                               ||  (pannello.h), con la
 *   |  Immagini     |                                                   ||  griglia o la tabella
 *   |  Testi        |                                                   ||
 *   |  Programmi    |                                                   ||
 *   | Tags          +---------------------------------------------------+|
 *   +--------------------------------------------------------------------+
 *
 *   - LA BARRA LATERALE (barra_laterale.h): le scorciatoie (la casa, la sua Scrivania) e i
 *     filtri, che mostrano della cartella solo le immagini, i testi o i programmi
 *     (riconosciuti dall'estensione del nome; i programmi dal permesso di esecuzione). Un
 *     filtro acceso si spegne con un altro clic. "Tags" e' solo il titolo, come nel disegno.
 *   - LA BARRA DEGLI STRUMENTI: i controlli cominciano dove finisce la barra laterale:
 *     indietro e avanti (la storia delle cartelle viste), icone o elenco, il percorso (un clic
 *     su un nome torna a quella cartella), la ricerca (mostra solo i file il cui nome contiene
 *     quello che si scrive).
 *   - IL CONTENUTO: la griglia di icone (griglia.h) o la tabella (tabella.h, colonne Nome,
 *     Tipo, Dimensione: un clic sull'intestazione ordina, la freccia dice il verso). Un clic
 *     sceglie un file, il doppio clic su una cartella la apre (gli altri file per ora non si
 *     aprono, tranne i pacchetti .app, che si chiedono al launcher: lui fa partire lo Store, o
 *     dice che le app vanno installate prima). Le icone sono gli oggetti di UIKit
 *     (oggetti.h), della variante piu' specifica (le cartelle con un ruolo, le estensioni).
 *   - IL TASTO DESTRO apre un menu (menu.h) dove si e' cliccato: su un file Apri, Rinomina,
 *     Duplica, Informazioni, Elimina; sul vuoto Nuova cartella, Ordina per (sottomenu),
 *     Mostra come icone o elenco. Rinominare (anche la cartella appena creata) si fa li', nel
 *     nome. Le informazioni si aprono in una mini finestra (informazioni.h).
 *   - LA TASTIERA (tasti.h): Invio apre (o conferma il nome nuovo), F2 rinomina, Ctrl+D
 *     duplica, Ctrl+I le informazioni, Canc elimina, Ctrl+Maiusc+N crea una cartella, Esc
 *     chiude il menu, annulla il nome nuovo o toglie la scelta.
 *   - GLI STATI (lo stato vuoto di UIKit): una cartella vuota, una che non si puo' leggere (un
 *     errore del disco, un percorso troppo lungo) e una che non si ha il permesso di aprire
 *     si dicono al centro del pannello.
 *
 * I file li legge e li cambia la libreria (apri, leggi, info, crea_cartella, rinomina,
 * cancella, cancella_tutto, copia_cartella), cioe' Linux: Files gira come la persona (uid 1000) e puo' quello che puo'
 * lei. Nella partizione data (la sua casa) scrive; /system e' in sola lettura, e le
 * cartelle del sistema non le puo' aprire. Quando qualcosa non riesce lo dice in basso,
 * in un avviso che sparisce alla prossima azione.
 *
 * LA CARTELLA DI PARTENZA e' quella che il launcher passa come argomento (vedi
 * apertura.h: aprire una cartella, anche dalla scrivania, vuol dire far partire Files su
 * di lei); senza, la casa (/home). */
#include "uikit/uikit.h"
#include "corekit/apertura.h"            /* aprire un pacchetto .app: lo chiede al launcher */

#define MAX_ELEMENTI 256
#define MAX_LUNGO    MAX_PERCORSO   /* un percorso in Files */
#define MAX_STORIA    32
#define CASA         OGGETTO_CASA

/* Le misure della finestra (il disegno di Figma e' 960 x 600: qui resta quella di prima, che sta
 * nella "scala" delle finestre sullo schermo da 1280 x 800). La struttura e' quella del disegno
 * (finestra a aree): il corpo ha 8 intorno e 8 tra le aree; a sinistra la barra laterale, a
 * tutta altezza; a destra una colonna con la barra degli strumenti e sotto il pannello. */
#define FINESTRA_LARGA 800
#define FINESTRA_ALTA  480
#define RICERCA_LARGA  200
#define MENU_FILES     220     /* la larghezza dei menu di Files, come nel disegno */

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

/* Che cosa e' un file, per i filtri e la colonna Tipo. */
enum { GENERE_CARTELLA, GENERE_FILE, GENERE_IMMAGINE, GENERE_TESTO, GENERE_PROGRAMMA };
static const char *const nomi_generi[] = { "cartella", "file", "immagine", "testo", "programma" };

struct elemento {
    char nome[MAX_NOME_FILE];
    unsigned int tipo;               /* TIPO_... (di un collegamento: quello a cui porta) */
    int genere;                      /* GENERE_... */
    int oggetto;                     /* l'icona: OGGETTO_... (oggetti.h) */
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

/* L'ordine: le colonne della tabella (Nome, Tipo, Dimensione) e la data di modifica. */
enum { PER_NOME, PER_TIPO, PER_DIMENSIONE, PER_DATA };
static int ordine = PER_NOME, verso = ORDINE_CRESCENTE;
enum { A_ICONE, A_ELENCO };
static int vista = A_ICONE;
static int filtro = -1;              /* GENERE_IMMAGINE, GENERE_TESTO, GENERE_PROGRAMMA, o -1 */

static const char *const estensioni_immagini[] = { "png", "jpg", "jpeg", "gif", "bmp", "webp", 0 };
static const char *const estensioni_testi[] = {
    "txt", "md", "c", "h", "py", "sh", "json", "csv", "log", "ini", "conf", "xml", "html",
    "css", "js", 0
};

/* L'estensione del nome (dopo l'ultimo punto, non all'inizio), o 0. */
static const char *estensione(const char *nome) {
    const char *punto = 0;
    for (const char *c = nome; *c; c++) if (*c == '.') punto = c;
    return punto && punto != nome && punto[1] ? punto + 1 : 0;
}

static int ha_estensione(const char *nome, const char *const *elenco) {
    const char *e = estensione(nome);
    if (!e) return 0;
    for (int i = 0; elenco[i]; i++)
        if (!confronta_nomi(e, elenco[i])) return 1;
    return 0;
}

/* Che file e': una cartella, un programma (un file che si puo' eseguire), un'immagine o un
 * testo (dall'estensione del nome), o un file qualunque. */
static int genere_di(const struct elemento *e) {
    if (e->tipo == TIPO_CARTELLA) return GENERE_CARTELLA;
    if (e->tipo == TIPO_FILE && (e->permessi & 0111)) return GENERE_PROGRAMMA;
    if (ha_estensione(e->nome, estensioni_immagini)) return GENERE_IMMAGINE;
    if (ha_estensione(e->nome, estensioni_testi)) return GENERE_TESTO;
    return GENERE_FILE;
}

/* Il percorso di un file della cartella 'cartella', in 'dove' (MAX_LUNGO); vuoto se e' troppo lungo. */
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
        e->oggetto = e->genere == GENERE_CARTELLA ? oggetto_di_cartella(percorso) : oggetto_di_file(e->nome);
    }
    chiudi(d);
}

/* L'ordine: prima le cartelle, poi gli altri; dentro, per la colonna scelta, nel verso
 * scelto (crescente: A-Z, dal piu' piccolo, dal piu' vecchio); a parita', per nome. */
static int prima_di(const struct elemento *a, const struct elemento *b) {
    int ca = a->genere == GENERE_CARTELLA, cb = b->genere == GENERE_CARTELLA;
    if (ca != cb) return ca;
    int c = 0;
    if (ordine == PER_TIPO) {
        c = confronta_nomi(nomi_generi[a->genere], nomi_generi[b->genere]);
    } else if (ordine == PER_DIMENSIONE) {
        unsigned long long da = ca ? (unsigned long long)(a->contenuti + 1) : a->dimensione;
        unsigned long long db = ca ? (unsigned long long)(b->contenuti + 1) : b->dimensione;
        c = da < db ? -1 : da > db;
    } else if (ordine == PER_DATA) {
        c = a->modificato < b->modificato ? -1 : a->modificato > b->modificato;
    }
    if (!c) c = confronta_nomi(a->nome, b->nome);
    return verso == ORDINE_DECRESCENTE ? c > 0 : c < 0;
}

static struct campo ricerca;

/* Chi si vede, e in che ordine: il filtro, la ricerca, poi l'ordine (per inserimento: i file
 * di una cartella sono pochi). */
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

/* Il posto in 'visibili' di un elemento, o -1 se non si vede. */
static int posto_visibile(int elemento) {
    for (int k = 0; k < quanti_visibili; k++) if (visibili[k] == elemento) return k;
    return -1;
}

/* --- le dimensioni, come le scrive il disegno --- */

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
    if (e->genere != GENERE_CARTELLA) { scrivi_byte(d, e->dimensione); return; }
    if (e->contenuti < 0) metti_testo(d, "--");
    else if (e->contenuti == 0) metti_testo(d, "vuota");
    else metti_testo(metti_numero(d, (unsigned long long)e->contenuti),
                     e->contenuti == 1 ? " elemento" : " elementi");
}

/* --- la finestra e i suoi pezzi --- */

static struct finestra_tk finestra;
static struct barra_strumenti strumenti;
static struct pillola frecce, viste;
static struct pulsante_barra indietro, avanti, a_icone, a_elenco;
static struct percorso percorso;
static struct contenitore corpo, area;
static struct barra_laterale_tk laterale;
static struct pannello_tk pannello;
static struct griglia_tk griglia;
static struct tabella_tk tabella;
static struct stato_vuoto stato;
static struct menu_tk menu;
static struct finestra_tk mini;          /* le informazioni di un file, nella mini finestra */
static struct informazioni_tk informazioni;

static void vai(const char *dove, int nella_storia);
static void aggiorna_contenuto(void);

/* LA SCELTA: l'elemento scelto e quello di cui si cambia il nome (indici in 'elementi', o -1).
 * La griglia e la tabella li vogliono come posti tra i visibili: sincronizza() glieli da'. */
static int scelto = -1, rinominato = -1;
static char nome_nuovo[MAX_NOME_FILE];

static void sincronizza(void) {
    int k = scelto >= 0 ? posto_visibile(scelto) : -1;
    int r = rinominato >= 0 ? posto_visibile(rinominato) : -1;
    griglia_scegli(&griglia, k);
    tabella_tk_scegli(&tabella, k);
    griglia_rinomina(&griglia, r, nome_nuovo);
    tabella_tk_rinomina(&tabella, r, nome_nuovo);
}

static void scegli(int e) {
    scelto = e;
    sincronizza();
}

/* LA STORIA: le cartelle viste, per "indietro" e "avanti", come in un browser. */
static char storia[MAX_STORIA][MAX_LUNGO];
static int quante_storia, posto_storia = -1;

static void aggiorna_frecce(void) {
    widget_disabilita(&indietro.base, posto_storia <= 0);
    widget_disabilita(&avanti.base, posto_storia >= quante_storia - 1);
}

/* L'AVVISO: quando qualcosa non riesce (un permesso che manca, un disco in
 * sola lettura), una pillola in basso nel pannello, fino alla prossima azione. Il disegno non
 * lo ha (e' una domanda aperta in STATO.md): resta quello di prima, con i colori del
 * suggerimento. */
static char avviso[96];
struct avviso_basso { struct widget base; };
static struct avviso_basso avviso_w;

static void avvisa(const char *testo) {
    copia_testo(avviso, testo, sizeof avviso);
    widget_cambiato(&avviso_w.base);
}

#define AVVISO_ALTEZZA ALTEZZA_S
#define AVVISO_DENTRO  PADDING_S
#define AVVISO_ICONA   ICONA_S
#define AVVISO_GAP     GAP_M
#define AVVISO_BASSO   SPAZIO_8     /* dal fondo dello spazio dentro il pannello: 8 + 8 = 16 dal bordo */

static void avviso_basso_disegna(struct widget *w, struct pennello *p) {
    if (!avviso[0]) return;
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

static const struct tipo_widget tipo_avviso_basso = { 0, 0, avviso_basso_disegna, 0, 0, 0, 0, 0, 0 };

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

static int voce_home, voce_scrivania, voce_immagini, voce_testi, voce_programmi;

static int laterale_selezionata(struct barra_laterale_tk *b, int voce, void *dati) {
    (void)b; (void)dati;
    if (voce == voce_home) return uguali(qui, CASA);
    if (voce == voce_scrivania) return uguali(qui, CASA "/Scrivania");
    if (voce == voce_immagini) return filtro == GENERE_IMMAGINE;
    if (voce == voce_testi) return filtro == GENERE_TESTO;
    if (voce == voce_programmi) return filtro == GENERE_PROGRAMMA;
    return 0;
}

static void laterale_clic(struct barra_laterale_tk *b, int voce, void *dati) {
    (void)b; (void)dati;
    avvisa("");
    if (voce == voce_home) { vai(CASA, 1); return; }
    if (voce == voce_scrivania) { vai(CASA "/Scrivania", 1); return; }
    int genere = voce == voce_immagini ? GENERE_IMMAGINE : voce == voce_testi ? GENERE_TESTO : GENERE_PROGRAMMA;
    filtro = filtro == genere ? -1 : genere;      /* un altro clic lo spegne */
    aggiorna_contenuto();
}

/* --- il contenuto: la griglia e la tabella prendono i dati da qui --- */

static void dati_tile(struct griglia_tk *g, int k, struct elemento_griglia_tk *e, void *dati) {
    (void)g; (void)dati;
    const struct elemento *el = &elementi[visibili[k]];
    copia_testo(e->nome, el->nome, MAX_CELLA);
    e->oggetto = el->oggetto;
}

/* Le colonne della tabella, come nel disegno: Nome (il resto), Tipo 120, Dimensione 100 a destra. */
static const struct colonna_tk colonne[] = {
    { "Nome",       0,   0, 1, 1 },
    { "Tipo",       120, 0, 0, 1 },
    { "Dimensione", 100, 1, 0, 1 },
};

static void dati_riga(struct tabella_tk *t, int k, struct riga_tk *r, void *dati) {
    (void)t; (void)dati;
    const struct elemento *el = &elementi[visibili[k]];
    copia_testo(r->testi[0], el->nome, MAX_CELLA);
    copia_testo(r->testi[1], nomi_generi[el->genere], MAX_CELLA);
    scrivi_dimensione(r->testi[2], el);
    r->oggetto = el->oggetto;
}

/* Perche' la cartella non si vede: vuota, permesso negato, errore, o tutto nascosto da un
 * filtro. Lo dice lo stato vuoto di UIKit, sopra la griglia (che resta, vuota, per il tasto
 * destro); la tabella in questi casi non si vede (il disegno mostra solo lo stato). */
static void aggiorna_stato_cartella(void) {
    int icona_stato;
    const char *titolo, *perche;
    int vuota = errore_qui || !quanti_visibili;
    widget_nascondi(&stato.base, !vuota);
    widget_nascondi(&griglia.base, vista != A_ICONE && !vuota);
    widget_nascondi(&tabella.base, vista != A_ELENCO || vuota);
    if (!vuota) return;
    if (errore_qui == ERRORE_PERMESSO) {
        icona_stato = ICONA_LOCK;
        titolo = "Non hai il permesso di aprire questa cartella";
        perche = proprietario_qui == UID_SISTEMA
                     ? "Appartiene al sistema: solo il sistema pu\x95 leggerla."
                     : "Appartiene a un altro utente: solo lui pu\x95 leggerla.";
    } else if (errore_qui) {
        icona_stato = ICONA_TRIANGLE_ALERT;
        titolo = "Impossibile leggere la cartella";
        perche = errore_qui == ERRORE_NOME ? "Il suo percorso \x8A troppo lungo per aprirla."
               : errore_qui == ERRORE_NON_TROVATO ? "Non c'\x8A pi\x97: forse \x8A stata spostata."
               : errore_qui == ERRORE_NON_CARTELLA ? "Non \x8A una cartella."
               : "Il disco non ha risposto. Riprova tra poco.";
    } else if (!quanti_elementi) {
        icona_stato = NESSUNA_ICONA;            /* come nel disegno: la cartella vuota non ha icona */
        titolo = "Questa cartella \x8A vuota";
        perche = "Usa il tasto destro per creare una nuova cartella.";
    } else {                                    /* ci sono file, ma il filtro o la ricerca li nascondono */
        icona_stato = ICONA_SEARCH;
        titolo = "Nessun file da mostrare";
        perche = "Nessun file di questa cartella corrisponde al filtro o alla ricerca.";
    }
    if (stato.icona != icona_stato || !uguali(stato.titolo, titolo) || !uguali(stato.spiegazione, perche))
        stato_vuoto_cambia(&stato, icona_stato, titolo, perche);
    /* come nel disegno: l'icona di un errore e' primaria, le altre secondarie */
    stato_vuoto_colore_icona(&stato, errore_qui ? COLORE_ICONA_PRIMARIO : COLORE_ICONA_SECONDARIO);
}

/* Il filtro, la ricerca o l'ordine sono cambiati, o la cartella e' stata riletta: si rifa'
 * chi si vede e dove. */
static void aggiorna_contenuto(void) {
    scegli_visibili();
    if (scelto >= 0 && posto_visibile(scelto) < 0) scelto = -1;
    tabella.ordinata = ordine == PER_DATA ? -1 : ordine;
    tabella.verso = verso;
    aggiorna_stato_cartella();
    griglia_aggiorna(&griglia, quanti_visibili);
    tabella_tk_aggiorna(&tabella, quanti_visibili);
    sincronizza();
    uikit_ridisponi(&finestra);
    widget_cambiato(&laterale.base);
}

/* --- cambiare i file: rinominare, creare, duplicare, eliminare --- */

/* Rilegge la cartella e sceglie di nuovo il file che si chiama 'nome' (se c'e'). */
static void rileggi(const char *nome) {
    leggi_cartella();
    scelto = rinominato = -1;
    if (nome)
        for (int i = 0; i < quanti_elementi; i++)
            if (uguali(elementi[i].nome, nome)) scelto = i;
    aggiorna_contenuto();
}

/* Un nome che non c'e' ancora nella cartella: 'base', poi "base 2", "base 3"... prima
 * dell'estensione ("foto copia.png", "foto copia 2.png"). */
static void nome_libero(char *dove, const char *base, const char *est) {
    if (lunghezza(est) > 64) est = "";                       /* un'estensione assurda: niente */
    int massimo = MAX_NOME_FILE - 1 - lunghezza(est) - 4;    /* il posto per " 999" e per lo 0 */
    for (int n = 1; n < 1000; n++) {
        char *d = dove;
        for (const char *s = base; *s && d - dove < massimo; s++) *d++ = *s;   /* un nome lungo si accorcia */
        if (n > 1) { *d++ = ' '; d = metti_numero(d, (unsigned long long)n); }
        metti_testo(d, est);
        char p[MAX_LUNGO];
        struct info_file i;
        percorso_di(p, qui, dove);
        if (!p[0] || info(p, &i) == ERRORE_NON_TROVATO) return;
    }
}

/* Il widget che mostra i file adesso (per dargli la tastiera). */
static struct widget *vista_attiva(void) { return vista == A_ICONE ? &griglia.base : &tabella.base; }

/* Comincia a rinominare l'elemento: il nome diventa un campo, con il nome di adesso. */
static void comincia_rinomina(int e) {
    if (e < 0) return;
    rinominato = scelto = e;
    copia_testo(nome_nuovo, elementi[e].nome, MAX_NOME_FILE);
    finestra_tk_dai_fuoco(&finestra, vista_attiva());   /* i tasti vanno al nome */
    sincronizza();
}

static void annulla_rinomina(void) {
    rinominato = -1;
    sincronizza();
}

/* Finisce di rinominare: se il nome e' cambiato (e va bene) il file si rinomina. */
static void finisci_rinomina(void) {
    int e = rinominato;
    if (e < 0) return;
    rinominato = -1;
    sincronizza();
    if (uguali(nome_nuovo, elementi[e].nome)) return;
    int va_bene = nome_nuovo[0] && !uguali(nome_nuovo, ".") && !uguali(nome_nuovo, "..");
    for (const char *k = nome_nuovo; *k; k++) if (*k == '/') va_bene = 0;
    if (!va_bene) { avvisa("Rinomina: il nome non va bene"); return; }
    char da[MAX_LUNGO], a[MAX_LUNGO];
    struct info_file i;
    percorso_di(da, qui, elementi[e].nome);
    percorso_di(a, qui, nome_nuovo);
    /* rinomina sostituirebbe un file che c'e' gia' con quel nome: prima si guarda */
    int errore = !a[0] ? ERRORE_NOME : info(a, &i) == 0 ? ERRORE_ESISTE : rinomina(da, a);
    if (errore < 0) { avvisa_errore("Rinomina", errore); return; }
    char nome[MAX_NOME_FILE];
    copia_testo(nome, nome_nuovo, MAX_NOME_FILE);
    rileggi(nome);
}

static void nuova_cartella(void) {
    if (errore_qui) return;
    char nome[MAX_NOME_FILE], p[MAX_LUNGO];
    nome_libero(nome, "Nuova cartella", "");
    percorso_di(p, qui, nome);
    int errore = p[0] ? crea_cartella(p) : ERRORE_NOME;
    if (errore < 0) { avvisa_errore("Nuova cartella", errore); return; }
    filtro = -1;                                 /* la cartella nuova deve vedersi */
    ricerca.testo[0] = 0;
    widget_cambiato(&ricerca.base);
    rileggi(nome);
    comincia_rinomina(scelto);                   /* come il Finder: si scrive subito il nome */
}

/* Duplica un file o una cartella: una copia accanto, "nome copia.est". Una cartella si copia con
 * tutto quello che ha dentro; il suo nome non ha estensione (come il Finder), anche se ha un punto. */
static void duplica(int e) {
    if (e < 0 || (elementi[e].tipo != TIPO_FILE && elementi[e].tipo != TIPO_CARTELLA)) return;
    int cartella = elementi[e].tipo == TIPO_CARTELLA;
    const char *nome = elementi[e].nome;
    char base[MAX_NOME_FILE], est[MAX_NOME_FILE] = "", copia[MAX_NOME_FILE];
    copia_testo(base, nome, MAX_NOME_FILE);
    int punto = -1;
    for (int i = 1; base[i] && !cartella; i++) if (base[i] == '.') punto = i;
    if (punto > 0) { copia_testo(est, base + punto, MAX_NOME_FILE); base[punto] = 0; }
    metti_testo(base + lunghezza(base), " copia");
    nome_libero(copia, base, est);
    char da[MAX_LUNGO], a[MAX_LUNGO];
    percorso_di(da, qui, nome);
    percorso_di(a, qui, copia);
    if (cartella) {
        int errore = a[0] ? copia_cartella(da, a) : ERRORE_NOME;
        if (errore < 0) { avvisa_errore("Duplica", errore); return; }
        rileggi(copia);
        return;
    }
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

/* Un file o una cartella vuota si eliminano subito. Una cartella con qualcosa dentro prima
 * chiede conferma (l'avviso di UIKit, come Store e Attivita'), dicendo quante cose ci sono
 * dentro, perche' non c'e' un cestino. Il disegno di Figma non ha questo avviso: il testo e' lo
 * stesso stile di quelli di Store. */
static struct avviso avviso_elimina;
static char da_eliminare[MAX_NOME_FILE];      /* la cartella su cui si e' chiesto */

static void elimina_adesso(const char *nome) {
    char p[MAX_LUNGO];
    percorso_di(p, qui, nome);
    int errore = p[0] ? cancella_tutto(p) : ERRORE_NOME;
    rileggi(0);                                  /* quello che non si e' tolto resta e si vede */
    if (errore < 0) avvisa_errore("Elimina", errore);
}

static void risposta_elimina(struct avviso *a, int scelta, void *dati) {
    (void)a; (void)dati;
    if (scelta == AVVISO_CONFERMA && da_eliminare[0]) elimina_adesso(da_eliminare);
    da_eliminare[0] = 0;
}

static void elimina(int e) {
    if (e < 0) return;
    char p[MAX_LUNGO];
    percorso_di(p, qui, elementi[e].nome);
    if (!p[0]) { avvisa_errore("Elimina", ERRORE_NOME); return; }
    unsigned long long dentro = 0;
    if (elementi[e].tipo == TIPO_CARTELLA && conta_dentro(p, &dentro) == 0 && dentro > 0) {
        char titolo[MAX_TESTO_WIDGET], testo[160], *d = metti_testo(titolo, "Eliminare \"");
        d = metti_testo(d, elementi[e].nome);
        metti_testo(d, "\"?");
        if (dentro == 1) d = metti_testo(testo, "Dentro c'\x8A 1 elemento. ");
        else {
            d = metti_testo(testo, "Dentro ci sono ");
            d = metti_testo(metti_numero(d, dentro), " elementi. ");
        }
        metti_testo(d, "Verranno eliminati per sempre: non si possono recuperare.");
        copia_testo(da_eliminare, elementi[e].nome, MAX_NOME_FILE);
        avviso_prepara(&avviso_elimina, titolo, testo, "Elimina", PERICOLO, 1, risposta_elimina, 0);
        finestra_tk_avviso(&finestra, &avviso_elimina);
        avviso_apri(&avviso_elimina);
        return;
    }
    elimina_adesso(elementi[e].nome);
}

/* Un pacchetto .app (un'app da installare, vedi pacchetto.h)? */
static const char *const estensioni_pacchetti[] = { "app", 0 };

static int e_pacchetto(int e) {
    return e >= 0 && elementi[e].tipo == TIPO_FILE && ha_estensione(elementi[e].nome, estensioni_pacchetti);
}

/* Si puo' aprire: le cartelle e i pacchetti (gli altri file non hanno ancora chi li apra). */
static int si_apre(int e) { return e >= 0 && (elementi[e].tipo == TIPO_CARTELLA || e_pacchetto(e)); }

/* Apre un elemento: una cartella ci si entra; un pacchetto .app lo si chiede al launcher,
 * che fa partire lo Store (o dice che le app vanno installate prima: apertura.h). */
static void apri_elemento(int e) {
    if (!si_apre(e)) return;
    char p[MAX_LUNGO];
    percorso_di(p, qui, elementi[e].nome);
    if (!p[0] || (e_pacchetto(e) && lunghezza(p) >= MAX_PERCORSO)) {
        avvisa("Il percorso \x8A troppo lungo");
        return;
    }
    if (elementi[e].tipo == TIPO_CARTELLA) { vai(p, 1); return; }
    int esito = apri_file(p, 0, 0);
    if (esito == APERTURA_SERVE_STORE) avvisa("Le app vanno installate prima di aprirle");
    else if (esito < 0) avvisa("Non si riesce ad aprire il pacchetto");
}

/* Il nome della cartella 'p' come lo dice il percorso: "Home" per la casa, "Disco" per la
 * radice, se no l'ultimo nome. */
static int dentro_casa(const char *p) {
    int n = lunghezza(CASA);
    for (int i = 0; i < n; i++) if (p[i] != CASA[i]) return 0;
    return p[n] == 0 || p[n] == '/';
}

static const char *nome_cartella(const char *p) {
    if (uguali(p, CASA)) return "Home";
    if (uguali(p, "/")) return "Disco";
    const char *ultimo = p;
    for (const char *c = p; *c; c++) if (*c == '/' && c[1]) ultimo = c + 1;
    return ultimo;
}

/* LE INFORMAZIONI di un file, nella mini finestra: il tipo, la dimensione, la posizione (come
 * nel disegno: "Tipo immagine: PNG"). */
static void mostra_informazioni(int e) {
    if (e < 0) return;
    const struct elemento *el = &elementi[e];
    informazioni_svuota(&informazioni);
    char etichetta[MAX_TESTO_WIDGET], valore[MAX_CELLA];
    const char *est = el->genere != GENERE_CARTELLA ? estensione(el->nome) : 0;
    if (est && lunghezza(est) < 8) {
        metti_testo(metti_testo(etichetta, "Tipo "), nomi_generi[el->genere]);
        int i = 0;
        for (; est[i]; i++) valore[i] = est[i] >= 'a' && est[i] <= 'z' ? (char)(est[i] - 'a' + 'A') : est[i];
        valore[i] = 0;
    } else {
        metti_testo(etichetta, "Tipo");
        metti_testo(valore, nomi_generi[el->genere]);
    }
    informazioni_riga(&informazioni, etichetta, valore);
    scrivi_dimensione(valore, el);
    informazioni_riga(&informazioni, "Dimensione", valore);
    informazioni_riga(&informazioni, "Posizione", nome_cartella(qui));
    char titolo[MAX_TESTO_WIDGET + MAX_CELLA];
    char *d = metti_testo(titolo, "Informazioni - ");
    copia_testo(d, el->nome, MAX_CELLA);
    informazioni_mostra(&finestra, &mini, &informazioni, titolo);
}

/* --- il menu del tasto destro --- */

enum {
    APRI, RINOMINA, DUPLICA, INFORMAZIONI, ELIMINA,
    NUOVA_CARTELLA, ORDINA, MOSTRA_ICONE, MOSTRA_ELENCO,
    ORDINA_NOME, ORDINA_TIPO, ORDINA_DIMENSIONE, ORDINA_DATA,
};

/* Il menu del file, come nel disegno ("Menu del file"): voci S, senza simboli. */
static const struct voce_menu_tk menu_file[] = {
    { APRI,         "Apri",         NESSUNA_ICONA, "Invio",  0, 0 },
    { RINOMINA,     "Rinomina",     NESSUNA_ICONA, "F2",     0, 0 },
    { DUPLICA,      "Duplica",      NESSUNA_ICONA, "Ctrl+D", 0, 0 },
    { INFORMAZIONI, "Informazioni", NESSUNA_ICONA, "Ctrl+I", 0, 0 },
    { MENU_VOCE_SEPARATORE, 0, NESSUNA_ICONA, 0, 0, 0 },
    { ELIMINA,      "Elimina",      NESSUNA_ICONA, "Canc",   0, 0 },
};
static const struct voce_menu_tk menu_ordina[] = {
    { ORDINA_NOME,       "Nome",             NESSUNA_ICONA, 0, 0, 0 },
    { ORDINA_TIPO,       "Tipo",             NESSUNA_ICONA, 0, 0, 0 },
    { ORDINA_DIMENSIONE, "Dimensione",       NESSUNA_ICONA, 0, 0, 0 },
    { ORDINA_DATA,       "Data di modifica", NESSUNA_ICONA, 0, 0, 0 },
};
#define QUANTE(v) ((int)(sizeof v / sizeof v[0]))
/* Il menu dello spazio vuoto: non e' nel disegno; e' fatto come quello del file. */
static const struct voce_menu_tk menu_cartella[] = {
    { NUOVA_CARTELLA, "Nuova cartella",     NESSUNA_ICONA, "Ctrl+Maiusc+N", 0, 0 },
    { MENU_VOCE_SEPARATORE, 0, NESSUNA_ICONA, 0, 0, 0 },
    { ORDINA,         "Ordina per",         NESSUNA_ICONA, 0, menu_ordina, QUANTE(menu_ordina) },
    { MENU_VOCE_SEPARATORE, 0, NESSUNA_ICONA, 0, 0, 0 },
    { MOSTRA_ICONE,   "Mostra come icone",  NESSUNA_ICONA, 0, 0, 0 },
    { MOSTRA_ELENCO,  "Mostra come elenco", NESSUNA_ICONA, 0, 0, 0 },
};

static int bersaglio = -1;           /* l'elemento su cui si e' aperto il menu (o -1) */

/* Apri solo le cartelle e i pacchetti; Duplica file e cartelle; Nuova cartella solo dove si legge. */
static int voce_attiva(int azione, void *dati) {
    (void)dati;
    if (azione == APRI) return si_apre(bersaglio);
    if (azione == DUPLICA)
        return bersaglio >= 0 && (elementi[bersaglio].tipo == TIPO_FILE || elementi[bersaglio].tipo == TIPO_CARTELLA);
    if (azione == NUOVA_CARTELLA) return !errore_qui;
    return 1;
}

/* La spunta: la vista e l'ordine di adesso. */
static int voce_spuntata(int azione, void *dati) {
    (void)dati;
    return (azione == MOSTRA_ICONE && vista == A_ICONE) || (azione == MOSTRA_ELENCO && vista == A_ELENCO) ||
           (azione == ORDINA_NOME && ordine == PER_NOME) || (azione == ORDINA_TIPO && ordine == PER_TIPO) ||
           (azione == ORDINA_DIMENSIONE && ordine == PER_DIMENSIONE) || (azione == ORDINA_DATA && ordine == PER_DATA);
}

static void cambia_vista(int nuova);

static void esegui_voce(int azione) {
    int e = bersaglio;
    switch (azione) {
    case APRI:           apri_elemento(e); break;
    case RINOMINA:       comincia_rinomina(e); break;
    case DUPLICA:        duplica(e); break;
    case INFORMAZIONI:   mostra_informazioni(e); break;
    case ELIMINA:        elimina(e); break;
    case NUOVA_CARTELLA: nuova_cartella(); break;
    case MOSTRA_ICONE:   cambia_vista(A_ICONE); break;
    case MOSTRA_ELENCO:  cambia_vista(A_ELENCO); break;
    case ORDINA_NOME: case ORDINA_TIPO: case ORDINA_DIMENSIONE: case ORDINA_DATA:
        ordine = azione == ORDINA_NOME ? PER_NOME : azione == ORDINA_TIPO ? PER_TIPO
               : azione == ORDINA_DIMENSIONE ? PER_DIMENSIONE : PER_DATA;
        verso = ORDINE_CRESCENTE;
        aggiorna_contenuto();
        break;
    }
}

static void scelta_menu(struct menu_tk *m, int azione, void *dati) {
    (void)m; (void)dati;
    esegui_voce(azione);
}

/* --- i clic e i tasti sul contenuto (la griglia o la tabella) --- */

/* L'elemento nel punto (x, y) della vista attiva, o -1. */
static int elemento_in(int x, int y) {
    int k = vista == A_ICONE || (!griglia.base.nascosto && tabella.base.nascosto)
                ? griglia_in(&griglia, x, y) : tabella_tk_riga_in(&tabella, x, y);
    return k >= 0 ? visibili[k] : -1;
}

static void clic_contenuto(int k, int come, int x, int y) {
    int e = k >= 0 ? visibili[k] : -1;
    if (avviso[0]) avvisa("");
    if (rinominato >= 0 && e != rinominato) {       /* clic altrove: il nome va bene cosi' */
        finisci_rinomina();
        e = elemento_in(x, y);                       /* la cartella puo' essere stata riletta */
    }
    if (rinominato >= 0) return;                     /* clic nel campo */
    scegli(e);
    if (come == CLIC_DESTRO) {
        bersaglio = e;
        if (e >= 0) menu_apri(&menu, menu_file, QUANTE(menu_file), x, y);
        else if (!errore_qui) menu_apri(&menu, menu_cartella, QUANTE(menu_cartella), x, y);
    } else if (come == CLIC_DOPPIO) {
        apri_elemento(e);
    }
}

static void clic_griglia(struct griglia_tk *g, int k, int come, int x, int y, void *dati) {
    (void)g; (void)dati;
    clic_contenuto(k, come, x, y);
}

static void clic_tabella(struct tabella_tk *t, int k, int come, int x, int y, void *dati) {
    (void)t; (void)dati;
    clic_contenuto(k, come, x, y);
}

/* Un clic sull'intestazione della tabella: l'ordine e il verso li ha gia' cambiati lei. */
static void ordine_tabella(struct tabella_tk *t, void *dati) {
    (void)dati;
    ordine = t->ordinata;
    verso = t->verso;
    aggiorna_contenuto();
}

/* I caratteri: mentre si rinomina vanno nel nome (Invio conferma); se no Invio apre. */
static void tasto_contenuto(unsigned int tasto) {
    if (rinominato >= 0) {
        int n = lunghezza(nome_nuovo);
        if (tasto == '\n') { finisci_rinomina(); return; }
        if (tasto == '\b') { if (n) nome_nuovo[n - 1] = 0; }
        else if (tasto >= ' ' && tasto != 0x7F && tasto != '/' && n < MAX_NOME_FILE - 1) {
            nome_nuovo[n] = (char)tasto;
            nome_nuovo[n + 1] = 0;
        } else return;
        sincronizza();
        return;
    }
    if (tasto == '\n') apri_elemento(scelto);
}

static void tasto_griglia(struct griglia_tk *g, unsigned int tasto, void *dati) { (void)g; (void)dati; tasto_contenuto(tasto); }
static void tasto_tabella(struct tabella_tk *t, unsigned int tasto, void *dati) { (void)t; (void)dati; tasto_contenuto(tasto); }

/* LE SCORCIATOIE, quelle scritte nei menu (Invio arriva come carattere, qui sopra). Mentre si
 * scrive nella ricerca valgono solo quelle con Ctrl. */
static int scorciatoie(struct finestra_tk *f, unsigned int codice, unsigned int mod, void *dati) {
    (void)f; (void)dati;
    int nella_ricerca = finestra.con_fuoco == &ricerca.base;
    if (codice == TASTO_N && mod == (MOD_CTRL | MOD_MAIUSC)) { avvisa(""); nuova_cartella(); return 1; }
    if (codice == TASTO_ESC && !mod) {
        if (rinominato >= 0) { annulla_rinomina(); return 1; }
        if (nella_ricerca) return 0;
        avvisa("");
        scegli(-1);
        return 1;
    }
    if (rinominato >= 0) return 0;                    /* si sta scrivendo un nome */
    if (codice == TASTO_D && mod == MOD_CTRL) { avvisa(""); duplica(scelto); return 1; }
    if (codice == TASTO_I && mod == MOD_CTRL) { mostra_informazioni(scelto); return 1; }
    if (nella_ricerca) return 0;
    if (codice == TASTO_F2 && !mod) { avvisa(""); comincia_rinomina(scelto); return 1; }
    if (codice == TASTO_CANC && !mod) { avvisa(""); elimina(scelto); return 1; }
    return 0;
}

/* --- andare in una cartella --- */

/* Il percorso nella barra: dentro la casa, "Home" e le cartelle; fuori, "Disco" (la radice)
 * e le cartelle. Se sono troppe, si tengono le ultime. */
static int primo_nome;   /* quanti nomi del percorso intero non si vedono, all'inizio */

/* I nomi del percorso 'p' in 'nomi' (puntatori dentro 'copia'): quanti sono. */
static int nomi_del_percorso(const char *p, char *copia, const char **nomi, int massimo) {
    int casa = dentro_casa(p), n = 0;
    copia_testo(copia, casa ? p + lunghezza(CASA) : p, MAX_LUNGO);
    nomi[n++] = casa ? "Home" : "Disco";
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
    percorso_metti(&percorso, nomi + primo_nome, n - primo_nome);

    /* il titolo della finestra e il suggerimento della ricerca: il nome della cartella */
    const char *ultimo = nomi[n - 1];
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
    menu_chiudi(&menu);
    if (rinominato >= 0) finisci_rinomina();
    copia_testo(qui, dove, MAX_LUNGO);
    int n = lunghezza(qui);
    while (n > 1 && qui[n - 1] == '/') qui[--n] = 0;   /* senza la barra in fondo */
    if (nella_storia && posto_storia >= 0 && uguali(storia[posto_storia], qui)) nella_storia = 0;
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
    scelto = rinominato = -1;
    griglia.barra.scorrimento.posizione = 0;
    tabella.barra.scorrimento.posizione = 0;
    aggiorna_contenuto();
    aggiorna_percorso();
    aggiorna_frecce();
    uikit_ridisponi(&finestra);
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

/* Icone o elenco (dal gruppo o dal menu): si accende il pulsante giusto, si torna in cima. */
static void cambia_vista(int nuova) {
    vista = nuova;
    pulsante_barra_accendi(nuova == A_ICONE ? &a_icone : &a_elenco, 1);
    griglia.barra.scorrimento.posizione = 0;
    tabella.barra.scorrimento.posizione = 0;
    aggiorna_contenuto();
}

static void premi_vista(struct pulsante_barra *b, void *dati) {
    (void)dati;
    cambia_vista(b == &a_icone ? A_ICONE : A_ELENCO);
}

static void ricerca_cambiata(struct campo *c, void *dati) {
    (void)c; (void)dati;
    griglia.barra.scorrimento.posizione = 0;
    tabella.barra.scorrimento.posizione = 0;
    aggiorna_contenuto();
}

int main(int quanti, char **argomenti) {
    /* la barra degli strumenti, come nel disegno: sopra il pannello, indietro e avanti, icone
     * o elenco, il percorso (prende il resto), la ricerca */
    barra_strumenti_prepara(&strumenti);
    pillola_prepara(&frecce, 0);
    pulsante_barra_prepara(&indietro, ICONA_CHEVRON_LEFT, premi_indietro, 0);
    pulsante_barra_prepara(&avanti, ICONA_CHEVRON_RIGHT, premi_avanti, 0);
    pillola_aggiungi(&frecce, &indietro);
    pillola_aggiungi(&frecce, &avanti);
    pillola_prepara(&viste, 1);
    pulsante_barra_prepara(&a_icone, ICONA_LAYOUT_GRID, premi_vista, 0);
    pulsante_barra_prepara(&a_elenco, ICONA_LIST, premi_vista, 0);
    pillola_aggiungi(&viste, &a_icone);
    pillola_aggiungi(&viste, &a_elenco);
    widget_suggerimento(&indietro.base, "Indietro", 0);
    widget_suggerimento(&avanti.base, "Avanti", 0);
    widget_suggerimento(&a_icone.base, "Vista a icone", 0);
    widget_suggerimento(&a_elenco.base, "Vista a elenco", 0);
    pulsante_barra_accendi(&a_icone, 1);
    percorso_prepara(&percorso, clic_percorso, 0);
    ricerca_prepara(&ricerca, "Cerca in Home", ricerca_cambiata, 0);
    ricerca.larghezza = RICERCA_LARGA;
    barra_strumenti_aggiungi(&strumenti, &frecce.base);
    barra_strumenti_aggiungi(&strumenti, &viste.base);
    barra_strumenti_aggiungi(&strumenti, &percorso.base);
    barra_strumenti_aggiungi(&strumenti, &ricerca.base);

    /* la barra laterale, come nel disegno */
    barra_laterale_prepara(&laterale, laterale_selezionata, laterale_clic, 0);
    barra_laterale_intestazione(&laterale, "Scorciatoie");
    voce_home = barra_laterale_voce(&laterale, "Home", ICONA_HOUSE);
    voce_scrivania = barra_laterale_voce(&laterale, "Scrivania", ICONA_MONITOR);
    barra_laterale_intestazione(&laterale, "Filtri");
    voce_immagini = barra_laterale_voce(&laterale, "Immagini", ICONA_IMAGE);
    voce_testi = barra_laterale_voce(&laterale, "Testi", ICONA_FILE_TEXT);
    voce_programmi = barra_laterale_voce(&laterale, "Programmi", ICONA_APP_WINDOW);
    barra_laterale_intestazione(&laterale, "Tags");

    /* il pannello (padding 8: le righe e gli elementi hanno il loro fondo con 8 dentro): la
     * griglia o la tabella, lo stato vuoto sopra, l'avviso sopra a tutto */
    pannello_prepara(&pannello, SPAZIO_8);
    griglia_prepara(&griglia, dati_tile, clic_griglia, 0);
    griglia.al_tasto = tasto_griglia;
    tabella_tk_prepara(&tabella, colonne, QUANTE(colonne), dati_riga, clic_tabella, 0);
    tabella.al_ordine = ordine_tabella;
    tabella.al_tasto = tasto_tabella;
    stato_vuoto_prepara(&stato, NESSUNA_ICONA, "", "");
    avviso_w = (struct avviso_basso){ .base = { .tipo = &tipo_avviso_basso } };
    widget_aggiungi(&pannello.base, &griglia.base);
    widget_aggiungi(&pannello.base, &tabella.base);
    widget_aggiungi(&pannello.base, &stato.base);
    widget_aggiungi(&pannello.base, &avviso_w.base);

    /* il corpo: la barra laterale e, a destra, la barra degli strumenti sopra il pannello (a
     * CORPO_GAP l'una dall'altro); il padding intorno e' quello della finestra a aree */
    colonna_prepara(&area, CORPO_GAP);
    area.base.elastico = area.base.largo = 1;
    widget_aggiungi(&area.base, &strumenti.base);
    widget_aggiungi(&area.base, &pannello.base);
    riga_prepara(&corpo, CORPO_GAP);
    widget_aggiungi(&corpo.base, &laterale.base);
    widget_aggiungi(&corpo.base, &area.base);

    finestra_tk_prepara(&finestra, "Files", &corpo.base);
    finestra_tk_aree(&finestra);
    finestra_tk_dimensione(&finestra, FINESTRA_LARGA, FINESTRA_ALTA);
    finestra_tk_ascolta_tasti(&finestra, scorciatoie, 0);

    /* il menu, sopra tutto (l'ultimo figlio della finestra); le informazioni, a parte */
    menu_prepara(&menu, MISURA_S, MENU_FILES, voce_attiva, voce_spuntata, scelta_menu, 0);
    menu_aggiungi(&finestra, &menu);
    informazioni_prepara(&mini, &informazioni);

    vai(quanti > 1 && argomenti[1][0] == '/' ? argomenti[1] : CASA, 1);
    return uikit_esegui(&finestra);
}
