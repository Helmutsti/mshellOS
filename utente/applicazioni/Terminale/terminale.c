/* terminale: una finestra con dentro la shell di prova. PROVVISORIO (STATO.md): serve a
 * provare la shell dalla scrivania (l'icona "Terminale"). L'aspetto e' quello del disegno
 * Figma (pagina "App - Terminale"): barra del titolo, niente barra degli strumenti, e nel
 * corpo il pannello chiaro incassato (uikit/pannello.h) con il testo e la barra di scorrimento.
 *
 * E' un'app (utente/applicazioni/Terminale): la fa partire il launcher come la persona
 * (uid 1000), dal vassoio delle app o dall'icona della scrivania. La shell
 * dentro, invece, la fa partire il launcher come SISTEMA (uid 0), dandole il nostro
 * terminale finto: PROVVISORIO, aperto a chiunque (launcher.h, APRI_SHELL).
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
 *     RIGHE caselle, con IBM Plex Mono 13 (larghezza fissa: ogni lettera una casella, righe
 *     da 20). Si capiscono le lettere, "\n" (riga dopo), "\r" (inizio riga), "\b" (indietro
 *     di una casella) e i colori e il grassetto (ESC [ ... m, le sequenze SGR dei terminali:
 *     0 ripristina, 1 grassetto, 22 niente grassetto, 30-36 i colori, 39 il colore del testo,
 *     90 grigio, 91 arancio); quando la griglia e' piena tutto sale di una riga (le righe che
 *     escono si perdono: niente storia). Le altre sequenze di escape (ESC ...) si saltano.
 *     I colori veri li sceglie il terminale secondo il tema (come nel disegno): la shell dice
 *     solo "rosso", "verde"...
 * Quando la shell finisce (exit, Ctrl-D) il padrone da' "fine" e la finestra si chiude;
 * chiudendo la finestra, la shell riceve SIGHUP e finisce. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "corekit/sistema/launcher.h"
#include "uikit/uikit.h"

#define SHELL "/system/bin/shell"
/* Le misure dal disegno: testo 13 con la riga da 20 e 25 righe. Le lettere di Plex Mono 13
 * avanzano di 7,8 pixel (Figma non li arrotonda): la casella k comincia a k * 39 / 5, arrotondato
 * (il font ha l'avanzo intero, 8: ogni lettera si disegna da sola al suo posto). In 712 pixel di
 * testo ci stanno 91 colonne. Il pannello ha SPAZIO_16 di padding (il testo non ne ha); il
 * pannello sta a CORPO_PADDING (8) dalla finestra (finestra a aree). */
#define COLONNE 91
#define TESTO_LARGO 712
#define CASELLA_X(k) (((k) * 39 + 2) / 5)
#define CURSORE_LARGO 8
#define RIGHE 25
#define FONT_TERMINALE (&plexmono_13)
#define FONT_GRASSETTO (&plexmono_grassetto_13)
#define RIGA_ALTA 20
#define CURSORE_ALTO 16                  /* il cursore a blocco: largo una casella, alto 16, al centro della riga */
#define TERMINALE_DENTRO SPAZIO_16       /* il padding del pannello: tra il suo bordo e il testo */

/* Il colore di una casella (i 4 bit bassi: 1-6 sono i numeri SGR 31-36) e il grassetto (il bit alto). */
enum { COL_TESTO, COL_ROSSO, COL_VERDE, COL_GIALLO, COL_BLU, COL_VIOLA, COL_MENTA, COL_GRIGIO, COL_ARANCIO };
#define GRASSETTO 0x80

static int padrone = -1;                 /* il capo del pty che teniamo noi */
static unsigned char griglia[RIGHE][COLONNE];         /* 0: casella vuota */
static unsigned char stile_casella[RIGHE][COLONNE];   /* colore e grassetto di ogni casella */
static unsigned char stile_ora;          /* quello con cui si scrive adesso */
static int riga, colonna;                /* il cursore (colonna == COLONNE: a fine riga) */
static enum { NORMALE, ESCAPE, CSI, OSC } stato;   /* dentro una sequenza di escape? */
static int parametri[8], quanti_parametri;         /* i numeri della sequenza CSI in corso */
static int cambiata_da = RIGHE, cambiata_a = -1;   /* le righe da ridisegnare */

static struct finestra_tk finestra;
static struct pannello_tk pannello;

/* --- la griglia --- */

static void segna(int r) {
    if (r < cambiata_da) cambiata_da = r;
    if (r > cambiata_a) cambiata_a = r;
}

static void a_capo(void) {
    if (riga < RIGHE - 1) { riga++; return; }
    memmove(griglia[0], griglia[1], (RIGHE - 1) * COLONNE);   /* tutto sale di una riga */
    memmove(stile_casella[0], stile_casella[1], (RIGHE - 1) * COLONNE);
    memset(griglia[RIGHE - 1], 0, COLONNE);
    memset(stile_casella[RIGHE - 1], 0, COLONNE);
    segna(0);
    segna(RIGHE - 1);
}

/* Una sequenza ESC [ ... m: i colori e il grassetto. */
static void colora(void) {
    for (int i = 0; i < quanti_parametri; i++) {
        int n = parametri[i];
        unsigned char peso = stile_ora & GRASSETTO;
        if (n == 0) stile_ora = 0;
        else if (n == 1) stile_ora |= GRASSETTO;
        else if (n == 22) stile_ora &= (unsigned char)~GRASSETTO;
        else if (n >= 30 && n <= 36) stile_ora = (unsigned char)(peso | (n - 30));   /* 30: nero = il testo */
        else if (n == 37 || n == 39) stile_ora = peso;                                 /* il colore del testo */
        else if (n == 90) stile_ora = (unsigned char)(peso | COL_GRIGIO);
        else if (n == 91) stile_ora = (unsigned char)(peso | COL_ARANCIO);
    }
}

/* Un byte scritto dalla shell. */
static void metti(unsigned char c) {
    switch (stato) {
    case ESCAPE:
        stato = c == '[' ? CSI : c == ']' ? OSC : NORMALE;
        if (stato == CSI) { quanti_parametri = 1; parametri[0] = 0; }
        return;
    case CSI:
        if (c >= '0' && c <= '9') { parametri[quanti_parametri - 1] = parametri[quanti_parametri - 1] * 10 + (c - '0'); return; }
        if (c == ';') { if (quanti_parametri < 8) parametri[quanti_parametri++] = 0; return; }
        if (c >= 0x40 && c <= 0x7E) { if (c == 'm') colora(); stato = NORMALE; }     /* la lettera finale */
        return;
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
        griglia[riga][colonna] = c;
        stile_casella[riga][colonna++] = stile_ora;
    }
    segna(riga);
}

/* --- il widget della griglia --- */

/* Sta dentro il padding del pannello (uikit/pannello.h), senza padding suo; la barra di
 * scorrimento e' quella della libreria, nel padding destro del pannello. Il terminale non ha storia: il contenuto e'
 * sempre alto quanto la vista, quindi la barra non compare. */
struct schermo_tk {
    struct widget base;
    struct barra_scorrimento barra;
};
static struct schermo_tk schermo;

static void schermo_misura(struct widget *w, int *l, int *a) {
    (void)w;
    *l = TESTO_LARGO;
    *a = RIGHE * RIGA_ALTA;
}

static void schermo_disponi(struct widget *w) {
    struct schermo_tk *s = (struct schermo_tk *)w;
    barra_scorrimento_posto(&s->barra, w->x, w->y, w->l, w->a);
    scorrimento_misure(&s->barra.scorrimento, w->a, w->a);
}

/* Il colore di una casella, dal tema in uso: i colori "testo pastello" dei colori di sistema. */
static unsigned int colore_di(unsigned char s) {
    switch (s & 0x0F) {
    case COL_ROSSO:   return tema_corrente->sistema[COLORE_ROSSO].testo_pastello;
    case COL_VERDE:   return tema_corrente->sistema[COLORE_VERDE].testo_pastello;
    case COL_GIALLO:  return tema_corrente->sistema[COLORE_GIALLO].testo_pastello;
    case COL_BLU:     return tema_corrente->sistema[COLORE_BLU].testo_pastello;
    case COL_VIOLA:   return tema_corrente->sistema[COLORE_VIOLA].testo_pastello;
    case COL_MENTA:   return tema_corrente->sistema[COLORE_MENTA].testo_pastello;
    case COL_ARANCIO: return tema_corrente->sistema[COLORE_ARANCIO].testo_pastello;
    case COL_GRIGIO:  return TESTO_SECONDARIO;
    default:          return TESTO_PRIMARIO;
    }
}

static void schermo_disegna(struct widget *w, struct pennello *p) {
    int x0 = w->x, y0 = w->y;
    int giu = (RIGA_ALTA - FONT_TERMINALE->altezza) / 2;      /* il testo al centro della riga */
    for (int r = 0; r < RIGHE; r++) {
        int k = 0;
        while (k < COLONNE) {                  /* un pezzo alla volta: stesso colore e peso */
            if (!griglia[r][k]) { k++; continue; }
            unsigned char s = stile_casella[r][k];
            int fine = k;
            pennello_colore(p, colore_di(s));
            while (fine < COLONNE && griglia[r][fine] && stile_casella[r][fine] == s) {
                char lettera[2] = { (char)griglia[r][fine], 0 };
                testo(p, s & GRASSETTO ? FONT_GRASSETTO : FONT_TERMINALE, x0 + CASELLA_X(fine),
                      y0 + r * RIGA_ALTA + giu, lettera);
                fine++;
            }
            k = fine;
        }
    }
    /* il cursore: un blocco nel colore del testo, con la lettera sotto nel colore del pannello */
    int k = colonna < COLONNE ? colonna : COLONNE - 1;
    int cx = x0 + CASELLA_X(k), cy = y0 + riga * RIGA_ALTA + (RIGA_ALTA - CURSORE_ALTO) / 2;
    pennello_colore(p, TESTO_PRIMARIO);
    riempi_rettangolo(p, cx, cy, CURSORE_LARGO, CURSORE_ALTO);
    if (griglia[riga][k]) {
        char sotto[2] = { (char)griglia[riga][k], 0 };
        pennello_colore(p, SUPERFICIE_ELEVATA);
        testo(p, stile_casella[riga][k] & GRASSETTO ? FONT_GRASSETTO : FONT_TERMINALE, cx,
              y0 + riga * RIGA_ALTA + giu, sotto);
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
    schermo_misura, schermo_disponi, schermo_disegna, schermo_puntatore, schermo_tasto, 0, 0, 0, 0
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
    widget_cambiato_zona(&schermo.base, schermo.base.x, schermo.base.y + cambiata_da * RIGA_ALTA,
                         schermo.base.l, (cambiata_a - cambiata_da + 1) * RIGA_ALTA);
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
    int numero = apri_shell(schiavo);         /* la shell parte come sistema (launcher.h) */
    close(schiavo);                           /* lo tiene solo la shell: quando finisce, "fine" */
    if (numero < 0) return -1;
    fcntl(padrone, F_SETFL, fcntl(padrone, F_GETFL) | O_NONBLOCK);
    return 0;
}

int main(void) {
    schermo = (struct schermo_tk){ .base = { .tipo = &tipo_schermo, .elastico = 1, .largo = 1 } };
    barra_scorrimento_prepara(&schermo.barra);
    widget_aggiungi(&schermo.base, &schermo.barra.base);
    /* il corpo: il pannello chiaro incassato (la finestra a aree gli lascia 8 intorno) */
    pannello_prepara(&pannello, TERMINALE_DENTRO);
    widget_aggiungi(&pannello.base, &schermo.base);
    /* il trattino lungo del titolo, nei font, e' il codice 0x96 */
    finestra_tk_prepara(&finestra, "Terminale \x96 super amministratore", &pannello.base);
    finestra_tk_aree(&finestra);
    /* la misura giusta per la griglia: il testo, il padding del pannello e quello del corpo */
    int l, a, intorno = 2 * (TERMINALE_DENTRO + CORPO_PADDING);
    schermo_misura(&schermo.base, &l, &a);
    finestra_tk_dimensione(&finestra, l + intorno, finestra_tk_corpo(&finestra) + a + intorno);
    finestra_tk_ascolta_tasti(&finestra, scorciatoie, 0);
    finestra_tk_dai_fuoco(&finestra, &schermo.base);   /* la tastiera va alla griglia da subito */
    if (avvia_shell() == 0) {
        finestra_tk_guarda(&finestra, padrone, dalla_shell, 0);
    } else {
        dici("terminale: non riesco a far partire " SHELL "\r\n");
        if (padrone >= 0) { close(padrone); padrone = -1; }
    }
    return uikit_esegui(&finestra);
}
