/* CoreKit: la tastiera italiana, dal tasto premuto al carattere (il contratto e' in tastiera_it.h).
 *
 * Questo e' il codice che prima stava dentro il servizio eventi: ora e' della libreria, cosi'
 * lo usano sia eventi (che legge la tastiera vera) sia GKit (client Wayland, che riceve dal
 * compositore solo il codice del tasto e i modificatori).
 *
 * I codici dei tasti sono quelli di Linux (KEY_...): per i tasti principali (1-88) sono gli
 * stessi degli scancode PS/2 "set 1" (KEY_A = 30 = 0x1E). AltGr e' KEY_RIGHTALT. Si tiene
 * traccia di Maiusc, AltGr, Bloc Maiusc e Bloc Num. Il carattere e' il suo numero Unicode
 * (U+00E0 e' a accentata): per le lettere ASCII e' il codice ASCII di sempre; chi riceve lo
 * scrive in UTF-8 (corekit/utf8.h).
 *
 * IL TASTIERINO NUMERICO: con Bloc Num acceso i tasti del tastierino sono cifre e virgola;
 * spento fanno i tasti di movimento (frecce, Home, Fine...). Gli operatori e l'Invio danno
 * sempre il loro carattere. Bloc Maiusc inverte maiuscole e minuscole solo sulle lettere. */
#include <linux/input.h>
#include <stddef.h>

#include "corekit/chiamate.h"
#include "corekit/tastiera_it.h"

#define A_GRAVE 0xE0   /* a accentata */
#define E_GRAVE 0xE8
#define E_ACUTA 0xE9
#define I_GRAVE 0xEC
#define O_GRAVE 0xF2
#define U_GRAVE 0xF9
#define C_CEDIG 0xE7
#define STERLINA 0xA3
#define GRADO   0xB0
#define PARAGR  0xA7
#define EURO    0x20AC

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

/* I codici di Linux: 71-73 = 7 8 9, 75-77 = 4 5 6, 79-81 = 1 2 3, 82 = 0, 83 = punto (KPDOT);
 * 74 e 78 = meno e piu', 55 = per, 98 = diviso, 96 = Invio del tastierino. */
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

/* Combinazioni con AltGr sulla tastiera italiana. */
static unsigned int con_altgr(const struct tastiera_it *t, unsigned int codice) {
    switch (codice) {
    case KEY_E:          return EURO;                       /* AltGr+E */
    case KEY_LEFTBRACE:  return t->maiusc ? '{' : '[';      /* tasto e grave */
    case KEY_RIGHTBRACE: return t->maiusc ? '}' : ']';      /* tasto + */
    case KEY_SEMICOLON:  return '@';                        /* tasto o grave */
    case KEY_APOSTROPHE: return '#';                        /* tasto a grave */
    default:             return 0;
    }
}

void tastiera_it_inizia(struct tastiera_it *t) {
    *t = (struct tastiera_it){ .bloc_num = 1 };
}

void tastiera_it_modificatori(struct tastiera_it *t, unsigned int grezzo) {
    t->maiusc = (grezzo & GREZZO_MAIUSC) != 0;
    t->altgr = (grezzo & GREZZO_ALTGR) != 0;
    t->bloc_maiusc = (grezzo & GREZZO_BLOC_MAIUSC) != 0;
    t->bloc_num = (grezzo & GREZZO_BLOC_NUM) != 0;
}

void tastiera_it_tasto(struct tastiera_it *t, unsigned int codice, int valore, unsigned int *fisico,
                       unsigned int *carattere) {
    *fisico = 0;
    *carattere = 0;
    /* i blocchi cambiano alla pressione */
    if (codice == KEY_NUMLOCK && valore == 1) t->bloc_num = !t->bloc_num;
    if (codice == KEY_CAPSLOCK && valore == 1) t->bloc_maiusc = !t->bloc_maiusc;
    int cifra = 0;   /* il tasto del tastierino che qui e' una cifra (o la virgola) */
    if (e_tasto_di_numeri(codice)) {
        /* Si ricorda cosa si e' dato alla pressione: ripetizione e rilascio fanno lo stesso anche
         * se nel frattempo Bloc Num e' cambiato (se no resterebbe una freccia "premuta"). */
        if (valore == 1)
            t->tastierino[codice] = (unsigned short)(t->bloc_num ? codice : movimento_del_tastierino(codice));
        unsigned int mandato = t->tastierino[codice];
        if (!mandato) return;                       /* il 5 con Bloc Num spento */
        cifra = mandato == codice;
        codice = mandato;                           /* da qui e' il tasto di movimento, o se stesso */
    }
    *fisico = codice;
    /* i tasti che cambiano lo stato, sia quando si premono sia quando si lasciano */
    if (codice == KEY_LEFTSHIFT || codice == KEY_RIGHTSHIFT) { t->maiusc = valore != 0; return; }
    if (codice == KEY_RIGHTALT)                              { t->altgr = valore != 0; return; }
    if (valore == 0) return;                             /* gli altri rilasci non ci interessano */
    if (codice == KEY_CAPSLOCK) return;                  /* (il blocco e' gia' cambiato, in cima) */
    unsigned int c;
    if (cifra || codice == KEY_KPMINUS || codice == KEY_KPPLUS || codice == KEY_KPSLASH ||
        codice == KEY_KPENTER) {
        c = carattere_del_tastierino(codice);
    } else if (codice >= sizeof normale / sizeof normale[0]) {
        /* frecce, tasti multimediali e simili: niente carattere. (Si contano gli ELEMENTI della
         * tabella, non i byte: con i byte qualche tasto multimediale scriveva un carattere a caso.) */
        return;
    } else if (t->altgr) {
        c = con_altgr(t, codice);
    } else {
        c = t->maiusc ? maiuscolo[codice] : normale[codice];
        /* Bloc Maiusc inverte maiuscole e minuscole, ma solo per le lettere */
        if (t->bloc_maiusc && c >= 'a' && c <= 'z') c -= 'a' - 'A';
        else if (t->bloc_maiusc && c >= 'A' && c <= 'Z') c += 'a' - 'A';
    }
    *carattere = c;
}
