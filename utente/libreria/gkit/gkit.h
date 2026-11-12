/* GKit, la base della grafica (vedi LIBRERIE.md): come la parte bassa di Quartz sul Mac o
 * libwayland su Linux. Non sa niente di stile, temi, font e icone del sistema, componenti:
 * GKit sa COME si disegna e si mostra, UIKit (utente/libreria/uikit/) sa COSA. Gli strumenti
 * per disegnare su una tela stanno accanto, in gkit_disegno.h. Un programma che disegna da
 * solo pixel per pixel (Doom) usa solo GKit.
 *
 * Tre parti, una dopo l'altra:
 *   1. IL PROTOCOLLO DELLE FINESTRE: cosa si dicono un'applicazione e il compositore
 *      (lo usano tutti e due);
 *   2. LO SCHERMO, il lato del compositore (le funzioni stanno in schermo.c, qui accanto):
 *      lo usa solo chi possiede lo schermo, cioe' il compositore (e init, che lo apre per
 *      lui);
 *   3. LA FINESTRA, il lato dell'applicazione: apre la finestra, da' una tela libera su
 *      cui disegnare, la fa mostrare, consegna gli eventi. */
#ifndef GKIT_H
#define GKIT_H

#include "libreria.h"

/* =====================================================================================
 * 1. IL PROTOCOLLO DELLE FINESTRE
 * =====================================================================================
 *
 * E' il "Wayland ridotto all'essenziale" di mkernel. Le idee sono quelle di Wayland:
 *   - l'applicazione disegna DA SOLA la sua finestra, in un foglio di memoria condivisa
 *     che passa al compositore; il compositore non disegna niente dentro le finestre;
 *   - il foglio contiene DUE immagini: mentre il compositore mostra una, l'applicazione
 *     disegna nell'altra. Con MOSTRA la passa al compositore, che quando non gli serve
 *     piu' quella di prima la restituisce con RILASCIATA. Cosi' non si vede mai una
 *     finestra disegnata a meta';
 *   - le coordinate del puntatore sono INTERNE alla finestra: l'applicazione non sa
 *     dov'e' sullo schermo e non vede le altre finestre;
 *   - la finestra e' TUTTA dell'applicazione, barra del titolo e bordo compresi: e'
 *     lei a decidere il suo aspetto (come su Mac e in GNOME, le "decorazioni lato
 *     client"). Se usa UIKit, sara' lui a disegnarle con lo stile del sistema;
 *   - a spostarla pero' e' il compositore, perche' l'applicazione non sa dov'e' sullo
 *     schermo: quando si preme sulla sua barra, lei gli dice TRASCINAMI e lui la sposta
 *     seguendo il mouse finche' il pulsante resta premuto;
 *   - un messaggio sbagliato, e il compositore la scollega;
 *   - ogni tanto il compositore chiede "ci sei?" (PING) e l'applicazione risponde
 *     "ci sono" (PONG). Lo fa la libreria delle finestre, da sola, ogni volta che
 *     l'applicazione aspetta le sue notizie. Se la risposta non arriva, l'applicazione
 *     e' bloccata: il compositore ne ridisegna la barra in rosso, con il pulsante di
 *     chiusura d'emergenza. Come GNOME e il Mac.
 *
 * I RUOLI SPECIALI (solo per i programmi del sistema, uid 0: il compositore lo chiede al
 * kernel con chi). Oltre alle finestre normali ci sono superfici che stanno in uno strato
 * fisso, e che sceglie la scrivania (come layer-shell di Wayland):
 *
 *     PANNELLO   sopra tutte le finestre (la userbar della scrivania, il vassoio delle app)
 *     finestre   le applicazioni, nel mezzo: si spostano e si portano davanti
 *     SFONDO     sotto tutto
 *
 * Un pannello e' di due tipi, che il compositore distingue solo per l'ombra intorno:
 * PANNELLO_BARRA (una barra che sta sempre li', come la userbar) e PANNELLO_SOPRA (si apre
 * sopra il resto e poi si richiude, come un menu o il vassoio delle app).
 *
 * Una superficie speciale sta dove dice lei (x, y), non si trascina e non prende il fuoco
 * della tastiera. A un PANNELLO il compositore racconta anche quali finestre ci sono:
 * FINESTRA_APERTA e FINESTRA_CHIUSA, con il programma a cui appartengono (come
 * foreign-toplevel di Wayland). Cosi' la scrivania sa quali applicazioni sono aperte.
 * Le racconta anche quale finestra ha il fuoco (FINESTRA_ATTIVA), e un pannello puo'
 * chiedere di riportarne una davanti (PORTA_DAVANTI), anche se era stata ridotta.
 *
 * IL "RIDUCI". Un'applicazione chiede RIDUCIMI (il pulsante nella sua barra): il
 * compositore nasconde la finestra, che non si vede e non sente il puntatore finche'
 * la scrivania non la fa tornare con PORTA_DAVANTI.
 *
 * Una finestra per collegamento: per due finestre, due collegamenti.
 * Ogni messaggio comincia con il suo tipo. */

#define NOME_COMPOSITORE "compositore"   /* il nome su cui ascolta */
#define MAX_TITOLO 32

/* I pixel nelle immagini: 4 byte ciascuno, 0xAARRGGBB PREMOLTIPLICATI (AA e' l'opacita',
 * e i colori sono gia' moltiplicati per lei: vedi gkit_disegno.h), come ARGB8888 di Wayland.
 * Riga dopo riga, senza spazi tra una riga e l'altra. Il compositore mescola ogni pixel
 * con quello che c'e' sotto: una finestra puo' essere di vetro, e i suoi angoli
 * arrotondati sono trasparenti. L'immagine k sta nel foglio a k * larghezza * altezza * 4. */

/* --- dall'applicazione al compositore --- */

#define VOGLIO_FINESTRA 1   /* il primo messaggio, con il foglio allegato */
#define MOSTRA          2   /* "l'immagine 'immagine' e' pronta: mostrala" */
#define TRASCINAMI      3   /* "hanno premuto sulla mia barra: spostami tu". Vale solo
                             * mentre il pulsante premuto dentro la finestra e' ancora
                             * giu'; se e' gia' stato lasciato, il compositore la ignora */
#define PONG            4   /* "ci sono": la risposta al PING 'numero' */
#define CHIEDI_SCHERMO  5   /* "quanto e' grande lo schermo?": risponde SCHERMO. Si puo'
                             * chiedere anche prima di VOGLIO_FINESTRA */
#define RIDUCIMI        6   /* "nascondimi": hanno premuto il mio pulsante riduci */
#define VOGLIO_TASTI_FISICI 8  /* "mandami anche TASTO_FISICO": si chiede una volta (facoltativo,
                             * UIKit non lo fa; serve ai giochi) */
#define PORTA_DAVANTI   7   /* (solo un PANNELLO) la finestra 'numero' torna visibile,
                             * davanti a tutte, con il fuoco */

#define RUOLO_FINESTRA  0   /* una finestra normale */
#define RUOLO_SFONDO    1   /* sotto tutto (solo il sistema) */
#define RUOLO_PANNELLO  2   /* sopra tutte le finestre (solo il sistema) */
#define PANNELLO_BARRA  0   /* (solo un PANNELLO) una barra, sempre li' */
#define PANNELLO_SOPRA  1   /* (solo un PANNELLO) si apre sopra il resto, come un menu */
#define RAGGIO_PREDEFINITO 12

struct voglio_finestra {
    unsigned int tipo;
    unsigned int larghezza, altezza;   /* tutta la finestra, barra compresa */
    char titolo[MAX_TITOLO];
    unsigned int ruolo;                /* RUOLO_... */
    int x, y;                          /* dove sta: solo per i ruoli speciali */
    unsigned int raggio;               /* gli angoli arrotondati: li usa il compositore per
                                        * l'ombra e il vetro smerigliato sotto */
    unsigned int pannello;             /* PANNELLO_...: solo per un PANNELLO */
};

struct chiedi_schermo {
    unsigned int tipo;
};

struct voglio_tasti_fisici {
    unsigned int tipo;
};

struct riducimi {
    unsigned int tipo;
};

struct porta_davanti {
    unsigned int tipo;
    unsigned int numero;
};

/* LE ZONE CAMBIATE. Con MOSTRA l'applicazione dice anche DOVE l'immagine nuova e' diversa da
 * quella che il compositore mostrava prima: al massimo MAX_ZONE_MOSTRA rettangoli, in pixel
 * della finestra (0 rettangoli: tutta la finestra, come la prima volta). Il compositore
 * ricompone solo li' (come wl_surface.damage di Wayland): se una riga cambia colore sotto il
 * puntatore, non si rifa' tutta la finestra con la sua ombra e il suo vetro smerigliato.
 * Fuori dalle zone l'immagine nuova DEVE essere uguale a quella di prima: e' l'applicazione
 * a garantirlo (finestra_arretrate, qui sotto, la aiuta con le due immagini). */
#define MAX_ZONE_MOSTRA 8

struct zona_finestra {
    int x, y, l, a;                    /* x <= ... < x + l, y <= ... < y + a */
};

struct mostra {
    unsigned int tipo;
    unsigned int immagine;             /* 0 o 1 */
    unsigned int quante;               /* le zone cambiate; 0: tutta la finestra */
    struct zona_finestra zone[MAX_ZONE_MOSTRA];
};

/* --- i rettangoli: un elenco di zone che non si sovrappongono troppo ---
 * Lo usano l'applicazione (le zone da ridisegnare e da mandare) e il compositore. Si aggiunge
 * un rettangolo alla volta: se tocca una zona gia' nell'elenco, o se unirle costa poco in piu'
 * (meno di un quarto della loro area), le due diventano il rettangolo che le contiene, e si
 * ricontrolla; se l'elenco e' pieno, tutto diventa un solo rettangolo. Due zone che si
 * sovrappongono vorrebbero dire disegnare due volte gli stessi pixel. */
static inline int zona_vuota(struct zona_finestra z) { return z.l <= 0 || z.a <= 0; }

static inline struct zona_finestra zona_unione(struct zona_finestra p, struct zona_finestra q) {
    int x0 = p.x < q.x ? p.x : q.x, y0 = p.y < q.y ? p.y : q.y;
    int x1 = p.x + p.l > q.x + q.l ? p.x + p.l : q.x + q.l;
    int y1 = p.y + p.a > q.y + q.a ? p.y + p.a : q.y + q.a;
    return (struct zona_finestra){ x0, y0, x1 - x0, y1 - y0 };
}

/* La parte comune (vuota se non si toccano). */
static inline struct zona_finestra zona_dentro(struct zona_finestra p, struct zona_finestra q) {
    int x0 = p.x > q.x ? p.x : q.x, y0 = p.y > q.y ? p.y : q.y;
    int x1 = p.x + p.l < q.x + q.l ? p.x + p.l : q.x + q.l;
    int y1 = p.y + p.a < q.y + q.a ? p.y + p.a : q.y + q.a;
    if (x1 <= x0 || y1 <= y0) return (struct zona_finestra){ 0, 0, 0, 0 };
    return (struct zona_finestra){ x0, y0, x1 - x0, y1 - y0 };
}

static inline void zone_aggiungi(struct zona_finestra *zone, int *quante, int massimo,
                                 struct zona_finestra z) {
    if (zona_vuota(z)) return;
    for (int k = 0; k < *quante; k++) {
        struct zona_finestra u = zona_unione(z, zone[k]);
        long long piu = (long long)u.l * u.a - (long long)z.l * z.a - (long long)zone[k].l * zone[k].a;
        int tocca = !zona_vuota(zona_dentro(z, zone[k]));
        if (tocca || piu * 4 <= (long long)z.l * z.a + (long long)zone[k].l * zone[k].a) {
            z = u;
            zone[k] = zone[--*quante];
            k = -1;                    /* la nuova, piu' grande, puo' toccarne altre */
        }
    }
    if (*quante == massimo) {
        for (int k = 0; k < *quante; k++) z = zona_unione(z, zone[k]);
        *quante = 0;
    }
    zone[(*quante)++] = z;
}

struct trascinami {
    unsigned int tipo;
};

struct pong {
    unsigned int tipo;
    unsigned int numero;
};

/* --- dal compositore all'applicazione --- */

#define RILASCIATA 1   /* l'immagine 'immagine' e' tornata tua: ci puoi ridisegnare */
#define PUNTATORE  2   /* il puntatore e' in x, y (dentro la finestra), con 'pulsanti'
                        * premuti (PULSANTE_... di chiamate.h) */
#define USCITO     3   /* il puntatore e' uscito dalla finestra */
#define TASTO      4   /* e' stato premuto il tasto 'tasto' (code page 437) */
#define FUOCO      5   /* 'fuoco' = 1: i tasti ora vengono a te; 0: non piu' */
#define PING       6   /* "ci sei?": rispondi PONG con lo stesso 'numero' */
#define FINESTRA_APERTA 7   /* (solo a un PANNELLO) e' comparsa la finestra 'numero', del
                             * programma 'testo' (il suo percorso sul disco) */
#define FINESTRA_CHIUSA 8   /* (solo a un PANNELLO) la finestra 'numero' non c'e' piu' */
#define SCHERMO    9   /* la risposta a CHIEDI_SCHERMO: lo schermo e' x per y pixel */
#define FINESTRA_ATTIVA 10  /* (solo a un PANNELLO) ora ha il fuoco la finestra 'numero'
                             * (0: nessuna) */
#define ROTELLA   11   /* la rotella del mouse ha girato di 'scatti_y' scatti (positivo: in
                        * basso, il contenuto va su), e 'scatti_x' quella orizzontale
                        * (positivo: a destra), con il puntatore in x, y. Va alla finestra
                        * sotto il puntatore, anche se non ha il fuoco: come sul Mac e in
                        * Wayland, si scorre quello che si guarda */
#define TASTO_FISICO 12  /* (solo a chi l'ha chiesto con VOGLIO_TASTI_FISICI) il tasto
                          * Linux 'tasto' (KEY_...) e' stato premuto ('premuto' = 1) o
                          * lasciato (0). Senza ripetizioni. Arriva in piu' di TASTO. */

struct notizia {
    unsigned int tipo;
    unsigned int immagine;   /* RILASCIATA */
    int x, y;                /* PUNTATORE, ROTELLA */
    unsigned int pulsanti;   /* PUNTATORE */
    unsigned int tasto;      /* TASTO */
    unsigned int fuoco;      /* FUOCO */
    unsigned int numero;     /* PING, FINESTRA_APERTA, FINESTRA_CHIUSA */
    char testo[MAX_PERCORSO_PROGRAMMA];   /* FINESTRA_APERTA */
    int scatti_x, scatti_y;  /* ROTELLA */
    unsigned int premuto;    /* TASTO_FISICO (il codice e' in 'tasto') */
};

/* =====================================================================================
 * 2. LO SCHERMO, il lato del compositore (i tipi info_schermo e zona_schermo sono in
 *    chiamate.h; le funzioni in schermo.c)
 * ===================================================================================== */

/* Lo schermo: un descrittore, o -1 se e' gia' di un altro programma. */
int apri_schermo(void);

/* Mette l'immagine dello schermo del descrittore 'd' nella memoria del programma:
 * scrivere li' un numero vuol dire accendere un pixel. L'indirizzo, o 0. 'info' dice
 * com'e' fatta. */
void *mappa_schermo(int d, struct info_schermo *info);

/* Dice che in quelle zone dello schermo si e' scritto qualcosa di nuovo. 0, o -1. */
int schermo_cambiato(int d, const struct zona_schermo *zone, int quante);

/* =====================================================================================
 * 3. LA FINESTRA, il lato dell'applicazione
 * =====================================================================================
 *
 * Il protocollo (la parte 1) visto dall'applicazione: apre la finestra, da' una tela
 * libera su cui disegnare, la fa mostrare, consegna gli eventi. COME disegnare
 * (rettangoli, testo...) non e' affar suo: e' di gkit_disegno.h.
 *
 *     struct finestra f;
 *     finestra_apri(&f, 300, 200, "Ciao");
 *     unsigned int *tela = finestra_tela(&f);   ... si disegna: tela[y * 300 + x] = 0xRRGGBB
 *     finestra_mostra(&f);
 *     struct notizia n;
 *     while (finestra_notizia(&f, &n)) { ... }
 *
 * LE DUE IMMAGINI. finestra_tela da' sempre un'immagine che il compositore non sta
 * usando: se sono tutte e due sue, aspetta che ne restituisca una. Le notizie arrivate
 * nel frattempo (un clic, un tasto) si mettono da parte, e finestra_notizia le
 * consegnera' dopo, in ordine.
 *
 * RIDISEGNARE SOLO UN PEZZO. Chi cambia solo una parte della finestra la ridisegna e la
 * mostra con finestra_mostra_zone. Ma l'immagine che riceve da finestra_tela e' quella di DUE
 * fotogrammi fa (l'altra e' del compositore): le mancano anche le zone cambiate nel
 * fotogramma di mezzo. Gliele dice finestra_arretrate (e' l'"eta' del buffer" di EGL e
 * Wayland): chi disegna rifa' le sue zone PIU' quelle arretrate, e l'immagine torna giusta
 * dappertutto. La prima volta (e dopo un finestra_mostra intero) e' tutta da rifare.
 *
 * LE NOTIZIE per l'applicazione sono quelle della parte 1: PUNTATORE, USCITO, TASTO,
 * FUOCO e ROTELLA (la rotella del mouse, con il punto della finestra in cui e' il
 * puntatore: sta all'applicazione decidere cosa scorre li' sotto). */

#define DA_PARTE 16

struct finestra {
    int collegamento, foglio;
    int larghezza, altezza;
    unsigned int *immagini[2];
    int libera[2];             /* 1 se l'immagine e' nostra, 0 se e' del compositore */
    int in_uso;                /* l'immagine data dall'ultima finestra_tela */
    struct notizia messe_da_parte[DA_PARTE];
    int quante;
    /* le zone che mancano a ogni immagine rispetto all'ultima mostrata (-1: tutta) */
    struct zona_finestra arretrate[2][MAX_ZONE_MOSTRA];
    int quante_arretrate[2];
};

/* Si collega al compositore; se non e' ancora pronto riprova per qualche secondo. */
static inline int collega_compositore(void) {
    int d = -1;
    for (int prove = 0; prove < 100 && d < 0; prove++) {
        d = collega(NOME_COMPOSITORE);
        if (d < 0) dormi(50);
    }
    return d;
}

/* Quanto e' grande lo schermo: lo si chiede al compositore. 0, o -1. */
static inline int finestra_schermo(int *larghezza, int *altezza) {
    int d = collega_compositore();
    if (d < 0) return -1;
    struct chiedi_schermo m = { .tipo = CHIEDI_SCHERMO };
    struct notizia n;
    int fatto = manda(d, &m, sizeof m) >= 0 && ricevi(d, &n, sizeof n) == sizeof n && n.tipo == SCHERMO;
    chiudi(d);
    if (!fatto) return -1;
    *larghezza = n.x;
    *altezza = n.y;
    return 0;
}

/* Si collega al compositore e chiede una superficie: il lavoro di finestra_apri_come e di
 * finestra_apri_pannello, qui sotto. */
static inline int finestra_apri_superficie(struct finestra *f, int larghezza, int altezza,
                                           const char *titolo, unsigned int ruolo,
                                           unsigned int pannello, int x, int y, unsigned int raggio) {
    f->collegamento = collega_compositore();
    if (f->collegamento < 0) return -1;

    unsigned int pixel = larghezza * altezza;
    f->foglio = foglio(2 * pixel * 4);
    unsigned int *memoria = f->foglio < 0 ? 0 : mappa(f->foglio, 0);
    if (!memoria) return -1;
    f->larghezza = larghezza;
    f->altezza = altezza;
    f->immagini[0] = memoria;
    f->immagini[1] = memoria + pixel;
    f->libera[0] = f->libera[1] = 1;
    f->in_uso = 0;
    f->quante = 0;
    f->quante_arretrate[0] = f->quante_arretrate[1] = -1;   /* tutte e due da fare da capo */

    struct voglio_finestra m = { .tipo = VOGLIO_FINESTRA, .larghezza = larghezza, .altezza = altezza,
                                 .ruolo = ruolo, .x = x, .y = y, .raggio = raggio,
                                 .pannello = pannello };
    for (int i = 0; i < MAX_TITOLO - 1 && titolo[i]; i++) m.titolo[i] = titolo[i];
    return manda_foglio(f->collegamento, &m, sizeof m, f->foglio) < 0 ? -1 : 0;
}

/* Come finestra_apri, ma con un ruolo speciale (RUOLO_SFONDO, RUOLO_PANNELLO: solo per i
 * programmi del sistema), il posto sullo schermo e il raggio degli angoli. Un PANNELLO
 * aperto cosi' e' una barra (PANNELLO_BARRA). */
static inline int finestra_apri_come(struct finestra *f, int larghezza, int altezza,
                                     const char *titolo, unsigned int ruolo, int x, int y,
                                     unsigned int raggio) {
    return finestra_apri_superficie(f, larghezza, altezza, titolo, ruolo, PANNELLO_BARRA, x, y, raggio);
}

/* Un PANNELLO del tipo 'pannello' (PANNELLO_BARRA o PANNELLO_SOPRA). */
static inline int finestra_apri_pannello(struct finestra *f, int larghezza, int altezza,
                                         const char *titolo, unsigned int pannello, int x, int y,
                                         unsigned int raggio) {
    return finestra_apri_superficie(f, larghezza, altezza, titolo, RUOLO_PANNELLO, pannello, x, y,
                                    raggio);
}

/* Chiude la finestra: il compositore la toglie dallo schermo (se ne accorge dal collegamento
 * chiuso) e la sua memoria si restituisce. Per riaverla si riapre. */
static inline void finestra_chiudi(struct finestra *f) {
    if (f->collegamento >= 0) chiudi(f->collegamento);
    if (f->immagini[0]) togli(f->immagini[0]);
    if (f->foglio >= 0) chiudi(f->foglio);
    f->collegamento = f->foglio = -1;
    f->immagini[0] = f->immagini[1] = 0;
}

/* Si collega al compositore (se non e' ancora pronto riprova per qualche secondo), crea
 * il foglio con le due immagini e chiede una finestra normale. 0 se fatto, -1 se no. */
static inline int finestra_apri(struct finestra *f, int larghezza, int altezza, const char *titolo) {
    return finestra_apri_come(f, larghezza, altezza, titolo, RUOLO_FINESTRA, 0, 0, RAGGIO_PREDEFINITO);
}

/* Riceve un messaggio dal compositore. Le immagini restituite si segnano qui, e al
 * "ci sei?" si risponde qui: l'applicazione non lo vede nemmeno. Finche' lei torna ad
 * aspettare le sue notizie, la risposta parte; se resta bloccata altrove, no, ed e'
 * proprio cosi' che il compositore se ne accorge.
 * 1 se e' arrivato qualcosa, 0 se il compositore ha chiuso. */
static inline int finestra_ricevi(struct finestra *f, struct notizia *n) {
    if (ricevi(f->collegamento, n, sizeof *n) != sizeof *n) return 0;
    if (n->tipo == RILASCIATA && n->immagine < 2) f->libera[n->immagine] = 1;
    if (n->tipo == PING) {
        struct pong p = { .tipo = PONG, .numero = n->numero };
        if (manda(f->collegamento, &p, sizeof p) < 0) return 0;
    }
    return 1;
}

/* I messaggi che la libreria sbriga da sola, senza passarli all'applicazione. */
static inline int finestra_di_servizio(const struct notizia *n) {
    return n->tipo == RILASCIATA || n->tipo == PING;
}

/* Un'immagine libera, su cui disegnare. 0 se il compositore ha chiuso. */
static inline unsigned int *finestra_tela(struct finestra *f) {
    while (!f->libera[0] && !f->libera[1]) {
        struct notizia n;
        if (!finestra_ricevi(f, &n)) return 0;
        if (!finestra_di_servizio(&n) && f->quante < DA_PARTE) f->messe_da_parte[f->quante++] = n;
    }
    f->in_uso = f->libera[0] ? 0 : 1;
    return f->immagini[f->in_uso];
}

/* C'e' un'immagine libera, senza aspettare? (Per non bloccarsi su finestra_tela mentre
 * arrivano altre notizie.) */
static inline int finestra_libera(const struct finestra *f) { return f->libera[0] || f->libera[1]; }

/* Le zone che mancano all'immagine data dall'ultima finestra_tela (vedi "ridisegnare solo un
 * pezzo"): le copia in 'zone' (al massimo MAX_ZONE_MOSTRA) e dice quante sono; -1 se
 * l'immagine e' tutta da rifare. */
static inline int finestra_arretrate(const struct finestra *f, struct zona_finestra *zone) {
    int n = f->quante_arretrate[f->in_uso];
    for (int k = 0; k < n; k++) zone[k] = f->arretrate[f->in_uso][k];
    return n;
}

/* L'immagine data dall'ultima finestra_tela e' pronta: il compositore la mostra. E' cambiata
 * solo nelle 'quante' zone (in pixel della finestra; 0: tutta). Da adesso l'altra immagine,
 * quando tornera' libera, sara' indietro anche di queste zone. */
static inline int finestra_mostra_zone(struct finestra *f, const struct zona_finestra *zone, int quante) {
    struct mostra m = { .tipo = MOSTRA, .immagine = f->in_uso };
    struct zona_finestra tutta = { 0, 0, f->larghezza, f->altezza };
    int n = 0;
    for (int k = 0; k < quante; k++) zone_aggiungi(m.zone, &n, MAX_ZONE_MOSTRA, zona_dentro(zone[k], tutta));
    if (quante > 0 && !n) {            /* niente di cambiato dentro la finestra: una zona vuota */
        m.zone[0] = (struct zona_finestra){ 0, 0, 0, 0 };
        n = 1;
    }
    m.quante = (unsigned int)n;
    int altra = 1 - f->in_uso;
    if (quante <= 0) f->quante_arretrate[altra] = -1;
    else if (f->quante_arretrate[altra] >= 0)
        for (int k = 0; k < n; k++)
            zone_aggiungi(f->arretrate[altra], &f->quante_arretrate[altra], MAX_ZONE_MOSTRA, m.zone[k]);
    f->quante_arretrate[f->in_uso] = 0;
    f->libera[f->in_uso] = 0;
    return manda(f->collegamento, &m, sizeof m);
}

/* Tutta l'immagine e' nuova (chi ridisegna sempre tutto, come Doom). */
static inline int finestra_mostra(struct finestra *f) { return finestra_mostra_zone(f, 0, 0); }

/* Da chiamare quando si preme sulla propria barra del titolo: il compositore sposta la
 * finestra seguendo il mouse, finche' il pulsante resta premuto. */
static inline int finestra_trascinami(struct finestra *f) {
    struct trascinami m = { .tipo = TRASCINAMI };
    return manda(f->collegamento, &m, sizeof m);
}

/* Chiede di ricevere anche le notizie TASTO_FISICO (tasto premuto/lasciato, codici KEY_ di
 * Linux): per i giochi. Le notizie TASTO continuano ad arrivare. */
static inline int finestra_tasti_fisici(struct finestra *f) {
    struct voglio_tasti_fisici m = { .tipo = VOGLIO_TASTI_FISICI };
    return manda(f->collegamento, &m, sizeof m);
}

/* Chiede al compositore di nascondere la finestra (il pulsante "riduci"). Tornera'
 * quando la scrivania la fa tornare. */
static inline int finestra_riducimi(struct finestra *f) {
    struct riducimi m = { .tipo = RIDUCIMI };
    return manda(f->collegamento, &m, sizeof m);
}

/* La prossima notizia (un clic, un tasto...); dorme se non ce n'e'. 0 se il
 * compositore ha chiuso. */
static inline int finestra_notizia(struct finestra *f, struct notizia *n) {
    if (f->quante) {
        *n = f->messe_da_parte[0];
        f->quante--;
        for (int i = 0; i < f->quante; i++) f->messe_da_parte[i] = f->messe_da_parte[i + 1];
        return 1;
    }
    do {
        if (!finestra_ricevi(f, n)) return 0;
    } while (finestra_di_servizio(n));
    return 1;
}

#endif
