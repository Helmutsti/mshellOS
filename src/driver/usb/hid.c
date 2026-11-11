/* HID: tastiere, mouse e tavolette USB ("Human Interface Device", dispositivi per le
 * persone).
 *
 * Un dispositivo HID ha un endpoint INTERRUPT IN: il controllore lo interroga da solo
 * (ogni 1-10 ms) e quando c'e' qualcosa di nuovo arriva un RAPPORTO, un pacchetto di
 * pochi byte che dice com'e' il dispositivo ADESSO: quali tasti sono giu', di quanto si
 * e' spostato il mouse, quali pulsanti sono premuti. Non "e' stato premuto A", ma
 * "adesso A e' giu'": i tasti premuti e rilasciati li troviamo noi, confrontando un
 * rapporto con quello di prima.
 *
 * COM'E' FATTO UN RAPPORTO? Due modi, i "protocolli":
 *
 *   BOOT     un formato fisso, scritto nello standard, pensato per il BIOS (che non
 *            ha spazio per capire formati diversi). Lo hanno tastiere e mouse, e lo
 *            si chiede con SET_PROTOCOL. Per la tastiera sono 8 byte:
 *              byte 0     i tasti "modificatori", un bit ciascuno (Ctrl, Shift, Alt,
 *                         Win, a sinistra e a destra)
 *              byte 1     riservato
 *              byte 2..7  fino a 6 tasti giu' insieme, con il loro numero HID ("usage":
 *                         0x04 = A, 0x05 = B, ... 0x28 = Invio). Se ne premi 7, arrivano
 *                         sei 0x01: "troppi tasti" (rollover), e il rapporto non vale.
 *
 *   REPORT   il formato lo decide il dispositivo, e lo descrive in un REPORT
 *            DESCRIPTOR: un piccolo "programma" di elementi, ognuno di 1-5 byte, che
 *            dice campo per campo cosa c'e' nel rapporto. Per un mouse, per esempio:
 *              Usage Page (Button), Usage Minimum 1, Usage Maximum 3,
 *              Report Size 1, Report Count 3, Input (Variable)
 *                  -> 3 bit: i pulsanti 1, 2, 3
 *              Report Size 5, Report Count 1, Input (Constant)
 *                  -> 5 bit di riempimento
 *              Usage Page (Generic Desktop), Usage X, Usage Y,
 *              Logical Minimum -127, Logical Maximum 127,
 *              Report Size 8, Report Count 2, Input (Variable, Relative)
 *                  -> 2 byte con segno: lo spostamento in x e in y
 *            Gli elementi sono di tre generi: GLOBALI (valgono finche' non cambiano:
 *            la pagina, il minimo e il massimo, quanti bit e quanti campi), LOCALI
 *            (valgono per il prossimo Input e basta: quali "usage") e MAIN (Input:
 *            "qui ci sono questi campi"; Collection: solo raggruppamenti). Leggendoli
 *            in fila contiamo i bit, e sappiamo dove sta ogni campo.
 *            Se un dispositivo ha piu' rapporti diversi (un ricevitore senza fili:
 *            mouse, tasti multimediali...), ognuno ha un REPORT ID, e il primo byte
 *            del rapporto dice quale e'.
 *
 * Qui: le TASTIERE le mettiamo in boot (ce l'hanno tutte, e il formato e' sempre
 * quello); i MOUSE e le TAVOLETTE (QEMU "usb-tablet", che da' la posizione assoluta
 * invece degli spostamenti) li lasciamo in report e leggiamo il loro descrittore:
 * cerchiamo X, Y e i pulsanti 1-3.
 *
 * LA RIPETIZIONE. Le tastiere USB non ripetono il tasto tenuto giu' (lo fa il sistema):
 * dopo mezzo secondo lo ripetiamo noi, una volta ogni 30 ms, con un lavoro del timer. */
#include "kernel.h"
#include "pagine.h"
#include "timer.h"
#include "eventi.h"
#include "tastiera_it.h"
#include "usb.h"

#define HID_DEBUG 0          /* 1: stampa dove stanno i campi del mouse (solo per provare) */

#define CLASSE_HID          3
#define PROTOCOLLO_TASTIERA 1
#define DESC_HID            0x21

/* Le richieste della classe HID (byte 1 del SETUP) */
#define RICH_SET_IDLE       0x0A
#define RICH_SET_PROTOCOL   0x0B
#define PROTOCOLLO_BOOT     0

#define MAX_INTERFACCE  4       /* per dispositivo: un ricevitore ha tastiera + mouse + ... */
#define MAX_TASTIERE    4
#define MAX_PUNTATORI   4

#define RIPETI_DOPO     50      /* tick (10 ms): mezzo secondo prima di ripetere */
#define RIPETI_OGNI     3       /* tick: poi una volta ogni 30 ms */

/* --- dal numero HID del tasto al KEY_... di Linux (come hid_keyboard[] in Linux) --- */

static const unsigned char tasti_hid[0x66] = {
    /* 0x00 */   0,   0,   0,   0,  30,  48,  46,  32,  18,  33,  34,  35,  23,  36,  37,  38,  /* -, -, -, -, a..l */
    /* 0x10 */  50,  49,  24,  25,  16,  19,  31,  20,  22,  47,  17,  45,  21,  44,   2,   3,  /* m..z, 1, 2 */
    /* 0x20 */   4,   5,   6,   7,   8,   9,  10,  11,  28,   1,  14,  15,  57,  12,  13,  26,  /* 3..0, Invio, Esc, <-, Tab, spazio, - = [ */
    /* 0x30 */  27,  43,  43,  39,  40,  41,  51,  52,  53,  58,  59,  60,  61,  62,  63,  64,  /* ] \ # ; ' ` , . / BlocMaiusc F1..F7 */
    /* 0x40 */  65,  66,  67,  68,  87,  88,  99,  70, 119, 110, 102, 104, 111, 107, 109, 106,  /* F8..F12, Stamp, BlocScorr, Pausa, Ins, Home, PagSu, Canc, Fine, PagGiu, destra */
    /* 0x50 */ 105, 108, 103,  69,  98,  55,  74,  78,  96,  79,  80,  81,  75,  76,  77,  71,  /* sinistra, giu', su, BlocNum, tastierino / * - + Invio 1..7 */
    /* 0x60 */  72,  73,  82,  83,  86, 127                                                      /* tastierino 8 9 0 . , il tasto < > , Menu */
};

/* I modificatori, bit 0..7 del byte 0: Ctrl, Shift, Alt, Win a sinistra, poi a destra. */
static const unsigned char modificatori[8] = { 29, 42, 56, 125, 97, 54, 100, 126 };

static unsigned int codice_linux(unsigned int usage) {
    return usage < sizeof tasti_hid ? tasti_hid[usage] : 0;
}

/* --- le tastiere --- */

struct tastiera {
    unsigned char prima[8];     /* il rapporto di prima, per trovare cosa e' cambiato */
    unsigned int ripeti;        /* il tasto da ripetere (KEY_... di Linux), 0 nessuno */
    unsigned int prossimo;      /* il tick in cui ripeterlo */
};

static struct tastiera tastiere[MAX_TASTIERE];
static int n_tastiere;
static int lavoro_iscritto;

static int contiene(const unsigned char *tasti, unsigned char usage) {
    for (int i = 2; i < 8; i++)
        if (tasti[i] == usage) return 1;
    return 0;
}

/* Un rapporto boot: dentro l'interruzione del timer. */
static void ricevi_tastiera(void *privato, const unsigned char *dati, unsigned int byte) {
    struct tastiera *t = privato;
    unsigned char ora[8];
    if (byte < 3) return;
    memset(ora, 0, sizeof ora);
    memcpy(ora, dati, byte < 8 ? byte : 8);
    if (ora[2] == 0x01) return;                 /* troppi tasti insieme: non si sa quali */

    /* i modificatori: un bit cambiato e' un tasto premuto o rilasciato */
    unsigned int cambiati = ora[0] ^ t->prima[0];
    for (int b = 0; b < 8; b++)
        if (cambiati & (1u << b)) tastiera_tasto(modificatori[b], (ora[0] >> b) & 1);

    /* rilasciati: c'erano prima e adesso no */
    for (int i = 2; i < 8; i++) {
        unsigned char u = t->prima[i];
        if (!u || contiene(ora, u)) continue;
        unsigned int codice = codice_linux(u);
        if (!codice) continue;
        tastiera_tasto(codice, 0);
        if (codice == t->ripeti) t->ripeti = 0;
    }
    /* premuti: ci sono adesso e prima no. L'ultimo premuto e' quello che si ripete. */
    for (int i = 2; i < 8; i++) {
        unsigned char u = ora[i];
        if (!u || contiene(t->prima, u)) continue;
        unsigned int codice = codice_linux(u);
        if (!codice) continue;
        tastiera_tasto(codice, 1);
        t->ripeti = codice;
        t->prossimo = timer_tick() + RIPETI_DOPO;
    }
    memcpy(t->prima, ora, 8);
}

/* A ogni tick: il tasto tenuto giu' abbastanza si ripete ("premuto" ancora una volta). */
static void ripeti(void) {
    unsigned int ora = timer_tick();
    for (int i = 0; i < n_tastiere; i++) {
        struct tastiera *t = &tastiere[i];
        if (t->ripeti && (int)(ora - t->prossimo) >= 0) {
            tastiera_tasto(t->ripeti, 1);
            t->prossimo = ora + RIPETI_OGNI;
        }
    }
}

/* --- i mouse e le tavolette --- */

struct campo {
    int trovato;
    unsigned int id;            /* in quale rapporto sta (0: il dispositivo non usa ID) */
    unsigned int bit;           /* dove comincia, in bit dall'inizio (dopo l'ID) */
    unsigned int dimensione;    /* quanti bit, da 1 a 32 */
    int segno;                  /* 1: e' un numero con segno */
    int minimo, massimo;        /* Logical Minimum / Maximum */
    int relativo;               /* 1: uno spostamento, 0: una posizione */
};

struct puntatore {
    struct campo x, y, pulsante[3];
    int usa_id;                 /* 1: ogni rapporto comincia con il suo ID */
    unsigned int id;            /* l'ID del rapporto con X e Y */
    int x_ora, y_ora;           /* tavoletta: l'ultima posizione mandata */
    unsigned int pulsanti;      /* gli ultimi pulsanti mandati */
    int mandato;                /* tavoletta: e' gia' partito almeno un evento */
};

static struct puntatore puntatori[MAX_PUNTATORI];
static int n_puntatori;

/* Gli "usage" che ci interessano, come pagina << 16 | numero. */
#define USAGE_X          0x00010030u    /* Generic Desktop, X */
#define USAGE_Y          0x00010031u    /* Generic Desktop, Y */
#define USAGE_ROTELLA    0x00010038u    /* Generic Desktop, Wheel: per ora nessun evento */
#define USAGE_PULSANTE1  0x00090001u    /* Button 1 (sinistro), 2 (destro), 3 (centrale) */

/* Un campo trovato nel descrittore: se e' uno dei nostri, ce lo segniamo (il primo). */
static void segna(struct puntatore *p, unsigned int usage, unsigned int id, unsigned int bit,
                  unsigned int dimensione, int minimo, int massimo, int relativo) {
    struct campo *c = 0;
    if (usage == USAGE_X) c = &p->x;
    else if (usage == USAGE_Y) c = &p->y;
    else if (usage >= USAGE_PULSANTE1 && usage < USAGE_PULSANTE1 + 3) c = &p->pulsante[usage - USAGE_PULSANTE1];
    if (!c || c->trovato || dimensione == 0 || dimensione > 32) return;
    c->trovato = 1;
    c->id = id;
    c->bit = bit;
    c->dimensione = dimensione;
    c->minimo = minimo;
    c->massimo = massimo;
    c->relativo = relativo;
    /* gli spostamenti hanno sempre il segno; le posizioni solo se il minimo e' negativo */
    c->segno = relativo || minimo < 0;
}

#define MAX_USAGE   16      /* Usage locali prima di un Input */
#define MAX_ID      16      /* rapporti diversi (Report ID) */

/* Legge il report descriptor e trova X, Y e i pulsanti. 1 se ci sono X e Y. */
static int analizza(struct puntatore *p, const unsigned char *r, unsigned int n) {
    /* i globali */
    unsigned int pagina = 0, dimensione = 0, conta = 0, id = 0;
    int minimo = 0, massimo_s = 0;
    unsigned int massimo_u = 0;
    /* i locali: si cancellano dopo ogni elemento main */
    unsigned int usage[MAX_USAGE];
    unsigned int n_usage = 0, u_min = 0, u_max = 0;
    int ha_min = 0;
    /* dove siamo arrivati, in bit, in ogni rapporto */
    unsigned int ids[MAX_ID], bit_di[MAX_ID];
    unsigned int n_id = 1, qui = 0;
    ids[0] = 0;
    bit_di[0] = 0;

    memset(p, 0, sizeof *p);
    unsigned int i = 0;
    while (i < n) {
        unsigned int prefisso = r[i];
        if (prefisso == 0xFE) {                 /* elemento "lungo": nessuno lo usa, si salta */
            if (i + 1 >= n) break;
            i += 3 + r[i + 1];
            continue;
        }
        unsigned int lung = prefisso & 3;
        if (lung == 3) lung = 4;
        if (i + 1 + lung > n) break;
        unsigned int u = 0;
        for (unsigned int k = 0; k < lung; k++) u |= (unsigned int)r[i + 1 + k] << (8 * k);
        int s = lung == 1 ? (int)(signed char)u : lung == 2 ? (int)(short)u : (int)u;
        unsigned int genere = (prefisso >> 2) & 3, tag = prefisso >> 4;
        i += 1 + lung;

        if (genere == 1) {                      /* GLOBALE */
            switch (tag) {
            case 0x0: pagina = u; break;                        /* Usage Page */
            case 0x1: minimo = s; break;                        /* Logical Minimum */
            case 0x2: massimo_s = s; massimo_u = u; break;      /* Logical Maximum */
            case 0x7: dimensione = u; break;                    /* Report Size */
            case 0x9: conta = u; break;                         /* Report Count */
            case 0x8: {                                         /* Report ID */
                p->usa_id = 1;
                id = u & 0xFF;
                for (qui = 0; qui < n_id && ids[qui] != id; qui++) {}
                if (qui == n_id) {
                    if (n_id == MAX_ID) return 0;               /* troppi: lasciamo perdere */
                    ids[n_id] = id;
                    bit_di[n_id++] = 0;
                }
                break;
            }
            }
        } else if (genere == 2) {               /* LOCALE */
            /* un usage di 4 byte ha gia' la pagina; se no vale la pagina di adesso */
            unsigned int pieno = lung == 4 ? u : (pagina << 16 | (u & 0xFFFF));
            if (tag == 0x0 && n_usage < MAX_USAGE) usage[n_usage++] = pieno;   /* Usage */
            else if (tag == 0x1) { u_min = pieno; ha_min = 1; }                  /* Usage Minimum */
            else if (tag == 0x2) u_max = pieno;                                  /* Usage Maximum */
        } else if (genere == 0) {               /* MAIN */
            if (tag == 0x8) {                   /* Input */
                /* il massimo: con segno solo se lo e' anche il minimo (0..255 sta in un byte) */
                int massimo = minimo < 0 ? massimo_s : (int)massimo_u;
                /* bit 0: costante (riempimento); bit 1: variabile (un campo per usage,
                 * se no e' un "array" di numeri, come i tasti); bit 2: relativo */
                if (!(u & 1) && (u & 2)) {
                    for (unsigned int k = 0; k < conta; k++) {
                        unsigned int quale = 0;
                        if (n_usage) quale = usage[k < n_usage ? k : n_usage - 1];
                        else if (ha_min) quale = u_min + k > u_max ? u_max : u_min + k;
                        segna(p, quale, id, bit_di[qui] + k * dimensione, dimensione,
                              minimo, massimo, (u & 4) != 0);
                    }
                }
                bit_di[qui] += conta * dimensione;
            }
            n_usage = 0;
            ha_min = 0;
            u_min = u_max = 0;
        }
        /* genere 3: riservato, si salta */
    }

    if (!p->x.trovato || !p->y.trovato || p->x.id != p->y.id) return 0;
    p->id = p->x.id;
    for (int b = 0; b < 3; b++)                 /* i pulsanti valgono se sono nello stesso rapporto */
        if (p->pulsante[b].id != p->id) p->pulsante[b].trovato = 0;
    return 1;
}

/* Il valore di un campo: 'dimensione' bit dal bit 'bit' (il bit 0 e' il piu' basso del byte 0). */
static int leggi(const struct campo *c, const unsigned char *dati, unsigned int byte) {
    unsigned int v = 0;
    for (unsigned int k = 0; k < c->dimensione; k++) {
        unsigned int b = c->bit + k;
        if ((b >> 3) >= byte) break;
        if (dati[b >> 3] & (1u << (b & 7))) v |= 1u << k;
    }
    if (c->segno && c->dimensione < 32 && (v & (1u << (c->dimensione - 1))))
        v |= ~0u << c->dimensione;              /* il segno si estende ai bit di sopra */
    return (int)v;
}

/* Una posizione, dal minimo/massimo alla scala 0..PUNTATORE_MAX. Con numeri a 32 bit
 * (come faceva il vecchio driver virtio-input): il campo si stringe fino a 65535, e 65535 * 32767 ci sta. */
static int scala(const struct campo *c, int valore) {
    if (valore < c->minimo) valore = c->minimo;
    if (valore > c->massimo) valore = c->massimo;
    unsigned int campo = (unsigned int)(c->massimo - c->minimo);
    unsigned int dist = (unsigned int)(valore - c->minimo);
    while (campo > 65535) { campo >>= 1; dist >>= 1; }
    if (campo == 0) return 0;
    return (int)(dist * PUNTATORE_MAX / campo);
}

static const unsigned int bit_pulsante[3] = { PULSANTE_SINISTRO, PULSANTE_DESTRO, PULSANTE_CENTRALE };

/* Un rapporto del mouse o della tavoletta: dentro l'interruzione del timer. */
static void ricevi_puntatore(void *privato, const unsigned char *dati, unsigned int byte) {
    struct puntatore *p = privato;
    if (p->usa_id) {                            /* il primo byte dice quale rapporto e' */
        if (byte < 1 || dati[0] != p->id) return;
        dati++;
        byte--;
    }
    unsigned int pulsanti = 0;
    for (int b = 0; b < 3; b++)
        if (p->pulsante[b].trovato && leggi(&p->pulsante[b], dati, byte)) pulsanti |= bit_pulsante[b];

    struct evento ev;
    memset(&ev, 0, sizeof ev);
    ev.pulsanti = pulsanti;
    if (p->x.relativo) {
        ev.tipo = EVENTO_MOUSE;
        ev.dx = leggi(&p->x, dati, byte);
        ev.dy = leggi(&p->y, dati, byte);       /* anche per l'USB: positivo = in basso */
        if (!ev.dx && !ev.dy && pulsanti == p->pulsanti) return;
    } else {
        ev.tipo = EVENTO_PUNTATORE;
        ev.dx = scala(&p->x, leggi(&p->x, dati, byte));
        ev.dy = scala(&p->y, leggi(&p->y, dati, byte));
        if (p->mandato && ev.dx == p->x_ora && ev.dy == p->y_ora && pulsanti == p->pulsanti) return;
        p->x_ora = ev.dx;
        p->y_ora = ev.dy;
        p->mandato = 1;
    }
    p->pulsanti = pulsanti;
    eventi_metti(&ev);
}

/* --- accogliere il dispositivo --- */

struct interfaccia {
    unsigned int numero, sottoclasse, protocollo;
    unsigned int lung_rapporto;             /* quanti byte ha il report descriptor */
    const unsigned char *desc_endpoint;     /* il primo interrupt IN, 0 se non c'e' */
    struct usb_endpoint *e;
};

static unsigned char *appunti;              /* una pagina per le domande sull'endpoint 0 */

/* Una tastiera: in boot, e senza rapporti ripetuti quando niente cambia. */
static int prendi_tastiera(struct usb_dispositivo *d, struct interfaccia *f) {
    if (n_tastiere == MAX_TASTIERE) return 0;
    usb_controllo(d, TIPO_CLASSE | A_INTERFACCIA, RICH_SET_PROTOCOL, PROTOCOLLO_BOOT, f->numero, 0, 0);
    /* SET_IDLE 0: "manda un rapporto solo quando cambia". Molte dicono no: va bene lo stesso. */
    usb_controllo(d, TIPO_CLASSE | A_INTERFACCIA, RICH_SET_IDLE, 0, f->numero, 0, 0);
    if (!lavoro_iscritto) {
        if (!timer_ogni_tick(ripeti)) return 0;
        lavoro_iscritto = 1;
    }
    struct tastiera *t = &tastiere[n_tastiere++];
    memset(t, 0, sizeof *t);
    usb_endpoint_avvia(f->e, ricevi_tastiera, t);
    return 1;
}

/* Un mouse o una tavoletta: resta in report, e si legge il suo descrittore.
 * 1 mouse, 2 tavoletta, 0 se non si capisce. */
static int prendi_puntatore(struct usb_dispositivo *d, struct interfaccia *f) {
    if (n_puntatori == MAX_PUNTATORI || f->lung_rapporto == 0) return 0;
    unsigned int lung = f->lung_rapporto > PAGINA ? PAGINA : f->lung_rapporto;
    int letti = usb_controllo(d, VERSO_IN | A_INTERFACCIA, RICH_GET_DESCRIPTOR, DESC_HID_RAPPORTO << 8,
                              f->numero, appunti, (unsigned short)lung);
    if (letti <= 0) return 0;

    struct puntatore *p = &puntatori[n_puntatori];
    if (!analizza(p, appunti, (unsigned int)letti)) return 0;
#if HID_DEBUG
    kprintf("\nhid: id %u, x %u+%u, y %u+%u, %s, %d..%d", p->id, p->x.bit, p->x.dimensione,
            p->y.bit, p->y.dimensione, p->x.relativo ? "relativo" : "assoluto", p->x.minimo, p->x.massimo);
#endif
    n_puntatori++;
    usb_endpoint_avvia(f->e, ricevi_puntatore, p);
    return p->x.relativo ? 1 : 2;
}

static void scrivi_nome(int *primo, const char *nome) {
    kprintf(*primo ? " -- %s" : " + %s", nome);
    *primo = 0;
}

int usb_hid_prova(struct usb_dispositivo *d, const unsigned char *config, unsigned int byte) {
    struct interfaccia f[MAX_INTERFACCE];
    int n = 0, qui = -1;
    if (byte < 9) return 0;
    unsigned int valore = config[5];            /* bConfigurationValue: il numero da scegliere */

    /* il descrittore di configurazione: tanti descrittori in fila, ognuno con la sua
     * lunghezza (byte 0) e il suo tipo (byte 1). Ogni interfaccia e' seguita dai suoi. */
    for (unsigned int i = 0; i + 2 <= byte; ) {
        const unsigned char *x = config + i;
        unsigned int lung = x[0], tipo = x[1];
        if (lung < 2 || i + lung > byte) break;
        if (tipo == DESC_INTERFACCIA && lung >= 9) {
            qui = -1;
            /* HID, impostazione 0 (le "alternate" non ci servono) */
            if (x[5] == CLASSE_HID && x[3] == 0 && n < MAX_INTERFACCE) {
                qui = n++;
                memset(&f[qui], 0, sizeof f[qui]);
                f[qui].numero = x[2];
                f[qui].sottoclasse = x[6];
                f[qui].protocollo = x[7];
            }
        } else if (qui >= 0 && tipo == DESC_HID && lung >= 9) {
            f[qui].lung_rapporto = x[7] | x[8] << 8;
        } else if (qui >= 0 && tipo == DESC_ENDPOINT && lung >= 7 && !f[qui].desc_endpoint &&
                   (x[2] & 0x80) && (x[3] & 3) == 3) {
            f[qui].desc_endpoint = x;
        }
        i += lung;
    }
    if (n == 0) return 0;

    if (!appunti) {
        indirizzo_t pagina = pagina_chiedi();
        if (!pagina) return 0;
        appunti = (unsigned char *)pagina;
    }

    /* gli endpoint si preparano, poi si sceglie la configurazione: da li' sono attivi */
    for (int k = 0; k < n; k++)
        f[k].e = f[k].desc_endpoint ? usb_endpoint_prepara(d, f[k].desc_endpoint) : 0;
    if (usb_controllo(d, 0, RICH_SET_CONFIGURATION, (unsigned short)valore, 0, 0, 0) < 0) {
        kprintf(" -- HID, ma non accetta la configurazione\n");
        return 1;
    }

    int presi = 0, primo = 1;
    for (int k = 0; k < n; k++) {
        if (!f[k].e) continue;
        if (f[k].protocollo == PROTOCOLLO_TASTIERA) {
            if (prendi_tastiera(d, &f[k])) { scrivi_nome(&primo, "tastiera"); presi++; }
        } else {
            int r = prendi_puntatore(d, &f[k]);
            if (r) { scrivi_nome(&primo, r == 1 ? "mouse" : "mouse (assoluto)"); presi++; }
        }
    }
    if (!presi) return 0;
    kprintf("\n");
    return 1;
}
