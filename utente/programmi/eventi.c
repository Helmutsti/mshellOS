/* eventi: il servizio del sistema che legge la tastiera e il mouse.
 *
 * In Linux ogni dispositivo d'ingresso (una tastiera, un mouse, la tavoletta di QEMU) e'
 * un file in /dev/input: event0, event1... Leggendolo si ricevono i suoi eventi nel
 * formato di Linux (struct input_event): "il tasto 30 e' giu'", "l'asse X vale 16000",
 * "fine del pacchetto" (SYN_REPORT). Il compositore invece vuole UN SOLO posto da cui
 * leggere, e eventi gia' pronti: un carattere per tasto, il puntatore sulla scala
 * 0..PUNTATORE_MAX. E' questo servizio a fare il passaggio, come InputReader in Android:
 *
 *   1. apre tutti i dispositivi che sono tastiere o puntatori, e li PRENDE PER SE'
 *      (EVIOCGRAB): nessun altro, nemmeno la console del kernel, li sente piu';
 *   2. guarda /dev/input (inotify): un dispositivo collegato dopo (le tastiere USB
 *      arrivano qualche momento dopo l'avvio, o quando le si attacca) si apre da solo;
 *   3. traduce: i tasti con la TASTIERA ITALIANA (qui sotto), i movimenti e i pulsanti
 *      in EVENTO_MOUSE / EVENTO_PUNTATORE, la rotella in EVENTO_ROTELLA (chiamate.h);
 *   4. manda ogni evento, un messaggio per evento, nel tubo che gli presta init al
 *      descrittore 0. All'altro capo legge il compositore.
 *
 * LA RIPETIZIONE: un tasto tenuto giu' lo ripete Linux (value 2), dopo mezzo secondo e
 * poi ogni 30 ms: glielo si chiede con EVIOCSREP. */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/input.h>

#include "libreria.h"

#define D_TUBO 0            /* prestato da init: qui si scrivono gli eventi */
#define MAX_DISPOSITIVI 16

#define RIPETI_DOPO 500     /* ms prima di ripetere un tasto tenuto giu' */
#define RIPETI_OGNI  30     /* poi una volta ogni 30 ms */

/* --- la tastiera italiana: dal TASTO premuto al CARATTERE ---
 *
 * I codici dei tasti sono quelli di Linux (KEY_...): per i tasti principali (1-88) sono
 * gli stessi degli scancode PS/2 "set 1" (KEY_A = 30 = 0x1E). AltGr e' KEY_RIGHTALT.
 * Si tiene traccia di Shift, AltGr e Caps Lock. Le lettere accentate usano i codici
 * della tabella di caratteri dei font del sistema (code page 437). */
#define A_GRAVE 0x85   /* à */
#define E_GRAVE 0x8A   /* è */
#define E_ACUTA 0x82   /* é */
#define I_GRAVE 0x8D   /* ì */
#define O_GRAVE 0x95   /* ò */
#define U_GRAVE 0x97   /* ù */
#define C_CEDIG 0x87   /* ç */
#define STERLINA 0x9C  /* £ */
#define GRADO   0xF8   /* ° */
#define PARAGR  0x15   /* § */

static const unsigned char normale[0x59] = {
    [0x02] = '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '\'', I_GRAVE, '\b',
    [0x0F] = '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', E_GRAVE, '+', '\n',
    [0x1E] = 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', O_GRAVE, A_GRAVE, '\\',
    [0x2B] = U_GRAVE, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '-',
    [0x37] = '*', [0x39] = ' ', [0x56] = '<',
};

static const unsigned char maiuscolo[0x59] = {
    [0x02] = '!', '"', STERLINA, '$', '%', '&', '/', '(', ')', '=', '?', '^', '\b',
    [0x0F] = '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', E_ACUTA, '*', '\n',
    [0x1E] = 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', C_CEDIG, GRADO, '|',
    [0x2B] = PARAGR, 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ';', ':', '_',
    [0x37] = '*', [0x39] = ' ', [0x56] = '>',
};

static int shift, altgr, caps;

static void manda_evento(const struct evento *e) {
    if (manda(D_TUBO, e, sizeof *e) < 0) esci();   /* il tubo non c'e' piu': init ci rifara' */
}

/* Combinazioni con AltGr sulla tastiera italiana. */
static unsigned char con_altgr(unsigned int codice) {
    switch (codice) {
    case KEY_LEFTBRACE:  return shift ? '{' : '[';   /* tasto è */
    case KEY_RIGHTBRACE: return shift ? '}' : ']';   /* tasto + */
    case KEY_SEMICOLON:  return '@';                 /* tasto ò */
    case KEY_APOSTROPHE: return '#';                 /* tasto à */
    default:             return 0;
    }
}

/* Un tasto: premuto (1), ripetuto (2) o lasciato (0). */
static void tasto(unsigned int codice, int valore) {
    /* i tasti che cambiano lo stato, sia quando si premono sia quando si lasciano */
    if (codice == KEY_LEFTSHIFT || codice == KEY_RIGHTSHIFT) { shift = valore != 0; return; }
    if (codice == KEY_RIGHTALT)                              { altgr = valore != 0; return; }
    if (valore == 0) return;                             /* gli altri rilasci non ci interessano */
    if (codice == KEY_CAPSLOCK) { if (valore == 1) caps = !caps; return; }
    if (codice >= sizeof normale) return;                /* frecce e simili: per ora no */

    unsigned char c;
    if (altgr) {
        c = con_altgr(codice);
    } else {
        c = shift ? maiuscolo[codice] : normale[codice];
        /* Caps Lock inverte maiuscole e minuscole, ma solo per le lettere */
        if (caps && c >= 'a' && c <= 'z') c -= 'a' - 'A';
        else if (caps && c >= 'A' && c <= 'Z') c += 'a' - 'A';
    }
    if (c) manda_evento(&(struct evento){ .tipo = EVENTO_TASTO, .tasto = c });
}

/* --- i dispositivi --- */

struct dispositivo {
    int d;                       /* -1: posto libero */
    dev_t numero;                /* il dispositivo (per non aprirlo due volte) */
    int assoluto, relativo;      /* tavoletta (posizione) e/o mouse (spostamenti) */
    int minimo[2], massimo[2];   /* la scala degli assi X e Y della tavoletta */
    int x, y;                    /* la posizione, gia' sulla scala 0..PUNTATORE_MAX */
    int dx, dy, su, destra;      /* sommati fino alla fine del pacchetto */
    int posizione_cambiata;
    unsigned int pulsanti, pulsanti_mandati;
};

static struct dispositivo dispositivi[MAX_DISPOSITIVI];

/* I bit di "cosa sa fare" un dispositivo (EVIOCGBIT): un bit per tipo, o per codice. */
#define BIT(elenco, n) ((elenco[(n) / 8] >> ((n) % 8)) & 1)

static int scala(const struct dispositivo *p, int asse, int valore) {
    long long campo = (long long)p->massimo[asse] - p->minimo[asse];
    if (campo <= 0) return 0;
    if (valore < p->minimo[asse]) valore = p->minimo[asse];
    if (valore > p->massimo[asse]) valore = p->massimo[asse];
    return (int)((valore - p->minimo[asse]) * (long long)PUNTATORE_MAX / campo);
}

static void scrivi_dispositivo(const char *cosa, const char *nome, const char *file) {
    scrivi_testo("eventi: ");
    scrivi_testo(cosa);
    scrivi_testo(" \"");
    scrivi_testo(nome);
    scrivi_testo("\" (");
    scrivi_testo(file);
    scrivi_testo(")\n");
}

/* Prova ad aprire /dev/input/<nome>: si tiene solo se e' una tastiera o un puntatore. */
static void prova(const char *nome) {
    if (strncmp(nome, "event", 5)) return;
    char file[64] = "/dev/input/";
    strncat(file, nome, sizeof file - strlen(file) - 1);
    struct stat s;
    if (stat(file, &s) < 0 || !S_ISCHR(s.st_mode)) return;
    for (int i = 0; i < MAX_DISPOSITIVI; i++)
        if (dispositivi[i].d >= 0 && dispositivi[i].numero == s.st_rdev) return;   /* c'e' gia' */
    int i = 0;
    while (i < MAX_DISPOSITIVI && dispositivi[i].d >= 0) i++;
    if (i == MAX_DISPOSITIVI) return;

    int d = open(file, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (d < 0) return;
    unsigned char tipi[EV_MAX / 8 + 1] = { 0 }, tasti[KEY_MAX / 8 + 1] = { 0 };
    unsigned char assi[ABS_MAX / 8 + 1] = { 0 }, relativi[REL_MAX / 8 + 1] = { 0 };
    ioctl(d, EVIOCGBIT(0, sizeof tipi), tipi);
    ioctl(d, EVIOCGBIT(EV_KEY, sizeof tasti), tasti);
    ioctl(d, EVIOCGBIT(EV_ABS, sizeof assi), assi);
    ioctl(d, EVIOCGBIT(EV_REL, sizeof relativi), relativi);
    int tastiera = BIT(tipi, EV_KEY) && BIT(tasti, KEY_A);
    int assoluto = BIT(tipi, EV_ABS) && BIT(assi, ABS_X) && BIT(assi, ABS_Y);
    int relativo = BIT(tipi, EV_REL) && BIT(relativi, REL_X) && BIT(relativi, REL_Y);
    int rotella = BIT(tipi, EV_REL) && BIT(relativi, REL_WHEEL);
    if ((!tastiera && !assoluto && !relativo) || ioctl(d, EVIOCGRAB, 1) < 0) {   /* non e' per noi, o e' gia' di un altro */
        close(d);
        return;
    }
    struct dispositivo *p = &dispositivi[i];
    *p = (struct dispositivo){ .d = d, .numero = s.st_rdev, .assoluto = assoluto,
                               .relativo = relativo || rotella };
    if (assoluto)
        for (int asse = 0; asse < 2; asse++) {
            struct input_absinfo a;
            if (ioctl(d, EVIOCGABS(asse == 0 ? ABS_X : ABS_Y), &a) < 0) continue;
            p->minimo[asse] = a.minimum;
            p->massimo[asse] = a.maximum;
            if (asse == 0) p->x = scala(p, 0, a.value);
            else p->y = scala(p, 1, a.value);
        }
    if (tastiera) {
        unsigned int ripeti[2] = { RIPETI_DOPO, RIPETI_OGNI };
        ioctl(d, EVIOCSREP, ripeti);
    }
    char nome_dispositivo[96] = "?";
    ioctl(d, EVIOCGNAME(sizeof nome_dispositivo - 1), nome_dispositivo);
    if (tastiera) scrivi_dispositivo("tastiera", nome_dispositivo, file);
    if (assoluto) scrivi_dispositivo("tavoletta", nome_dispositivo, file);
    else if (relativo) scrivi_dispositivo("mouse", nome_dispositivo, file);
}

static void togli_dispositivo(struct dispositivo *p) {
    close(p->d);
    p->d = -1;
}

static unsigned int bit_pulsante(unsigned int codice) {
    return codice == BTN_LEFT ? PULSANTE_SINISTRO : codice == BTN_RIGHT ? PULSANTE_DESTRO
         : codice == BTN_MIDDLE ? PULSANTE_CENTRALE : 0;
}

/* Fine di un pacchetto (SYN_REPORT): si manda quello che e' cambiato. Prima il
 * puntatore (posizione o pulsanti), poi la rotella, come i rapporti USB. */
static void fine_pacchetto(struct dispositivo *p) {
    int pulsanti_cambiati = p->pulsanti != p->pulsanti_mandati;
    if (p->assoluto && (p->posizione_cambiata || pulsanti_cambiati)) {
        manda_evento(&(struct evento){ .tipo = EVENTO_PUNTATORE, .dx = p->x, .dy = p->y,
                                       .pulsanti = p->pulsanti });
        p->pulsanti_mandati = p->pulsanti;
    } else if (p->relativo && (p->dx || p->dy || pulsanti_cambiati)) {
        manda_evento(&(struct evento){ .tipo = EVENTO_MOUSE, .dx = p->dx, .dy = p->dy,
                                       .pulsanti = p->pulsanti });
        p->pulsanti_mandati = p->pulsanti;
    }
    if (p->su || p->destra)       /* Linux: positivo = in su; per noi positivo = in basso */
        manda_evento(&(struct evento){ .tipo = EVENTO_ROTELLA, .dx = p->destra, .dy = -p->su,
                                       .pulsanti = p->pulsanti });
    p->dx = p->dy = p->su = p->destra = p->posizione_cambiata = 0;
}

/* Legge tutto quello che c'e' nel file del dispositivo. */
static void leggi_dispositivo(struct dispositivo *p) {
    struct input_event e[64];
    for (;;) {
        ssize_t n = read(p->d, e, sizeof e);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && errno == EAGAIN) return;
        if (n <= 0) { togli_dispositivo(p); return; }   /* staccato (ENODEV) */
        for (int k = 0; k < (int)(n / sizeof e[0]); k++) {
            const struct input_event *v = &e[k];
            if (v->type == EV_KEY && v->code < BTN_MISC) {
                tasto(v->code, v->value);
            } else if (v->type == EV_KEY && bit_pulsante(v->code)) {
                if (v->value) p->pulsanti |= bit_pulsante(v->code);
                else p->pulsanti &= ~bit_pulsante(v->code);
            } else if (v->type == EV_ABS && (v->code == ABS_X || v->code == ABS_Y)) {
                int asse = v->code == ABS_X ? 0 : 1, valore = scala(p, asse, v->value);
                if (asse == 0) p->x = valore;
                else p->y = valore;
                p->posizione_cambiata = 1;
            } else if (v->type == EV_REL) {
                if (v->code == REL_X) p->dx += v->value;
                else if (v->code == REL_Y) p->dy += v->value;
                else if (v->code == REL_WHEEL) p->su += v->value;
                else if (v->code == REL_HWHEEL) p->destra += v->value;
            } else if (v->type == EV_SYN && v->code == SYN_REPORT) {
                fine_pacchetto(p);
            } else if (v->type == EV_SYN && v->code == SYN_DROPPED) {
                /* Linux ha perso degli eventi: si ricomincia dal prossimo pacchetto */
                p->dx = p->dy = p->su = p->destra = p->posizione_cambiata = 0;
            }
        }
    }
}

int main(void) {
    for (int i = 0; i < MAX_DISPOSITIVI; i++) dispositivi[i].d = -1;

    /* prima ci si mette a guardare /dev/input, poi si guarda cosa c'e' gia': cosi'
     * nessun dispositivo arrivato nel frattempo si perde (prova non apre due volte).
     * La cartella la crea il kernel con il primo dispositivo; se non c'e' ancora (su ARM
     * la tastiera USB arriva dopo), la si crea noi, per poterla guardare */
    mkdir("/dev/input", 0755);
    int guardia = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (guardia >= 0 && inotify_add_watch(guardia, "/dev/input", IN_CREATE | IN_ATTRIB) < 0) {
        close(guardia);
        guardia = -1;
    }
    DIR *cartella = opendir("/dev/input");
    for (struct dirent *v; cartella && (v = readdir(cartella));) prova(v->d_name);
    if (cartella) closedir(cartella);

    for (;;) {
        struct pollfd g[1 + MAX_DISPOSITIVI];
        struct dispositivo *di[1 + MAX_DISPOSITIVI];
        int n = 0;
        g[n] = (struct pollfd){ guardia, POLLIN, 0 };
        di[n++] = 0;
        for (int i = 0; i < MAX_DISPOSITIVI; i++)
            if (dispositivi[i].d >= 0) {
                g[n] = (struct pollfd){ dispositivi[i].d, POLLIN, 0 };
                di[n++] = &dispositivi[i];
            }
        if (poll(g, (nfds_t)n, -1) < 0) {
            if (errno == EINTR) continue;
            return 1;
        }
        for (int k = 1; k < n; k++)
            if (g[k].revents) leggi_dispositivo(di[k]);
        if (g[0].revents & POLLIN) {                       /* un file nuovo in /dev/input */
            char b[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
            ssize_t l = read(guardia, b, sizeof b);
            for (ssize_t k = 0; k < l;) {
                const struct inotify_event *v = (const void *)(b + k);
                if (v->len) prova(v->name);
                k += (ssize_t)(sizeof *v + v->len);
            }
        }
    }
}
