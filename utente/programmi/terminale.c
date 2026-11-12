/* terminale: una finestra con dentro la shell di prova. PROVVISORIO (STATO.md): serve a
 * provare la shell dalla scrivania (l'icona "Shell"); l'aspetto e' da disegnare in Figma.
 *
 * Non e' un'app ma un programma normale (APPLICAZIONI.md, sezione 2), in /system/bin: lo fa
 * partire il launcher come la persona (uid 1000), quando lo chiede la scrivania.
 *
 * COME FUNZIONA. Si apre un TERMINALE FINTO (pty, come xterm o il Terminale del Mac): una
 * coppia di capi, il "padrone" (master) che teniamo noi e lo "schiavo" (/dev/pts/N) che diamo
 * alla shell come 0, 1 e 2. In mezzo c'e' il kernel, che fa per la shell quello che farebbe
 * con un terminale vero: l'eco di quello che si scrive, il tasto di cancellazione, la riga
 * intera consegnata con Invio, Ctrl-C (SIGINT) e Ctrl-D (fine). Noi:
 *   - scriviamo nel padrone i tasti premuti: le lettere come sono, Invio come "\r",
 *     cancella come DEL (0x7F, il tasto di cancellazione dei terminali Linux), Ctrl-C e
 *     Ctrl-D come i caratteri 3 e 4 (arrivano come tasti fisici: tasti.h);
 *   - leggiamo dal padrone quello che la shell scrive e lo mettiamo nella GRIGLIA, COLONNE x
 *     RIGHE caselle, con IBM Plex Mono 14 (larghezza fissa: ogni lettera una casella).
 *     Si capiscono solo le lettere, "\n" (riga dopo), "\r" (inizio riga) e "\b" (indietro di
 *     una casella); quando la griglia e' piena tutto sale di una riga (le righe che escono
 *     si perdono: niente storia). Le sequenze di escape (ESC ...) si saltano.
 * Quando la shell finisce (exit, Ctrl-D) il padrone da' "fine" e la finestra si chiude;
 * chiudendo la finestra, la shell riceve SIGHUP e finisce. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "uikit/uikit.h"

#define SHELL "/system/bin/shell"
#define COLONNE 80
#define RIGHE 24
#define FONT_TERMINALE (&plexmono_14)
#define TERMINALE_DENTRO SPAZIO_16       /* tra il bordo della finestra e il testo */
/* i colori: testo chiaro sulla superficie del tema scuro, in tutti e due i temi */
#define TERMINALE_FONDO (tema_scuro.superficie_elevata)
#define TERMINALE_TESTO (tema_scuro.testo_primario)

static int padrone = -1;                 /* il capo del pty che teniamo noi */
static unsigned char griglia[RIGHE][COLONNE];   /* 0: casella vuota */
static int riga, colonna;                /* il cursore (colonna == COLONNE: a fine riga) */
static enum { NORMALE, ESCAPE, CSI, OSC } stato;   /* dentro una sequenza di escape? */
static int cambiata_da = RIGHE, cambiata_a = -1;   /* le righe da ridisegnare */

static struct finestra_tk finestra;

/* --- la griglia --- */

static int casella_l(void) { return glifo_di(FONT_TERMINALE, 'M')->avanzo; }
static int casella_a(void) { return FONT_TERMINALE->altezza; }

static void segna(int r) {
    if (r < cambiata_da) cambiata_da = r;
    if (r > cambiata_a) cambiata_a = r;
}

static void a_capo(void) {
    if (riga < RIGHE - 1) { riga++; return; }
    memmove(griglia[0], griglia[1], (RIGHE - 1) * COLONNE);   /* tutto sale di una riga */
    memset(griglia[RIGHE - 1], 0, COLONNE);
    segna(0);
    segna(RIGHE - 1);
}

/* Un byte scritto dalla shell. */
static void metti(unsigned char c) {
    switch (stato) {
    case ESCAPE: stato = c == '[' ? CSI : c == ']' ? OSC : NORMALE; return;
    case CSI:    if (c >= 0x40 && c <= 0x7E) stato = NORMALE; return;     /* la lettera finale */
    case OSC:    if (c == 7) stato = NORMALE; else if (c == 0x1B) stato = ESCAPE; return;
    case NORMALE: break;
    }
    segna(riga);
    if (c == 0x1B) {
        stato = ESCAPE;
    } else if (c == '\n') {
        a_capo();
    } else if (c == '\r') {
        colonna = 0;
    } else if (c == '\b') {
        if (colonna > 0) colonna--;
    } else if (c >= ' ' && c != 0x7F) {
        if (colonna >= COLONNE) { colonna = 0; a_capo(); }     /* la riga e' piena: si va a capo */
        griglia[riga][colonna++] = c;
    }
    segna(riga);
}

/* --- il widget della griglia --- */

struct schermo_tk { struct widget base; };
static struct schermo_tk schermo;

static void schermo_misura(struct widget *w, int *l, int *a) {
    (void)w;
    *l = COLONNE * casella_l() + 2 * TERMINALE_DENTRO;
    *a = RIGHE * casella_a() + 2 * TERMINALE_DENTRO;
}

static void schermo_disegna(struct widget *w, struct pennello *p) {
    /* il fondo arriva ai bordi della finestra: angoli in basso arrotondati come lei */
    struct tondo fondo = { w->x, w->y, w->l, w->a, { 0, 0, RAGGIO_FINESTRA_NUOVO, RAGGIO_FINESTRA_NUOVO } };
    pennello_colore(p, TERMINALE_FONDO);
    riempi_forma(p, &fondo);
    int cl = casella_l(), ca = casella_a();
    int x0 = w->x + TERMINALE_DENTRO, y0 = w->y + TERMINALE_DENTRO;
    pennello_colore(p, TERMINALE_TESTO);
    for (int r = 0; r < RIGHE; r++) {
        char testo_riga[COLONNE + 1];
        for (int k = 0; k < COLONNE; k++) testo_riga[k] = griglia[r][k] ? (char)griglia[r][k] : ' ';
        testo_riga[COLONNE] = 0;
        testo(p, FONT_TERMINALE, x0, y0 + r * ca, testo_riga);
    }
    /* il cursore: un blocco pieno, con la lettera sotto nel colore del fondo */
    int k = colonna < COLONNE ? colonna : COLONNE - 1;
    int cx = x0 + k * cl, cy = y0 + riga * ca;
    riempi_rettangolo(p, cx, cy, cl, ca);
    if (griglia[riga][k]) {
        char sotto[2] = { (char)griglia[riga][k], 0 };
        pennello_colore(p, TERMINALE_FONDO);
        testo(p, FONT_TERMINALE, cx, cy, sotto);
    }
}

static void schermo_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    (void)w; (void)x; (void)y; (void)pulsanti;   /* un clic gli da' solo il fuoco */
}

static void scrivi_alla_shell(const void *b, int n) {
    if (padrone >= 0 && write(padrone, b, (size_t)n) < 0) { /* pieno: il tasto si perde */ }
}

/* Un carattere (code page 437, come i font) dalla tastiera. */
static void schermo_tasto(struct widget *w, unsigned int tasto) {
    (void)w;
    unsigned char c;
    if (tasto == '\n') c = '\r';              /* Invio: il kernel lo fa diventare "\n" */
    else if (tasto == '\b') c = 0x7F;         /* cancella */
    else if (tasto >= ' ' && tasto < 256 && tasto != 0x7F) c = (unsigned char)tasto;
    else return;
    scrivi_alla_shell(&c, 1);
}

static const struct tipo_widget tipo_schermo = {
    schermo_misura, 0, schermo_disegna, schermo_puntatore, schermo_tasto, 0, 0, 0, 0
};

/* Ctrl-C e Ctrl-D: con Ctrl giu' i caratteri non arrivano (uikit.h), arrivano i tasti fisici. */
static int scorciatoie(struct finestra_tk *f, unsigned int codice, unsigned int modificatori, void *dati) {
    (void)f; (void)dati;
    if (modificatori != MOD_CTRL) return 0;
    unsigned char c = codice == TASTO_C ? 3 : codice == TASTO_D ? 4 : 0;
    if (!c) return 0;
    scrivi_alla_shell(&c, 1);
    return 1;
}

/* Il padrone e' pronto: quello che la shell ha scritto, o la fine (la shell e' finita). */
static void dalla_shell(struct finestra_tk *f, void *dati) {
    (void)dati;
    unsigned char b[4096];
    ssize_t n = read(padrone, b, sizeof b);
    if (n < 0 && (errno == EAGAIN || errno == EINTR)) return;
    if (n <= 0) {                             /* EIO: nessuno tiene piu' lo schiavo */
        finestra_tk_guarda(f, -1, 0, 0);
        finestra_tk_chiudi(f);
        return;
    }
    for (ssize_t i = 0; i < n; i++) metti(b[i]);
    if (cambiata_a < 0) return;
    int ca = casella_a();
    widget_cambiato_zona(&schermo.base, schermo.base.x, schermo.base.y + TERMINALE_DENTRO + cambiata_da * ca,
                         schermo.base.l, (cambiata_a - cambiata_da + 1) * ca);
    cambiata_da = RIGHE;
    cambiata_a = -1;
}

/* Scrive un messaggio nella griglia (quando la shell non parte). */
static void dici(const char *s) {
    while (*s) metti((unsigned char)*s++);
}

/* --- la shell nel pty --- */

static int avvia_shell(void) {
    padrone = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (padrone < 0 || grantpt(padrone) < 0 || unlockpt(padrone) < 0) return -1;
    const char *nome = ptsname(padrone);
    int schiavo = nome ? open(nome, O_RDWR | O_NOCTTY | O_CLOEXEC) : -1;
    if (schiavo < 0) return -1;
    struct winsize misura = { .ws_row = RIGHE, .ws_col = COLONNE };
    ioctl(padrone, TIOCSWINSZ, &misura);
    int passati[3] = { schiavo, schiavo, schiavo };
    int numero = avvia_con(SHELL, passati, 3);
    close(schiavo);                           /* lo tiene solo la shell: quando finisce, "fine" */
    if (numero < 0) return -1;
    fcntl(padrone, F_SETFL, fcntl(padrone, F_GETFL) | O_NONBLOCK);
    return 0;
}

int main(void) {
    schermo = (struct schermo_tk){ .base = { .tipo = &tipo_schermo } };
    finestra_tk_prepara(&finestra, "Shell", &schermo.base);
    finestra_tk_bordo(&finestra);
    /* la misura giusta per la griglia (con il bordo, la finestra la allargherebbe del padding) */
    int l, a;
    schermo_misura(&schermo.base, &l, &a);
    finestra_tk_dimensione(&finestra, l, finestra_tk_corpo(&finestra) + a);
    finestra_tk_ascolta_tasti(&finestra, scorciatoie, 0);
    uikit_fuoco(&finestra, &schermo.base);    /* la tastiera va alla griglia da subito */
    if (avvia_shell() == 0) {
        finestra_tk_guarda(&finestra, padrone, dalla_shell, 0);
    } else {
        dici("terminale: non riesco a far partire " SHELL "\r\n");
        if (padrone >= 0) { close(padrone); padrone = -1; }
    }
    return uikit_esegui(&finestra);
}
