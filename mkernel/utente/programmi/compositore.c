/* Il compositore: il programma padrone dello schermo.
 *
 * Come in un sistema vero, il kernel non disegna finestre: presta lo schermo a un solo
 * programma, questo, e gli consegna gli eventi di tastiera e mouse. Il compositore
 * disegna lo sfondo, le finestre e il puntatore, e decide a quale finestra va ogni
 * evento. Le applicazioni (non ancora) gli manderanno ORDINI ("un rettangolo qui",
 * "questo testo la'") invece di disegnare da sole.
 *
 * Per ora: due finestre finte che si spostano trascinandole per la barra del titolo.
 * Un clic su una finestra la porta in primo piano.
 *
 * LO SCHERMO, VISTO DA QUI. Dopo prendi_schermo la memoria dello schermo e' nella
 * nostra mappa, a SCHERMO_PROGRAMMA: scrivere li' un numero vuol dire accendere un
 * pixel. Riga dopo riga, da sinistra a destra, 4 byte per pixel. Tra l'inizio di una
 * riga e la successiva ci sono 'passo' byte (puo' essere piu' della larghezza * 4).
 *
 * IL DOPPIO BUFFER. Non si disegna direttamente sullo schermo: se no, a ogni movimento
 * si vedrebbe lo sfondo cancellare per un attimo le finestre (lo schermo "sfarfalla").
 * Si disegna tutta la scena in una memoria nostra, 'disegno', e poi la si copia sullo
 * schermo in un colpo. Lo fanno tutti i sistemi grafici. */
#include "libreria.h"

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

static void sfondo(void) {   /* una sfumatura dall'alto in basso, una riga alla volta */
    for (int y = 0; y < altezza; y++) {
        unsigned int t = y * 255 / altezza;
        rettangolo(0, y, larghezza, 1, (0x20 + t / 8) << 16 | (0x40 + t / 4) << 8 | (0x70 + t / 3));
    }
}

/* --- le finestre --- */

#define BARRA 23   /* altezza della barra del titolo, bordo compreso */

struct finestra { int x, y, l, a; unsigned int barra; };

static struct finestra finestre[] = {
    { 80, 70, 360, 260, 0x3A6EA5 },
    { 320, 220, 400, 300, 0x5A8F4E },
};
#define QUANTE ((int)(sizeof finestre / sizeof finestre[0]))

/* L'ordine: ordine[0] e' quella piu' in fondo, l'ultima quella in primo piano.
 * Si disegnano dal fondo: ognuna copre quelle disegnate prima. */
static int ordine[QUANTE] = { 0, 1 };

static void disegna_finestra(const struct finestra *f) {
    rettangolo(f->x, f->y, f->l, f->a, 0x202020);                         /* il bordo */
    rettangolo(f->x + 1, f->y + 1, f->l - 2, BARRA - 1, f->barra);        /* la barra */
    rettangolo(f->x + 1, f->y + BARRA, f->l - 2, f->a - BARRA - 1, 0xF0F0F0); /* il contenuto */
}

/* La finestra sotto il punto (x, y): si cerca dal primo piano in giu', perche' quella
 * davanti copre le altre. -1 se sotto c'e' lo sfondo. Restituisce la posizione in 'ordine'. */
static int finestra_sotto(int x, int y) {
    for (int i = QUANTE - 1; i >= 0; i--) {
        const struct finestra *f = &finestre[ordine[i]];
        if (x >= f->x && x < f->x + f->l && y >= f->y && y < f->y + f->a) return i;
    }
    return -1;
}

static void in_primo_piano(int i) {   /* la finestra in posizione i va in cima */
    int f = ordine[i];
    for (; i < QUANTE - 1; i++) ordine[i] = ordine[i + 1];
    ordine[QUANTE - 1] = f;
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

static int px, py;   /* dov'e' il puntatore: il mouse dice solo di quanto si muove */

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
    for (int i = 0; i < QUANTE; i++) disegna_finestra(&finestre[ordine[i]]);
    disegna_puntatore();

    for (int y = 0; y < altezza; y++) {   /* la copia, riga per riga */
        unsigned int *da = &disegno[y * larghezza];
        unsigned int *a = (unsigned int *)(memoria + y * schermo.passo);
        for (int x = 0; x < larghezza; x++) a[x] = da[x];
    }
}

/* --- gli eventi --- */

static unsigned int pulsanti_prima;
static int trascino;          /* si sta trascinando la finestra in primo piano */
static int presa_x, presa_y;  /* dove la si e' presa, rispetto al suo angolo */

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

    if (appena_premuto) {   /* un clic: la finestra sotto va davanti; sulla barra, la si prende */
        int i = finestra_sotto(px, py);
        if (i >= 0) {
            in_primo_piano(i);
            struct finestra *f = &finestre[ordine[QUANTE - 1]];
            trascino = py < f->y + BARRA;
            presa_x = px - f->x;
            presa_y = py - f->y;
        }
    }
    if (!premuto) trascino = 0;
    if (trascino) {
        struct finestra *f = &finestre[ordine[QUANTE - 1]];
        f->x = px - presa_x;
        f->y = py - presa_y;
    }
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
    if (eventi < 0) {
        scrivi_testo("compositore: gli eventi sono gia' di un altro programma\n");
        return 0;
    }
    componi();

    /* Il ciclo di ogni sistema grafico: aspetta un evento, aggiorna, ridisegna. */
    for (;;) {
        struct evento e;
        if (ricevi(eventi, &e, sizeof e) != sizeof e) return 0;
        if (e.tipo == EVENTO_MOUSE) al_mouse(&e);
        /* i tasti: andranno alla finestra in primo piano, quando ci saranno le applicazioni */
        componi();
    }
}
