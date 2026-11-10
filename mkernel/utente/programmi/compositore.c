/* Il compositore: il programma padrone dello schermo.
 *
 * Come in un sistema vero, il kernel non disegna finestre: presta lo schermo a un solo
 * programma, questo, e gli consegna gli eventi di tastiera e mouse. Le applicazioni si
 * collegano al socket "compositore" e parlano con il protocollo delle finestre
 * (protocollo.h): ognuna disegna da sola la sua finestra in un foglio di memoria
 * condivisa, e dice "mostra". Il compositore:
 *   - COMPONE: lo sfondo, poi le finestre dal fondo al primo piano, ognuna con la sua
 *     ombra morbida intorno, poi il puntatore. Le finestre sono di VETRO SMERIGLIATO:
 *     sotto ognuna il compositore sfoca quello che c'e' dietro, e ci mescola sopra
 *     l'immagine dell'applicazione (i suoi pixel hanno un'opacita'). Dentro le finestre
 *     non disegna niente, neanche la barra del titolo: ogni applicazione si disegna da
 *     sola, tutta;
 *   - SMISTA L'INPUT: il puntatore alla finestra che ci sta sotto (in coordinate sue),
 *     i tasti a quella che ha il fuoco;
 *   - SPOSTA le finestre quando un'applicazione chiede TRASCINAMI, e porta davanti
 *     quella cliccata.
 *
 * LE SUE RISORSE GLIELE PRESTA INIT. Lo schermo, tastiera e mouse, e il nome
 * "compositore" su cui si collegano le applicazioni non li apre lui: li tiene init,
 * che glieli passa gia' aperti ai descrittori 0, 1 e 2 (avvia_con). Se il compositore
 * cade, init li ha ancora, e lo fa ripartire con le stesse.
 *
 * LO SCHERMO, VISTO DA QUI. Dopo mappa_schermo la memoria dello schermo e' nella
 * nostra mappa, a SCHERMO_PROGRAMMA: scrivere li' un numero vuol dire accendere un
 * pixel. Riga dopo riga, da sinistra a destra, 4 byte per pixel. Tra l'inizio di una
 * riga e la successiva ci sono 'passo' byte (puo' essere piu' della larghezza * 4).
 *
 * IL DOPPIO BUFFER. Non si disegna direttamente sullo schermo: se no, a ogni movimento
 * si vedrebbe lo sfondo cancellare per un attimo le finestre (lo schermo "sfarfalla").
 * Si compone tutta la scena in una memoria nostra, 'disegno', e poi la si copia sullo
 * schermo in un colpo.
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
 * LO STILE (colori, misure, font) e' quello del sistema, in stile.h: lo stesso del
 * toolkit, cosi' la barra di emergenza e' uguale a quella delle applicazioni. */
#include "libreria.h"
#include "protocollo.h"
#include "stile.h"

/* Le risorse che init gli passa gia' aperte (vedi sopra). */
#define D_SCHERMO 0
#define D_EVENTI  1
#define D_ASCOLTO 2

#define MAX_LARGHEZZA 1280  /* quanto chiede il kernel a GRUB (kernel_head.s) */
#define MAX_ALTEZZA   800

static struct info_schermo schermo;
static unsigned char *memoria = (unsigned char *)SCHERMO_PROGRAMMA;
static int larghezza, altezza;

/* La scena, prima dello schermo: pixel premoltiplicati, come quelli delle finestre.
 * Ci si disegna con la libreria di disegno, come in una finestra. */
static unsigned int disegno[MAX_LARGHEZZA * MAX_ALTEZZA];
static struct tela scena;
static struct pennello pennello;

/* LO SFONDO, ingrandito una volta sola all'avvio da quello piccolo (320x200) di
 * grafica_dati.h. E' una sfumatura morbida: ingrandendolo "a mescolanza" (ogni pixel
 * nuovo e' una media pesata dei quattro vicini di quello piccolo) non si perde niente. */
static unsigned int sfondo[MAX_LARGHEZZA * MAX_ALTEZZA];

static void prepara_sfondo(void) {
    for (int y = 0; y < altezza; y++) {
        int fy = y * ((SFONDO_A - 1) << 8) / (altezza - 1);   /* in 256esimi di pixel piccolo */
        int iy = fy >> 8, ty = fy & 255, iy2 = iy + 1 < SFONDO_A ? iy + 1 : iy;
        for (int x = 0; x < larghezza; x++) {
            int fx = x * ((SFONDO_L - 1) << 8) / (larghezza - 1);
            int ix = fx >> 8, tx = fx & 255, ix2 = ix + 1 < SFONDO_L ? ix + 1 : ix;
            unsigned int a = sfondo_piccolo[iy * SFONDO_L + ix], b = sfondo_piccolo[iy * SFONDO_L + ix2];
            unsigned int c = sfondo_piccolo[iy2 * SFONDO_L + ix], d = sfondo_piccolo[iy2 * SFONDO_L + ix2];
            unsigned int px = 0xFF000000u;
            for (int k = 0; k < 24; k += 8) {
                int sopra = ((a >> k) & 0xFF) * (256 - tx) + ((b >> k) & 0xFF) * tx;
                int sotto = ((c >> k) & 0xFF) * (256 - tx) + ((d >> k) & 0xFF) * tx;
                px |= (unsigned int)((sopra * (256 - ty) + sotto * ty) >> 16) << k;
            }
            sfondo[y * larghezza + x] = px;
        }
    }
}

/* Un'immagine di un'applicazione (premoltiplicata) a x, y, mescolata con quello che c'e'
 * sotto; le prime 'salta' righe no. Quello che esce dallo schermo si taglia. */
static void immagine(int x, int y, int l, int a, const unsigned int *pixel, int salta) {
    for (int r = salta; r < a; r++) {
        int sy = y + r;
        if (sy < 0 || sy >= altezza) continue;
        for (int k = 0; k < l; k++) {
            int sx = x + k;
            if (sx < 0 || sx >= larghezza) continue;
            unsigned int *d = &disegno[sy * larghezza + sx];
            *d = sopra_a(pixel[r * l + k], *d);
        }
    }
}

/* --- le finestre, una per applicazione collegata --- */

#define MAX_FINESTRE 8

struct finestra {
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
};

static struct finestra finestre[MAX_FINESTRE];

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

/* L'OMBRA di una finestra: morbida, spostata in giu' e larga quanto la sfocatura, come
 * nel disegno. Calcolarla costa (le distanze dal bordo arrotondato), e dipende solo
 * dalla misura della finestra: la si calcola una volta, quando la finestra nasce, come
 * una tabella di coperture, e a ogni composizione la si "versa" con il suo colore. */
#define OMBRA_MARGINE FINESTRA_OMBRA_SFOCATA
#define OMBRA_L(f) ((f)->l + 2 * OMBRA_MARGINE)
#define OMBRA_A(f) ((f)->a + 2 * OMBRA_MARGINE + FINESTRA_OMBRA_GIU)
static unsigned char ombre[MAX_FINESTRE][(MAX_LARGHEZZA + 2 * OMBRA_MARGINE) *
                                         (MAX_ALTEZZA + 2 * OMBRA_MARGINE + FINESTRA_OMBRA_GIU)];

static void prepara_ombra(int i) {
    const struct finestra *f = &finestre[i];
    struct tondo forma = tondo(0, 0, f->l, f->a, RAGGIO_FINESTRA);
    struct tondo giu = tondo(0, FINESTRA_OMBRA_GIU, f->l, f->a, RAGGIO_FINESTRA);
    int s = FINESTRA_OMBRA_SFOCATA * 256;
    for (int y = 0; y < OMBRA_A(f); y++)
        for (int x = 0; x < OMBRA_L(f); x++) {
            int wx = x - OMBRA_MARGINE, wy = y - OMBRA_MARGINE;
            int fuori = 256 - coperto(distanza(&forma, wx, wy));   /* sotto la finestra no */
            int v = fuori ? (s - distanza(&giu, wx, wy)) * 128 / s : 0;
            if (v < 0) v = 0;
            if (v > 256) v = 256;
            int dolce = v * v * (768 - 2 * v) >> 16;            /* smoothstep */
            ombre[i][y * OMBRA_L(f) + x] = (dolce * fuori >> 8) * 255 >> 8;
        }
}

static void disegna_ombra(int i) {
    const struct finestra *f = &finestre[i];
    pennello_colore(&pennello, i == fuoco ? FINESTRA_OMBRA : FINESTRA_OMBRA_SFONDO);
    for (int y = 0; y < OMBRA_A(f); y++)
        for (int x = 0; x < OMBRA_L(f); x++) {
            int v = ombre[i][y * OMBRA_L(f) + x];
            if (v) copri(&pennello, f->x - OMBRA_MARGINE + x, f->y - OMBRA_MARGINE + y, v + (v >> 7));
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
 *      "vera" (gaussiana) del disegno: qui, circa 20 pixel;
 *   4. la si rifa' solo quando cambia qualcosa sotto (vedi componi): se si muove solo
 *      il mouse, si riusa quella gia' fatta.
 * Poi, a ogni composizione, la si ingrandisce di nuovo (ogni pixel e' la media pesata
 * dei quattro vicini di quella piccola) dentro la forma arrotondata della finestra. */
#define SCALA          4    /* la copia rimpicciolita: un pixel ogni 4x4 */
#define RAGGIO_MEDIA   5    /* in pixel rimpiccioliti: medie di 11, tre volte */
#define BORDO_PICCOLO 16    /* quanto si prende intorno alla finestra, in pixel rimpiccioliti */
#define PICCOLA_L (MAX_LARGHEZZA / SCALA + 2 * BORDO_PICCOLO + 2)
#define PICCOLA_A (MAX_ALTEZZA / SCALA + 2 * BORDO_PICCOLO + 2)

struct sfocata {
    int x, y, l, a;         /* dove sta, in pixel rimpiccioliti (x * SCALA = pixel veri) */
    unsigned int pixel[PICCOLA_L * PICCOLA_A];
};
static struct sfocata sfocate[MAX_FINESTRE];
static unsigned int riga_media[PICCOLA_L > PICCOLA_A ? PICCOLA_L : PICCOLA_A];

/* La media scorrevole su una fila di n pixel (passo 'salto' tra uno e l'altro), fatta
 * tre volte. Ai bordi si ripete il pixel del bordo. */
static void media_fila(unsigned int *fila, int n, int salto) {
    for (int volta = 0; volta < 3; volta++) {
        for (int k = 0; k < 24; k += 8) {
            int somma = 0;
            for (int i = -RAGGIO_MEDIA; i <= RAGGIO_MEDIA; i++) {
                int j = i < 0 ? 0 : i >= n ? n - 1 : i;
                somma += (fila[j * salto] >> k) & 0xFF;
            }
            for (int i = 0; i < n; i++) {
                riga_media[i] = (riga_media[i] & ~(0xFFu << k)) |
                                (unsigned int)(somma / (2 * RAGGIO_MEDIA + 1)) << k;
                int esce = i - RAGGIO_MEDIA, entra = i + RAGGIO_MEDIA + 1;
                somma -= (fila[(esce < 0 ? 0 : esce) * salto] >> k) & 0xFF;
                somma += (fila[(entra >= n ? n - 1 : entra) * salto] >> k) & 0xFF;
            }
        }
        for (int i = 0; i < n; i++) fila[i * salto] = riga_media[i];
    }
}

/* Sfoca quello che c'e' adesso nella scena dietro la finestra i (e un po' intorno). */
static void prepara_sfocatura(int i) {
    const struct finestra *f = &finestre[i];
    struct sfocata *s = &sfocate[i];
    s->x = (f->x < 0 ? f->x - SCALA + 1 : f->x) / SCALA - BORDO_PICCOLO;
    s->y = (f->y < 0 ? f->y - SCALA + 1 : f->y) / SCALA - BORDO_PICCOLO;
    s->l = f->l / SCALA + 2 * BORDO_PICCOLO + 2;
    s->a = f->a / SCALA + 2 * BORDO_PICCOLO + 2;
    /* 1. rimpicciolire: ogni pixel piccolo e' la media di 4x4 pixel della scena */
    for (int v = 0; v < s->a; v++)
        for (int u = 0; u < s->l; u++) {
            unsigned int r = 0, g = 0, b = 0;
            for (int dy = 0; dy < SCALA; dy++)
                for (int dx = 0; dx < SCALA; dx++) {
                    int x = (s->x + u) * SCALA + dx, y = (s->y + v) * SCALA + dy;
                    x = x < 0 ? 0 : x >= larghezza ? larghezza - 1 : x;   /* fuori: il bordo */
                    y = y < 0 ? 0 : y >= altezza ? altezza - 1 : y;
                    unsigned int px = disegno[y * larghezza + x];
                    r += (px >> 16) & 0xFF; g += (px >> 8) & 0xFF; b += px & 0xFF;
                }
            s->pixel[v * s->l + u] = (r / (SCALA * SCALA)) << 16 | (g / (SCALA * SCALA)) << 8 |
                                     b / (SCALA * SCALA);
        }
    /* 2. e 3. le medie: tutte le righe, poi tutte le colonne */
    for (int v = 0; v < s->a; v++) media_fila(&s->pixel[v * s->l], s->l, 1);
    for (int u = 0; u < s->l; u++) media_fila(&s->pixel[u], s->a, s->l);
}

/* Mette la sfocatura dentro la forma della finestra, ingrandendola di nuovo. */
static void metti_sfocatura(int i) {
    const struct finestra *f = &finestre[i];
    const struct sfocata *s = &sfocate[i];
    struct tondo forma = tondo(f->x, f->y, f->l, f->a, RAGGIO_FINESTRA);
    for (int y = f->y; y < f->y + f->a; y++) {
        if (y < 0 || y >= altezza) continue;
        int vicino_sopra_sotto = y < f->y + RAGGIO_FINESTRA || y >= f->y + f->a - RAGGIO_FINESTRA;
        /* dove cade il pixel nella copia piccola, in 256esimi (centro su centro) */
        int fy = (y * 256 + 128) / SCALA - 128 - s->y * 256;
        int vy = fy >> 8, ty = fy & 255;
        if (vy < 0) { vy = 0; ty = 0; }
        if (vy >= s->a - 1) { vy = s->a - 2; ty = 255; }
        for (int x = f->x; x < f->x + f->l; x++) {
            if (x < 0 || x >= larghezza) continue;
            int c = 256;
            if (vicino_sopra_sotto && (x < f->x + RAGGIO_FINESTRA || x >= f->x + f->l - RAGGIO_FINESTRA))
                c = coperto(distanza(&forma, x, y));        /* gli angoli arrotondati */
            if (!c) continue;
            int fx = (x * 256 + 128) / SCALA - 128 - s->x * 256;
            int ux = fx >> 8, tx = fx & 255;
            if (ux < 0) { ux = 0; tx = 0; }
            if (ux >= s->l - 1) { ux = s->l - 2; tx = 255; }
            const unsigned int *q = &s->pixel[vy * s->l + ux];
            unsigned int a = q[0], b = q[1], cc = q[s->l], d = q[s->l + 1];
            unsigned int *dst = &disegno[y * larghezza + x], px = 0xFF000000u;
            for (int k = 0; k < 24; k += 8) {
                int su = ((a >> k) & 0xFF) * (256 - tx) + ((b >> k) & 0xFF) * tx;
                int giu = ((cc >> k) & 0xFF) * (256 - tx) + ((d >> k) & 0xFF) * tx;
                int sfoc = (su * (256 - ty) + giu * ty) >> 16;
                int prima = (*dst >> k) & 0xFF;
                px |= (unsigned int)((sfoc * c + prima * (256 - c)) >> 8) << k;
            }
            *dst = px;
        }
    }
}

/* I due comandi della barra, come li mette il toolkit: riduci e chiudi, a destra. */
static int comando_x(const struct finestra *f, int chiudi) {
    return f->x + f->l - BARRA_MARGINE - BARRA_COMANDO - (chiudi ? 0 : BARRA_COMANDO + 6);
}
static int comando_y(const struct finestra *f) { return f->y + (BARRA_ALTEZZA - BARRA_COMANDO) / 2; }

static int sul_chiudi(const struct finestra *f, int x, int y) {
    return x >= comando_x(f, 1) && x < comando_x(f, 1) + BARRA_COMANDO &&
           y >= comando_y(f) && y < comando_y(f) + BARRA_COMANDO;
}

/* La barra di una finestra che non risponde, al posto di quella dell'applicazione:
 * rossa, con l'avviso e il titolo al centro e il "chiudi" rosso. Come nel disegno. */
static void barra_non_risponde(const struct finestra *f) {
    struct pennello *p = &pennello;
    struct tondo barra = { f->x, f->y, f->l, BARRA_ALTEZZA, { RAGGIO_FINESTRA, RAGGIO_FINESTRA, 0, 0 } };
    pennello_colore(p, VETRO_FONDO);
    riempi_forma(p, &barra);
    pennello_colore(p, STATO_ERRORE_VELO);
    riempi_forma(p, &barra);

    char avviso[MAX_TITOLO + 16];
    int n = 0;
    for (const char *c = f->titolo; *c; c++) avviso[n++] = *c;
    for (const char *c = " non risponde"; *c; c++) avviso[n++] = *c;
    avviso[n] = 0;
    int largo = 14 + 6 + larghezza_testo(FONT_BARRA, avviso);
    int x = f->x + (f->l - largo) / 2;
    pennello_salva(p);
    pennello_ritaglia(p, f->x, f->y, f->l, BARRA_ALTEZZA);
    pennello_colore(p, CONTENUTO_PRINCIPALE);
    icona(p, ICONA_TRIANGLE_ALERT, 14, x, f->y + (BARRA_ALTEZZA - 14) / 2);
    testo(p, FONT_BARRA, x + 14 + 6, f->y + (BARRA_ALTEZZA - FONT_BARRA->altezza) / 2, avviso);
    pennello_ripristina(p);

    for (int chiudi = 0; chiudi < 2; chiudi++) {
        pennello_colore(p, chiudi ? STATO_ERRORE : VETRO_SUPERFICIE);
        riempi_tondo(p, comando_x(f, chiudi), comando_y(f), BARRA_COMANDO, BARRA_COMANDO, BARRA_COMANDO / 2);
        pennello_colore(p, CONTENUTO_PRINCIPALE);
        icona(p, chiudi ? ICONA_X : ICONA_MINUS, 12, comando_x(f, chiudi) + 5, comando_y(f) + 5);
    }
    struct tondo tutta = tondo(f->x, f->y, f->l, f->a, RAGGIO_FINESTRA);
    pennello_colore(p, STATO_ERRORE);
    contorna_forma(p, &tutta, 256);
}

static void disegna_finestra(int i, int ricalcola) {
    const struct finestra *f = &finestre[i];
    disegna_ombra(i);
    if (ricalcola) prepara_sfocatura(i);
    metti_sfocatura(i);
    immagine(f->x, f->y, f->l, f->a, f->immagini[f->mostrata], f->non_risponde ? BARRA_ALTEZZA : 0);
    if (f->non_risponde) barra_non_risponde(f);
}

/* Un messaggio a un'applicazione, senza aspettare. Se non si puo', la sua casella e'
 * piena: non legge piu', quindi e' bloccata. 1 se e' partito, 0 se no. (Se invece ha
 * chiuso, ce ne accorgeremo leggendo dal suo collegamento.) */
static int avvisa(int i, struct notizia n) {
    if (manda_subito(finestre[i].collegamento, &n, sizeof n) == sizeof n) return 1;
    finestre[i].non_risponde = 1;
    return 0;
}

static void dai_fuoco(int i) {
    if (i == fuoco) return;
    int prima = fuoco;
    fuoco = i;
    if (prima >= 0) avvisa(prima, (struct notizia){ .tipo = FUOCO, .fuoco = 0 });
    if (i >= 0) avvisa(i, (struct notizia){ .tipo = FUOCO, .fuoco = 1 });
}

static void in_primo_piano(int i) {
    int k = 0;
    while (k < quante && ordine[k] != i) k++;
    for (; k < quante - 1; k++) ordine[k] = ordine[k + 1];
    ordine[quante - 1] = i;
}

/* La finestra sotto il punto (x, y): si cerca dal primo piano in giu', perche' quella
 * davanti copre le altre. -1 se sotto c'e' lo sfondo. */
static int finestra_sotto(int x, int y) {
    for (int k = quante - 1; k >= 0; k--) {
        const struct finestra *f = &finestre[ordine[k]];
        if (f->mostrata < 0) continue;
        if (x >= f->x && x < f->x + f->l && y >= f->y && y < f->y + f->a)
            return ordine[k];
    }
    return -1;
}

static void scollega(int i) {
    struct finestra *f = &finestre[i];
    if (!f->usata) return;
    if (f->immagini[0]) togli(f->immagini[0]);
    if (f->foglio >= 0) chiudi(f->foglio);
    chiudi(f->collegamento);
    f->usata = 0;
    int k = 0;
    while (k < quante && ordine[k] != i) k++;
    for (; k < quante - 1; k++) ordine[k] = ordine[k + 1];
    if (k < quante) quante--;
    if (sotto == i) sotto = -1;
    if (presa == i) presa = -1;
    if (trascinata == i) trascinata = -1;
    if (fuoco == i) fuoco = -1;
}

/* Un collegamento nuovo: si accetta e si aspetta il suo VOGLIO_FINESTRA. */
static void nuovo_collegamento(int ascolto) {
    int d = accetta(ascolto);
    if (d < 0) return;
    int i = 0;
    while (i < MAX_FINESTRE && finestre[i].usata) i++;
    if (i == MAX_FINESTRE) { chiudi(d); return; }   /* troppe finestre */
    struct finestra *f = &finestre[i];
    *f = (struct finestra){ .usata = 1, .collegamento = d, .foglio = -1, .mostrata = -1,
                            .da_rilasciare = -1, .ultimo_ping = tempo() };
    ordine[quante++] = i;
}

/* Il primo messaggio: la finestra, con il suo foglio. Tutto si controlla. */
static void voglio_finestra(int i, const struct voglio_finestra *m, int foglio) {
    struct finestra *f = &finestre[i];
    if (f->immagini[0]) {                /* ne ha gia' una: messaggio sbagliato */
        if (foglio >= 0) chiudi(foglio);
        scollega(i);
        return;
    }
    f->foglio = foglio;
    int l = m->larghezza, a = m->altezza;
    unsigned int byte;
    unsigned int *pixel = foglio >= 0 ? mappa(foglio, &byte) : 0;
    if (!pixel || l < 1 || a < 1 || l > larghezza || a > altezza || byte < 2u * l * a * 4) {
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
    f->x = 60 + i * 40;   /* a cascata */
    f->y = 50 + i * 40;
    prepara_ombra(i);
}

static void mostra(int i, const struct mostra *m) {
    struct finestra *f = &finestre[i];
    if (!f->immagini[0] || m->immagine > 1) { scollega(i); return; }
    int prima = f->mostrata;
    f->mostrata = m->immagine;
    f->generazione++;
    if (prima < 0) {                 /* la prima volta: davanti e con il fuoco */
        in_primo_piano(i);
        dai_fuoco(i);
    } else if (prima != f->mostrata) {
        if (!avvisa(i, (struct notizia){ .tipo = RILASCIATA, .immagine = prima }))
            f->da_rilasciare = prima;
    }
}

/* PONG: ha risposto. Se era bloccata, torna normale, e le si dice quello che si era
 * perso: l'immagine da restituire e se ha il fuoco. */
static void pong(int i, const struct pong *m) {
    struct finestra *f = &finestre[i];
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
        struct finestra *f = &finestre[i];
        if (!f->usata || !f->immagini[0]) continue;
        if (f->ping_atteso && adesso - f->ultimo_ping >= 3000) f->non_risponde = 1;
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
    if (presa != i) return;
    trascinata = i;
    presa_x = px - finestre[i].x;
    presa_y = py - finestre[i].y;
    if (sotto == i) {   /* finche' si sposta, il puntatore non e' suo */
        avvisa(i, (struct notizia){ .tipo = USCITO });
        sotto = -1;
    }
}

/* Un messaggio da un'applicazione. */
static void da_applicazione(int i) {
    union {
        unsigned int tipo;
        struct voglio_finestra v;
        struct mostra m;
        struct trascinami t;
        struct pong p;
    } m;
    int foglio = -1;
    int n = ricevi_foglio(finestre[i].collegamento, &m, sizeof m, &foglio);
    if (n <= 0) { if (foglio >= 0) chiudi(foglio); scollega(i); return; }   /* ha chiuso */
    if (m.tipo == VOGLIO_FINESTRA && n == sizeof m.v) { voglio_finestra(i, &m.v, foglio); return; }
    if (foglio >= 0) chiudi(foglio);   /* un foglio va solo col primo messaggio */
    if (m.tipo == MOSTRA && n == sizeof m.m) { mostra(i, &m.m); return; }
    if (m.tipo == TRASCINAMI && n == sizeof m.t && finestre[i].immagini[0]) { trascinami(i); return; }
    if (m.tipo == PONG && n == sizeof m.p) { pong(i, &m.p); return; }
    scollega(i);                       /* messaggio sbagliato */
}

/* --- il puntatore --- */

/* 'X' bordo nero, '.' interno bianco, ' ' trasparente. La punta e' in alto a sinistra:
 * e' li' il punto (px, py). */
static const char *const freccia[] = {
    "X           ",
    "XX          ",
    "X.X         ",
    "X..X        ",
    "X...X       ",
    "X....X      ",
    "X.....X     ",
    "X......X    ",
    "X.......X   ",
    "X........X  ",
    "X.........X ",
    "X..........X",
    "X......XXXXX",
    "X...X..X    ",
    "X..XX..X    ",
    "X.X  X..X   ",
    "XX   X..X   ",
    "X     X..X  ",
    "       XX   ",
};

static void disegna_puntatore(void) {
    for (int r = 0; r < (int)(sizeof freccia / sizeof freccia[0]); r++)
        for (int k = 0; freccia[r][k]; k++) {
            if (freccia[r][k] == ' ') continue;
            pennello_colore(&pennello, OPACO(freccia[r][k] == 'X' ? 0x000000 : 0xFFFFFF));
            copri(&pennello, px + k, py + r, 256);
        }
}

/* --- la scena intera, poi sullo schermo --- */

/* COSA C'E' SULLO SCHERMO, puntatore escluso: l'ordine delle finestre, dove sono, quale
 * immagine mostrano, chi ha il fuoco (cambia l'ombra), chi non risponde. Se non e'
 * cambiato niente, le sfocature fatte l'ultima volta vanno ancora bene. */
#define MAX_FIRMA (1 + 8 * MAX_FINESTRE)
static unsigned int firma_prima[MAX_FIRMA];

static int scena_cambiata(void) {
    unsigned int firma[MAX_FIRMA] = { quante };
    int n = 1, cambiata = 0;
    for (int k = 0; k < quante; k++) {
        const struct finestra *f = &finestre[ordine[k]];
        unsigned int valori[] = { ordine[k], f->x, f->y, f->l, f->mostrata, f->generazione,
                                  f->non_risponde, ordine[k] == fuoco };
        for (int v = 0; v < 8; v++) firma[n++] = valori[v];
    }
    for (int k = 0; k < MAX_FIRMA; k++) {
        if (firma[k] != firma_prima[k]) cambiata = 1;
        firma_prima[k] = firma[k];
    }
    return cambiata;
}

static void componi(void) {
    int ricalcola = scena_cambiata();
    for (int i = 0; i < larghezza * altezza; i++) disegno[i] = sfondo[i];
    for (int k = 0; k < quante; k++)
        if (finestre[ordine[k]].mostrata >= 0) disegna_finestra(ordine[k], ricalcola);
    disegna_puntatore();

    /* la copia, riga per riga. La scena e' tutta opaca: i colori sono gia' quelli veri.
     * Se lo schermo li vuole in un altro ordine, si spostano. */
    int stesso_ordine = schermo.rosso == 16 && schermo.verde == 8 && schermo.blu == 0;
    for (int y = 0; y < altezza; y++) {
        unsigned int *da = &disegno[y * larghezza];
        unsigned int *a = (unsigned int *)(memoria + y * schermo.passo);
        if (stesso_ordine) {
            for (int x = 0; x < larghezza; x++) a[x] = da[x] & 0xFFFFFF;
        } else {
            for (int x = 0; x < larghezza; x++)
                a[x] = ((da[x] >> 16) & 0xFF) << schermo.rosso | ((da[x] >> 8) & 0xFF) << schermo.verde |
                       (da[x] & 0xFF) << schermo.blu;
        }
    }
}

/* --- gli eventi del kernel: mouse e tastiera --- */

static unsigned int pulsanti_prima;

static void al_mouse(const struct evento *e) {
    px += e->dx;
    py += e->dy;
    if (px < 0) px = 0;
    if (py < 0) py = 0;
    if (px >= larghezza) px = larghezza - 1;
    if (py >= altezza) py = altezza - 1;

    int premuto = e->pulsanti & PULSANTE_SINISTRO;
    int appena_premuto = premuto && !(pulsanti_prima & PULSANTE_SINISTRO);
    pulsanti_prima = e->pulsanti;

    if (appena_premuto) {   /* un clic: la finestra sotto va davanti e prende il fuoco */
        int i = finestra_sotto(px, py);
        if (i >= 0 && finestre[i].non_risponde && sul_chiudi(&finestre[i], px, py)) {
            /* il "chiudi" rosso di emergenza: si chiude il programma. La finestra sparira'
             * quando il suo collegamento si chiude (lo chiude il kernel). */
            termina(finestre[i].collegamento);
            return;
        }
        dai_fuoco(i);
        if (i >= 0) {
            in_primo_piano(i);
            presa = i;
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
     * deve sapere che e' finito (come in Wayland) */
    int a_chi = presa >= 0 ? presa : finestra_sotto(px, py);
    if (sotto >= 0 && sotto != a_chi) avvisa(sotto, (struct notizia){ .tipo = USCITO });
    sotto = a_chi;
    if (a_chi >= 0) {
        const struct finestra *f = &finestre[a_chi];
        avvisa(a_chi, (struct notizia){ .tipo = PUNTATORE, .x = px - f->x, .y = py - f->y,
                                        .pulsanti = e->pulsanti });
    }
    if (!premuto) presa = -1;
}

static void al_tasto(const struct evento *e) {
    if (fuoco >= 0) avvisa(fuoco, (struct notizia){ .tipo = TASTO, .tasto = e->tasto });
}

int main(void) {
    if (mappa_schermo(D_SCHERMO, &schermo) < 0) {
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
    scena = (struct tela){ disegno, larghezza, altezza };
    pennello_prepara(&pennello, &scena);
    prepara_sfondo();
    int eventi = D_EVENTI, ascolto = D_ASCOLTO;   /* prestati da init */
    componi();

    /* Il ciclo di ogni compositore: aspetta che succeda qualcosa (un evento, un
     * collegamento, un messaggio da un'applicazione), aggiorna, ricompone. */
    for (;;) {
        struct da_guardare g[2 + MAX_FINESTRE];
        int chi[2 + MAX_FINESTRE];
        int n = 0;
        g[n].descrittore = eventi;  chi[n++] = -1;
        g[n].descrittore = ascolto; chi[n++] = -1;
        for (int i = 0; i < MAX_FINESTRE; i++)
            if (finestre[i].usata) { g[n].descrittore = finestre[i].collegamento; chi[n++] = i; }
        /* al massimo mezzo secondo: anche se non succede niente, bisogna controllare
         * chi non risponde */
        if (aspetta(g, n, 500) < 0) return 0;

        if (g[0].pronto) {
            struct evento e;
            if (ricevi(eventi, &e, sizeof e) != sizeof e) return 0;
            if (e.tipo == EVENTO_MOUSE) al_mouse(&e);
            else if (e.tipo == EVENTO_TASTO) al_tasto(&e);
        }
        /* prima i messaggi, poi i collegamenti nuovi: un collegamento nuovo potrebbe
         * riusare il numero di uno appena scollegato, e prendersi il suo 'pronto' */
        for (int k = 2; k < n; k++)
            if (g[k].pronto && finestre[chi[k]].usata && finestre[chi[k]].collegamento == g[k].descrittore)
                da_applicazione(chi[k]);
        if (g[1].pronto) nuovo_collegamento(ascolto);
        controlla_risposte();
        componi();
    }
}
