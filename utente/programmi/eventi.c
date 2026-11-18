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
 *      in EVENTO_MOUSE / EVENTO_PUNTATORE, la rotella in EVENTO_ROTELLA (chiamate.h); e per ogni
 *      tasto manda anche, prima, un EVENTO_TASTO_GREZZO (il tasto com'e', per Wayland);
 *   4. manda ogni evento, un messaggio per evento, nel tubo che gli presta init al
 *      descrittore 0. All'altro capo legge il compositore.
 *
 * LA RIPETIZIONE: un tasto tenuto giu' lo ripete Linux (value 2), dopo mezzo secondo e
 * poi ogni 30 ms: glielo si chiede con EVIOCSREP.
 *
 * IL TASTIERINO NUMERICO e il suo Bloc Num (qui sotto): con Bloc Num acceso i tasti del
 * tastierino sono cifre e virgola; spento fanno i tasti di movimento (frecce, Home, Fine...).
 * Lo stato lo tiene questo servizio (Linux non lo fa per noi: la tastiera e' presa con
 * EVIOCGRAB, quindi la gestione dei tasti del kernel non la vede) e accende o spegne la
 * spia della tastiera (EV_LED, LED_NUML) a ogni cambio.
 *
 * I TASTI MULTIMEDIALI (volume, riproduzione...) passano come tutti gli altri tasti fisici:
 * a decidere che sono "del sistema" e non dell'app in primo piano e' il compositore. */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/input.h>

#include "corekit/corekit.h"

#define D_TUBO 0            /* prestato da init: qui si scrivono gli eventi */
#define MAX_DISPOSITIVI 16

#define RIPETI_DOPO TASTI_RIPETI_DOPO   /* ms prima di ripetere un tasto tenuto giu' (chiamate.h) */
#define RIPETI_OGNI TASTI_RIPETI_OGNI   /* poi una volta ogni 30 ms */

/* --- la tastiera italiana: dal TASTO premuto al CARATTERE ---
 *
 * I codici dei tasti sono quelli di Linux (KEY_...): per i tasti principali (1-88) sono
 * gli stessi degli scancode PS/2 "set 1" (KEY_A = 30 = 0x1E). AltGr e' KEY_RIGHTALT.
 * Si tiene traccia di Shift, AltGr e Caps Lock. Il carattere e' il suo numero Unicode
 * (U+00E0 e' à): per le lettere ASCII e' il codice ASCII di sempre; chi riceve lo scrive in
 * UTF-8 (corekit/utf8.h). */
#define A_GRAVE 0xE0   /* à */
#define E_GRAVE 0xE8   /* è */
#define E_ACUTA 0xE9   /* é */
#define I_GRAVE 0xEC   /* ì */
#define O_GRAVE 0xF2   /* ò */
#define U_GRAVE 0xF9   /* ù */
#define C_CEDIG 0xE7   /* ç */
#define STERLINA 0xA3  /* £ */
#define GRADO   0xB0   /* ° */
#define PARAGR  0xA7   /* § */
#define EURO    0x20AC /* € */

static const unsigned short normale[0x59] = {
    [0x02] = '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '\'', I_GRAVE, '\b',
    [0x0F] = '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', E_GRAVE, '+', '\n',
    [0x1E] = 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', O_GRAVE, A_GRAVE, '\\',
    [0x2B] = U_GRAVE, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '-',
    [0x37] = '*', [0x39] = ' ', [0x56] = '<',
};

static const unsigned short maiuscolo[0x59] = {
    [0x02] = '!', '"', STERLINA, '$', '%', '&', '/', '(', ')', '=', '?', '^', '\b',
    [0x0F] = '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', E_ACUTA, '*', '\n',
    [0x1E] = 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', C_CEDIG, GRADO, '|',
    [0x2B] = PARAGR, 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ';', ':', '_',
    [0x37] = '*', [0x39] = ' ', [0x56] = '>',
};

static int shift, altgr, caps;
static int numlock = 1;   /* Bloc Num parte acceso, come su Windows e Mac */

/* --- il tastierino numerico ---
 *
 * I codici di Linux: 71-73 = 7 8 9, 75-77 = 4 5 6, 79-81 = 1 2 3, 82 = 0, 83 = punto (KPDOT);
 * 74 e 78 = meno e piu', 55 = per, 98 = diviso, 96 = Invio del tastierino. Gli operatori e
 * l'Invio danno sempre il loro carattere, con Bloc Num acceso o spento (come su Windows);
 * le cifre e il punto dipendono da Bloc Num. */
static int e_tasto_di_numeri(unsigned int codice) {
    return codice >= KEY_KP7 && codice <= KEY_KPDOT && codice != KEY_KPMINUS && codice != KEY_KPPLUS;
}

/* Cosa fa il tasto del tastierino con Bloc Num SPENTO: lo stesso del tasto di movimento. */
static unsigned int movimento_del_tastierino(unsigned int codice) {
    switch (codice) {
    case KEY_KP7:   return KEY_HOME;
    case KEY_KP8:   return KEY_UP;
    case KEY_KP9:   return KEY_PAGEUP;
    case KEY_KP4:   return KEY_LEFT;
    case KEY_KP6:   return KEY_RIGHT;
    case KEY_KP1:   return KEY_END;
    case KEY_KP2:   return KEY_DOWN;
    case KEY_KP3:   return KEY_PAGEDOWN;
    case KEY_KP0:   return KEY_INSERT;
    case KEY_KPDOT: return KEY_DELETE;
    default:        return 0;   /* il 5: non fa niente */
    }
}

/* Il carattere dei tasti del tastierino che lo danno (0 per gli altri). Tastiera italiana:
 * il tasto del punto da' la virgola, come su Windows con quel layout. */
static unsigned int carattere_del_tastierino(unsigned int codice) {
    switch (codice) {
    case KEY_KP0: return '0';  case KEY_KP1: return '1';  case KEY_KP2: return '2';
    case KEY_KP3: return '3';  case KEY_KP4: return '4';  case KEY_KP5: return '5';
    case KEY_KP6: return '6';  case KEY_KP7: return '7';  case KEY_KP8: return '8';
    case KEY_KP9: return '9';  case KEY_KPDOT: return ',';
    case KEY_KPMINUS: return '-';  case KEY_KPPLUS: return '+';
    case KEY_KPSLASH: return '/';  case KEY_KPENTER: return '\n';
    default: return 0;   /* il per (55) lo da' gia' la tabella dei caratteri */
    }
}

/* Cosa si e' mandato quando un tasto di numeri e' stato premuto: il tasto stesso (Bloc Num
 * acceso), il tasto di movimento (spento) o 0 (il 5 spento). Si ricorda perche' la ripetizione
 * e il rilascio devono fare come la pressione, anche se nel frattempo Bloc Num e' cambiato: se
 * no resterebbe una freccia "premuta" per sempre. */
static unsigned short mandato_dal_tastierino[KEY_KPDOT + 1];

static void luce_bloc_num(void);

/* --- i tasti GREZZI, per Wayland ---
 *
 * I programmi Wayland vogliono il tasto com'e' (il codice di Linux, premuto o lasciato) e lo
 * stato dei modificatori: il carattere se lo traducono da se' con la keymap. Per ogni tasto,
 * prima di tradurlo, si manda un EVENTO_TASTO_GREZZO. 'mod_giu' ha un bit per ogni tasto
 * modificatore tenuto giu' (a sinistra e a destra contano ciascuno), cosi' rilasciare uno dei
 * due Maiusc non spegne l'altro. */
enum { M_LSHIFT, M_RSHIFT, M_LCTRL, M_RCTRL, M_LALT, M_RALT, M_LMETA, M_RMETA };
static unsigned int mod_giu;

static int bit_modificatore(unsigned int codice) {
    switch (codice) {
    case KEY_LEFTSHIFT: return M_LSHIFT;   case KEY_RIGHTSHIFT: return M_RSHIFT;
    case KEY_LEFTCTRL:  return M_LCTRL;    case KEY_RIGHTCTRL:  return M_RCTRL;
    case KEY_LEFTALT:   return M_LALT;     case KEY_RIGHTALT:   return M_RALT;
    case KEY_LEFTMETA:  return M_LMETA;    case KEY_RIGHTMETA:  return M_RMETA;
    default:            return -1;
    }
}

/* I modificatori adesso, come bit GREZZO_... */
static int modificatori_adesso(void) {
    int m = 0;
    if (mod_giu & (1u << M_LSHIFT | 1u << M_RSHIFT)) m |= GREZZO_MAIUSC;
    if (mod_giu & (1u << M_LCTRL | 1u << M_RCTRL))   m |= GREZZO_CTRL;
    if (mod_giu & (1u << M_LALT))                    m |= GREZZO_ALT;
    if (mod_giu & (1u << M_RALT))                    m |= GREZZO_ALTGR;
    if (mod_giu & (1u << M_LMETA | 1u << M_RMETA))   m |= GREZZO_META;
    if (caps)    m |= GREZZO_BLOC_MAIUSC;
    if (numlock) m |= GREZZO_BLOC_NUM;
    return m;
}

static void manda_evento(const struct evento *e) {
    if (manda(D_TUBO, e, sizeof *e) < 0) esci();   /* il tubo non c'e' piu': init ci rifara' */
}

/* Combinazioni con AltGr sulla tastiera italiana. */
static unsigned int con_altgr(unsigned int codice) {
    switch (codice) {
    case KEY_E:          return EURO;                /* AltGr+E */
    case KEY_LEFTBRACE:  return shift ? '{' : '[';   /* tasto è */
    case KEY_RIGHTBRACE: return shift ? '}' : ']';   /* tasto + */
    case KEY_SEMICOLON:  return '@';                 /* tasto ò */
    case KEY_APOSTROPHE: return '#';                 /* tasto à */
    default:             return 0;
    }
}

/* I tasti che non si ripetono mai, come su Windows e Mac: i modificatori e i blocchi. */
static int e_modificatore(unsigned int codice) {
    return codice == KEY_LEFTSHIFT || codice == KEY_RIGHTSHIFT || codice == KEY_LEFTCTRL ||
           codice == KEY_RIGHTCTRL || codice == KEY_LEFTALT || codice == KEY_RIGHTALT ||
           codice == KEY_LEFTMETA || codice == KEY_RIGHTMETA || codice == KEY_CAPSLOCK ||
           codice == KEY_NUMLOCK || codice == KEY_SCROLLLOCK;
}

/* Un tasto: premuto (1), ripetuto (2) o lasciato (0). LA RIPETIZIONE nasce in un posto solo:
 * il kernel (un tasto tenuto giu' per RIPETI_DOPO ms si ripete ogni RIPETI_OGNI ms, valore 2:
 * lo si regola con EVIOCSREP in prova()). Qui la si passa uguale per tutti i tasti, fisici e
 * caratteri, tranne i modificatori. */
static void tasto(unsigned int codice, int valore) {
    if (valore == 2 && e_modificatore(codice)) return;
    if (codice == KEY_NUMLOCK && valore == 1) { numlock = !numlock; luce_bloc_num(); }
    if (codice == KEY_CAPSLOCK && valore == 1) caps = !caps;
    /* il tasto grezzo (per Wayland), con lo stato dei modificatori DOPO di lui: i blocchi sono
     * gia' cambiati qui sopra, i modificatori si aggiornano adesso */
    int bit = bit_modificatore(codice);
    if (bit >= 0 && valore != 2) mod_giu = valore ? mod_giu | 1u << bit : mod_giu & ~(1u << bit);
    manda_evento(&(struct evento){ .tipo = EVENTO_TASTO_GREZZO, .tasto = codice, .dx = modificatori_adesso(),
                                   .pulsanti = (unsigned int)valore });
    int cifra = 0;   /* il tasto del tastierino che qui e' una cifra (o la virgola) */
    if (e_tasto_di_numeri(codice)) {
        if (valore == 1)
            mandato_dal_tastierino[codice] = (unsigned short)(numlock ? codice : movimento_del_tastierino(codice));
        unsigned int mandato = mandato_dal_tastierino[codice];
        if (!mandato) return;                       /* il 5 con Bloc Num spento */
        cifra = mandato == codice;
        codice = mandato;                           /* da qui e' il tasto di movimento, o se stesso */
    }
    /* il tasto "fisico" (frecce, Canc, F1-F12, tastierino, Esc...): 'pulsanti' e' 1 premuto,
     * 2 ripetuto, 0 lasciato. Chi non vuole le ripetizioni (i giochi) scarta il 2. */
    manda_evento(&(struct evento){ .tipo = EVENTO_TASTO_FISICO, .tasto = codice,
                                   .pulsanti = (unsigned int)valore });
    /* i tasti che cambiano lo stato, sia quando si premono sia quando si lasciano */
    if (codice == KEY_LEFTSHIFT || codice == KEY_RIGHTSHIFT) { shift = valore != 0; return; }
    if (codice == KEY_RIGHTALT)                              { altgr = valore != 0; return; }
    if (valore == 0) return;                             /* gli altri rilasci non ci interessano */
    if (codice == KEY_CAPSLOCK) return;   /* (il blocco e' gia' cambiato, in cima) */
    unsigned int c;
    if (cifra || codice == KEY_KPMINUS || codice == KEY_KPPLUS || codice == KEY_KPSLASH ||
        codice == KEY_KPENTER) {
        c = carattere_del_tastierino(codice);
    } else if (codice >= sizeof normale / sizeof normale[0]) {
        /* frecce, tasti multimediali e simili: niente carattere. (Si contano gli ELEMENTI della
         * tabella, non i byte: con i byte i codici fino a 177 leggevano oltre la tabella e
         * qualche tasto multimediale scriveva un carattere a caso.) */
        return;
    } else if (altgr) {
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
    int tastiera;                /* e' una tastiera: ha la spia di Bloc Num da accendere */
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
    /* cinque pezzi variabili: si compone la riga e si scrive con una sola write */
    char riga[512];
    int n = snprintf(riga, sizeof riga, "eventi: %s \"%s\" (%s)\n", cosa, nome, file);
    if (n >= (int)sizeof riga) n = (int)sizeof riga - 1;
    scrivi(riga, n);
}

/* La spia di Bloc Num di una tastiera: si scrive nel suo file un evento EV_LED. Se la
 * tastiera non ha la spia (o non si puo' scrivere) non succede niente: lo stato resta qui. */
static void luce_alla_tastiera(const struct dispositivo *p) {
    struct input_event e[2] = {
        { .type = EV_LED, .code = LED_NUML, .value = numlock },
        { .type = EV_SYN, .code = SYN_REPORT, .value = 0 },
    };
    if (write(p->d, e, sizeof e) < 0) return;
}

static void luce_bloc_num(void) {
    for (int i = 0; i < MAX_DISPOSITIVI; i++)
        if (dispositivi[i].d >= 0 && dispositivi[i].tastiera) luce_alla_tastiera(&dispositivi[i]);
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

    /* in lettura e scrittura: la scrittura serve solo per le spie (EV_LED) della tastiera */
    int d = open(file, O_RDWR | O_NONBLOCK | O_CLOEXEC);
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
    *p = (struct dispositivo){ .d = d, .numero = s.st_rdev, .tastiera = tastiera, .assoluto = assoluto,
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
        luce_alla_tastiera(p);   /* una tastiera collegata dopo prende lo stato di adesso */
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
