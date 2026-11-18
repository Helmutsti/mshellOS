/* Il compositore: il programma padrone dello schermo.
 *
 * Come in un sistema vero, il kernel non disegna finestre: presta lo schermo a un solo
 * programma, questo, e gli consegna gli eventi di tastiera e mouse. Le applicazioni si
 * collegano al socket "compositore" e parlano con il protocollo delle finestre
 * (gkit/sistema/protocollo.h): ognuna disegna da sola la sua finestra in un foglio di memoria
 * condivisa, e dice "mostra". Il compositore:
 *   - COMPONE, a strati: lo sfondo (una superficie della scrivania), le finestre dal
 *     fondo al primo piano, ognuna con la sua ombra morbida intorno, i pannelli (la
 *     userbar e il vassoio delle app, superfici della scrivania), poi, solo sullo schermo, il puntatore. Le finestre sono di VETRO SMERIGLIATO:
 *     sotto ognuna il compositore sfoca quello che c'e' dietro, e ci mescola sopra
 *     l'immagine dell'applicazione (i suoi pixel hanno un'opacita'). Dentro le finestre
 *     non disegna niente, neanche la barra del titolo: ogni applicazione si disegna da
 *     sola, tutta;
 *   - SMISTA L'INPUT: il puntatore alla finestra che ci sta sotto (in coordinate sue),
 *     i tasti a quella che ha il fuoco;
 *   - SPOSTA le finestre quando un'applicazione chiede TRASCINAMI, e porta davanti
 *     quella cliccata.
 *
 * WAYLAND, ACCANTO. Il compositore e' anche un server Wayland (compositore_wayland.c, passo 1
 * della decisione "il compositore passa a Wayland", SISTEMA.md): il ciclo guarda anche il suo
 * descrittore, e una finestra Wayland diventa un posto di 'finestre' come le altre (il ponte,
 * piu' sotto: ponte_*). Il resto di questo file non sa cos'e' Wayland.
 *
 * LE SUE RISORSE GLIELE PRESTA INIT. Lo schermo, tastiera e mouse, e il nome
 * "compositore" su cui si collegano le applicazioni non li apre lui: li tiene init,
 * che glieli passa gia' aperti ai descrittori 0, 1 e 2 (avvia_con). Se il compositore
 * cade, init li ha ancora, e lo fa ripartire con le stesse.
 *
 * LO SCHERMO, VISTO DA QUI. Dopo mappa_schermo l'immagine che si vede sullo schermo e'
 * nella nostra memoria, a 'memoria': scrivere li' un numero vuol dire accendere un
 * pixel. Riga dopo riga, da sinistra a destra, 4 byte per pixel. Tra l'inizio di una
 * riga e la successiva ci sono 'passo' byte (puo' essere piu' della larghezza * 4).
 * Finito un fotogramma, si dice al sistema quali rettangoli sono cambiati
 * (schermo_cambiato): alcune schede video, come quella virtuale di QEMU, li copiano.
 *
 * IL DOPPIO BUFFER. Non si disegna direttamente sullo schermo: se no, a ogni movimento
 * si vedrebbe lo sfondo cancellare per un attimo le finestre (lo schermo "sfarfalla").
 * Si compone la scena in una memoria nostra, 'disegno', e poi la si copia sullo
 * schermo in un colpo. E non si rifa' mai tutta: solo i pezzi che sono cambiati davvero
 * (vedi cosa_rifare), come fanno Wayland, macOS e Windows ("le zone rovinate").
 * Il puntatore non sta in 'disegno': lo si disegna solo sullo schermo, dopo la copia.
 * Cosi' quando si muove solo il mouse non si compone niente: si rimette sullo schermo
 * il pezzetto di scena dove stava il puntatore, e lo si disegna dove sta adesso.
 *
 * NON SI ASPETTA MAI UN'APPLICAZIONE. Il compositore manda i suoi messaggi senza
 * aspettare (manda_subito), e non si fida di quello che riceve: misure, immagini e
 * foglio si controllano.
 *
 * LE APPLICAZIONI BLOCCATE. Ogni secondo il compositore chiede a ogni applicazione
 * "ci sei?" (PING). Se non risponde entro 3 secondi, o se la sua casella e' cosi' piena
 * che non le si puo' piu' scrivere, e' bloccata. Lei non puo' piu' ridisegnarsi, quindi
 * la sua barra la disegna il compositore, rossa, con "... non risponde" e il pulsante
 * "chiudi" rosso, che chiude il programma (termina). Se invece ricomincia a rispondere,
 * torna normale. Come GNOME e il Mac.
 *
 * LA GRAFICA. Il compositore usa solo GKit (vedi LIBRERIE.md): il protocollo, lo schermo e i
 * conti per pixel (gkit/sistema/: protocollo.h, schermo.h, pixel.h), la finestra e le zone
 * (gkit.h) e gli strumenti di disegno (gkit_disegno.h). Non vede lo stile del sistema: le
 * poche cose del design che deve disegnare lui (la barra di emergenza, uguale a quella delle
 * applicazioni, e l'ombra e il vetro delle finestre) gliele da' UIKit, con le sole funzioni
 * di uikit/sistema/uikit_compositore.h. */
#include "corekit/corekit.h"
#include "gkit/gkit.h"
#include "gkit/gkit_disegno.h"
#include "gkit/sistema/protocollo.h"   /* i messaggi delle finestre, i ruoli speciali */
#include "gkit/sistema/schermo.h"      /* lo schermo (il DRM) */
#include "gkit/sistema/pixel.h"        /* i conti per pixel: comporre le finestre */
#include "uikit/sistema/uikit_compositore.h"
#include "corekit/catalog.h"             /* il tema, e i suoi avvisi */
#include "compositore_wayland.h"         /* il server Wayland, e il ponte verso questo file */

/* Le risorse che init gli passa gia' aperte (vedi sopra). */
#define D_SCHERMO 0
#define D_EVENTI  1
#define D_ASCOLTO 2

#define MAX_LARGHEZZA 1280  /* la modalita' che chiede GKit (gkit/interno/schermo.c) */
#define MAX_ALTEZZA   800

static struct info_schermo schermo;
static unsigned char *memoria;
static int larghezza, altezza;

/* La scena, prima dello schermo: pixel premoltiplicati, come quelli delle finestre.
 * Ci si disegna con la libreria di disegno, come in una finestra. C'e' tutto tranne il
 * puntatore: e' sempre la scena intera di adesso, senza il puntatore. */
static unsigned int disegno[MAX_LARGHEZZA * MAX_ALTEZZA];
static struct tela scena;
static struct pennello pennello;

/* LE ZONE DA RIFARE, in pixel dello schermo: rettangoli che non si toccano (vedi
 * cosa_rifare). Fuori, 'disegno' e lo schermo hanno gia' quello giusto. Mentre si
 * compone, quella su cui si sta lavorando e' in zx0..zy1: si disegna solo con
 * zx0 <= x < zx1, zy0 <= y < zy1. */
struct zona { int x0, y0, x1, y1; };   /* x0 <= x < x1, y0 <= y < y1 */
#define MAX_ZONE 16
static struct zona zone[MAX_ZONE];
static int quante_zone;
static int zx0, zy0, zx1, zy1;

/* LO SFONDO NON E' SUO: e' una superficie della scrivania (RUOLO_SFONDO). Finche' la
 * scrivania non c'e', sotto le finestre c'e' solo questo colore. */
#define SENZA_SFONDO OPACO(0x0F172A)

/* Un'immagine di un'applicazione (premoltiplicata) a x, y, mescolata con quello che c'e'
 * sotto; le prime 'salta' righe no. Quello che esce dallo schermo si taglia. */
static void immagine(int x, int y, int l, int a, const unsigned int *pixel, int salta) {
    /* solo le righe e le colonne che cadono nella zona da rifare (che sta nello schermo) */
    int r0 = zy0 - y > salta ? zy0 - y : salta, r1 = zy1 - y < a ? zy1 - y : a;
    int k0 = zx0 - x > 0 ? zx0 - x : 0,         k1 = zx1 - x < l ? zx1 - x : l;
    for (int r = r0; r < r1 && k0 < k1; r++)
        sopra_a_riga(&disegno[(y + r) * larghezza + x + k0], &pixel[r * l + k0], k1 - k0);
}

/* --- le finestre, una per applicazione collegata ---
 * (struct finestra_compositore: 'struct finestra' e' la finestra vista dall'applicazione,
 * in gkit.h) */

#define MAX_FINESTRE 8

struct finestra_compositore {
    int usata;
    int collegamento, foglio;
    unsigned int *immagini[2];
    int mostrata;          /* l'immagine che si vede (-1: nessuna ancora, non si disegna) */
    int x, y;              /* l'angolo in alto a sinistra sullo schermo */
    int l, a;
    char titolo[MAX_TITOLO];

    /* "ci sei?" */
    unsigned int ultimo_ping;   /* quando si e' chiesto l'ultima volta (ms) */
    unsigned int ping_numero;   /* il numero dell'ultimo PING */
    int ping_atteso;            /* aspettiamo ancora la sua risposta */
    int non_risponde;           /* bloccata: la barra la disegna il compositore */
    int da_rilasciare;          /* un'immagine da restituire che non le si e' potuto
                                 * dire (casella piena): glielo si dice quando risponde */
    unsigned int generazione;   /* cresce a ogni MOSTRA: l'immagine e' cambiata */
    /* DOVE e' cambiata, dall'ultima composizione: le zone dei MOSTRA arrivati nel frattempo
     * (in pixel della finestra), sommate; -1: tutta */
    struct zona_finestra cambiate[MAX_ZONE_MOSTRA];
    int quante_cambiate;

    unsigned int ruolo;         /* RUOLO_FINESTRA, o una superficie speciale della scrivania */
    int raggio;                 /* gli angoli: per l'ombra e il vetro smerigliato */
    unsigned int pannello;      /* un PANNELLO: PANNELLO_BARRA o PANNELLO_SOPRA (l'ombra) */
    unsigned int id;            /* il suo numero per la scrivania (non si riusa) */
    char programma[MAX_PERCORSO_PROGRAMMA];   /* di quale programma e': lo dice il kernel, o il
                                               * launcher se e' arrivata dalla porta dell'app */
    char sopra[MAX_PERCORSO_PROGRAMMA];   /* una SCELTA: il programma dell'app che copre ("" per le altre) */
    int dalla_porta;            /* arrivata dalla porta di un'app: 'programma' e' gia' giusto */
    int wayland;                /* una finestra Wayland (compositore_wayland.c): niente collegamento
                                 * (-1) ne' foglio, l'immagine e' di Wayland e la cornice la disegna
                                 * il compositore; vedi "IL PONTE" */
    int annunciata;             /* la scrivania sa che c'e' (FINESTRA_APERTA) */
    int tasti_fisici;           /* vuole anche TASTO_FISICO (VOGLIO_TASTI_FISICI) */
    int ridotta;                /* nascosta (RIDUCIMI): torna con PORTA_DAVANTI */
    unsigned int forma;         /* la forma del puntatore quando e' sulla finestra (MOSTRA_PUNTATORE) */
};

/* Il solo programma che puo' aprire una finestra di scelta (RUOLO_SCELTA). */
#define PROGRAMMA_PICKER "/system/services/picker/picker"

static struct finestra_compositore finestre[MAX_FINESTRE];

/* LE PORTE DELLE APP (protocollo.h, LA PORTA DELL'APP): il capo del compositore, e il programma
 * dell'app che il launcher ha detto. Una porta si chiude quando l'app la chiude (o finisce). */
#define MAX_PORTE MAX_FINESTRE
static struct {
    int usata;
    int d;
    char programma[MAX_PERCORSO_PROGRAMMA];
} porte[MAX_PORTE];

/* L'ordine: ordine[0] e' quella piu' in fondo, ordine[quante-1] quella in primo piano.
 * Si disegnano dal fondo: ognuna copre quelle disegnate prima. */
static int ordine[MAX_FINESTRE];
static int quante;

static int fuoco = -1;      /* la finestra che riceve i tasti */
static int sotto = -1;      /* quella che ha ricevuto l'ultimo PUNTATORE */
static int presa = -1;      /* quella su cui si e' premuto il pulsante: finche' e' premuto,
                             * il puntatore va a lei anche fuori (come in Wayland) */
static int trascinata = -1; /* quella che si sta spostando (TRASCINAMI) */
static int presa_x, presa_y;
static int px, py;          /* dov'e' il puntatore: il mouse dice solo di quanto si muove */
static unsigned int prossimo_ping = 1;
static unsigned int prossimo_id = 1;
/* Il prossimo fotogramma si ricompone tutto, sfocature comprese: la prima volta, e quando
 * cambia il tema (le ombre e il vetro di tutte le finestre). */
static int tutto_da_rifare = 1;

/* L'OMBRA di una finestra: a STRATI, come nel disegno (il token "ombra/finestra", Chiaro o
 * Scuro, che UIKit da' con ombra_finestra). Sono tre ombre esterne sovrapposte, una vicina e netta e
 * una larga e leggera, ognuna spostata in giu' e con una diffusione negativa (piu' stretta
 * della finestra, cosi' si vede soprattutto sotto, come con una luce alta). Calcolarla costa
 * (le distanze dal bordo arrotondato per ogni strato), e dipende solo dalla misura della
 * finestra: la si calcola una volta, quando la finestra nasce, come una tabella di
 * coperture, e a ogni composizione la si "versa" con il suo colore.
 *
 * UNA SOLA TABELLA PER TUTTI GLI STRATI. Gli strati esterni dei token hanno tutti lo stesso
 * colore (nel chiaro 0x14141A, nello scuro nero) e cambiano solo l'opacita': mescolati uno
 * sopra l'altro danno ancora quel colore, con l'opacita' composta
 *     alfa = 1 - (1 - a1) * (1 - a2) * (1 - a3)
 * e quindi basta una tabella di un byte per pixel, con l'alfa gia' dentro. Il colore si
 * legge dal primo strato esterno al momento di versarla. (Gli strati INTERNI della tabella
 * dello scuro, il filo di luce sul bordo alto, li disegna l'applicazione dentro la sua
 * immagine: sono dentro la finestra, il compositore non c'entra.)
 *
 * QUANTO E' GRANDE. La piu' larga e lontana e' l'ultima: sfocatura 64 (72 nello scuro),
 * diffusione -16, spostata di 32 (40) in giu'. Arriva a 48 (56) pixel dai lati e dall'alto
 * (meno lo spostamento) e a 80 (96) sotto. OMBRA_MARGINE e OMBRA_MARGINE_GIU sono i valori
 * del tema piu' grande. */
#define OMBRA_MARGINE      56
#define OMBRA_MARGINE_GIU  96
#define OMBRA_L(f) ((f)->l + 2 * OMBRA_MARGINE)
#define OMBRA_A(f) ((f)->a + OMBRA_MARGINE + OMBRA_MARGINE_GIU)
static unsigned char ombre[MAX_FINESTRE][(MAX_LARGHEZZA + 2 * OMBRA_MARGINE) *
                                         (MAX_ALTEZZA + OMBRA_MARGINE + OMBRA_MARGINE_GIU)];

/* Una finestra che non ha il fuoco ha un'ombra piu' leggera (nel vecchio disegno il 22%
 * contro il 38%, cioe' il 58%). Il Figma ha una sola ombra per la finestra: questo e' un
 * ritocco nostro; per toglierlo basta mettere 256. In 256esimi. */
#define OMBRA_SENZA_FUOCO  150

/* La tabella degli strati di una finestra, secondo il suo ruolo e il tema in uso. Un
 * pannello ha l'ombra del suo tipo (la userbar quella di una barra, il vassoio delle app
 * quella di un menu): le da' UIKit, come quella delle finestre. */
static const struct strato_ombra *strati_ombra(const struct finestra_compositore *f, int *quanti) {
    if (f->ruolo == RUOLO_PANNELLO) return ombra_pannello(f->pannello == PANNELLO_SOPRA, quanti);
    return ombra_finestra(quanti);
}

/* IL BUCO di ogni riga della tabella: le colonne in mezzo dove vale 0 (sotto la finestra),
 * che disegna_ombra salta. E IL RIENTRO: di quanto stare dentro dai bordi della finestra
 * perche' sotto non ci sia piu' ombra in nessun punto (gli angoli arrotondati: fuori dalla
 * curva, ma dentro il rettangolo, l'ombra c'e'). Serve al vetro (vedi pianifica); -1: non c'e'. */
static unsigned short buchi[MAX_FINESTRE][MAX_ALTEZZA + OMBRA_MARGINE + OMBRA_MARGINE_GIU][2];
static int rientro[MAX_FINESTRE];

static void prepara_ombra(int i) {
    const struct finestra_compositore *f = &finestre[i];
    int n;
    const struct strato_ombra *strati = strati_ombra(f, &n);
    struct tondo forma = tondo(0, 0, f->l, f->a, f->raggio);
    for (int y = 0; y < OMBRA_A(f); y++)
        for (int x = 0; x < OMBRA_L(f); x++) {
            int wx = x - OMBRA_MARGINE, wy = y - OMBRA_MARGINE;
            int fuori = 256 - coperto(distanza(&forma, wx, wy));   /* sotto la finestra no */
            int passa = 255;                      /* quanta luce passa attraverso gli strati */
            if (fuori)
                for (int k = 0; k < n; k++) {
                    if (strati[k].tipo != OMBRA_ESTERNA) continue;
                    struct tondo spostata = forma;
                    spostata.x += strati[k].dx;
                    spostata.y += strati[k].dy;
                    int s = strati[k].sfocatura * 256;
                    if (s < 256) s = 256;
                    int dolce = sfuma(distanza(&spostata, wx, wy) - strati[k].diffusione * 256, s);
                    if (!dolce) continue;
                    int alfa = (dolce * (int)(strati[k].colore >> 24)) >> 8;   /* 0-255 */
                    passa = passa * (255 - alfa) / 255;
                }
            int v = (255 - passa) * fuori >> 8;
            ombre[i][y * OMBRA_L(f) + x] = (unsigned char)v;
        }
    /* i buchi: dalla colonna di mezzo, finche' la tabella vale 0 */
    int lt = OMBRA_L(f);
    for (int y = 0; y < OMBRA_A(f); y++) {
        const unsigned char *riga = &ombre[i][y * lt];
        int b0 = lt / 2, b1 = lt / 2;
        if (!riga[b0]) {
            while (b0 > 0 && !riga[b0 - 1]) b0--;
            while (b1 < lt && !riga[b1]) b1++;
        }
        buchi[i][y][0] = (unsigned short)b0;
        buchi[i][y][1] = (unsigned short)b1;
    }
    /* il rientro: il primo d per cui il rettangolo della finestra stretto di d per parte sta
     * tutto nei buchi */
    rientro[i] = -1;
    for (int d = 0; 2 * d < f->l && 2 * d < f->a && rientro[i] < 0; d++) {
        int dentro_buco = 1;
        for (int y = d; y < f->a - d && dentro_buco; y++) {
            const unsigned short *b = buchi[i][y + OMBRA_MARGINE];
            dentro_buco = b[0] <= OMBRA_MARGINE + d && b[1] >= OMBRA_MARGINE + f->l - d;
        }
        if (dentro_buco) rientro[i] = d;
    }
}

/* L'ombra nella zona: per ogni pixel della tabella quello che farebbe copri con il colore
 * dell'ombra e la copertura del pixel. Il colore da mettere sopra dipende solo dal valore
 * della tabella: lo si prepara una volta per i 256 valori (con colore_coperto, come copri),
 * e per ogni pixel lo si mescola e basta (due byte alla volta: e' un premoltiplicato vero).
 * Il buco di ogni riga (sotto la finestra) si salta. */
static void disegna_ombra(int i) {
    const struct finestra_compositore *f = &finestre[i];
    int n;
    const struct strato_ombra *strati = strati_ombra(f, &n);
    unsigned int rgb = 0;
    for (int k = 0; k < n; k++)
        if (strati[k].tipo == OMBRA_ESTERNA) { rgb = strati[k].colore & 0xFFFFFF; break; }
    pennello_colore(&pennello, OPACO(rgb));
    int quota = f->ruolo == RUOLO_FINESTRA && i != fuoco ? OMBRA_SENZA_FUOCO : 256;
    unsigned int colori[256];             /* 0: copri non mette niente */
    for (int v = 0; v < 256; v++) {
        int c = ((v + (v >> 7)) * quota) >> 8;
        unsigned int a = 0, p = c > 0 ? colore_coperto(&pennello, c > 256 ? 256 : c, &a) : 0;
        colori[v] = v && a ? p : 0;
    }
    int ox = f->x - OMBRA_MARGINE, oy = f->y - OMBRA_MARGINE;   /* dove comincia l'ombra */
    int y0 = zy0 - oy > 0 ? zy0 - oy : 0, y1 = zy1 - oy < OMBRA_A(f) ? zy1 - oy : OMBRA_A(f);
    int x0 = zx0 - ox > 0 ? zx0 - ox : 0, x1 = zx1 - ox < OMBRA_L(f) ? zx1 - ox : OMBRA_L(f);
    for (int y = y0; y < y1; y++) {
        const unsigned char *riga = &ombre[i][y * OMBRA_L(f)];
        unsigned int *d = &disegno[(oy + y) * larghezza + ox];
        int b0 = buchi[i][y][0], b1 = buchi[i][y][1];
        int fine = x1 < b0 ? x1 : b0;
        for (int x = x0; x < fine; x++)
            if (riga[x]) d[x] = sopra_a_vero(colori[riga[x]], d[x]);
        for (int x = x0 > b1 ? x0 : b1; x < x1; x++)
            if (riga[x]) d[x] = sopra_a_vero(colori[riga[x]], d[x]);
    }
}

/* IL VETRO SMERIGLIATO. Sotto ogni finestra si sfoca quello che c'e' dietro: ogni pixel
 * diventa la media di quelli che ha intorno, e i dettagli spariscono. I trucchi per farlo
 * senza scheda video, gli stessi di KDE e dei giochi:
 *   1. si lavora su una copia RIMPICCIOLITA a un quarto (16 volte meno pixel): quello
 *      che e' sfocato non ha dettagli da perdere;
 *   2. la media si fa in due passate, prima in orizzontale e poi in verticale, con una
 *      SOMMA CHE SCORRE (si aggiunge il pixel che entra, si toglie quello che esce): il
 *      costo non dipende da quanto e' larga la sfocatura;
 *   3. tre medie una dopo l'altra danno una sfocatura morbida quasi uguale a quella
 *      "vera" (gaussiana) del disegno. Nel disegno la sfocatura dello sfondo della finestra
 *      e' 28 (sfocatura_finestra(), da UIKit): in Figma il raggio vale due
 *      deviazioni standard, quindi sigma = 14 pixel. Tre medie larghe L pixel hanno
 *      sigma = L * 0.5 (radice di 3 * L^2 / 12): per sigma 14 serve L = 28, cioe' 7 pixel
 *      rimpiccioliti, raggio_media 3 (prima erano medie di 11: sigma 22, troppo sfocato);
 *   4. la si rifa' solo quando cambia qualcosa in quello che guarda (la finestra si
 *      sposta, o cambia qualcosa sotto di lei li' intorno: vedi cosa_rifare); se no si
 *      riusa quella gia' fatta.
 * Rifatta, la si ingrandisce di nuovo alla misura vera della finestra (ogni pixel e' la media
 * pesata dei quattro vicini di quella piccola) e la si tiene da parte, IL VETRO della finestra
 * ('vetri'): a ogni composizione lo si copia dentro la forma arrotondata, solo nelle zone da
 * rifare. Cosi' quando cambia solo un pezzo di una finestra (una riga sotto il puntatore) non
 * si rifanno i conti dell'ingrandimento, che erano piu' di meta' del lavoro.
 *
 * LA FINESTRA CHE SI SPOSTA. Il vetro va rifatto a ogni passo, ma quasi tutto quello che c'e'
 * dietro e' lo sfondo, che non cambia. Ogni pixel del vetro dipende solo dalla scena a meno
 * di 'dipendenza' pixel da lui (i conti qui sotto sono tutti con numeri interi, e lontano dai
 * bordi della copia piccola danno lo stesso numero dovunque stia la copia). Quindi dove li'
 * intorno c'e' solo lo sfondo, il vetro e' quello dello SFONDO DA SOLO, uguale pixel per
 * pixel: lo si calcola una volta per tutto lo schermo ('vetro_fondo', rifatto solo quando lo
 * sfondo cambia) e lo si copia. Si rifanno davvero solo i PEZZI vicini a quello che non e'
 * sfondo: l'ombra della finestra stessa (una cornice lungo i bordi) e le finestre che le
 * stanno sotto (vedi pianifica). */
#define SCALA          4    /* la copia rimpicciolita: un pixel ogni 4x4 */
static int raggio_media;    /* sfocatura_finestra() / SCALA / 2 = 3: medie di 7 pixel
                             * rimpiccioliti (28 veri), tre volte. Lo si chiede a UIKit in main */
#define BORDO_PICCOLO 16    /* quanto si prende intorno alla finestra, in pixel rimpiccioliti */
#define PICCOLA_L (MAX_LARGHEZZA / SCALA + 2 * BORDO_PICCOLO + 2)
#define PICCOLA_A (MAX_ALTEZZA / SCALA + 2 * BORDO_PICCOLO + 2)

struct sfocata {
    int x, y, l, a;         /* dove sta, in pixel rimpiccioliti (x * SCALA = pixel veri) */
    unsigned int pixel[PICCOLA_L * PICCOLA_A];
};
static struct sfocata piccola;   /* la copia piccola su cui si lavora (una alla volta) */
/* il vetro di ogni finestra: la sfocatura gia' ingrandita, l x a pixel (0x00RRGGBB); valgono
 * solo i pixel che stanno sullo schermo */
static unsigned int vetri[MAX_FINESTRE][MAX_LARGHEZZA * MAX_ALTEZZA];

/* LE MEDIE. raggio_media non va oltre MAX_RAGGIO_MEDIA (main): medie piu' larghe di 33 pixel
 * piccoli (132 veri) non servono a nessun vetro. La divisione della media e' gia' fatta per
 * ogni somma possibile (una divisione per ogni pixel e colore costava piu' di tutto il resto). */
#define MAX_RAGGIO_MEDIA 16
static unsigned short diviso_media[(2 * MAX_RAGGIO_MEDIA + 1) * 255 + 1];

/* I tre colori di un pixel in un numero a 64 bit, a 21 bit di distanza ("corsie"): la somma
 * di una media (al massimo 33 * 255) sta nella sua corsia, e si fanno tutti e tre insieme. */
#define CORSIA 0x1FFFFFull
static unsigned long long corsie[2][(PICCOLA_L > PICCOLA_A ? PICCOLA_L : PICCOLA_A) + 2 * MAX_RAGGIO_MEDIA + 2];

/* La media scorrevole su una fila di n pixel (passo 'salto' tra uno e l'altro), fatta
 * tre volte. Ai bordi si ripete il pixel del bordo (la fila si allunga di raggio_media pixel
 * uguali al bordo, da ogni parte). */
static void media_fila(unsigned int *fila, int n, int salto) {
    int r = raggio_media;
    unsigned long long *a = corsie[0] + r, *b = corsie[1] + r;
    for (int i = 0; i < n; i++) {
        unsigned int p = fila[i * salto];
        a[i] = ((p >> 16) & 0xFF) | (unsigned long long)((p >> 8) & 0xFF) << 21 |
               (unsigned long long)(p & 0xFF) << 42;
    }
    for (int volta = 0; volta < 3; volta++) {
        for (int k = 1; k <= r; k++) a[-k] = a[0];
        for (int k = 1; k <= r + 1; k++) a[n - 1 + k] = a[n - 1];
        unsigned long long somma = 0;
        for (int k = -r; k <= r; k++) somma += a[k];
        for (int i = 0; i < n; i++) {
            b[i] = diviso_media[somma & CORSIA] |
                   (unsigned long long)diviso_media[(somma >> 21) & CORSIA] << 21 |
                   (unsigned long long)diviso_media[somma >> 42] << 42;
            somma += a[i + r + 1] - a[i - r];   /* entra uno, esce uno */
        }
        unsigned long long *t = a; a = b; b = t;
    }
    for (int i = 0; i < n; i++)
        fila[i * salto] = (unsigned int)(a[i] & 0xFF) << 16 | (unsigned int)((a[i] >> 21) & 0xFF) << 8 |
                          (unsigned int)(a[i] >> 42);
}

/* Dove sta la copia piccola della finestra f, in pixel rimpiccioliti. */
static void posto_sfocata(const struct finestra_compositore *f, int *x, int *y, int *l, int *a) {
    *x = (f->x < 0 ? f->x - SCALA + 1 : f->x) / SCALA - BORDO_PICCOLO;
    *y = (f->y < 0 ? f->y - SCALA + 1 : f->y) / SCALA - BORDO_PICCOLO;
    *l = f->l / SCALA + 2 * BORDO_PICCOLO + 2;
    *a = f->a / SCALA + 2 * BORDO_PICCOLO + 2;
}

/* QUELLO CHE GUARDA una copia piccola: i pixel della scena che sfoca legge (fuori dallo
 * schermo legge il bordo, quindi si taglia). */
static int dentro(int v, int min, int max) { return v < min ? min : v > max ? max : v; }

static struct zona letta(int x, int y, int l, int a) {
    return (struct zona){ dentro(x * SCALA, 0, larghezza - 1), dentro(y * SCALA, 0, altezza - 1),
                          dentro((x + l) * SCALA - 1, 0, larghezza - 1) + 1,
                          dentro((y + a) * SCALA - 1, 0, altezza - 1) + 1 };
}

/* quello che guarda lo smerigliato intero della finestra f */
static struct zona guardata(const struct finestra_compositore *f) {
    int x, y, l, a;
    posto_sfocata(f, &x, &y, &l, &a);
    return letta(x, y, l, a);
}

/* Sfoca la scena (il disegno, o lo sfondo da solo) nella copia piccola s, gia' messa al suo
 * posto. 1. rimpicciolire: ogni pixel piccolo e' la media di 4x4 pixel della scena (fuori
 * dallo schermo, il bordo): per ogni riga piccola prima le somme delle SCALA righe vere,
 * colonna per colonna, poi a gruppi di SCALA colonne. Le somme del rosso e del blu stanno in
 * un numero solo, a 16 bit di distanza (al massimo 16 * 255 ciascuna: non si toccano).
 * 2. e 3. le medie: tutte le righe, poi tutte le colonne. */
static unsigned int somme_rb[MAX_LARGHEZZA], somme_g[MAX_LARGHEZZA];

static void sfoca(const unsigned int *scena, struct sfocata *s) {
    int xa = s->x * SCALA, xb = (s->x + s->l) * SCALA;
    int ca = xa > 0 ? xa : 0, cb = xb < larghezza ? xb : larghezza;   /* le colonne lette */
    for (int v = 0; v < s->a; v++) {
        const unsigned int *righe[SCALA];
        for (int dy = 0; dy < SCALA; dy++)
            righe[dy] = &scena[dentro((s->y + v) * SCALA + dy, 0, altezza - 1) * larghezza];
        for (int x = ca; x < cb; x++) {
            unsigned int rb = 0, g = 0;
            for (int dy = 0; dy < SCALA; dy++) {
                unsigned int px = righe[dy][x];
                rb += px & 0x00FF00FFu;
                g += (px >> 8) & 0xFF;
            }
            somme_rb[x] = rb;
            somme_g[x] = g;
        }
        unsigned int *dst = &s->pixel[v * s->l];
        for (int u = 0; u < s->l; u++) {
            int x0 = (s->x + u) * SCALA;
            unsigned int rb = 0, g = 0;
            if (x0 >= 0 && x0 + SCALA <= larghezza) {
                for (int dx = 0; dx < SCALA; dx++) { rb += somme_rb[x0 + dx]; g += somme_g[x0 + dx]; }
            } else {
                for (int dx = 0; dx < SCALA; dx++) {
                    int x = dentro(x0 + dx, 0, larghezza - 1);
                    rb += somme_rb[x];
                    g += somme_g[x];
                }
            }
            dst[u] = ((rb >> 16) / (SCALA * SCALA)) << 16 | (g / (SCALA * SCALA)) << 8 |
                     (rb & 0xFFFF) / (SCALA * SCALA);
        }
    }
    for (int v = 0; v < s->a; v++) media_fila(&s->pixel[v * s->l], s->l, 1);
    for (int u = 0; u < s->l; u++) media_fila(&s->pixel[u], s->a, s->l);
}

/* Ingrandisce la copia piccola s nel rettangolo g dello schermo (dentro lo schermo), in 'dst'
 * (il pixel (x, y) va in dst[(y - oy) * passo + x - ox]). Ogni pixel e' la media pesata dei
 * quattro vicini della copia piccola: prima in verticale, una volta per riga e per colonna
 * piccola, poi in orizzontale (gli stessi conti, solo raccolti diversamente: la somma e' la
 * stessa, intera). Il rosso e il blu insieme in un numero a 64 bit, a 32 bit di distanza. */
static int colonna_u[MAX_LARGHEZZA], colonna_t[MAX_LARGHEZZA];
static unsigned long long colonna_rb[PICCOLA_L];
static unsigned int colonna_g[PICCOLA_L];

static void ingrandisci(const struct sfocata *s, struct zona g, unsigned int *dst, int ox, int oy, int passo) {
    if (g.x0 >= g.x1 || g.y0 >= g.y1) return;
    int n = g.x1 - g.x0;
    for (int k = 0; k < n; k++) {
        /* dove cade il pixel nella copia piccola, in 256esimi (centro su centro) */
        int fx = ((g.x0 + k) * 256 + 128) / SCALA - 128 - s->x * 256;
        int ux = fx >> 8, tx = fx & 255;
        if (ux < 0) { ux = 0; tx = 0; }
        if (ux >= s->l - 1) { ux = s->l - 2; tx = 255; }
        colonna_u[k] = ux;
        colonna_t[k] = tx;
    }
    int u0 = colonna_u[0], u1 = colonna_u[n - 1] + 1;
    for (int y = g.y0; y < g.y1; y++) {
        int fy = (y * 256 + 128) / SCALA - 128 - s->y * 256;
        int vy = fy >> 8, ty = fy & 255;
        if (vy < 0) { vy = 0; ty = 0; }
        if (vy >= s->a - 1) { vy = s->a - 2; ty = 255; }
        const unsigned int *su = &s->pixel[vy * s->l], *giu = su + s->l;
        for (int u = u0; u <= u1; u++) {
            unsigned int a = su[u], c = giu[u];
            unsigned long long r = ((a >> 16) & 0xFF) * (256 - ty) + ((c >> 16) & 0xFF) * ty;
            unsigned long long b = (a & 0xFF) * (256 - ty) + (c & 0xFF) * ty;
            colonna_rb[u] = r | b << 32;
            colonna_g[u] = ((a >> 8) & 0xFF) * (256 - ty) + ((c >> 8) & 0xFF) * ty;
        }
        unsigned int *d = &dst[(y - oy) * passo + g.x0 - ox];
        for (int k = 0; k < n; k++) {
            int u = colonna_u[k];
            unsigned int t = (unsigned int)colonna_t[k];
            unsigned long long rb = colonna_rb[u] * (256 - t) + colonna_rb[u + 1] * t;
            unsigned int gg = colonna_g[u] * (256 - t) + colonna_g[u + 1] * t;
            d[k] = (unsigned int)((rb >> 16) & 0xFF) << 16 | ((gg >> 16) & 0xFF) << 8 |
                   (unsigned int)((rb >> 48) & 0xFF);
        }
    }
}

/* Quanto lontano guarda un pixel del vetro, in pixel veri: le colonne piccole che legge
 * l'ingrandimento (la sua e quella dopo), le 3 medie di raggio_media intorno a ognuna, i
 * SCALA pixel veri di ogni pixel piccolo. Con R = 3 * raggio_media, il pixel x guarda da
 * x - 4R - 6 a x + 4R + 6 al massimo: SCALA * (R + 2) basta (main). */
static int dipendenza;
/* Si puo' fare il vetro a pezzi solo se le copie piccole delle finestre hanno intorno
 * abbastanza bordo (le 3 medie non arrivano a toccarlo): se no si rifa' sempre tutto. */
static int a_pezzi;

/* LO SFONDO DA SOLO ('fondo': il colore di quando non c'e' e le superfici dello sfondo) e il
 * suo vetro su tutto lo schermo. Si rifanno solo quando lo sfondo cambia (cosa_rifare mette
 * 0 in 'fondo_pronto'); il vetro solo quando serve a qualcuno. Lo sfondo da solo serve anche
 * a comporre: ogni zona comincia come una sua copia (vedi componi_zone). */
static unsigned int fondo[MAX_LARGHEZZA * MAX_ALTEZZA];
static unsigned int vetro_fondo[MAX_LARGHEZZA * MAX_ALTEZZA];
static int fondo_pronto, vetro_fondo_pronto;

static int lista[MAX_FINESTRE], in_lista;   /* quelle da disegnare, in ordine (fai_lista) */

/* La copia piccola che serve per rifare il vetro nel rettangolo g (dentro lo schermo): le
 * colonne e le righe piccole che legge l'ingrandimento, piu' 3 medie e 2 per parte (cosi' le
 * medie non toccano il bordo della copia, e i numeri vengono quelli della copia intera). */
static int piccola_di(int x) { return ((x * 256 + 128) / SCALA - 128) >> 8; }   /* x >= 0 */

static void copia_per(struct zona g, struct sfocata *s) {
    int m = 3 * raggio_media + 2;
    int u0 = piccola_di(g.x0), u1 = piccola_di(g.x1 - 1) + 1;
    int v0 = piccola_di(g.y0), v1 = piccola_di(g.y1 - 1) + 1;
    s->x = u0 - m;
    s->l = u1 - u0 + 1 + 2 * m;
    s->y = v0 - m;
    s->a = v1 - v0 + 1 + 2 * m;
}

static void prepara_fondo(void) {
    for (int k = 0; k < larghezza * altezza; k++) fondo[k] = SENZA_SFONDO;
    for (int k = 0; k < in_lista; k++) {
        const struct finestra_compositore *f = &finestre[lista[k]];
        if (f->ruolo != RUOLO_SFONDO) continue;
        int x0 = f->x > 0 ? f->x : 0, x1 = f->x + f->l < larghezza ? f->x + f->l : larghezza;
        int y0 = f->y > 0 ? f->y : 0, y1 = f->y + f->a < altezza ? f->y + f->a : altezza;
        for (int y = y0; y < y1 && x0 < x1; y++)
            sopra_a_riga(&fondo[y * larghezza + x0],
                         &f->immagini[f->mostrata][(y - f->y) * f->l + x0 - f->x], x1 - x0);
    }
    fondo_pronto = 1;
    vetro_fondo_pronto = 0;
}

static void prepara_vetro_fondo(void) {
    struct zona schermo_intero = { 0, 0, larghezza, altezza };
    copia_per(schermo_intero, &piccola);
    sfoca(fondo, &piccola);
    ingrandisci(&piccola, schermo_intero, vetro_fondo, 0, 0, larghezza);
    vetro_fondo_pronto = 1;
}

/* IL PIANO per rifare il vetro di una finestra: tutto (la copia piccola intera, come sempre),
 * o il vetro dello sfondo con sopra solo alcuni pezzi rifatti. */
#define MAX_PEZZI 12
struct piano {
    int tutto, quanti;
    struct zona pezzi[MAX_PEZZI];   /* in pixel dello schermo, dentro lo schermo */
};
static struct piano piani[MAX_FINESTRE];

static int vuota(struct zona z) { return z.x0 >= z.x1 || z.y0 >= z.y1; }

static struct zona taglia(struct zona p, struct zona q) {
    return (struct zona){ p.x0 > q.x0 ? p.x0 : q.x0, p.y0 > q.y0 ? p.y0 : q.y0,
                          p.x1 < q.x1 ? p.x1 : q.x1, p.y1 < q.y1 ? p.y1 : q.y1 };
}

static struct zona sullo_schermo(const struct finestra_compositore *f) {
    return taglia((struct zona){ f->x, f->y, f->x + f->l, f->y + f->a },
                  (struct zona){ 0, 0, larghezza, altezza });
}

static struct zona con_ombra(unsigned int ruolo, int x, int y, int l, int a);

static void pezzo(struct piano *p, struct zona z) {
    if (vuota(z)) return;
    if (p->quanti == MAX_PEZZI) p->tutto = 1;
    else p->pezzi[p->quanti++] = z;
}

/* Cosa rifare del vetro della finestra i, che sta al posto 'posto' della lista. Sotto di lei
 * la scena e' lo sfondo da solo tranne: la sua ombra (fuori dal rettangolo stretto del
 * rientro) e le finestre e i pannelli che le stanno sotto, con le loro ombre. Un pixel del
 * vetro va rifatto se a meno di 'dipendenza' da lui c'e' una di queste cose: la cornice lungo
 * i bordi della finestra, e intorno a ogni finestra sotto. Se i pezzi sono quasi tutta la
 * finestra, conviene rifare tutto in un colpo. */
static void pianifica(int i, int posto) {
    const struct finestra_compositore *f = &finestre[i];
    struct piano *p = &piani[i];
    p->tutto = p->quanti = 0;
    struct zona w = sullo_schermo(f);
    if (vuota(w)) return;                       /* niente sullo schermo: niente da rifare */
    int m = dipendenza, d = rientro[i];
    struct zona pulita = { f->x + d + m, f->y + d + m, f->x + f->l - d - m, f->y + f->a - d - m };
    if (!a_pezzi || d < 0 || vuota(pulita)) { p->tutto = 1; return; }
    int ya = pulita.y0 > w.y0 ? pulita.y0 : w.y0, yb = pulita.y1 < w.y1 ? pulita.y1 : w.y1;
    pezzo(p, (struct zona){ w.x0, w.y0, w.x1, pulita.y0 < w.y1 ? pulita.y0 : w.y1 });   /* sopra */
    pezzo(p, (struct zona){ w.x0, pulita.y1 > w.y0 ? pulita.y1 : w.y0, w.x1, w.y1 });   /* sotto */
    pezzo(p, (struct zona){ w.x0, ya, pulita.x0 < w.x1 ? pulita.x0 : w.x1, yb });       /* sinistra */
    pezzo(p, (struct zona){ pulita.x1 > w.x0 ? pulita.x1 : w.x0, ya, w.x1, yb });       /* destra */
    for (int k = 0; k < posto; k++) {
        const struct finestra_compositore *g = &finestre[lista[k]];
        if (g->ruolo == RUOLO_SFONDO) continue;
        struct zona o = taglia(con_ombra(g->ruolo, g->x, g->y, g->l, g->a),
                               (struct zona){ 0, 0, larghezza, altezza });
        if (vuota(o)) continue;
        pezzo(p, taglia((struct zona){ o.x0 - m, o.y0 - m, o.x1 + m, o.y1 + m }, w));
    }
    long somma = 0;
    for (int k = 0; k < p->quanti; k++)
        somma += (long)(p->pezzi[k].x1 - p->pezzi[k].x0) * (p->pezzi[k].y1 - p->pezzi[k].y0);
    if (p->tutto || somma * 4 >= (long)(w.x1 - w.x0) * (w.y1 - w.y0) * 3) {
        p->tutto = 1;
        p->quanti = 0;
    }
}

/* Rifa' il vetro della finestra i secondo il suo piano. La scena sotto di lei e' pronta in
 * tutto quello che i pezzi (o la copia intera) guardano: cosa_rifare li ha messi nelle zone. */
static void rifai_vetro(int i) {
    const struct finestra_compositore *f = &finestre[i];
    const struct piano *p = &piani[i];
    struct zona w = sullo_schermo(f);
    if (vuota(w)) return;
    if (p->tutto) {
        posto_sfocata(f, &piccola.x, &piccola.y, &piccola.l, &piccola.a);
        sfoca(disegno, &piccola);
        ingrandisci(&piccola, w, vetri[i], f->x, f->y, f->l);
        return;
    }
    if (!vetro_fondo_pronto) prepara_vetro_fondo();
    for (int y = w.y0; y < w.y1; y++) {
        const unsigned int *da = &vetro_fondo[y * larghezza + w.x0];
        unsigned int *a = &vetri[i][(y - f->y) * f->l + w.x0 - f->x];
        for (int k = 0; k < w.x1 - w.x0; k++) a[k] = da[k];
    }
    for (int k = 0; k < p->quanti; k++) {
        copia_per(p->pezzi[k], &piccola);
        sfoca(disegno, &piccola);
        ingrandisci(&piccola, p->pezzi[k], vetri[i], f->x, f->y, f->l);
    }
}

/* Mette il vetro della finestra (la sfocatura ingrandita) dentro la sua forma, nella zona. */
static void metti_sfocatura(int i) {
    const struct finestra_compositore *f = &finestre[i];
    const unsigned int *vetro = vetri[i];
    struct tondo forma = tondo(f->x, f->y, f->l, f->a, f->raggio);
    int y0 = f->y > zy0 ? f->y : zy0, y1 = f->y + f->a < zy1 ? f->y + f->a : zy1;
    int x0 = f->x > zx0 ? f->x : zx0, x1 = f->x + f->l < zx1 ? f->x + f->l : zx1;
    for (int y = y0; y < y1; y++) {
        int vicino_sopra_sotto = y < f->y + f->raggio || y >= f->y + f->a - f->raggio;
        const unsigned int *riga = &vetro[(y - f->y) * f->l];   /* riga[x - f->x] */
        unsigned int *dst = &disegno[y * larghezza];
        for (int x = x0; x < x1; x++) {
            int c = 256;
            if (vicino_sopra_sotto && (x < f->x + f->raggio || x >= f->x + f->l - f->raggio))
                c = coperto(distanza(&forma, x, y));        /* gli angoli arrotondati */
            if (!c) continue;
            if (c == 256) { dst[x] = 0xFF000000u | riga[x - f->x]; continue; }
            unsigned int sfoc = riga[x - f->x], prima = dst[x], px = 0xFF000000u;
            for (int k = 0; k < 24; k += 8)
                px |= (unsigned int)((((sfoc >> k) & 0xFF) * c + ((prima >> k) & 0xFF) * (256 - c)) >> 8) << k;
            dst[x] = px;
        }
    }
}

/* LA FINESTRA DI SCELTA (protocollo.h): le stringhe dei programmi si confrontano qui. */
static int stesso_programma(const char *a, const char *b) {
    int k = 0;
    while (a[k] && a[k] == b[k]) k++;
    return a[k] == b[k];
}

/* La finestra di scelta che copre la finestra normale 'i' (aperta e che si vede), o -1: finche'
 * c'e', 'i' e' velata e non sente ne' puntatore, ne' rotella. Se le scelte sono piu' d'una,
 * quella piu' davanti. */
static int scelta_sopra(int i) {
    const struct finestra_compositore *f = &finestre[i];
    if (!f->usata || f->ruolo != RUOLO_FINESTRA || !f->programma[0]) return -1;
    for (int k = quante - 1; k >= 0; k--) {
        const struct finestra_compositore *g = &finestre[ordine[k]];
        if (g->usata && g->ruolo == RUOLO_SCELTA && g->mostrata >= 0 && stesso_programma(g->sopra, f->programma))
            return ordine[k];
    }
    return -1;
}

/* LA BARRA DI EMERGENZA di una finestra che non risponde (vedi in cima): la disegna UIKit,
 * con barra_congelata_disegna, al posto delle prime barra_congelata_altezza righe
 * dell'applicazione. Il compositore non sa se una finestra e' una mini finestra (il
 * protocollo non lo dice): per lui sono tutte normali ('mini' = 0). */
#define MINI 0

/* Quanto occupa una finestra sullo schermo, con l'ombra (lo sfondo non ne ha). */
static struct zona con_ombra(unsigned int ruolo, int x, int y, int l, int a) {
    if (ruolo == RUOLO_SFONDO) return (struct zona){ x, y, x + l, y + a };
    return (struct zona){ x - OMBRA_MARGINE, y - OMBRA_MARGINE,
                          x + l + OMBRA_MARGINE, y + a + OMBRA_MARGINE_GIU };
}

static int si_toccano(struct zona p, struct zona q) {
    return p.x0 < q.x1 && q.x0 < p.x1 && p.y0 < q.y1 && q.y0 < p.y1;
}

/* Lavora sulla zona k: da adesso si disegna solo li' (anche il pennello). */
static void nella_zona(int k) {
    zx0 = zone[k].x0; zy0 = zone[k].y0; zx1 = zone[k].x1; zy1 = zone[k].y1;
    pennello.s.rx = zx0; pennello.s.ry = zy0;
    pennello.s.rx2 = zx1; pennello.s.ry2 = zy1;
}

static int nella_zona_si_vede(const struct finestra_compositore *f) {
    struct zona z = { zx0, zy0, zx1, zy1 };
    return si_toccano(con_ombra(f->ruolo, f->x, f->y, f->l, f->a), z);
}

/* UNA FINESTRA (o un pannello: lo sfondo no, e' gia' in 'fondo'), in tutte le zone da
 * rifare. In due tempi, come in una composizione intera: prima l'ombra in tutte le zone, poi
 * (se va rifatta) la sfocatura, che guarda la scena fatta fin qui (ombra compresa), poi vetro,
 * immagine e barra in tutte le zone. */
static void disegna_finestra(int i, int ricalcola) {
    const struct finestra_compositore *f = &finestre[i];
    for (int k = 0; k < quante_zone; k++) {
        nella_zona(k);
        if (nella_zona_si_vede(f)) disegna_ombra(i);
    }
    if (ricalcola) rifai_vetro(i);
    for (int k = 0; k < quante_zone; k++) {
        nella_zona(k);
        if (!nella_zona_si_vede(f)) continue;
        metti_sfocatura(i);
        immagine(f->x, f->y, f->l, f->a, f->immagini[f->mostrata],
                 f->non_risponde ? barra_congelata_altezza(MINI) : 0);
        if (f->non_risponde && f->ruolo == RUOLO_FINESTRA)
            barra_congelata_disegna(&pennello, f->x, f->y, f->l, f->a, f->titolo, MINI);
        if (scelta_sopra(i) >= 0)       /* velata da una finestra di scelta */
            velo_finestra_disegna(&pennello, f->x, f->y, f->l, f->a, f->raggio);
    }
}

/* Un messaggio a un'applicazione, senza aspettare. Se non si puo', la sua casella e'
 * piena: non legge piu', quindi e' bloccata. 1 se e' partito, 0 se no. (Se invece ha
 * chiuso, ce ne accorgeremo leggendo dal suo collegamento.) */
static int avvisa(int i, struct notizia n) {
    if (finestre[i].wayland) {          /* non ha un collegamento: la notizia va a Wayland */
        wayland_notizia(i, &n);
        return 1;
    }
    if (manda_subito(finestre[i].collegamento, &n, sizeof n) == sizeof n) return 1;
    finestre[i].non_risponde = 1;
    return 0;
}

static void racconta_attiva(void);

static void dai_fuoco(int i) {
    if (i == fuoco) return;
    int prima = fuoco;
    fuoco = i;
    /* chi perde il fuoco non sente piu' il puntatore che passa (vedi al_mouse): se lo aveva
     * sopra, gli si dice che e' uscito, cosi' spegne quello che aveva acceso */
    if (prima >= 0 && sotto == prima && presa != prima) {
        avvisa(prima, (struct notizia){ .tipo = USCITO });
        sotto = -1;
    }
    if (prima >= 0) avvisa(prima, (struct notizia){ .tipo = FUOCO, .fuoco = 0 });
    if (i >= 0) avvisa(i, (struct notizia){ .tipo = FUOCO, .fuoco = 1 });
    racconta_attiva();
}

/* La finestra normale piu' davanti tra quelle che si vedono, o -1: quando quella con il
 * fuoco sparisce (chiusa o ridotta), il fuoco passa a lei. */
static int piu_davanti(void) {
    for (int k = quante - 1; k >= 0; k--) {
        const struct finestra_compositore *f = &finestre[ordine[k]];
        if (f->usata && f->ruolo == RUOLO_FINESTRA && f->mostrata >= 0 && !f->ridotta) return ordine[k];
    }
    return -1;
}

static void in_primo_piano(int i) {
    int k = 0;
    while (k < quante && ordine[k] != i) k++;
    for (; k < quante - 1; k++) ordine[k] = ordine[k + 1];
    ordine[quante - 1] = i;
}

/* La finestra sotto il punto (x, y): si cerca dal primo piano in giu', perche' quella
 * davanti copre le altre. -1 se sotto c'e' lo sfondo. */
/* GLI STRATI, dal basso: le superfici SFONDO, poi le finestre normali nel loro ordine,
 * poi i PANNELLI. Si disegna in quest'ordine, e si cerca chi sta sotto il puntatore al
 * contrario. */
static const unsigned int strati[] = { RUOLO_SFONDO, RUOLO_FINESTRA, RUOLO_SCELTA, RUOLO_PANNELLO };
#define QUANTI_STRATI 4

static int finestra_sotto(int x, int y) {
    for (int s = QUANTI_STRATI - 1; s >= 0; s--)
        for (int k = quante - 1; k >= 0; k--) {
            const struct finestra_compositore *f = &finestre[ordine[k]];
            if (f->mostrata < 0 || f->ridotta || f->ruolo != strati[s]) continue;
            if (x >= f->x && x < f->x + f->l && y >= f->y && y < f->y + f->a)
                return ordine[k];
        }
    return -1;
}

/* FINESTRA_APERTA e FINESTRA_CHIUSA a tutti i pannelli (la scrivania): una finestra
 * normale e' comparsa, o non c'e' piu'. */
static void racconta_a(int pannello, int i, unsigned int tipo) {
    struct notizia n = { .tipo = tipo, .numero = finestre[i].id };
    for (int k = 0; k < MAX_PERCORSO_PROGRAMMA; k++) n.testo[k] = finestre[i].programma[k];
    avvisa(pannello, n);
}

static void racconta(int i, unsigned int tipo) {
    for (int k = 0; k < MAX_FINESTRE; k++)
        if (finestre[k].usata && finestre[k].ruolo == RUOLO_PANNELLO && finestre[k].immagini[0])
            racconta_a(k, i, tipo);
}

/* FINESTRA_ATTIVA: quale finestra ha il fuoco (0 nessuna). */
static void racconta_attiva_a(int pannello) {
    avvisa(pannello, (struct notizia){ .tipo = FINESTRA_ATTIVA,
                                       .numero = fuoco >= 0 ? finestre[fuoco].id : 0 });
}

static void racconta_attiva(void) {
    for (int k = 0; k < MAX_FINESTRE; k++)
        if (finestre[k].usata && finestre[k].ruolo == RUOLO_PANNELLO && finestre[k].mostrata >= 0)
            racconta_attiva_a(k);
}

static void scollega(int i) {
    struct finestra_compositore *f = &finestre[i];
    if (!f->usata) return;
    if (f->annunciata) racconta(i, FINESTRA_CHIUSA);
    if (f->immagini[0] && !f->wayland) togli(f->immagini[0]);   /* quella di Wayland non e' un foglio */
    if (f->foglio >= 0) chiudi(f->foglio);
    if (f->collegamento >= 0) chiudi(f->collegamento);
    f->usata = 0;
    int k = 0;
    while (k < quante && ordine[k] != i) k++;
    for (; k < quante - 1; k++) ordine[k] = ordine[k + 1];
    if (k < quante) quante--;
    if (sotto == i) sotto = -1;
    if (presa == i) presa = -1;
    if (trascinata == i) trascinata = -1;
    if (fuoco == i) {                 /* il fuoco passa alla finestra che c'era sotto */
        fuoco = -1;
        int dopo = piu_davanti();
        if (f->ruolo == RUOLO_SCELTA) {   /* una scelta: all'app che copriva, se si vede ancora */
            for (int k = quante - 1; k >= 0; k--) {
                const struct finestra_compositore *g = &finestre[ordine[k]];
                if (g->usata && g->ruolo == RUOLO_FINESTRA && g->mostrata >= 0 && !g->ridotta &&
                    stesso_programma(g->programma, f->sopra)) { dopo = ordine[k]; break; }
            }
        }
        dai_fuoco(dopo);
    }
}

/* Un collegamento nuovo 'd' (accettato, o arrivato da una porta): si aspetta il suo
 * VOGLIO_FINESTRA. Il suo posto, o -1 (troppe finestre: 'd' si chiude). */
static int nuova_finestra(int d) {
    int i = 0;
    while (i < MAX_FINESTRE && finestre[i].usata) i++;
    if (i == MAX_FINESTRE) {   /* troppe finestre: il collegamento nuovo si rifiuta, il compositore resta */
        scrivi_riga("compositore", "troppe finestre: collegamento rifiutato", "");
        if (d >= 0) chiudi(d);
        return -1;
    }
    struct finestra_compositore *f = &finestre[i];
    *f = (struct finestra_compositore){ .usata = 1, .collegamento = d, .foglio = -1, .mostrata = -1,
                            .da_rilasciare = -1, .ultimo_ping = tempo() };
    ordine[quante++] = i;
    return i;
}

static void nuovo_collegamento(int ascolto) {
    int d = accetta(ascolto);
    if (d >= 0) nuova_finestra(d);
}

/* PORTA_APP sul collegamento 'i': solo dal sistema (il launcher; lo dice il kernel), prima di
 * ogni finestra, con il capo allegato. Si tiene la porta; il collegamento 'i' si chiude. */
static void porta_app(int i, const struct porta_app *m, int capo) {
    int k = 0;
    while (k < MAX_PORTE && porte[k].usata) k++;
    if (capo < 0 || finestre[i].immagini[0] || chi(finestre[i].collegamento) != UID_SISTEMA || k == MAX_PORTE) {
        if (capo >= 0) chiudi(capo);
        scollega(i);
        return;
    }
    porte[k].usata = 1;
    porte[k].d = capo;
    for (int c = 0; c < MAX_PERCORSO_PROGRAMMA; c++) porte[k].programma[c] = m->programma[c];
    porte[k].programma[MAX_PERCORSO_PROGRAMMA - 1] = 0;
    scrivi_riga("compositore", "porta dell'app ", porte[k].programma);
    scollega(i);
}

/* Un messaggio sulla porta 'k': NUOVO_COLLEGAMENTO con il capo allegato diventa una finestra
 * dell'app della porta. L'app chiusa, o un messaggio sbagliato: la porta si chiude. */
static void da_porta(int k) {
    struct nuovo_collegamento m;
    int capo = -1;
    int n = ricevi_foglio(porte[k].d, &m, sizeof m, &capo);
    if (n == sizeof m && m.tipo == NUOVO_COLLEGAMENTO && capo >= 0) {
        int i = nuova_finestra(capo);
        if (i >= 0) {
            for (int c = 0; c < MAX_PERCORSO_PROGRAMMA; c++) finestre[i].programma[c] = porte[k].programma[c];
            finestre[i].dalla_porta = 1;
        }
        return;
    }
    if (capo >= 0) chiudi(capo);
    chiudi(porte[k].d);
    porte[k].usata = 0;
}

/* Il primo messaggio: la finestra, con il suo foglio. Tutto si controlla. */
static void voglio_finestra(int i, const struct voglio_finestra *m, int foglio) {
    struct finestra_compositore *f = &finestre[i];
    if (f->immagini[0]) {                /* ne ha gia' una: messaggio sbagliato */
        if (foglio >= 0) chiudi(foglio);
        scollega(i);
        return;
    }
    f->foglio = foglio;
    int l = m->larghezza, a = m->altezza;
    unsigned int byte;
    unsigned int *pixel = foglio >= 0 ? mappa(foglio, &byte) : 0;
    /* i ruoli speciali solo al sistema: lo dice il kernel, non lo si puo' fingere */
    int ruolo_valido = m->ruolo == RUOLO_FINESTRA ||
                       ((m->ruolo == RUOLO_SFONDO || m->ruolo == RUOLO_PANNELLO) &&
                        chi(f->collegamento) == UID_SISTEMA);
    if (m->ruolo == RUOLO_SCELTA) {   /* la scelta solo dal servizio picker: lo dice il kernel */
        char chi_e[MAX_PERCORSO_PROGRAMMA] = "";
        if (programma_di(f->collegamento, chi_e) < 0) chi_e[0] = 0;
        ruolo_valido = stesso_programma(chi_e, PROGRAMMA_PICKER);
        if (!ruolo_valido) scrivi_riga("compositore", "finestra di scelta rifiutata a ", chi_e);
    }
    if (!pixel || !ruolo_valido || l < 1 || a < 1 || l > larghezza || a > altezza ||
        byte < 2u * l * a * 4) {
        if (pixel) f->immagini[0] = pixel;   /* lo toglie scollega */
        scollega(i);
        return;
    }
    f->immagini[0] = pixel;
    f->immagini[1] = pixel + l * a;
    f->l = l;
    f->a = a;
    for (int k = 0; k < MAX_TITOLO; k++) f->titolo[k] = m->titolo[k];
    f->titolo[MAX_TITOLO - 1] = 0;
    if (!f->dalla_porta) programma_di(f->collegamento, f->programma);
    f->id = prossimo_id++;
    f->ruolo = m->ruolo;
    f->pannello = m->pannello;
    f->raggio = m->raggio > (unsigned int)(l < a ? l : a) / 2 ? (l < a ? l : a) / 2 : (int)m->raggio;
    if (f->ruolo == RUOLO_FINESTRA) {
        f->x = 60 + i * 40;   /* a cascata */
        f->y = 50 + i * 40;
    } else if (f->ruolo == RUOLO_SCELTA) {
        /* centrata sopra la finestra piu' in primo piano dell'app 'sopra'; se non ne ha, sullo
         * schermo. Dentro lo schermo comunque. */
        for (int k = 0; k < MAX_PERCORSO_PROGRAMMA - 1 && m->sopra[k]; k++) f->sopra[k] = m->sopra[k];
        int cx = larghezza / 2, cy = altezza / 2;
        for (int k = quante - 1; k >= 0; k--) {
            const struct finestra_compositore *g = &finestre[ordine[k]];
            if (!g->usata || g->ruolo != RUOLO_FINESTRA || g->mostrata < 0 || g->ridotta ||
                !stesso_programma(g->programma, f->sopra))
                continue;
            cx = g->x + g->l / 2;
            cy = g->y + g->a / 2;
            break;
        }
        f->x = cx - l / 2;
        f->y = cy - a / 2;
        if (f->x + l > larghezza) f->x = larghezza - l;
        if (f->y + a > altezza) f->y = altezza - a;
        if (f->x < 0) f->x = 0;
        if (f->y < 0) f->y = 0;
    } else {
        f->x = m->x;          /* le superfici speciali stanno dove dicono loro */
        f->y = m->y;
    }
    if (f->ruolo != RUOLO_SFONDO) prepara_ombra(i);
}

static void mostra(int i, const struct mostra *m) {
    struct finestra_compositore *f = &finestre[i];
    if (!f->immagini[0] || m->immagine > 1) { scollega(i); return; }
    if (m->quante > MAX_ZONE_MOSTRA) { scollega(i); return; }
    int prima = f->mostrata;
    f->mostrata = m->immagine;
    f->generazione++;
    /* le zone cambiate si sommano a quelle dei MOSTRA non ancora composti (se ne arrivano due
     * prima di una composizione, si vede solo l'ultima immagine: e' cambiata in tutte e due) */
    if (!m->quante) f->quante_cambiate = -1;
    struct zona_finestra tutta = { 0, 0, f->l, f->a };
    for (unsigned int k = 0; k < m->quante && f->quante_cambiate >= 0; k++)
        zone_aggiungi(f->cambiate, &f->quante_cambiate, MAX_ZONE_MOSTRA, zona_dentro(m->zone[k], tutta));
    if (prima < 0 && f->ruolo == RUOLO_SCELTA) {     /* la scelta: davanti, con il fuoco, non annunciata */
        in_primo_piano(i);
        dai_fuoco(i);
    } else if (prima < 0 && f->ruolo == RUOLO_FINESTRA) {   /* la prima volta: davanti e con il fuoco */
        in_primo_piano(i);
        if (scelta_sopra(i) < 0) dai_fuoco(i);       /* se e' velata il fuoco resta alla scelta */
        f->annunciata = 1;
        racconta(i, FINESTRA_APERTA);
    } else if (prima < 0 && f->ruolo == RUOLO_PANNELLO) {   /* un pannello nuovo: le finestre che ci sono */
        for (int k = 0; k < MAX_FINESTRE; k++)
            if (finestre[k].usata && finestre[k].annunciata) racconta_a(i, k, FINESTRA_APERTA);
        racconta_attiva_a(i);
    } else if (prima != f->mostrata) {
        if (!avvisa(i, (struct notizia){ .tipo = RILASCIATA, .immagine = prima }))
            f->da_rilasciare = prima;
    }
}

/* IL PONTE VERSO WAYLAND (compositore_wayland.h). Il server Wayland sta dentro questo
 * programma (compositore_wayland.c): i programmi Wayland non passano dal nostro protocollo
 * (collegamento + foglio), ma una loro finestra diventa uno di questi posti di 'finestre', come
 * le altre. Cosa cambia per un posto con 'wayland' a 1:
 *   - non ha collegamento ne' foglio: chi parla con quel programma e' Wayland, e le notizie
 *     (il fuoco) gli arrivano da avvisa;
 *   - l'immagine e' memoria di Wayland (cornice del compositore piu' contenuto del programma),
 *     sempre l'immagine 0: ogni volta che cambia, ponte_mostra e' il suo MOSTRA;
 *   - non si manda il "ci sei?": Wayland ha i suoi ping;
 *   - la cornice (titolo, riduci, chiudi, trascinamento) la gestisce il compositore: lo fa
 *     al_mouse, perche' il programma non sa dove sta il puntatore.
 * Il resto (ombra, vetro, primo piano, fuoco, userbar) e' quello di tutte le finestre. */

/* Un posto per la finestra di un programma Wayland, di cui 'cliente' e' il socket (serve a
 * sapere di chi e': programma_di). Il numero del posto, o -1 se sono tutti presi. */
int ponte_apri(int cliente) {
    int i = nuova_finestra(-1);
    if (i < 0) return -1;
    struct finestra_compositore *f = &finestre[i];
    f->wayland = 1;
    f->ruolo = RUOLO_FINESTRA;
    if (programma_di(cliente, f->programma) < 0) f->programma[0] = 0;
    return i;
}

/* L'immagine del posto 'i' e' pronta (o e' cambiata): 'pixel' sono l x a pixel premoltiplicati,
 * titolo compreso. La prima volta la finestra compare (a cascata, davanti, con il fuoco);
 * se cambia la misura si rifa' l'ombra. */
void ponte_mostra(int i, unsigned int *pixel, int l, int a, const char *titolo) {
    struct finestra_compositore *f = &finestre[i];
    if (!f->usata || !f->wayland || l < 1 || a < 1 || l > larghezza || a > altezza) return;
    int nuova = !f->immagini[0];
    int misura = f->l != l || f->a != a;
    f->immagini[0] = f->immagini[1] = pixel;
    f->l = l;
    f->a = a;
    for (int k = 0; k < MAX_TITOLO - 1 && titolo[k]; k++) f->titolo[k] = titolo[k];
    f->titolo[MAX_TITOLO - 1] = 0;
    if (nuova) {
        f->id = prossimo_id++;
        f->raggio = cornice_raggio();
        f->x = 60 + i * 40;   /* a cascata, dentro lo schermo */
        f->y = 50 + i * 40;
        if (f->x + l > larghezza) f->x = larghezza - l;
        if (f->y + a > altezza) f->y = altezza - a;
    }
    if (nuova || misura) prepara_ombra(i);
    mostra(i, &(struct mostra){ .tipo = MOSTRA, .immagine = 0, .quante = 0 });   /* tutta cambiata */
}

/* Wayland non ha piu' la finestra del posto 'i' (il programma l'ha chiusa o e' finito). */
void ponte_togli(int i) {
    if (finestre[i].usata && finestre[i].wayland) scollega(i);
}

/* Si vede (non e' ridotta)? Una finestra nascosta non riceve i "disegna il prossimo fotogramma". */
int ponte_visibile(int i) {
    return finestre[i].usata && finestre[i].mostrata >= 0 && !finestre[i].ridotta;
}

/* Quanto e' grande lo schermo. */
void ponte_schermo(int *l, int *a) {
    *l = larghezza;
    *a = altezza;
}

/* PONG: ha risposto. Se era bloccata, torna normale, e le si dice quello che si era
 * perso: l'immagine da restituire e se ha il fuoco. */
static void pong(int i, const struct pong *m) {
    struct finestra_compositore *f = &finestre[i];
    if (!f->ping_atteso || m->numero != f->ping_numero) return;   /* una risposta vecchia */
    f->ping_atteso = 0;
    if (!f->non_risponde) return;
    f->non_risponde = 0;
    if (f->da_rilasciare >= 0 &&
        avvisa(i, (struct notizia){ .tipo = RILASCIATA, .immagine = f->da_rilasciare }))
        f->da_rilasciare = -1;
    avvisa(i, (struct notizia){ .tipo = FUOCO, .fuoco = i == fuoco });
}

/* Ogni secondo un "ci sei?" a chi ha una finestra; senza risposta in 3 secondi, e'
 * bloccata. */
static void controlla_risposte(void) {
    unsigned int adesso = tempo();
    for (int i = 0; i < MAX_FINESTRE; i++) {
        struct finestra_compositore *f = &finestre[i];
        if (!f->usata || !f->immagini[0] || f->wayland) continue;   /* Wayland ha i suoi ping */
        /* Con sopra una finestra di scelta (Apri/Salva) l'app aspetta la risposta e non legge: non si giudica,
         * e il conto riparte (deciso il 17/11/2026, STATO.md sez. 7). */
        if (f->ping_atteso && scelta_sopra(i) >= 0) f->ultimo_ping = adesso;
        else if (f->ping_atteso && adesso - f->ultimo_ping >= 3000) f->non_risponde = 1;
        if (!f->ping_atteso && adesso - f->ultimo_ping >= 1000) {
            f->ultimo_ping = adesso;
            f->ping_numero = prossimo_ping++;
            if (avvisa(i, (struct notizia){ .tipo = PING, .numero = f->ping_numero }))
                f->ping_atteso = 1;
        }
    }
}

/* TRASCINAMI: vale solo se il pulsante premuto su questa finestra e' ancora giu'.
 * Se l'utente l'ha gia' lasciato (il messaggio e' arrivato tardi), non si sposta niente:
 * se no la finestra seguirebbe il mouse senza che nessuno la tenga. */
static void trascinami(int i) {
    if (presa != i || (finestre[i].ruolo != RUOLO_FINESTRA && finestre[i].ruolo != RUOLO_SCELTA)) return;   /* le superfici speciali no */
    trascinata = i;
    presa_x = px - finestre[i].x;
    presa_y = py - finestre[i].y;
    if (sotto == i) {   /* finche' si sposta, il puntatore non e' suo */
        avvisa(i, (struct notizia){ .tipo = USCITO });
        sotto = -1;
    }
}

/* RIDUCIMI: la finestra si nasconde, e il fuoco passa a quella che c'era sotto. */
static void riducimi(int i) {
    struct finestra_compositore *f = &finestre[i];
    if (f->ruolo != RUOLO_FINESTRA || f->mostrata < 0 || f->ridotta) return;
    f->ridotta = 1;
    if (presa == i) presa = -1;
    if (trascinata == i) trascinata = -1;
    if (sotto == i) {
        avvisa(i, (struct notizia){ .tipo = USCITO });
        sotto = -1;
    }
    if (fuoco == i) dai_fuoco(piu_davanti());
}

/* PORTA_DAVANTI, da un pannello (la scrivania): la finestra torna visibile, davanti a
 * tutte, con il fuoco. */
static void porta_davanti(int pannello, unsigned int numero) {
    if (finestre[pannello].ruolo != RUOLO_PANNELLO) return;
    for (int i = 0; i < MAX_FINESTRE; i++) {
        struct finestra_compositore *f = &finestre[i];
        if (!f->usata || f->ruolo != RUOLO_FINESTRA || f->id != numero || f->mostrata < 0) continue;
        f->ridotta = 0;
        in_primo_piano(i);
        dai_fuoco(i);
        return;
    }
}

/* IL TEMA (gkit/sistema/protocollo.h): lo tiene catalog (corekit/catalog.h), che avvisa chi
 * osserva quando cambia. Il compositore osserva per le sue cose (le ombre intorno alle finestre
 * e la barra di emergenza dipendono dal tema); le app e la scrivania osservano da sole, e si
 * ridisegnano da sole. -1 se catalog non risponde. */
static int avvisi_catalog = -1;

/* Il tema nuovo: si sceglie, si rifanno le ombre di tutte le finestre e tutto il fotogramma. */
static void tema_nuovo(const struct catalog_valori *v) {
    compositore_scegli_tema(v);
    for (int k = 0; k < MAX_FINESTRE; k++)
        if (finestre[k].usata && finestre[k].immagini[0] && finestre[k].ruolo != RUOLO_SFONDO) prepara_ombra(k);
    tutto_da_rifare = 1;
}

/* Un avviso di catalog. Se catalog e' ripartito, ci si ricollega una volta. */
static void avviso_catalog(void) {
    struct catalog_valori v;
    int r = catalog_avviso(avvisi_catalog, &v);
    if (r == 0) {
        chiudi(avvisi_catalog);
        avvisi_catalog = catalog_osserva(&v);
        r = avvisi_catalog >= 0;
    }
    if (r == 1) tema_nuovo(&v);
}

/* Un messaggio da un'applicazione. */
static void da_applicazione(int i) {
    union {
        unsigned int tipo;
        struct chiedi_schermo c;
        struct riducimi r;
        struct voglio_tasti_fisici k;
        struct porta_davanti d;
        struct voglio_finestra v;
        struct mostra m;
        struct trascinami t;
        struct mostra_puntatore u;
        struct pong p;
        struct porta_app a;
    } m;
    int foglio = -1;
    int n = ricevi_foglio(finestre[i].collegamento, &m, sizeof m, &foglio);
    if (n <= 0) { if (foglio >= 0) chiudi(foglio); scollega(i); return; }   /* ha chiuso */
    if (m.tipo == VOGLIO_FINESTRA && n == sizeof m.v) { voglio_finestra(i, &m.v, foglio); return; }
    if (m.tipo == PORTA_APP && n == sizeof m.a) { porta_app(i, &m.a, foglio); return; }
    if (foglio >= 0) chiudi(foglio);   /* un foglio va solo col primo messaggio */
    if (m.tipo == MOSTRA && n == sizeof m.m) { mostra(i, &m.m); return; }
    if (m.tipo == TRASCINAMI && n == sizeof m.t && finestre[i].immagini[0]) { trascinami(i); return; }
    if (m.tipo == MOSTRA_PUNTATORE && n == sizeof m.u && m.u.forma <= FORMA_ATTESA) {
        finestre[i].forma = m.u.forma;
        return;
    }
    if (m.tipo == PONG && n == sizeof m.p) { pong(i, &m.p); return; }
    if (m.tipo == VOGLIO_TASTI_FISICI && n == sizeof m.k) { finestre[i].tasti_fisici = 1; return; }
    if (m.tipo == RIDUCIMI && n == sizeof m.r) { riducimi(i); return; }
    if (m.tipo == PORTA_DAVANTI && n == sizeof m.d) { porta_davanti(i, m.d.numero); return; }
    if (m.tipo == CHIEDI_SCHERMO && n == sizeof m.c) {
        avvisa(i, (struct notizia){ .tipo = SCHERMO, .x = larghezza, .y = altezza });
        return;
    }
    scollega(i);                       /* messaggio sbagliato */
}

/* --- il puntatore --- */

/* LE FORME DEL PUNTATORE (disegno: "Componenti base", Puntatore): la freccia, la mano, la barra a
 * I, la freccia col divieto e l'anello dell'attesa, ognuna con il suo contorno, la sua ombra e
 * il suo punto caldo. Sono le immagini del disegno, esportate da Figma e usate cosi' come sono
 * (risorse/puntatore/, convertite in puntatori.h da strumenti/grafica/puntatore.py): ogni pixel
 * e' gia' colore e ombra insieme, premoltiplicato, uguale nel chiaro e nello scuro. Qui si
 * mescolano con la scena, e basta.
 *
 * IL PUNTO CALDO, (px, py), e' dove sta il puntatore: nell'immagine e' il pixel (immagine.px,
 * immagine.py), quindi si disegna a partire da (px - immagine.px, py - immagine.py). Nell'immagine c'e'
 * il margine per l'ombra, che sborda anche a sinistra e in alto.
 *
 * QUALE FORMA. La dice l'applicazione (MOSTRA_PUNTATORE) per quando il puntatore sta sulla sua
 * finestra: forma_voluta() guarda la finestra sotto il puntatore (o quella "presa", mentre un
 * pulsante e' premuto). Un'applicazione bloccata mostra sempre l'attesa. L'attesa gira da
 * sola, 1 giro al secondo: l'immagine dipende dall'ora (fotogramma_ora), e finche' c'e' si
 * ricompone ogni 1/FOTOGRAMMI_ATTESA di secondo. */
#include "puntatori.h"

/* un'immagine di una forma: i pixel e le misure; il fotogramma conta solo per l'attesa */
struct forma_ora {
    const unsigned int *pixel;
    int l, a, px, py;
};

static struct forma_ora immagine_forma(unsigned int forma, int fotogramma) {
    const struct forma_puntatore_immagine *f = &forme_puntatore[forma];
    return (struct forma_ora){ forma == FORMA_ATTESA ? fotogrammi_attesa[fotogramma] : f->pixel,
                               f->l, f->a, f->px, f->py };
}

/* --- dalla scena allo schermo --- */

/* Un pixel della scena (o della freccia) come lo vuole lo schermo. La scena e' tutta
 * opaca: i colori sono gia' quelli veri. Se lo schermo li vuole in un altro ordine, si
 * spostano. */
static unsigned int per_lo_schermo(unsigned int c) {
    if (schermo.rosso == 16 && schermo.verde == 8 && schermo.blu == 0) return c & 0xFFFFFF;
    return ((c >> 16) & 0xFF) << schermo.rosso | ((c >> 8) & 0xFF) << schermo.verde |
           (c & 0xFF) << schermo.blu;
}

/* Copia sullo schermo un rettangolo della scena (tagliato dentro lo schermo). */
static void copia(int x0, int y0, int x1, int y1) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > larghezza) x1 = larghezza;
    if (y1 > altezza) y1 = altezza;
    int stesso_ordine = schermo.rosso == 16 && schermo.verde == 8 && schermo.blu == 0;
    for (int y = y0; y < y1; y++) {
        unsigned int *da = &disegno[y * larghezza];
        unsigned int *a = (unsigned int *)(memoria + y * schermo.passo);
        if (stesso_ordine) {
            for (int x = x0; x < x1; x++) a[x] = da[x] & 0xFFFFFF;
        } else {
            for (int x = x0; x < x1; x++) a[x] = per_lo_schermo(da[x]);
        }
    }
}

/* Il puntatore, direttamente sullo schermo (non nella scena). I suoi pixel sono in parte
 * trasparenti (gli orli, l'ombra): ognuno si mescola con il pixel della SCENA che sta
 * sotto (da 'disegno', non dallo schermo, dove potrebbe esserci ancora il puntatore di
 * prima), e il risultato va sullo schermo. Solo il rettangolo dell'immagine, tagliato
 * dentro lo schermo. */
static void disegna_puntatore(const struct forma_ora *im) {
    int x0 = px - im->px, y0 = py - im->py;
    int r0 = y0 < 0 ? -y0 : 0, r1 = altezza - y0 < im->a ? altezza - y0 : im->a;
    int k0 = x0 < 0 ? -x0 : 0, k1 = larghezza - x0 < im->l ? larghezza - x0 : im->l;
    for (int r = r0; r < r1; r++) {
        const unsigned int *sotto = &disegno[(y0 + r) * larghezza];
        unsigned int *riga = (unsigned int *)(memoria + (y0 + r) * schermo.passo);
        for (int k = k0; k < k1; k++)
            riga[x0 + k] = per_lo_schermo(sopra_a(im->pixel[r * im->l + k], sotto[x0 + k]));
    }
}

/* Il rettangolo dell'immagine (ombra compresa) con il punto caldo in (x, y), tagliato dentro
 * lo schermo. */
static struct zona_schermo dentro_lo_schermo(const struct forma_ora *im, int x, int y) {
    int x0 = x - im->px, y0 = y - im->py;
    return (struct zona_schermo){ x0 > 0 ? x0 : 0, y0 > 0 ? y0 : 0,
                                  x0 + im->l < larghezza ? x0 + im->l : larghezza,
                                  y0 + im->a < altezza ? y0 + im->a : altezza };
}

/* --- la scena, poi sullo schermo --- */

/* L'ELENCO DI QUELLE DA DISEGNARE, nell'ordine in cui si disegnano: gli strati dal
 * basso (sfondo, finestre, pannelli), e dentro ogni strato dal fondo al primo piano
 * ('lista', 'in_lista': dichiarate sopra, con il vetro). */
static void fai_lista(void) {
    in_lista = 0;
    for (int s = 0; s < QUANTI_STRATI; s++)
        for (int k = 0; k < quante; k++) {
            const struct finestra_compositore *f = &finestre[ordine[k]];
            if (f->mostrata >= 0 && !f->ridotta && f->ruolo == strati[s]) lista[in_lista++] = ordine[k];
        }
}

/* L'IMPRONTA di una finestra nel fotogramma: tutto quello che decide come appare.
 * 'posto' e' dove sta in 'lista' (0 la piu' in fondo); 'id' dice se e' ancora lei (un
 * posto in 'finestre' si puo' riusare per un'altra). */
struct impronta {
    int vista, x, y, l, a, posto, col_fuoco, non_risponde, velata;
    unsigned int ruolo, generazione, id;
};
static struct impronta impronte_prima[MAX_FINESTRE];
static int px_prima, py_prima;
static struct forma_ora immagine_prima;   /* l'immagine del puntatore di prima: da cancellare */
static unsigned int forma_prima = FORMA_NORMALE;
static int fotogramma_prima;


/* Le zone da rifare: si aggiunge un rettangolo alla volta. Le zone non devono mai
 * toccarsi (in una parte comune a due zone si disegnerebbe due volte, e le cose
 * semitrasparenti, come le ombre, verrebbero piu' scure): se il nuovo ne tocca una, la
 * si unisce a lui (il rettangolo che le contiene tutte e due), e si ricontrolla. Le si
 * unisce anche se sono vicine e l'unione costa poco in piu'. Se le zone sono troppe,
 * una sola che le contiene tutte. */
static int area(struct zona z) { return (z.x1 - z.x0) * (z.y1 - z.y0); }

static struct zona unione(struct zona p, struct zona q) {
    return (struct zona){ p.x0 < q.x0 ? p.x0 : q.x0, p.y0 < q.y0 ? p.y0 : q.y0,
                          p.x1 > q.x1 ? p.x1 : q.x1, p.y1 > q.y1 ? p.y1 : q.y1 };
}

static void zona_aggiungi(struct zona z) {
    if (z.x0 < 0) z.x0 = 0;
    if (z.y0 < 0) z.y0 = 0;
    if (z.x1 > larghezza) z.x1 = larghezza;
    if (z.y1 > altezza) z.y1 = altezza;
    if (z.x0 >= z.x1 || z.y0 >= z.y1) return;
    for (int k = 0; k < quante_zone; k++) {
        struct zona u = unione(z, zone[k]);
        int poco_di_piu = (area(u) - area(z) - area(zone[k])) * 4 <= area(z) + area(zone[k]);
        if (si_toccano(z, zone[k]) || poco_di_piu) {
            z = u;
            zone[k] = zone[--quante_zone];
            k = -1;                     /* la nuova, piu' grande, puo' toccarne altre */
        }
    }
    if (quante_zone == MAX_ZONE) {
        for (int k = 0; k < quante_zone; k++) z = unione(z, zone[k]);
        quante_zone = 0;
    }
    zone[quante_zone++] = z;
}

/* COSA E' CAMBIATO dall'ultimo fotogramma, finestra per finestra: i rettangoli dove i
 * suoi pixel non sono piu' quelli di prima ('danni'). */
#define MAX_DANNI (MAX_ZONE_MOSTRA + 2)
static struct zona danni[MAX_FINESTRE][MAX_DANNI];
static int quanti_danni[MAX_FINESTRE];
static int da_sfocare[MAX_FINESTRE];   /* il suo smerigliato va rifatto */

static void danno(int i, struct zona z) {
    if (quanti_danni[i] < MAX_DANNI) danni[i][quanti_danni[i]++] = z;
    else danni[i][MAX_DANNI - 1] = unione(danni[i][MAX_DANNI - 1], z);   /* piena: l'ultima si allarga */
}

static int danni_toccano(int i, struct zona z) {
    for (int d = 0; d < quanti_danni[i]; d++)
        if (si_toccano(danni[i][d], z)) return 1;
    return 0;
}

static struct zona impronta_con_ombra(const struct impronta *p) {
    return con_ombra(p->ruolo, p->x, p->y, p->l, p->a);
}

static struct zona impronta_dentro(const struct impronta *p) {
    return (struct zona){ p->x, p->y, p->x + p->l, p->y + p->a };
}

/* COSA RIFARE, confrontando con il fotogramma prima. Riempie le zone (zone[]) e dice
 * di quali finestre va rifatto lo smerigliato (da_sfocare[]).
 *
 * 1. Ogni finestra, da sola: cosa e' cambiato in lei e dove.
 *      comparsa, sparita, spostata:  il posto di prima e quello di adesso, ombra compresa
 *      ha preso o perso il fuoco:    lei con l'ombra (cambia il colore dell'ombra)
 *      immagine nuova, non risponde: solo il suo rettangolo
 * 2. LO SMERIGLIATO. Quello di una finestra e' fatto della scena SOTTO di lei, li'
 *    intorno (guardata). Va rifatto se la finestra si e' spostata o e' nuova, se e'
 *    cambiata la sua ombra (la guarda anche lei), o se qualcosa di quello che guarda e'
 *    cambiato in una finestra che le sta sotto (prima o adesso). Anche se due finestre
 *    si sono scambiate di posto (una e' passata davanti): quella che prima stava sotto e
 *    adesso no, o il contrario, cambia quello che c'e' dietro. Si guarda dal basso in
 *    alto: uno smerigliato rifatto e' un cambiamento anche lui (dentro la finestra), e
 *    puo' far rifare quello delle finestre sopra, a catena.
 * 3. Le zone: tutti i danni, i pezzi dove due finestre scambiate si sovrappongono, e
 *    tutto quello che guarda ogni smerigliato da rifare: per rifarlo bisogna ricomporre
 *    li' la scena, dal fondo fino a lei.
 * Il risultato e' lo stesso, pixel per pixel, che si avrebbe ricomponendo tutto. */
static void cosa_rifare(void) {
    struct impronta adesso[MAX_FINESTRE] = { 0 };
    for (int k = 0; k < in_lista; k++) {
        int i = lista[k];
        const struct finestra_compositore *f = &finestre[i];
        adesso[i] = (struct impronta){ 1, f->x, f->y, f->l, f->a, k, i == fuoco, f->non_risponde,
                                       scelta_sopra(i) >= 0, f->ruolo, f->generazione, f->id };
    }
    quante_zone = 0;
    for (int i = 0; i < MAX_FINESTRE; i++) quanti_danni[i] = da_sfocare[i] = 0;

    if (tutto_da_rifare) {              /* tutto, sfocature comprese */
        tutto_da_rifare = 0;
        zona_aggiungi((struct zona){ 0, 0, larghezza, altezza });
        for (int i = 0; i < MAX_FINESTRE; i++) da_sfocare[i] = adesso[i].vista;
        for (int i = 0; i < MAX_FINESTRE; i++) piani[i] = (struct piano){ .tutto = 1 };
        fondo_pronto = 0;
        for (int i = 0; i < MAX_FINESTRE; i++) impronte_prima[i] = adesso[i];
        for (int i = 0; i < MAX_FINESTRE; i++) finestre[i].quante_cambiate = 0;
        return;
    }

    /* 1. ognuna da sola */
    int stessa[MAX_FINESTRE];   /* c'era e c'e', la stessa finestra, nello stesso posto */
    for (int i = 0; i < MAX_FINESTRE; i++) {
        const struct impronta *p = &impronte_prima[i], *q = &adesso[i];
        stessa[i] = p->vista && q->vista && p->id == q->id && p->x == q->x && p->y == q->y &&
                    p->l == q->l && p->a == q->a && p->ruolo == q->ruolo;
        if (!p->vista && !q->vista) continue;
        if ((p->vista && p->ruolo == RUOLO_SFONDO) || (q->vista && q->ruolo == RUOLO_SFONDO))
            if (!stessa[i] || p->generazione != q->generazione || p->posto != q->posto)
                fondo_pronto = 0;               /* lo sfondo e' cambiato: anche il suo vetro */
        if (!stessa[i]) {
            if (p->vista) danno(i, impronta_con_ombra(p));
            if (q->vista) { danno(i, impronta_con_ombra(q)); da_sfocare[i] = 1; }
        } else if (p->col_fuoco != q->col_fuoco) {
            danno(i, impronta_con_ombra(q));
            da_sfocare[i] = 1;
        } else if (p->non_risponde != q->non_risponde || p->velata != q->velata ||
                   finestre[i].quante_cambiate < 0) {
            danno(i, impronta_dentro(q));
        } else if (p->generazione != q->generazione) {   /* solo dove l'applicazione dice */
            const struct finestra_compositore *f = &finestre[i];
            for (int k = 0; k < f->quante_cambiate; k++) {
                struct zona_finestra z = f->cambiate[k];
                if (!zona_vuota(z)) danno(i, (struct zona){ q->x + z.x, q->y + z.y, q->x + z.x + z.l, q->y + z.y + z.a });
            }
        }
    }

    /* 2. gli smerigliati, dal basso */
    for (int k = 0; k < in_lista; k++) {
        int i = lista[k];
        const struct impronta *q = &adesso[i];
        if (q->ruolo == RUOLO_SFONDO || da_sfocare[i]) continue;
        struct zona vista = guardata(&finestre[i]);
        for (int j = 0; j < MAX_FINESTRE && !da_sfocare[i]; j++) {
            if (j == i) continue;
            const struct impronta *pj = &impronte_prima[j], *qj = &adesso[j];
            int sotto_prima = pj->vista && pj->posto < impronte_prima[i].posto;
            int sotto_ora = qj->vista && qj->posto < q->posto;
            if (stessa[j] && sotto_prima != sotto_ora)    /* scambiate di posto */
                da_sfocare[i] = si_toccano(impronta_con_ombra(qj), vista);
            else if (sotto_prima || sotto_ora)
                da_sfocare[i] = danni_toccano(j, vista);
        }
        if (da_sfocare[i]) danno(i, impronta_dentro(q));
    }
    for (int k = 0; k < in_lista; k++)       /* come rifarli: tutti o a pezzi */
        if (da_sfocare[lista[k]] && adesso[lista[k]].ruolo != RUOLO_SFONDO) pianifica(lista[k], k);

    /* 3. le zone */
    for (int i = 0; i < MAX_FINESTRE; i++) {
        for (int d = 0; d < quanti_danni[i]; d++) zona_aggiungi(danni[i][d]);
        if (da_sfocare[i] && adesso[i].ruolo != RUOLO_SFONDO) {
            if (piani[i].tutto) zona_aggiungi(guardata(&finestre[i]));
            for (int k = 0; k < piani[i].quanti; k++) {   /* quello che guarda ogni pezzo */
                struct sfocata *s = &piccola;
                copia_per(piani[i].pezzi[k], s);
                zona_aggiungi(letta(s->x, s->y, s->l, s->a));
            }
        }
        for (int j = i + 1; j < MAX_FINESTRE; j++) {
            if (!stessa[i] || !stessa[j]) continue;
            int prima = impronte_prima[i].posto < impronte_prima[j].posto;
            int ora = adesso[i].posto < adesso[j].posto;
            struct zona a = impronta_con_ombra(&adesso[i]), b = impronta_con_ombra(&adesso[j]);
            if (prima != ora && si_toccano(a, b))
                zona_aggiungi((struct zona){ a.x0 > b.x0 ? a.x0 : b.x0, a.y0 > b.y0 ? a.y0 : b.y0,
                                             a.x1 < b.x1 ? a.x1 : b.x1, a.y1 < b.y1 ? a.y1 : b.y1 });
        }
    }
    for (int i = 0; i < MAX_FINESTRE; i++) impronte_prima[i] = adesso[i];
    for (int i = 0; i < MAX_FINESTRE; i++) finestre[i].quante_cambiate = 0;   /* usate */
}

/* La composizione delle zone: si comincia dallo sfondo da solo (il colore di quando non c'e'
 * e le superfici dello sfondo, gia' composti in 'fondo': le si copia), poi ci si disegnano le
 * altre finestre, una alla volta dal fondo, in tutte le zone (disegna_finestra): quando tocca
 * a una finestra di rifare lo smerigliato, la scena sotto di lei e' gia' pronta in tutto
 * quello che guarda. */
static void componi_zone(void) {
    if (!fondo_pronto) prepara_fondo();
    for (int k = 0; k < quante_zone; k++)
        for (int y = zone[k].y0; y < zone[k].y1; y++)
            for (int x = zone[k].x0; x < zone[k].x1; x++) disegno[y * larghezza + x] = fondo[y * larghezza + x];
    for (int k = 0; k < in_lista; k++)
        if (finestre[lista[k]].ruolo != RUOLO_SFONDO) disegna_finestra(lista[k], da_sfocare[lista[k]]);
}

/* Quale forma ha il puntatore adesso: quella voluta dalla finestra su cui sta (o dalla "presa"),
 * l'attesa se e' bloccata, la freccia se non e' su nessuna o si sta spostando una finestra. */
static unsigned int forma_voluta(void) {
    if (trascinata >= 0) return FORMA_NORMALE;
    int i = presa >= 0 ? presa : finestra_sotto(px, py);
    if (i < 0 || scelta_sopra(i) >= 0) return FORMA_NORMALE;
    return finestre[i].non_risponde ? FORMA_ATTESA : finestre[i].forma;
}

/* Il fotogramma dell'attesa all'ora di adesso: un giro al secondo, a velocita' costante. */
static int fotogramma_ora(void) { return (int)(tempo() % 1000) * FOTOGRAMMI_ATTESA / 1000; }

static void componi(void) {
    fai_lista();
    cosa_rifare();
    unsigned int forma = forma_voluta();
    int fotogramma = forma == FORMA_ATTESA ? fotogramma_ora() : 0;
    int mosso = px != px_prima || py != py_prima || forma != forma_prima || fotogramma != fotogramma_prima;
    if (!quante_zone && !mosso) return;

    componi_zone();

    /* sullo schermo: le zone, poi dove stava il puntatore (la scena senza di lui), poi
     * il puntatore dove sta adesso (sopra le zone appena copiate) */
    struct zona_schermo cambiate[MAX_ZONE + 2];
    int n = 0;
    for (int k = 0; k < quante_zone; k++) {
        copia(zone[k].x0, zone[k].y0, zone[k].x1, zone[k].y1);
        cambiate[n++] = (struct zona_schermo){ zone[k].x0, zone[k].y0, zone[k].x1, zone[k].y1 };
    }
    struct forma_ora adesso = immagine_forma(forma, fotogramma);
    if (mosso) {   /* tutto il rettangolo del puntatore di prima, ombra compresa */
        copia(px_prima - immagine_prima.px, py_prima - immagine_prima.py,
              px_prima - immagine_prima.px + immagine_prima.l, py_prima - immagine_prima.py + immagine_prima.a);
        cambiate[n++] = dentro_lo_schermo(&immagine_prima, px_prima, py_prima);
    }
    disegna_puntatore(&adesso);
    cambiate[n++] = dentro_lo_schermo(&adesso, px, py);
    schermo_cambiato(D_SCHERMO, cambiate, n);   /* tutto in una volta, a fine fotogramma */
    px_prima = px;
    py_prima = py;
    immagine_prima = adesso;
    forma_prima = forma;
    fotogramma_prima = fotogramma;
}

/* --- gli eventi del kernel: mouse e tastiera --- */

static unsigned int pulsanti_prima;

static void al_mouse(const struct evento *e) {
    if (e->tipo == EVENTO_PUNTATORE) {   /* una tavoletta: dove sta, sullo schermo intero */
        px = e->dx * (larghezza - 1) / PUNTATORE_MAX;   /* al massimo 32767 x 1279: ci sta */
        py = e->dy * (altezza - 1) / PUNTATORE_MAX;
    } else {                             /* un mouse: di quanto si e' spostato */
        px += e->dx;
        py += e->dy;
    }
    if (px < 0) px = 0;
    if (py < 0) py = 0;
    if (px >= larghezza) px = larghezza - 1;
    if (py >= altezza) py = altezza - 1;

    /* il destro conta come il sinistro: porta davanti, da' il fuoco e "prende" la
     * finestra finche' non si lascia (cosi' il menu del tasto destro arriva a lei) */
    const unsigned int premere = PULSANTE_SINISTRO | PULSANTE_DESTRO;
    int premuto = e->pulsanti & premere;
    int appena_premuto = premuto && !(pulsanti_prima & premere);
    pulsanti_prima = e->pulsanti;

    if (appena_premuto) {   /* un clic: la finestra sotto va davanti e prende il fuoco */
        int i = finestra_sotto(px, py);
        int velo = i >= 0 ? scelta_sopra(i) : -1;
        if (velo >= 0) {    /* una finestra velata: il clic va alla scelta, non all'app */
            in_primo_piano(velo);
            dai_fuoco(velo);
            return;         /* e all'app non arriva niente */
        }
        if (i >= 0 && finestre[i].non_risponde &&
            barra_congelata_sul_chiudi(finestre[i].l, MINI, px - finestre[i].x, py - finestre[i].y)) {
            /* il "chiudi" rosso di emergenza: si chiude il programma. La finestra sparira'
             * quando il suo collegamento si chiude (lo chiude il kernel). */
            termina(finestre[i].collegamento);
            return;
        }
        if (i < 0 || finestre[i].ruolo == RUOLO_FINESTRA || finestre[i].ruolo == RUOLO_SCELTA) {
            dai_fuoco(i);
            if (i >= 0) in_primo_piano(i);
        }
        if (i >= 0) presa = i;
        if (i >= 0 && finestre[i].wayland) {
            /* la cornice di una finestra Wayland e' nostra: i suoi comandi e il trascinamento li
             * gestiamo qui (una finestra nostra lo chiede con RIDUCIMI e TRASCINAMI). Sotto la
             * testata c'e' il programma, ma senza seat non sente il puntatore. */
            switch (cornice_comando(finestre[i].l, px - finestre[i].x, py - finestre[i].y)) {
            case CORNICE_CHIUDI:  wayland_chiedi_chiusura(i); break;   /* "chiuditi": decide lui */
            case CORNICE_RIDUCI:  riducimi(i); break;
            case CORNICE_TESTATA: trascinami(i); break;
            default: break;
            }
        }
    }
    if (trascinata >= 0) {
        if (!premuto) presa = trascinata = -1;   /* lasciata: lo spostamento finisce */
        else {
            finestre[trascinata].x = px - presa_x;
            finestre[trascinata].y = py - presa_y;
            return;
        }
    }

    /* a chi va il puntatore: a quella presa, se c'e'; se no a quella sotto. Anche il
     * "lasciato" va a quella presa, ovunque sia il puntatore: chi ha avuto il "premuto"
     * deve sapere che e' finito (come in Wayland).
     * UNA FINESTRA SENZA FUOCO NON SENTE IL PUNTATORE CHE PASSA (scelta della persona): solo
     * il clic, che la porta davanti e le da' il fuoco (qui sopra), e la rotella (alla_rotella).
     * Cosi' sotto il puntatore non si accende niente nelle finestre dietro. I pannelli della
     * scrivania (userbar, vassoio) non hanno mai il fuoco: loro lo sentono sempre. */
    int a_chi = presa >= 0 ? presa : finestra_sotto(px, py);
    if (a_chi >= 0 && presa < 0 && a_chi != fuoco && (finestre[a_chi].ruolo == RUOLO_FINESTRA || finestre[a_chi].ruolo == RUOLO_SCELTA)) a_chi = -1;
    if (a_chi >= 0 && scelta_sopra(a_chi) >= 0) a_chi = -1;   /* velata: non sente il puntatore */
    if (sotto >= 0 && sotto != a_chi) avvisa(sotto, (struct notizia){ .tipo = USCITO });
    sotto = a_chi;
    if (a_chi >= 0) {
        const struct finestra_compositore *f = &finestre[a_chi];
        avvisa(a_chi, (struct notizia){ .tipo = PUNTATORE, .x = px - f->x, .y = py - f->y,
                                        .pulsanti = e->pulsanti });
    }
    if (!premuto) presa = -1;
}

/* La rotella va alla finestra sotto il puntatore (non a quella con il fuoco), con il
 * punto in cui e' il puntatore: l'applicazione scorre quello che c'e' li' sotto. Come il
 * puntatore, mentre un pulsante e' premuto va a quella presa; mentre una finestra si
 * sposta, a nessuno. Non cambia niente sullo schermo: se l'applicazione scorre,
 * ridisegnera' lei. */
static void alla_rotella(const struct evento *e) {
    if (trascinata >= 0) return;
    int i = presa >= 0 ? presa : finestra_sotto(px, py);
    if (i < 0 || scelta_sopra(i) >= 0) return;   /* una velata non sente la rotella */
    const struct finestra_compositore *f = &finestre[i];
    avvisa(i, (struct notizia){ .tipo = ROTELLA, .x = px - f->x, .y = py - f->y,
                                .scatti_x = e->dx, .scatti_y = e->dy });
}

/* I TASTI MULTIMEDIALI sono del SISTEMA, non dell'app in primo piano: valgono in ogni app,
 * e nessuna app li riceve (ne' premuti, ne' ripetuti, ne' lasciati). Sono i codici di Linux
 * (linux/input-event-codes.h). Un servizio del suono oggi non c'e': il compositore li
 * riconosce, li ferma e scrive una riga nel registro, una per pressione. Quando ci sara' il
 * servizio, e' qui che gli si girera' il comando. */
static const char *tasto_di_sistema(unsigned int codice) {
    switch (codice) {
    case 115: return "volume su";           /* KEY_VOLUMEUP */
    case 114: return "volume giu'";         /* KEY_VOLUMEDOWN */
    case 113: return "muto";                /* KEY_MUTE */
    case 164: return "riproduci/pausa";     /* KEY_PLAYPAUSE */
    case 163: return "brano successivo";    /* KEY_NEXTSONG */
    case 165: return "brano precedente";    /* KEY_PREVIOUSSONG */
    case 166: return "stop";                /* KEY_STOPCD */
    default:  return 0;
    }
}

static void al_tasto_fisico(const struct evento *e) {
    const char *di_sistema = tasto_di_sistema(e->tasto);
    if (di_sistema) {
        /* una riga solo alla pressione: la ripetizione (2) e il rilascio (0) si fermano qui senza rumore */
        if (e->pulsanti == 1) scrivi_riga("compositore", "tasto di sistema: ", di_sistema);
        return;
    }
    if (fuoco >= 0 && finestre[fuoco].tasti_fisici)
        avvisa(fuoco, (struct notizia){ .tipo = TASTO_FISICO, .tasto = e->tasto, .premuto = e->pulsanti });
}

static void al_tasto(const struct evento *e) {
    if (fuoco >= 0) avvisa(fuoco, (struct notizia){ .tipo = TASTO, .tasto = e->tasto });
}

/* Il descrittore del server Wayland (wayland_inizia), che il ciclo guarda insieme agli altri;
 * -1 se non si e' acceso (il compositore funziona uguale, solo senza Wayland). */
static int descrittore_wayland = -1;

/* Un giro di attesa: aspetta al massimo 'ms' millisecondi (0: guarda e basta) che
 * succeda qualcosa, e serve quello che e' pronto: un evento, i messaggi delle
 * applicazioni, un collegamento nuovo. Quanti erano pronti (0: niente), -1 se le
 * risorse di init non ci sono piu'. */
static int servi(int ms) {
    const int eventi = D_EVENTI, ascolto = D_ASCOLTO;   /* prestati da init */
    struct da_guardare g[4 + MAX_FINESTRE + MAX_PORTE];
    int chi[4 + MAX_FINESTRE + MAX_PORTE];   /* una finestra (>= 0), una porta (-2 - k), o -1 */
    int n = 0;
    g[n].descrittore = eventi;  chi[n++] = -1;
    g[n].descrittore = ascolto; chi[n++] = -1;
    for (int i = 0; i < MAX_FINESTRE; i++)   /* quelle di Wayland non hanno un collegamento */
        if (finestre[i].usata && finestre[i].collegamento >= 0) { g[n].descrittore = finestre[i].collegamento; chi[n++] = i; }
    for (int k = 0; k < MAX_PORTE; k++)
        if (porte[k].usata) { g[n].descrittore = porte[k].d; chi[n++] = -2 - k; }
    int catalog = 0;   /* gli avvisi di catalog, in fondo (se ci sono) */
    if (avvisi_catalog >= 0) { catalog = n; g[n].descrittore = avvisi_catalog; chi[n++] = -1; }
    int wl = -1;       /* Wayland, accanto al nostro protocollo (se si e' acceso) */
    if (descrittore_wayland >= 0) { wl = n; g[n].descrittore = descrittore_wayland; chi[n++] = -1; }
    int pronti = aspetta(g, n, ms);
    if (pronti <= 0) return pronti;
    if (catalog && g[catalog].pronto) avviso_catalog();
    if (wl >= 0 && g[wl].pronto) wayland_servi();

    if (g[0].pronto) {
        struct evento e;
        if (ricevi(eventi, &e, sizeof e) != sizeof e) return -1;
        if (e.tipo == EVENTO_MOUSE || e.tipo == EVENTO_PUNTATORE) al_mouse(&e);
        else if (e.tipo == EVENTO_ROTELLA) alla_rotella(&e);
        else if (e.tipo == EVENTO_TASTO) al_tasto(&e);
        else if (e.tipo == EVENTO_TASTO_FISICO) al_tasto_fisico(&e);
    }
    /* prima i messaggi, poi i collegamenti nuovi: un collegamento nuovo potrebbe
     * riusare il numero di uno appena scollegato, e prendersi il suo 'pronto' */
    for (int k = 2; k < n; k++)
        if (g[k].pronto && chi[k] >= 0 && finestre[chi[k]].usata && finestre[chi[k]].collegamento == g[k].descrittore)
            da_applicazione(chi[k]);
    for (int k = 2; k < n; k++) {
        int p = -2 - chi[k];
        if (g[k].pronto && chi[k] <= -2 && porte[p].usata && porte[p].d == g[k].descrittore) da_porta(p);
    }
    if (g[1].pronto) nuovo_collegamento(ascolto);
    return pronti;
}

int main(void) {
    raggio_media = sfocatura_finestra() / SCALA / 2;
    if (raggio_media > MAX_RAGGIO_MEDIA) raggio_media = MAX_RAGGIO_MEDIA;
    for (int s = 0; s < (int)(sizeof diviso_media / sizeof diviso_media[0]); s++)
        diviso_media[s] = (unsigned short)(s / (2 * raggio_media + 1));
    dipendenza = SCALA * (3 * raggio_media + 2);
    a_pezzi = 3 * raggio_media + 2 <= BORDO_PICCOLO;
    if (!(memoria = mappa_schermo(D_SCHERMO, &schermo))) {
        scrivi_testo("compositore: non ho lo schermo (mi deve far partire init)\n");
        return 0;
    }
    if (schermo.larghezza > MAX_LARGHEZZA || schermo.altezza > MAX_ALTEZZA) {
        scrivi_testo("compositore: lo schermo e' piu' grande di quanto so disegnare\n");
        return 0;
    }
    larghezza = schermo.larghezza;
    altezza = schermo.altezza;
    px = larghezza / 2;
    py = altezza / 2;
    struct catalog_valori impostazioni;
    avvisi_catalog = catalog_osserva(&impostazioni);   /* il tema, e i suoi avvisi */
    if (avvisi_catalog >= 0) compositore_scegli_tema(&impostazioni);
    immagine_prima = immagine_forma(FORMA_NORMALE, 0);
    scena = (struct tela){ disegno, larghezza, altezza };
    pennello_prepara(&pennello, &scena);
    componi();
    descrittore_wayland = wayland_inizia();   /* Wayland accanto al nostro protocollo (passo 1) */

    /* Il ciclo di ogni compositore: aspetta che succeda qualcosa (un evento, un
     * collegamento, un messaggio da un'applicazione), poi SVUOTA LA CODA: serve tutto
     * quello che e' arrivato nel frattempo, e solo alla fine ricompone, una volta.
     * Il mouse manda decine di pacchetti al secondo: ricomporre per ognuno vorrebbe dire
     * disegnare posizioni gia' vecchie e restare sempre indietro. Le posizioni di mezzo
     * non le vedrebbe nessuno: conta solo l'ultima. (Come Wayland, macOS e Windows: un
     * fotogramma con tutto quello che e' successo dall'ultimo.) */
    for (;;) {
        /* al massimo mezzo secondo: i "ci sei?"; con l'attesa sullo schermo, un fotogramma dell'anello */
        if (servi(forma_voluta() == FORMA_ATTESA ? 1000 / FOTOGRAMMI_ATTESA : 500) < 0) return 0;
        /* il resto della coda, senza dormire; con un limite, per ridisegnare comunque
         * ogni tanto anche se gli eventi non smettono mai di arrivare */
        for (int giri = 0; giri < 64; giri++) {
            int pronti = servi(0);
            if (pronti < 0) return 0;
            if (pronti == 0) break;
        }
        controlla_risposte();
        componi();
        if (descrittore_wayland >= 0) wayland_fotogramma();   /* "fatto": il programma disegna il prossimo */
    }
}
