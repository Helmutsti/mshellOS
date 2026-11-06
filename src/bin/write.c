// write: programma di scrittura in stile nano.  Uso: write <file>
// Quello che scrivi entra subito nel testo (non ci sono modi come in vi).
//   frecce, Inizio, Fine   muovono il cursore
//   Invio                  va a capo
//   Backspace, Canc        cancellano
//   Ctrl+S                 salva
//   Ctrl+X                 esce (se ci sono modifiche non salvate chiede conferma)
//
// Come funziona:
//   - il testo sta in memoria come elenco di righe;
//   - la console viene messa in modo "grezzo": ogni tasto arriva subito al programma,
//     senza aspettare Invio e senza che la console lo stampi da sola;
//   - a ogni tasto si ridisegna lo schermo con le sequenze di escape ANSI
//     (ESC[H = cursore in alto, ESC[K = cancella fino a fine riga, ESC[7m = colori invertiti);
//   - il salvataggio usa la scrittura sicura della libreria mshelllib.
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include "mshelllib.h"

#define CTRL(c) ((c) & 0x1f)
#define TAB_LARGO 4

enum tasto { SU = 1000, GIU, SINISTRA, DESTRA, INIZIO, FINE, CANC };

struct riga {
    char *testo;
    int lung;
};

static struct {
    const char *file;
    struct riga *righe;
    int n_righe;
    int cx, cy;                 // cursore: carattere e riga nel testo
    int prima_riga, prima_col;  // primo pezzo di testo visibile (scorrimento)
    int alto, largo;            // righe e colonne dello schermo per il testo
    int modificato;
    char messaggio[128];
    struct termios console_prima;
} ed;

// --- console --------------------------------------------------------------

static void scrivi(const char *s, size_t n) {
    scrivi_tutto(1, s, n);
}

static void ripristina_console(void) {
    tcsetattr(0, TCSAFLUSH, &ed.console_prima);
    scrivi("\033[2J\033[H", 7);
}

static void console_grezza(void) {
    tcgetattr(0, &ed.console_prima);
    struct termios t = ed.console_prima;
    t.c_lflag &= ~(tcflag_t)(ECHO | ICANON | ISIG | IEXTEN);  // niente eco, niente attesa di Invio, Ctrl+C e' un tasto
    t.c_iflag &= ~(tcflag_t)(IXON | ICRNL | BRKINT | INPCK | ISTRIP);  // Ctrl+S e' un tasto, Invio non diventa \n
    t.c_oflag &= ~(tcflag_t)OPOST;
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;
    tcsetattr(0, TCSAFLUSH, &t);
}

static void misura_schermo(void) {
    struct winsize w;
    if (ioctl(1, TIOCGWINSZ, &w) == 0 && w.ws_row > 2 && w.ws_col > 10) {
        ed.alto = w.ws_row - 2;                // una riga in alto e una in basso per le scritte
        ed.largo = w.ws_col;
    } else {
        ed.alto = 23;
        ed.largo = 80;
    }
}

// Legge un tasto. I tasti speciali arrivano come sequenze: ESC [ A = freccia su, ecc.
static int leggi_tasto(void) {
    char c;
    while (read(0, &c, 1) != 1)
        ;
    if (c != '\033')
        return (unsigned char)c;
    char s[3];
    if (read(0, &s[0], 1) != 1 || read(0, &s[1], 1) != 1)
        return '\033';
    if (s[0] != '[')
        return '\033';
    switch (s[1]) {
    case 'A': return SU;
    case 'B': return GIU;
    case 'C': return DESTRA;
    case 'D': return SINISTRA;
    case 'H': return INIZIO;
    case 'F': return FINE;
    }
    if (s[1] >= '0' && s[1] <= '9' && read(0, &s[2], 1) == 1 && s[2] == '~') {
        switch (s[1]) {
        case '1': case '7': return INIZIO;
        case '4': case '8': return FINE;
        case '3': return CANC;
        }
    }
    return '\033';
}

// --- testo ----------------------------------------------------------------

static void inserisci_riga(int dove, const char *testo, int lung) {
    ed.righe = realloc(ed.righe, sizeof(struct riga) * (size_t)(ed.n_righe + 1));
    memmove(&ed.righe[dove + 1], &ed.righe[dove], sizeof(struct riga) * (size_t)(ed.n_righe - dove));
    ed.righe[dove].testo = malloc((size_t)lung + 1);
    memcpy(ed.righe[dove].testo, testo, (size_t)lung);
    ed.righe[dove].testo[lung] = '\0';
    ed.righe[dove].lung = lung;
    ed.n_righe++;
}

static void togli_riga(int dove) {
    free(ed.righe[dove].testo);
    memmove(&ed.righe[dove], &ed.righe[dove + 1], sizeof(struct riga) * (size_t)(ed.n_righe - dove - 1));
    ed.n_righe--;
}

static void inserisci_carattere(int c) {
    struct riga *r = &ed.righe[ed.cy];
    r->testo = realloc(r->testo, (size_t)r->lung + 2);
    memmove(&r->testo[ed.cx + 1], &r->testo[ed.cx], (size_t)(r->lung - ed.cx + 1));
    r->testo[ed.cx] = (char)c;
    r->lung++;
    ed.cx++;
    ed.modificato = 1;
}

static void a_capo(void) {
    struct riga *r = &ed.righe[ed.cy];
    inserisci_riga(ed.cy + 1, r->testo + ed.cx, r->lung - ed.cx);
    r = &ed.righe[ed.cy];                      // inserisci_riga puo' aver spostato l'elenco
    r->lung = ed.cx;
    r->testo[r->lung] = '\0';
    ed.cy++;
    ed.cx = 0;
    ed.modificato = 1;
}

// Cancella il carattere prima del cursore; a inizio riga unisce la riga a quella sopra.
static void cancella_indietro(void) {
    if (ed.cx > 0) {
        struct riga *r = &ed.righe[ed.cy];
        memmove(&r->testo[ed.cx - 1], &r->testo[ed.cx], (size_t)(r->lung - ed.cx + 1));
        r->lung--;
        ed.cx--;
    } else if (ed.cy > 0) {
        struct riga *sopra = &ed.righe[ed.cy - 1], *r = &ed.righe[ed.cy];
        ed.cx = sopra->lung;
        sopra->testo = realloc(sopra->testo, (size_t)(sopra->lung + r->lung) + 1);
        memcpy(&sopra->testo[sopra->lung], r->testo, (size_t)r->lung + 1);
        sopra->lung += r->lung;
        togli_riga(ed.cy);
        ed.cy--;
    } else {
        return;
    }
    ed.modificato = 1;
}

static void cancella_avanti(void) {
    if (ed.cx < ed.righe[ed.cy].lung || ed.cy + 1 < ed.n_righe) {
        if (ed.cx < ed.righe[ed.cy].lung) {
            ed.cx++;
        } else {
            ed.cy++;
            ed.cx = 0;
        }
        cancella_indietro();
    }
}

static void muovi(int tasto) {
    switch (tasto) {
    case SINISTRA:
        if (ed.cx > 0)
            ed.cx--;
        else if (ed.cy > 0)
            ed.cx = ed.righe[--ed.cy].lung;
        break;
    case DESTRA:
        if (ed.cx < ed.righe[ed.cy].lung)
            ed.cx++;
        else if (ed.cy + 1 < ed.n_righe) {
            ed.cy++;
            ed.cx = 0;
        }
        break;
    case SU:   if (ed.cy > 0) ed.cy--; break;
    case GIU:  if (ed.cy + 1 < ed.n_righe) ed.cy++; break;
    case INIZIO: ed.cx = 0; break;
    case FINE:   ed.cx = ed.righe[ed.cy].lung; break;
    }
    if (ed.cx > ed.righe[ed.cy].lung)          // la riga nuova puo' essere piu' corta
        ed.cx = ed.righe[ed.cy].lung;
}

// --- file -----------------------------------------------------------------

static int apri(void) {
    FILE *f = fopen(ed.file, "r");
    if (!f) {
        if (errno != ENOENT)
            return -1;
        inserisci_riga(0, "", 0);              // file nuovo: si crea al primo salvataggio
        snprintf(ed.messaggio, sizeof ed.messaggio, "File nuovo");
        return 0;
    }
    char *linea = NULL;
    size_t dim = 0;
    ssize_t n;
    while ((n = getline(&linea, &dim, f)) >= 0) {
        while (n > 0 && (linea[n - 1] == '\n' || linea[n - 1] == '\r'))
            n--;
        inserisci_riga(ed.n_righe, linea, (int)n);
    }
    free(linea);
    fclose(f);
    if (ed.n_righe == 0)
        inserisci_riga(0, "", 0);
    snprintf(ed.messaggio, sizeof ed.messaggio, "Lette %d righe", ed.n_righe);
    return 0;
}

static void salva(void) {
    size_t totale = 0;
    for (int i = 0; i < ed.n_righe; i++)
        totale += (size_t)ed.righe[i].lung + 1;
    char *dati = malloc(totale + 1), *p = dati;
    for (int i = 0; i < ed.n_righe; i++) {
        memcpy(p, ed.righe[i].testo, (size_t)ed.righe[i].lung);
        p += ed.righe[i].lung;
        *p++ = '\n';
    }
    struct stat s;
    mode_t permessi = stat(ed.file, &s) == 0 ? (s.st_mode & 07777) : 0644;
    if (scrivi_file_sicuro(ed.file, dati, totale, permessi) == 0) {
        ed.modificato = 0;
        snprintf(ed.messaggio, sizeof ed.messaggio, "Salvato: %d righe", ed.n_righe);
    } else {
        snprintf(ed.messaggio, sizeof ed.messaggio, "Impossibile salvare: %s", strerror(errno));
    }
    free(dati);
}

// --- schermo --------------------------------------------------------------

// Colonna sullo schermo del carattere cx di una riga (le tabulazioni occupano piu' posti).
static int colonna(const struct riga *r, int cx) {
    int col = 0;
    for (int i = 0; i < cx; i++)
        col += r->testo[i] == '\t' ? TAB_LARGO - col % TAB_LARGO : 1;
    return col;
}

static void disegna(void) {
    int col_cursore = colonna(&ed.righe[ed.cy], ed.cx);
    // scorrimento: tiene il cursore dentro lo schermo
    if (ed.cy < ed.prima_riga) ed.prima_riga = ed.cy;
    if (ed.cy >= ed.prima_riga + ed.alto) ed.prima_riga = ed.cy - ed.alto + 1;
    if (col_cursore < ed.prima_col) ed.prima_col = col_cursore;
    if (col_cursore >= ed.prima_col + ed.largo) ed.prima_col = col_cursore - ed.largo + 1;

    char *buf = malloc((size_t)(ed.alto + 2) * (size_t)(ed.largo + 16) + 256);
    int n = 0;
    n += sprintf(buf + n, "\033[?25l\033[H");  // nasconde il cursore mentre disegna

    // riga in alto: nome del file
    char titolo[256];
    int lt = snprintf(titolo, sizeof titolo, "  write  %s%s", ed.file, ed.modificato ? "  (modificato)" : "");
    if (lt > ed.largo) lt = ed.largo;
    n += sprintf(buf + n, "\033[7m%.*s%*s\033[0m\r\n", lt, titolo, ed.largo - lt, "");

    for (int y = 0; y < ed.alto; y++) {
        int i = ed.prima_riga + y;
        if (i < ed.n_righe) {
            struct riga *r = &ed.righe[i];
            int col = 0;
            for (int k = 0; k < r->lung && col < ed.prima_col + ed.largo; k++) {
                int quanti = r->testo[k] == '\t' ? TAB_LARGO - col % TAB_LARGO : 1;
                for (int q = 0; q < quanti; q++, col++) {
                    if (col >= ed.prima_col && col < ed.prima_col + ed.largo) {
                        unsigned char c = (unsigned char)r->testo[k];
                        buf[n++] = (c == '\t' || c < 32) ? ' ' : (char)c;
                    }
                }
            }
        }
        n += sprintf(buf + n, "\033[K\r\n");
    }

    // riga in basso: messaggio oppure i tasti disponibili
    const char *aiuto = ed.messaggio[0] ? ed.messaggio : "^S Salva   ^X Esci";
    n += sprintf(buf + n, "\033[7m %-*.*s\033[0m", ed.largo - 1, ed.largo - 1, aiuto);

    n += sprintf(buf + n, "\033[%d;%dH\033[?25h", ed.cy - ed.prima_riga + 2, col_cursore - ed.prima_col + 1);
    scrivi(buf, (size_t)n);
    free(buf);
}

// Fa una domanda nella riga in basso e aspetta s oppure n.
static int conferma(const char *domanda) {
    snprintf(ed.messaggio, sizeof ed.messaggio, "%s (s/n)", domanda);
    disegna();
    for (;;) {
        int t = leggi_tasto();
        if (t == 's' || t == 'S') return 1;
        if (t == 'n' || t == 'N' || t == '\033') return 0;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "uso: write <file>\n");
        return 1;
    }
    if (!isatty(0) || !isatty(1)) {
        fprintf(stderr, "write: funziona solo sulla console\n");
        return 1;
    }
    ed.file = argv[1];
    if (apri() != 0) {
        fprintf(stderr, "write: %s: %s\n", ed.file, strerror(errno));
        return 1;
    }
    misura_schermo();
    console_grezza();

    for (;;) {
        disegna();
        int t = leggi_tasto();
        ed.messaggio[0] = '\0';
        if (t == CTRL('x')) {
            if (!ed.modificato || conferma("Modifiche non salvate: uscire lo stesso?"))
                break;
            ed.messaggio[0] = '\0';
        } else if (t == CTRL('s')) {
            salva();
        } else if (t == '\r') {
            a_capo();
        } else if (t == 127 || t == CTRL('h')) {
            cancella_indietro();
        } else if (t == CANC) {
            cancella_avanti();
        } else if (t >= SU) {
            muovi(t);
        } else if (t == '\t' || (t >= 32 && t < 127) || t >= 128) {
            inserisci_carattere(t);
        }
    }
    ripristina_console();
    return 0;
}
