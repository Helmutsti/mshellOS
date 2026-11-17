/* terminale: una finestra con dentro la shell di prova. PROVVISORIO (STATO.md): serve a
 * provare la shell dalla scrivania (l'icona "Terminale"). L'aspetto e' quello del disegno
 * Figma (pagina "App - Terminale"): barra del titolo, niente barra degli strumenti, e nel
 * corpo il pannello chiaro incassato (uikit/pannello.h) con il testo e la barra di scorrimento.
 *
 * E' un'app (utente/applicazioni/Terminale): la fa partire il launcher, dal vassoio delle app
 * o dall'icona della scrivania, e (unica tra le app) come SISTEMA, uid 0: PROVVISORIO,
 * decisione della persona del 17/11/2026 (il launcher, uid_dell_app). La shell dentro la
 * facciamo partire NOI (avvia_shell), e ha i nostri poteri: oggi quelli del sistema. Quando
 * il Terminale girera' come la persona, anche la shell sara' della persona.
 *
 * COME FUNZIONA. Si apre un TERMINALE FINTO (pty, come xterm o il Terminale del Mac): una
 * coppia di capi, il "padrone" (master) che teniamo noi e lo "schiavo" (/dev/pts/N) che diamo
 * alla shell come 0, 1 e 2. In mezzo c'e' il kernel, che fa per la shell quello che farebbe
 * con un terminale vero: l'eco di quello che si scrive, il tasto di cancellazione, la riga
 * intera consegnata con Invio, Ctrl-C (SIGINT) e Ctrl-D (fine).
 *
 * IL LAVORO DEL TERMINALE LO FA libvterm (la libreria di Neovim e Vim: strumenti/compilazione/
 * libvterm.sh dice da dove viene). Noi:
 *   - quello che la shell (o un programma) scrive nel padrone lo passiamo a libvterm senza
 *     guardarlo (vterm_input_write): lei legge le lettere e le sequenze di escape (cursore,
 *     pulizia dello schermo, colori, schermo alternativo...) e tiene la GRIGLIA, COLONNE x
 *     RIGHE caselle: lettera, colore e attributi di ogni casella, il cursore. Ci avvisa delle
 *     righe cambiate (damage) e noi ridisegniamo solo quelle;
 *   - ridisegniamo con il nostro disegno: IBM Plex Mono 13 (larghezza fissa: ogni lettera
 *     una casella, righe da 20) e i colori del tema. I 16 colori ANSI (SGR 30-37, 90-97 e gli
 *     sfondi) sono i colori del tema (mappa_ansi: il testo pastello per il testo, il pastello
 *     per lo sfondo); i colori a 256 e RGB sono quelli che da'
 *     libvterm. Le righe che escono dall'alto si perdono: niente storia;
 *   - i tasti li diamo a libvterm (vterm_keyboard_key e vterm_keyboard_unichar) e quello che
 *     produce (le lettere, le frecce come sequenze, Ctrl+lettera) va nel padrone; vanno nel
 *     padrone anche le risposte di libvterm alle domande dei programmi.
 * Quando la shell finisce (exit, Ctrl-D) il padrone da' "fine" e la finestra si chiude;
 * chiudendo la finestra, la shell riceve SIGHUP e finisce. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <vterm.h>

#include "uikit/uikit.h"

#define SHELL "/system/bin/shell"
/* Il tipo di terminale che diamo alla shell, che lo passa ai comandi: libvterm parla come
 * xterm, con 256 colori. I programmi che disegnano a schermo intero (vim) lo leggono per sapere
 * quali sequenze usare; la descrizione sta in /etc/terminfo (strumenti/compilazione/vim.sh). */
#define TERM_SHELL "TERM=xterm-256color"
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

static int padrone = -1;                 /* il capo del pty che teniamo noi */
static VTerm *vt;                        /* il terminale di libvterm */
static VTermScreen *vschermo;            /* la sua griglia */
static int riga, colonna;                /* il cursore (da movecursor) */
static int cursore_visibile = 1;         /* da settermprop */
static int cambiata_da = RIGHE, cambiata_a = -1;   /* le righe da ridisegnare */

static struct finestra_tk finestra;
static struct pannello_tk pannello;

static void scrivi_alla_shell(const void *b, size_t n) {
    if (padrone >= 0 && write(padrone, b, n) < 0) { /* pieno: il tasto si perde */ }
}

/* --- gli avvisi di libvterm --- */

static void segna(int da, int a) {
    if (da < 0) da = 0;
    if (a > RIGHE - 1) a = RIGHE - 1;
    if (da < cambiata_da) cambiata_da = da;
    if (a > cambiata_a) cambiata_a = a;
}

/* Una zona della griglia e' cambiata: si ridisegnano le sue righe. */
static int al_cambio(VTermRect r, void *dati) {
    (void)dati;
    segna(r.start_row, r.end_row - 1);
    return 1;
}

static int al_cursore(VTermPos nuova, VTermPos vecchia, int visibile, void *dati) {
    (void)dati;
    segna(vecchia.row, vecchia.row);
    segna(nuova.row, nuova.row);
    riga = nuova.row;
    colonna = nuova.col;
    cursore_visibile = visibile;
    return 1;
}

static int alla_proprieta(VTermProp proprieta, VTermValue *valore, void *dati) {
    (void)dati;
    if (proprieta == VTERM_PROP_CURSORVISIBLE) {
        cursore_visibile = valore->boolean;
        segna(riga, riga);
    } else if (proprieta == VTERM_PROP_ALTSCREEN) {
        segna(0, RIGHE - 1);                 /* lo schermo e' un altro: tutto da rifare */
    }
    return 1;
}

/* Quello che libvterm produce (i tasti, le risposte ai programmi) va alla shell. */
static void dal_terminale(const char *b, size_t n, void *dati) {
    (void)dati;
    scrivi_alla_shell(b, n);
}

static const VTermScreenCallbacks callback_schermo = {
    .damage = al_cambio, .movecursor = al_cursore, .settermprop = alla_proprieta,
};

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

/* I 16 COLORI ANSI (i numeri SGR 30-37 e 90-97 per il testo, 40-47 e 100-107 per lo sfondo)
 * sono i colori del tema in uso, i colori di sistema. Ogni colore di sistema ha nel tema una
 * COPPIA fatta per stare insieme: il "pastello" (fondi leggeri) e il "testo pastello" (testo
 * sul pastello e sul pannello, almeno 5:1). Il testo prende il testo pastello, lo sfondo il
 * pastello: cosi' ogni testo sta su ogni sfondo, in chiaro e in scuro (il meno leggibile e' il
 * grigio, 8, su un pastello: 3,3:1).
 * PERCHE' DUE TAVOLOZZE. Con una sola (il testo pastello anche per gli sfondi) un testo del
 * colore del testo su uno sfondo colorato non si leggeva: i messaggi di errore di vim (bianco,
 * 15, su rosso, 1) uscivano grigio scuro su rosso scuro nel tema chiaro, e chiaro su rosso
 * chiaro nello scuro: i testi pastello hanno la stessa luminosita' del testo.
 * IL TESTO: il nero (0) e il bianco (7 e 15) sono il colore del testo (seguono il tema: in
 * scuro il testo e' chiaro); 8 (nero acceso, 90) e' il grigio e 9 (rosso acceso, 91)
 * l'arancio, come la shell li chiede; gli altri accesi (10-14) come i loro normali.
 * LO SFONDO: gli stessi colori nel pastello; i neutri (0, 7, 8, 15) sono tutti il pastello del
 * grigio, perche' sopra ci deve stare il colore del testo (che e' 0, 7 e 15 insieme). */
static unsigned int mappa_ansi(int indice, int e_sfondo) {
    int sistema;
    switch (indice) {
    case 1:           sistema = COLORE_ROSSO; break;
    case 2: case 10:  sistema = COLORE_VERDE; break;
    case 3: case 11:  sistema = COLORE_GIALLO; break;
    case 4: case 12:  sistema = COLORE_BLU; break;
    case 5: case 13:  sistema = COLORE_VIOLA; break;
    case 6: case 14:  sistema = COLORE_MENTA; break;
    case 9:           sistema = COLORE_ARANCIO; break;
    default:          sistema = COLORE_GRIGIO; break;   /* 0, 7, 8, 15 */
    }
    if (e_sfondo) return tema_corrente->sistema[sistema].pastello;
    if (sistema != COLORE_GRIGIO) return tema_corrente->sistema[sistema].testo_pastello;
    return indice == 8 ? TESTO_SECONDARIO : TESTO_PRIMARIO;
}

/* Il colore di un colore di libvterm, come testo o come sfondo: quello predefinito e' del tema
 * (il testo; per lo sfondo 0, "non si disegna": c'e' il pannello); i primi 16 sono i colori
 * ANSI del tema (mappa_ansi); gli altri (256 colori e RGB) li da' libvterm, uguali per testo e
 * sfondo. In inverso (reverse) i due colori si scambiano gia' scelti: lo sfondo prende il
 * colore del testo e il testo quello dello sfondo (schermo_disegna), come in xterm; la coppia
 * resta la stessa, quindi resta leggibile. */
static unsigned int colore_di(VTermColor c, int e_sfondo) {
    if (VTERM_COLOR_IS_DEFAULT_FG(&c) || VTERM_COLOR_IS_DEFAULT_BG(&c))
        return e_sfondo ? 0 : TESTO_PRIMARIO;
    if (VTERM_COLOR_IS_INDEXED(&c) && c.indexed.idx < 16) return mappa_ansi(c.indexed.idx, e_sfondo);
    vterm_screen_convert_color_to_rgb(vschermo, &c);
    return OPACO(((unsigned int)c.rgb.red << 16) | ((unsigned int)c.rgb.green << 8) | c.rgb.blue);
}

/* La lettera di una casella, in UTF-8 (il font ha i caratteri per numero Unicode; uno che non
 * ha lo disegna come il segno sostitutivo). Scrive in 'dove' (5 byte) e dice se la casella ha
 * qualcosa da disegnare: 0 se e' vuota. Si disegna il carattere base: gli accenti combinati
 * (le altre chars[]) non ci sono. */
static int lettera_di(const VTermScreenCell *cella, char dove[5]) {
    uint32_t c = cella->chars[0];
    if (c == 0 || c == (uint32_t)-1 || c == ' ') return 0;       /* -1: la seconda meta' di una lettera larga */
    dove[utf8_scrivi(dove, c)] = 0;
    return 1;
}

static void schermo_disegna(struct widget *w, struct pennello *p) {
    int x0 = w->x, y0 = w->y;
    int giu = (RIGA_ALTA - FONT_TERMINALE->altezza) / 2;      /* il testo al centro della riga */
    for (int r = 0; r < RIGHE; r++) {
        /* prima gli sfondi (colori diversi da quello del pannello), poi le lettere */
        for (int k = 0; k < COLONNE; k++) {
            VTermScreenCell cella;
            vterm_screen_get_cell(vschermo, (VTermPos){ r, k }, &cella);
            unsigned int sfondo = cella.attrs.reverse ? colore_di(cella.fg, 0) : colore_di(cella.bg, 1);
            if (!sfondo) continue;
            pennello_colore(p, sfondo);
            riempi_rettangolo(p, x0 + CASELLA_X(k), y0 + r * RIGA_ALTA, CASELLA_X(k + 1) - CASELLA_X(k), RIGA_ALTA);
        }
        for (int k = 0; k < COLONNE; k++) {
            VTermScreenCell cella;
            vterm_screen_get_cell(vschermo, (VTermPos){ r, k }, &cella);
            char lettera[5];
            if (!lettera_di(&cella, lettera) || cella.attrs.conceal) continue;
            unsigned int colore = colore_di(cella.fg, 0);
            if (cella.attrs.reverse) colore = colore_di(cella.bg, 1) ? colore_di(cella.bg, 1) : SUPERFICIE_ELEVATA;
            pennello_colore(p, colore);
            testo(p, cella.attrs.bold ? FONT_GRASSETTO : FONT_TERMINALE, x0 + CASELLA_X(k),
                  y0 + r * RIGA_ALTA + giu, lettera);
        }
    }
    if (!cursore_visibile) return;
    /* il cursore: un blocco nel colore del testo, con la lettera sotto nel colore del pannello */
    int k = colonna < COLONNE ? colonna : COLONNE - 1;
    int cx = x0 + CASELLA_X(k), cy = y0 + riga * RIGA_ALTA + (RIGA_ALTA - CURSORE_ALTO) / 2;
    pennello_colore(p, TESTO_PRIMARIO);
    riempi_rettangolo(p, cx, cy, CURSORE_LARGO, CURSORE_ALTO);
    VTermScreenCell sotto;
    vterm_screen_get_cell(vschermo, (VTermPos){ riga, k }, &sotto);
    char lettera[5];
    if (lettera_di(&sotto, lettera)) {
        pennello_colore(p, SUPERFICIE_ELEVATA);
        testo(p, sotto.attrs.bold ? FONT_GRASSETTO : FONT_TERMINALE, cx, y0 + riga * RIGA_ALTA + giu, lettera);
    }
}

static void schermo_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    (void)w; (void)x; (void)y; (void)pulsanti;   /* un clic gli da' solo il fuoco */
}

/* Un carattere dalla tastiera (il suo numero Unicode): Invio, cancella e Tab sono tasti per
 * libvterm, tutte le lettere di stampa (anche le accentate) sono caratteri, che libvterm manda
 * alla shell in UTF-8. */
static void schermo_tasto(struct widget *w, unsigned int tasto) {
    (void)w;
    if (tasto == '\n') vterm_keyboard_key(vt, VTERM_KEY_ENTER, VTERM_MOD_NONE);
    else if (tasto == '\b') vterm_keyboard_key(vt, VTERM_KEY_BACKSPACE, VTERM_MOD_NONE);
    else if (tasto == '\t') vterm_keyboard_key(vt, VTERM_KEY_TAB, VTERM_MOD_NONE);
    else if ((tasto >= ' ' && tasto < 0x7F) || tasto >= 0xA0) vterm_keyboard_unichar(vt, tasto, VTERM_MOD_NONE);
}

static const struct tipo_widget tipo_schermo = {
    schermo_misura, schermo_disponi, schermo_disegna, schermo_puntatore, schermo_tasto, 0, 0, 0, 0
};

/* I TASTI FISICI (codici Linux, tasti.h): Esc, frecce, Home/End, PagSu/PagGiu, Ins, Canc, i tasti
 * funzione, e Ctrl+lettera (con Ctrl giu' i caratteri non arrivano: uikit.h). Gli altri (le
 * lettere, Invio, Tab, cancella) arrivano come caratteri. */
#define TASTO_HOME 102
#define TASTO_SU_PAGINA 104
#define TASTO_FINE 107
#define TASTO_GIU_PAGINA 109
#define TASTO_INS 110
#define TASTO_F3 61
#define TASTO_F10 68
#define TASTO_F11 87
#define TASTO_F12 88

/* La lettera di un tasto fisico (la tastiera QWERTY: la posizione), o 0. */
static char lettera_del_tasto(unsigned int codice) {
    static const char fila1[] = "qwertyuiop", fila2[] = "asdfghjkl", fila3[] = "zxcvbnm";
    if (codice >= 16 && codice < 16 + 10) return fila1[codice - 16];
    if (codice >= 30 && codice < 30 + 9) return fila2[codice - 30];
    if (codice >= 44 && codice < 44 + 7) return fila3[codice - 44];
    return 0;
}

static VTermKey tasto_di_vterm(unsigned int codice) {
    switch (codice) {
    case TASTO_ESC:          return VTERM_KEY_ESCAPE;
    case TASTO_SU:           return VTERM_KEY_UP;
    case TASTO_GIU:          return VTERM_KEY_DOWN;
    case TASTO_SINISTRA:     return VTERM_KEY_LEFT;
    case TASTO_DESTRA:       return VTERM_KEY_RIGHT;
    case TASTO_HOME:         return VTERM_KEY_HOME;
    case TASTO_FINE:         return VTERM_KEY_END;
    case TASTO_SU_PAGINA:    return VTERM_KEY_PAGEUP;
    case TASTO_GIU_PAGINA:   return VTERM_KEY_PAGEDOWN;
    case TASTO_INS:          return VTERM_KEY_INS;
    case TASTO_CANC:         return VTERM_KEY_DEL;
    case TASTO_F11:          return VTERM_KEY_FUNCTION(11);
    case TASTO_F12:          return VTERM_KEY_FUNCTION(12);
    default:
        if (codice >= TASTO_F1 && codice <= TASTO_F2) return VTERM_KEY_FUNCTION(codice - TASTO_F1 + 1);
        if (codice >= TASTO_F3 && codice <= TASTO_F10) return VTERM_KEY_FUNCTION(codice - TASTO_F3 + 3);
        return VTERM_KEY_NONE;
    }
}

static int scorciatoie(struct finestra_tk *f, unsigned int codice, unsigned int modificatori, void *dati) {
    (void)f; (void)dati;
    VTermModifier mod = (modificatori & MOD_CTRL ? VTERM_MOD_CTRL : 0) |
                        (modificatori & MOD_MAIUSC ? VTERM_MOD_SHIFT : 0) |
                        (modificatori & MOD_ALT ? VTERM_MOD_ALT : 0);
    VTermKey tasto = tasto_di_vterm(codice);
    if (tasto != VTERM_KEY_NONE) {
        vterm_keyboard_key(vt, tasto, mod);
        return 1;
    }
    char lettera = lettera_del_tasto(codice);
    if (lettera && (modificatori & MOD_CTRL)) {
        vterm_keyboard_unichar(vt, (uint32_t)lettera, mod);
        return 1;
    }
    return 0;
}

/* Il padrone e' pronto: quello che la shell ha scritto, o la fine (la shell e' finita). */
static void dalla_shell(struct finestra_tk *f, void *dati) {
    (void)dati;
    char b[4096];
    ssize_t n = read(padrone, b, sizeof b);
    if (n < 0 && (errno == EAGAIN || errno == EINTR)) return;
    if (n <= 0) {                             /* EIO: nessuno tiene piu' lo schiavo */
        finestra_tk_guarda(f, -1, 0, 0);
        finestra_tk_chiudi(f);
        return;
    }
    vterm_input_write(vt, b, (size_t)n);      /* libvterm legge, aggiorna la griglia e ci avvisa */
    vterm_screen_flush_damage(vschermo);
    if (cambiata_a < 0) return;
    widget_cambiato_zona(&schermo.base, schermo.base.x, schermo.base.y + cambiata_da * RIGA_ALTA,
                         schermo.base.l, (cambiata_a - cambiata_da + 1) * RIGA_ALTA);
    cambiata_da = RIGHE;
    cambiata_a = -1;
}

/* Scrive un messaggio nella griglia (quando la shell non parte). */
static void dici(const char *s) {
    vterm_input_write(vt, s, strlen(s));
    vterm_screen_flush_damage(vschermo);
}

/* --- libvterm --- */

static void prepara_vterm(void) {
    vt = vterm_new(RIGHE, COLONNE);
    vterm_set_utf8(vt, 1);
    vterm_output_set_callback(vt, dal_terminale, 0);
    vschermo = vterm_obtain_screen(vt);
    vterm_screen_enable_altscreen(vschermo, 1);
    vterm_screen_set_callbacks(vschermo, &callback_schermo, 0);
    vterm_screen_set_damage_merge(vschermo, VTERM_DAMAGE_ROW);
    vterm_screen_reset(vschermo, 1);
}

/* --- la shell nel pty ---
 *
 * Apriamo il padrone, diamo la misura della griglia (TIOCSWINSZ: la legge chi disegna a schermo
 * intero), e facciamo partire la shell con lo schiavo come 0, 1 e 2 (avvia_con_variabile: fork
 * ed exec, come per ogni programma; il figlio non si porta dietro nient'altro di nostro, tranne
 * il registro del sistema, e nemmeno le porte verso i servizi). L'ambiente della shell e' solo
 * TERM. La SESSIONE la fa la shell da se', appena parte: setsid e TIOCSCTTY, cosi' lo schiavo
 * diventa il suo terminale di controllo (Ctrl-C ai programmi, e SIGHUP a lei quando il padrone
 * si chiude, cioe' quando la finestra si chiude). Lo schiavo lo apriamo con O_NOCTTY: non e' il
 * nostro terminale. */
static int avvia_shell(void) {
    padrone = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (padrone < 0 || grantpt(padrone) < 0 || unlockpt(padrone) < 0) return -1;
    const char *nome = ptsname(padrone);
    int schiavo = nome ? open(nome, O_RDWR | O_NOCTTY | O_CLOEXEC) : -1;
    if (schiavo < 0) return -1;
    struct winsize misura = { .ws_row = RIGHE, .ws_col = COLONNE };
    ioctl(padrone, TIOCSWINSZ, &misura);
    int passati[3] = { schiavo, schiavo, schiavo };
    int numero = avvia_con_variabile(SHELL, passati, 3, TERM_SHELL);
    close(schiavo);                           /* lo tiene solo la shell: quando finisce, "fine" */
    if (numero < 0) return -1;
    fcntl(padrone, F_SETFL, fcntl(padrone, F_GETFL) | O_NONBLOCK);
    return 0;
}

int main(void) {
    prepara_vterm();
    schermo = (struct schermo_tk){ .base = { .tipo = &tipo_schermo, .elastico = 1, .largo = 1 } };
    barra_scorrimento_prepara(&schermo.barra);
    widget_aggiungi(&schermo.base, &schermo.barra.base);
    /* il corpo: il pannello chiaro incassato (la finestra a aree gli lascia 8 intorno) */
    pannello_prepara(&pannello, TERMINALE_DENTRO);
    widget_aggiungi(&pannello.base, &schermo.base);
    finestra_tk_prepara(&finestra, "Terminale \xe2\x80\x93 super amministratore", &pannello.base);   /* U+2013, il trattino lungo */
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
