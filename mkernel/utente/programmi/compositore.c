/* Il compositore: il programma padrone dello schermo.
 *
 * Come in un sistema vero, il kernel non disegna finestre: presta lo schermo a un solo
 * programma, questo, e gli consegna gli eventi di tastiera e mouse. Le applicazioni si
 * collegano al socket "compositore" e parlano con il protocollo delle finestre
 * (protocollo.h): ognuna disegna da sola la sua finestra in un foglio di memoria
 * condivisa, e dice "mostra". Il compositore:
 *   - COMPONE: sfondo, poi le immagini delle finestre dal fondo al primo piano, poi il
 *     puntatore. Dentro le finestre non disegna niente, neanche la barra del titolo:
 *     ogni applicazione si disegna da sola, tutta;
 *   - SMISTA L'INPUT: il puntatore alla finestra che ci sta sotto (in coordinate sue),
 *     i tasti a quella che ha il fuoco;
 *   - SPOSTA le finestre quando un'applicazione chiede TRASCINAMI, e porta davanti
 *     quella cliccata.
 *
 * LO SCHERMO, VISTO DA QUI. Dopo prendi_schermo la memoria dello schermo e' nella
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
 * che non le si puo' piu' scrivere, e' bloccata: la sua finestra si oscura e ci compare
 * sopra un pulsante rosso di chiusura d'emergenza, che chiude il programma (termina).
 * Se invece ricomincia a rispondere, torna normale. Come GNOME e il Mac. */
#include "libreria.h"
#include "protocollo.h"

#define MAX_LARGHEZZA 800   /* quanto chiede il kernel a GRUB (kernel_head.s) */
#define MAX_ALTEZZA   600

static struct info_schermo schermo;
static unsigned char *memoria = (unsigned char *)SCHERMO_PROGRAMMA;
static unsigned int disegno[MAX_LARGHEZZA * MAX_ALTEZZA];   /* la scena, prima dello schermo */
static int larghezza, altezza;

/* --- disegnare nella scena --- */

/* Un colore scritto come 0xRRGGBB, messo nell'ordine che vuole lo schermo. */
static unsigned int colore(unsigned int rgb) {
    return ((rgb >> 16) & 0xFF) << schermo.rosso
         | ((rgb >> 8) & 0xFF) << schermo.verde
         | (rgb & 0xFF) << schermo.blu;
}

/* Un rettangolo pieno. Quello che esce dallo schermo si taglia. */
static void rettangolo(int x, int y, int l, int a, unsigned int rgb) {
    int x2 = x + l, y2 = y + a;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x2 > larghezza) x2 = larghezza;
    if (y2 > altezza) y2 = altezza;
    unsigned int c = colore(rgb);
    for (int r = y; r < y2; r++)
        for (int k = x; k < x2; k++)
            disegno[r * larghezza + k] = c;
}

/* Un'immagine di un'applicazione (0xRRGGBB) a x, y; 'scura' la mostra a meta' luce.
 * Quello che esce si taglia. */
static void immagine(int x, int y, int l, int a, const unsigned int *pixel, int scura) {
    for (int r = 0; r < a; r++) {
        int sy = y + r;
        if (sy < 0 || sy >= altezza) continue;
        for (int k = 0; k < l; k++) {
            int sx = x + k;
            if (sx < 0 || sx >= larghezza) continue;
            unsigned int p = pixel[r * l + k];
            if (scura) p = (p >> 1) & 0x7F7F7F;   /* ogni colore a meta' */
            disegno[sy * larghezza + sx] = colore(p);
        }
    }
}

static void sfondo(void) {   /* una sfumatura dall'alto in basso, una riga alla volta */
    for (int y = 0; y < altezza; y++) {
        unsigned int t = y * 255 / altezza;
        rettangolo(0, y, larghezza, 1, (0x20 + t / 8) << 16 | (0x40 + t / 4) << 8 | (0x70 + t / 3));
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
    int non_risponde;           /* bloccata: oscurata, con il pulsante d'emergenza */
    int da_rilasciare;          /* un'immagine da restituire che non le si e' potuto
                                 * dire (casella piena): glielo si dice quando risponde */
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

/* Il pulsante d'emergenza: un quadrato rosso con una X, in alto a destra. */
#define EMERGENZA 22
static int emergenza_x(const struct finestra *f) { return f->x + f->l - EMERGENZA - 4; }
static int emergenza_y(const struct finestra *f) { return f->y + 4; }

static int sul_pulsante(const struct finestra *f, int x, int y) {
    return x >= emergenza_x(f) && x < emergenza_x(f) + EMERGENZA &&
           y >= emergenza_y(f) && y < emergenza_y(f) + EMERGENZA;
}

static void disegna_finestra(int i) {
    const struct finestra *f = &finestre[i];
    immagine(f->x, f->y, f->l, f->a, f->immagini[f->mostrata], f->non_risponde);
    if (!f->non_risponde) return;
    int bx = emergenza_x(f), by = emergenza_y(f);
    rettangolo(bx, by, EMERGENZA, EMERGENZA, 0xC03030);
    for (int k = 5; k < EMERGENZA - 5; k++) {          /* la X: le due diagonali, spesse 2 */
        rettangolo(bx + k, by + k, 2, 2, 0xFFFFFF);
        rettangolo(bx + EMERGENZA - 2 - k, by + k, 2, 2, 0xFFFFFF);
    }
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
    f->x = 40 + i * 40;   /* a cascata */
    f->y = 30 + i * 40;
}

static void mostra(int i, const struct mostra *m) {
    struct finestra *f = &finestre[i];
    if (!f->immagini[0] || m->immagine > 1) { scollega(i); return; }
    int prima = f->mostrata;
    f->mostrata = m->immagine;
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
    unsigned int nero = colore(0x000000), bianco = colore(0xFFFFFF);
    for (int r = 0; r < (int)(sizeof freccia / sizeof freccia[0]); r++)
        for (int k = 0; freccia[r][k]; k++) {
            int x = px + k, y = py + r;
            if (freccia[r][k] == ' ' || x >= larghezza || y >= altezza) continue;
            disegno[y * larghezza + x] = freccia[r][k] == 'X' ? nero : bianco;
        }
}

/* --- la scena intera, poi sullo schermo --- */

static void componi(void) {
    sfondo();
    for (int k = 0; k < quante; k++)
        if (finestre[ordine[k]].mostrata >= 0) disegna_finestra(ordine[k]);
    disegna_puntatore();

    for (int y = 0; y < altezza; y++) {   /* la copia, riga per riga */
        unsigned int *da = &disegno[y * larghezza];
        unsigned int *a = (unsigned int *)(memoria + y * schermo.passo);
        for (int x = 0; x < larghezza; x++) a[x] = da[x];
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
        if (i >= 0 && finestre[i].non_risponde && sul_pulsante(&finestre[i], px, py)) {
            /* il pulsante d'emergenza: si chiude il programma. La finestra sparira'
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
    if (prendi_schermo(&schermo) < 0) {
        scrivi_testo("compositore: lo schermo e' gia' di un altro programma\n");
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
    int eventi = apri_eventi();   /* tastiera e mouse, come un descrittore */
    int ascolto = ascolta(NOME_COMPOSITORE);
    if (eventi < 0 || ascolto < 0) {
        scrivi_testo("compositore: eventi o nome gia' di un altro programma\n");
        return 0;
    }
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
