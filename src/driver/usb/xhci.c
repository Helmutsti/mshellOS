/* XHCI: il controllore USB dei computer di oggi (USB 3, e anche USB 2 e 1 sulle stesse
 * prese). Ce l'hanno i PC dal 2012 circa, i server ARM, i Raspberry Pi 4 e 5; QEMU lo
 * offre con "-device qemu-xhci". Sul bus PCI e' la classe 0C 03 30.
 *
 * COME SI PARLA CON LUI. Non con registri per ogni cosa, ma con ANELLI di TRB: pagine
 * di caselle da 16 byte, ognuna un ordine o una notizia. Un anello si percorre in
 * giro: l'ultima casella e' un LINK che rimanda alla prima. Il bit "ciclo" dice di chi
 * e' una casella: chi scrive lo mette al valore del giro attuale, e a ogni giro il
 * valore si inverte; cosi' chi legge sa dove finiscono le caselle nuove.
 *   - l'anello dei COMANDI: noi scriviamo ("dammi un posto per un dispositivo"), poi
 *     suoniamo il campanello 0;
 *   - un anello per ogni ENDPOINT: noi scriviamo i trasferimenti, poi suoniamo il
 *     campanello del dispositivo;
 *   - l'anello degli EVENTI: scrive LUI ("comando finito", "trasferimento finito",
 *     "si e' collegato qualcosa"). Noi lo leggiamo a ogni tick (polling: le
 *     interruzioni del controllore restano spente) e diciamo fin dove abbiamo letto.
 *
 * I CONTESTI. Per ogni dispositivo il controllore tiene una pagina di "contesto": la
 * scheda del dispositivo (slot) e una scheda per endpoint. Non la scriviamo noi:
 * prepariamo un contesto di INPUT con le schede nuove e un comando gli dice di
 * prenderle (Address Device, Configure Endpoint, Evaluate Context).
 *
 * COME SI ACCOGLIE UN DISPOSITIVO (enumerazione):
 *   1. la presa ha qualcosa: per USB 2 la si "resetta" e diventa abilitata (USB 3 lo
 *      fa da sola);
 *   2. Enable Slot: il controllore ci da' un numero di posto;
 *   3. Address Device: gli diamo la scheda (velocita', presa, endpoint 0), e lui da'
 *      un indirizzo al dispositivo;
 *   4. sull'endpoint 0 chiediamo i descrittori: chi e', come e' fatto;
 *   5. il driver della classe (hid.c) decide se prenderlo.
 *
 * PRIMA DI TUTTO, IL BIOS. Sui PC il firmware usa l'USB da solo (per far funzionare
 * la tastiera USB nel BIOS e in GRUB) e finge una tastiera PS/2. Il controllore va
 * chiesto a lui: c'e' una "capacita' estesa" apposta, con un semaforo (BIOS e OS).
 *
 * DA IMPLEMENTARE (funziona in QEMU; per le macchine vere manca):
 *   - gli hub: oggi i dispositivi devono stare sulle prese del controllore, non dietro
 *     un hub (prese del monitor, docking station, tastiere con prese USB);
 *   - collegare un dispositivo dopo l'avvio (gli eventi "presa cambiata" si ignorano);
 *   - i controllori piu' vecchi, per i PC di prima del 2012 circa: EHCI, UHCI, OHCI;
 *   - le tastiere che non parlano il formato standard "boot" (rare): vedi hid.c;
 *   - Raspberry Pi 4 e 5: prima serve il loro PCIe, e i buffer vanno resi visibili al
 *     controllore a mano (la cache non e' condivisa come in QEMU);
 *   - la prova su un computer vero. */
#include "kernel.h"
#include "pagine.h"
#include "paginazione.h"
#include "driver.h"
#include "timer.h"
#include "usb.h"

/* --- i registri --- */

/* Capacita' (all'inizio del BAR 0) */
#define CAP_LUNGHEZZA   0x00      /* byte: dove cominciano i registri operativi */
#define CAP_HCS1        0x04      /* posti (0-7), interruttori (8-18), prese (24-31) */
#define CAP_HCS2        0x08      /* le pagine di appunti (scratchpad) */
#define CAP_HCC1        0x10      /* bit 2: contesti da 64 byte; 16-31: capacita' estese */
#define CAP_DBOFF       0x14      /* dove sono i campanelli */
#define CAP_RTSOFF      0x18      /* dove sono i registri "di esecuzione" */

/* Operativi */
#define OP_USBCMD       0x00
#define OP_USBSTS       0x04
#define OP_PAGESIZE     0x08
#define OP_CRCR         0x18      /* l'anello dei comandi (64 bit) */
#define OP_DCBAAP       0x30      /* l'elenco dei contesti (64 bit) */
#define OP_CONFIG       0x38
#define OP_PORTSC(n)    (0x400 + 0x10 * ((n) - 1))

#define CMD_RUN         (1u << 0)
#define CMD_RESET       (1u << 1)
#define STS_FERMO       (1u << 0)   /* HCHalted */
#define STS_NON_PRONTO  (1u << 11)  /* Controller Not Ready */

/* PORTSC */
#define P_COLLEGATO     (1u << 0)
#define P_ABILITATO     (1u << 1)   /* attenzione: scriverci 1 la DISABILITA */
#define P_RESET         (1u << 4)
#define P_ALIMENTATA    (1u << 9)
#define P_VELOCITA(v)   (((v) >> 10) & 0xF)
#define P_CAMBI         0x00FE0000u /* i bit "e' cambiato": si cancellano scrivendo 1 */
#define P_RESET_FATTO   (1u << 21)
/* I bit che si riscrivono uguali senza effetti. Gli altri (abilitato, reset, i cambi)
 * a scriverli fanno qualcosa: in "neutro" sono a 0. */
#define P_NEUTRO        0x4E00FFE9u

/* Interruttore 0, nei registri di esecuzione */
#define IR0             0x20
#define IR_IMAN         0x00
#define IR_ERSTSZ       0x08
#define IR_ERSTBA       0x10
#define IR_ERDP         0x18
#define ERDP_OCCUPATO   (1u << 3)   /* "Event Handler Busy": si cancella scrivendo 1 */

/* --- i TRB --- */

struct trb {
    unsigned int p0, p1;          /* il parametro: di solito un indirizzo a 64 bit */
    unsigned int stato;
    unsigned int controllo;       /* bit 0: ciclo; 10-15: il tipo */
};

#define TRB_NORMALE        1
#define TRB_SETUP          2
#define TRB_DATI           3
#define TRB_STATO          4
#define TRB_LINK           6
#define TRB_ABILITA_POSTO  9
#define TRB_INDIRIZZO      11
#define TRB_CONFIGURA_EP   12
#define TRB_VALUTA         13
#define TRB_LIBERA_POSTO   10
#define TRB_RESET_EP       14
#define TRB_DEQUEUE        16
#define EV_TRASFERIMENTO   32
#define EV_COMANDO         33
#define EV_PRESA           34

#define TIPO(t)            ((unsigned int)(t) << 10)
#define TIPO_DI(c)         (((c) >> 10) & 0x3F)
#define T_CICLO            (1u << 0)
#define T_CAMBIA_CICLO     (1u << 1)  /* nel LINK: al giro dopo il ciclo si inverte */
#define T_CORTO_OK         (1u << 2)  /* ISP: un trasferimento piu' corto manda un evento */
#define T_AVVISA           (1u << 5)  /* IOC: a trasferimento finito, un evento */
#define T_DATI_DENTRO      (1u << 6)  /* IDT: il SETUP sta nel TRB stesso */
#define T_IN               (1u << 16)

#define CC_OK       1
#define CC_STALLO   6     /* il dispositivo ha detto "no" (o non capisce) */
#define CC_CORTO    13    /* ha mandato meno byte di quanti ne aspettavamo: per noi e' normale */

/* --- un anello --- */

#define CASELLE 256        /* una pagina: 256 TRB da 16 byte; l'ultima e' il LINK */

struct anello {
    volatile struct trb *trb;
    unsigned int dove;     /* la prossima casella da scrivere (o da leggere: eventi) */
    unsigned int ciclo;
};

static int anello_nuovo(struct anello *a) {
    indirizzo_t p = pagina_chiedi();
    if (!p) return 0;
    memset((void *)p, 0, PAGINA);
    a->trb = (volatile struct trb *)p;
    a->dove = 0;
    a->ciclo = 1;
    /* il LINK in fondo: rimanda all'inizio (il suo ciclo si scrive quando ci si arriva) */
    a->trb[CASELLE - 1].p0 = (unsigned int)p;
    a->trb[CASELLE - 1].p1 = (unsigned int)((unsigned long long)p >> 32);
    return 1;
}

static indirizzo_t indirizzo_di(struct anello *a, unsigned int casella) {
    return (indirizzo_t)&a->trb[casella];
}

/* Scrive un TRB e ritorna dov'e'. Il controllo (con il ciclo) per ultimo: finche' il
 * ciclo e' quello vecchio, il controllore non lo considera. */
static indirizzo_t anello_metti(struct anello *a, unsigned long long parametro,
                                unsigned int stato, unsigned int controllo) {
    volatile struct trb *t = &a->trb[a->dove];
    t->p0 = (unsigned int)parametro;
    t->p1 = (unsigned int)(parametro >> 32);
    t->stato = stato;
    __sync_synchronize();
    t->controllo = controllo | a->ciclo;
    indirizzo_t qui = (indirizzo_t)t;
    if (++a->dove == CASELLE - 1) {
        volatile struct trb *link = &a->trb[CASELLE - 1];
        __sync_synchronize();
        link->controllo = TIPO(TRB_LINK) | T_CAMBIA_CICLO | a->ciclo;
        a->dove = 0;
        a->ciclo ^= 1;
    }
    return qui;
}

/* --- controllori, dispositivi, endpoint --- */

#define MAX_CONTROLLORI  2
#define MAX_DISPOSITIVI  8      /* per controllore */
#define MAX_ENDPOINT     4      /* interrupt, per dispositivo */
#define BUFFER_EP        8      /* rapporti in attesa per endpoint */
#define MAX_RAPPORTO     512    /* byte per rapporto (8 x 512 = una pagina) */

struct xhci;

struct usb_endpoint {
    struct usb_dispositivo *disp;
    unsigned int dci;           /* il numero nel contesto: 2 * numero + 1 (per IN) */
    unsigned int byte;          /* quanti byte per rapporto */
    struct anello anello;
    unsigned char *buffer;      /* BUFFER_EP buffer da 'byte' */
    unsigned char buffer_di[CASELLE];   /* quale buffer usa ogni casella dell'anello */
    usb_ricevi ricevi;
    void *privato;
    indirizzo_t ripresa;        /* il comando "riparti da qui" dopo un errore (0: nessuno) */
};

struct usb_dispositivo {
    struct xhci *hc;
    unsigned int posto;         /* lo slot dato dal controllore (1..) */
    unsigned int velocita, presa;
    unsigned char *input;       /* il contesto di input (una pagina) */
    unsigned char *uscita;      /* il contesto del controllore (una pagina) */
    struct anello ep0;
    unsigned char *appunti;     /* una pagina per le risposte sull'endpoint 0 */
    unsigned int dci_massimo;
    /* l'attesa sull'endpoint 0 */
    volatile int ep0_fatto;
    unsigned int ep0_cc, ep0_residuo;
    indirizzo_t ep0_dati, ep0_stato;
    struct usb_endpoint ep[MAX_ENDPOINT];
    int quanti_ep;
};

struct xhci {
    volatile unsigned char *cap, *op, *run;
    volatile unsigned int *campanelli;
    unsigned int prese, posti, ctx;     /* ctx: byte per scheda di contesto (32 o 64) */
    unsigned long long *dcbaa;          /* per ogni posto, il suo contesto */
    struct anello comandi, eventi;
    int attivo;
    /* l'attesa di un comando */
    indirizzo_t cmd_atteso;
    volatile int cmd_fatto;
    unsigned int cmd_cc, cmd_posto;
    struct usb_dispositivo *disp[MAX_DISPOSITIVI + 1];   /* per numero di posto */
    struct usb_dispositivo tutti[MAX_DISPOSITIVI];
    int quanti;
};

static struct xhci controllori[MAX_CONTROLLORI];
static int n_controllori;
static int lavoro_iscritto;

/* --- registri: 32 bit alla volta (anche quelli da 64: prima la parte bassa) --- */

static unsigned int r32(volatile unsigned char *b, unsigned int o) { return *(volatile unsigned int *)(b + o); }
static void w32(volatile unsigned char *b, unsigned int o, unsigned int v) { *(volatile unsigned int *)(b + o) = v; }
static void w64(volatile unsigned char *b, unsigned int o, unsigned long long v) {
    w32(b, o, (unsigned int)v);
    w32(b, o + 4, (unsigned int)(v >> 32));
}

/* Aspetta 'ms' millisecondi (almeno un tick). Il timer gira gia' quando partono i driver. */
static void aspetta_ms(unsigned int ms) {
    unsigned int inizio = timer_tick(), tick = (ms + 9) / 10 + 1;
    while (timer_tick() - inizio < tick) cpu_riposa();
}

/* Aspetta che (registro & maschera) == valore, fino a 'ms' millisecondi. */
static int aspetta_bit(volatile unsigned char *b, unsigned int o, unsigned int maschera,
                       unsigned int valore, unsigned int ms) {
    unsigned int inizio = timer_tick(), tick = ms / 10 + 2;
    while ((r32(b, o) & maschera) != valore)
        if (timer_tick() - inizio > tick) return 0;
    return 1;
}

/* --- le schede di contesto --- */

/* La scheda numero i del contesto di input: 0 = controllo, 1 = slot, 2 = endpoint 0 (dci 1)... */
static volatile unsigned int *scheda_input(struct usb_dispositivo *d, unsigned int i) {
    return (volatile unsigned int *)(d->input + i * d->hc->ctx);
}

/* --- gli eventi --- */

/* Rimette in coda tutti i buffer di un endpoint, dalla casella dove si e' arrivati. */
static void ep_riempi(struct usb_endpoint *e) {
    unsigned int byte = e->byte > MAX_RAPPORTO ? MAX_RAPPORTO : e->byte;
    for (unsigned int b = 0; b < BUFFER_EP; b++) {
        e->buffer_di[e->anello.dove] = (unsigned char)b;
        anello_metti(&e->anello, (indirizzo_t)(e->buffer + b * MAX_RAPPORTO), byte,
                     TIPO(TRB_NORMALE) | T_AVVISA | T_CORTO_OK);
    }
}

/* Un errore (un disturbo sul filo, un "no") FERMA l'endpoint: il controllore non lo
 * interroga piu'. Siamo dentro il tick, non si puo' aspettare: si mandano i due comandi
 * (riparti; riprendi dalla casella nuova) e si rimettono in coda i buffer. Il campanello
 * si suona quando arriva la risposta al secondo comando (vedi evento). */
static void ep_ripristina(struct xhci *hc, struct usb_endpoint *e) {
    if (e->ripresa) return;                       /* gia' in corso */
    unsigned int posto = e->disp->posto;
    anello_metti(&hc->comandi, 0, 0, TIPO(TRB_RESET_EP) | e->dci << 16 | posto << 24);
    unsigned long long da = (unsigned long long)indirizzo_di(&e->anello, e->anello.dove) | e->anello.ciclo;
    ep_riempi(e);
    e->ripresa = anello_metti(&hc->comandi, da, 0, TIPO(TRB_DEQUEUE) | e->dci << 16 | posto << 24);
    __sync_synchronize();
    hc->campanelli[0] = 0;
}

static void evento(struct xhci *hc, volatile struct trb *t) {
    unsigned int tipo = TIPO_DI(t->controllo);
    unsigned int cc = t->stato >> 24;
    unsigned long long dove = t->p0 | (unsigned long long)t->p1 << 32;

    if (tipo == EV_COMANDO) {
        if (dove == hc->cmd_atteso) {
            hc->cmd_cc = cc;
            hc->cmd_posto = t->controllo >> 24;
            hc->cmd_fatto = 1;
            return;
        }
        /* la fine di un ripristino: l'endpoint puo' ripartire */
        for (int i = 1; i <= MAX_DISPOSITIVI; i++) {
            struct usb_dispositivo *d = hc->disp[i];
            for (int j = 0; d && j < d->quanti_ep; j++)
                if (d->ep[j].ripresa && dove == d->ep[j].ripresa) {
                    d->ep[j].ripresa = 0;
                    __sync_synchronize();
                    hc->campanelli[d->posto] = d->ep[j].dci;
                }
        }
        return;
    }
    if (tipo != EV_TRASFERIMENTO) return;      /* le prese che cambiano: per ora niente */

    unsigned int posto = t->controllo >> 24, dci = (t->controllo >> 16) & 0x1F;
    if (posto == 0 || posto > MAX_DISPOSITIVI || !hc->disp[posto]) return;
    struct usb_dispositivo *d = hc->disp[posto];

    if (dci == 1) {                             /* l'endpoint 0 */
        if (cc == CC_CORTO && dove == d->ep0_dati) {
            d->ep0_residuo = t->stato & 0xFFFFFF;   /* poi arriva anche quello dello STATUS */
            return;
        }
        /* finito: con lo STATUS, o con un errore. Un evento in ritardo di una domanda
         * gia' scaduta (non e' il nostro STATUS e va tutto bene) si ignora. */
        if (dove != d->ep0_stato && (cc == CC_OK || cc == CC_CORTO)) return;
        d->ep0_cc = cc;
        d->ep0_fatto = 1;
        return;
    }
    for (int i = 0; i < d->quanti_ep; i++) {
        struct usb_endpoint *e = &d->ep[i];
        if (e->dci != dci) continue;
        if (cc != CC_OK && cc != CC_CORTO) { ep_ripristina(hc, e); return; }
        if (!e->ricevi) return;
        unsigned int casella = (unsigned int)(dove - indirizzo_di(&e->anello, 0)) / (unsigned int)sizeof(struct trb);
        if (casella >= CASELLE - 1) return;
        unsigned int b = e->buffer_di[casella];
        unsigned int resto = t->stato & 0xFFFFFF;
        unsigned int arrivati = resto > e->byte ? 0 : e->byte - resto;
        unsigned char *dati = e->buffer + b * MAX_RAPPORTO;
        e->ricevi(e->privato, dati, arrivati);
        if (e->ripresa) return;                 /* in ripristino: i buffer li rimette lui */
        /* il buffer torna in coda */
        e->buffer_di[e->anello.dove] = (unsigned char)b;
        anello_metti(&e->anello, (indirizzo_t)dati, e->byte,
                     TIPO(TRB_NORMALE) | T_AVVISA | T_CORTO_OK);
        __sync_synchronize();
        hc->campanelli[d->posto] = e->dci;
        return;
    }
}

/* Legge gli eventi nuovi e dice al controllore fin dove e' arrivato. Con le
 * interruzioni spente: la chiamano sia il tick sia chi aspetta un comando. */
static void processa(struct xhci *hc) {
    unsigned int s = interruzioni_spegni();
    int letti = 0;
    for (;;) {
        volatile struct trb *t = &hc->eventi.trb[hc->eventi.dove];
        if ((t->controllo & T_CICLO) != hc->eventi.ciclo) break;
        __sync_synchronize();
        evento(hc, t);
        letti = 1;
        if (++hc->eventi.dove == CASELLE) { hc->eventi.dove = 0; hc->eventi.ciclo ^= 1; }
    }
    if (letti)
        w64(hc->run, IR0 + IR_ERDP, (unsigned long long)indirizzo_di(&hc->eventi, hc->eventi.dove) | ERDP_OCCUPATO);
    interruzioni_ripristina(s);
}

static void controlla(void) {
    for (int i = 0; i < n_controllori; i++)
        if (controllori[i].attivo) processa(&controllori[i]);
}

/* --- i comandi --- */

/* Manda un comando e aspetta la risposta (fino a un secondo). Il codice di
 * completamento, o 0 se non e' arrivato. */
static unsigned int comando(struct xhci *hc, unsigned long long parametro, unsigned int controllo) {
    unsigned int s = interruzioni_spegni();
    hc->cmd_fatto = 0;
    hc->cmd_atteso = anello_metti(&hc->comandi, parametro, 0, controllo);
    __sync_synchronize();
    hc->campanelli[0] = 0;
    interruzioni_ripristina(s);
    unsigned int inizio = timer_tick();
    while (!hc->cmd_fatto) {
        processa(hc);
        if (timer_tick() - inizio > 100) return 0;
    }
    return hc->cmd_cc;
}

/* --- l'endpoint 0 --- */

/* Dopo un "no" (stallo) l'endpoint 0 resta fermo: lo si fa ripartire, e gli si dice
 * di riprendere dalla casella dopo quelle del trasferimento fallito. */
static void ep0_ripristina(struct usb_dispositivo *d) {
    comando(d->hc, 0, TIPO(TRB_RESET_EP) | (1u << 16) | (d->posto << 24));
    comando(d->hc, (unsigned long long)indirizzo_di(&d->ep0, d->ep0.dove) | d->ep0.ciclo,
            TIPO(TRB_DEQUEUE) | (1u << 16) | (d->posto << 24));
}

int usb_controllo(struct usb_dispositivo *d, unsigned char tipo, unsigned char richiesta,
                  unsigned short valore, unsigned short indice, void *buffer, unsigned short byte) {
    struct xhci *hc = d->hc;
    int dentro = (tipo & VERSO_IN) != 0;
    /* TRT: che fase dati c'e' (0 nessuna, 2 verso il dispositivo, 3 verso di noi) */
    unsigned int trt = byte == 0 ? 0 : dentro ? 3 : 2;
    unsigned long long setup = tipo | (unsigned int)richiesta << 8 | (unsigned int)valore << 16 |
                               (unsigned long long)(indice | (unsigned int)byte << 16) << 32;

    unsigned int s = interruzioni_spegni();
    d->ep0_fatto = 0;
    d->ep0_residuo = 0;
    d->ep0_dati = 0;
    anello_metti(&d->ep0, setup, 8, TIPO(TRB_SETUP) | T_DATI_DENTRO | trt << 16);
    if (byte)
        d->ep0_dati = anello_metti(&d->ep0, (indirizzo_t)buffer, byte,
                                   TIPO(TRB_DATI) | T_CORTO_OK | (dentro ? T_IN : 0));
    /* lo STATUS va nel verso opposto ai dati (IN se non ce ne sono) */
    d->ep0_stato = anello_metti(&d->ep0, 0, 0,
                                TIPO(TRB_STATO) | T_AVVISA | ((byte && dentro) ? 0 : T_IN));
    __sync_synchronize();
    hc->campanelli[d->posto] = 1;
    interruzioni_ripristina(s);

    unsigned int inizio = timer_tick();
    while (!d->ep0_fatto) {
        processa(hc);
        if (timer_tick() - inizio > 100) return -1;
    }
    if (d->ep0_cc == CC_STALLO) { ep0_ripristina(d); return -1; }
    if (d->ep0_cc != CC_OK && d->ep0_cc != CC_CORTO) return -1;
    return byte - (int)d->ep0_residuo;
}

/* --- gli endpoint interrupt --- */

/* L'intervallo nel formato del controllore: 2^n x 125 microsecondi. 'b' e' il
 * bInterval del descrittore: millisecondi per USB 1, e gia' un esponente (+1) dopo. */
static unsigned int intervallo(unsigned int velocita, unsigned int b) {
    if (b == 0) b = 1;
    if (velocita == USB_FULL || velocita == USB_LOW) {
        unsigned int n = 3;                       /* 8 x 125 us = 1 ms */
        while (n < 10 && (1u << (n + 1)) <= b * 8) n++;
        return n;
    }
    return b - 1 > 15 ? 15 : b - 1;
}

struct usb_endpoint *usb_endpoint_prepara(struct usb_dispositivo *d, const unsigned char *desc) {
    if (d->quanti_ep == MAX_ENDPOINT) return 0;
    unsigned int indirizzo = desc[2], attributi = desc[3];
    unsigned int byte = (desc[4] | desc[5] << 8) & 0x7FF;
    if (!(indirizzo & 0x80) || (attributi & 3) != 3 || byte == 0) return 0;   /* solo interrupt IN */
    unsigned int pacchetto = byte;                 /* il contesto vuole quello vero */
    if (byte > MAX_RAPPORTO) byte = MAX_RAPPORTO;  /* i nostri buffer: un rapporto HID ci sta */

    struct usb_endpoint *e = &d->ep[d->quanti_ep];
    memset(e, 0, sizeof *e);
    e->disp = d;
    e->dci = (indirizzo & 0xF) * 2 + 1;
    e->byte = byte;
    indirizzo_t pagina = pagina_chiedi();
    if (!pagina || !anello_nuovo(&e->anello)) return 0;
    memset((void *)pagina, 0, PAGINA);
    e->buffer = (unsigned char *)pagina;

    /* il contesto di input: aggiungo questo endpoint (e la scheda dello slot, che dice
     * fin dove arrivano gli endpoint) */
    struct xhci *hc = d->hc;
    memset((void *)scheda_input(d, 0), 0, hc->ctx);
    scheda_input(d, 0)[1] = 1u | (1u << e->dci);
    if (e->dci > d->dci_massimo) d->dci_massimo = e->dci;
    volatile unsigned int *slot = scheda_input(d, 1);
    slot[0] = (slot[0] & 0x07FFFFFF) | d->dci_massimo << 27;
    volatile unsigned int *ep = scheda_input(d, 1 + e->dci);
    memset((void *)ep, 0, hc->ctx);
    ep[0] = intervallo(d->velocita, desc[6]) << 16;
    ep[1] = 3u << 1 | 7u << 3 | pacchetto << 16;     /* 3 tentativi, tipo 7 = interrupt IN */
    ep[2] = (unsigned int)indirizzo_di(&e->anello, 0) | 1;
    ep[3] = (unsigned int)((unsigned long long)indirizzo_di(&e->anello, 0) >> 32);
    ep[4] = byte | pacchetto << 16;                   /* lunghezza media, carico massimo */

    if (comando(hc, (indirizzo_t)d->input, TIPO(TRB_CONFIGURA_EP) | d->posto << 24) != CC_OK) {
        kprintf("xhci: l'endpoint %x non si configura\n", indirizzo);
        return 0;
    }
    d->quanti_ep++;
    return e;
}

void usb_endpoint_avvia(struct usb_endpoint *e, usb_ricevi ricevi, void *privato) {
    unsigned int s = interruzioni_spegni();
    e->ricevi = ricevi;
    e->privato = privato;
    ep_riempi(e);
    __sync_synchronize();
    e->disp->hc->campanelli[e->disp->posto] = e->dci;
    interruzioni_ripristina(s);
}

/* --- accogliere un dispositivo --- */

static const char *nome_velocita(unsigned int v) {
    return v == USB_LOW ? "1,5 Mbit/s" : v == USB_FULL ? "12 Mbit/s" : v == USB_HIGH ? "480 Mbit/s" : "USB 3";
}

static void accogli(struct xhci *hc, unsigned int presa, unsigned int velocita) {
    if (hc->quanti == MAX_DISPOSITIVI) return;
    unsigned int cc = comando(hc, 0, TIPO(TRB_ABILITA_POSTO));
    unsigned int posto = hc->cmd_posto;
    if (cc != CC_OK || posto == 0 || posto > MAX_DISPOSITIVI) {
        kprintf("xhci: presa %u, nessun posto (%u)\n", presa, cc);
        return;
    }

    struct usb_dispositivo *d = &hc->tutti[hc->quanti];
    memset(d, 0, sizeof *d);
    d->hc = hc;
    d->posto = posto;
    d->velocita = velocita;
    d->presa = presa;
    indirizzo_t in = pagina_chiedi(), out = pagina_chiedi(), app = pagina_chiedi();
    if (!in || !out || !app || !anello_nuovo(&d->ep0)) {
        comando(hc, 0, TIPO(TRB_LIBERA_POSTO) | posto << 24);
        return;
    }
    memset((void *)in, 0, PAGINA);
    memset((void *)out, 0, PAGINA);
    d->input = (unsigned char *)in;
    d->uscita = (unsigned char *)out;
    d->appunti = (unsigned char *)app;
    d->dci_massimo = 1;
    hc->dcbaa[posto] = out;

    /* l'endpoint 0: quanto e' grande un pacchetto. Per USB 1 a 12 Mbit/s puo' essere
     * 8, 16, 32 o 64: si parte da 8 e lo si chiede al dispositivo. */
    unsigned int pacchetto = velocita >= USB_SUPER ? 512 : velocita == USB_HIGH ? 64 : 8;

    volatile unsigned int *ctrl = scheda_input(d, 0), *slot = scheda_input(d, 1), *ep0 = scheda_input(d, 2);
    ctrl[1] = 1u | 2u;                                  /* aggiungo slot ed endpoint 0 */
    slot[0] = velocita << 20 | 1u << 27;                /* velocita', un endpoint (il dci 1) */
    slot[1] = presa << 16;                              /* su quale presa del controllore */
    ep0[1] = 3u << 1 | 4u << 3 | pacchetto << 16;       /* 3 tentativi, tipo 4 = controllo */
    ep0[2] = (unsigned int)indirizzo_di(&d->ep0, 0) | 1;
    ep0[3] = (unsigned int)((unsigned long long)indirizzo_di(&d->ep0, 0) >> 32);
    ep0[4] = 8;                                         /* lunghezza media di un trasferimento */

    /* la scheda diventa sua solo se riceve l'indirizzo; da qui gli eventi arrivano a lui */
    hc->disp[posto] = d;
    cc = comando(hc, in, TIPO(TRB_INDIRIZZO) | posto << 24);
    if (cc != CC_OK) {
        kprintf("xhci: presa %u, nessun indirizzo (%u)\n", presa, cc);
        hc->disp[posto] = 0;
        hc->dcbaa[posto] = 0;
        comando(hc, 0, TIPO(TRB_LIBERA_POSTO) | posto << 24);
        return;
    }
    hc->quanti++;

    /* i primi 8 byte del descrittore: dentro c'e' la grandezza vera del pacchetto */
    unsigned char *a = d->appunti;
    if (usb_controllo(d, VERSO_IN, RICH_GET_DESCRIPTOR, DESC_DISPOSITIVO << 8, 0, a, 8) < 8) {
        kprintf("xhci: presa %u, il dispositivo non risponde\n", presa);
        return;
    }
    if (velocita == USB_FULL && a[7] && a[7] != pacchetto) {
        ctrl[0] = 0;
        ctrl[1] = 2u;                                   /* cambio solo l'endpoint 0 */
        ep0[1] = (ep0[1] & 0xFFFF) | (unsigned int)a[7] << 16;
        if (comando(hc, in, TIPO(TRB_VALUTA) | posto << 24) != CC_OK) {
            kprintf("xhci: presa %u, il pacchetto da %u byte non si imposta\n", presa, a[7]);
            return;
        }
    }
    if (usb_controllo(d, VERSO_IN, RICH_GET_DESCRIPTOR, DESC_DISPOSITIVO << 8, 0, a, 18) < 18) return;
    unsigned int vendor = a[8] | a[9] << 8, prodotto = a[10] | a[11] << 8;
    kprintf("USB     : presa %u, %04x:%04x, %s", presa, vendor, prodotto, nome_velocita(velocita));

    /* il descrittore di configurazione: prima i 9 byte con la lunghezza totale, poi tutto */
    if (usb_controllo(d, VERSO_IN, RICH_GET_DESCRIPTOR, DESC_CONFIGURAZIONE << 8, 0, a, 9) < 9) {
        kprintf("\n");
        return;
    }
    unsigned int totale = a[2] | a[3] << 8;
    if (totale > PAGINA) totale = PAGINA;
    int letti = usb_controllo(d, VERSO_IN, RICH_GET_DESCRIPTOR, DESC_CONFIGURAZIONE << 8, 0, a, (unsigned short)totale);
    if (letti < 9) { kprintf("\n"); return; }

    /* la configurazione si copia: la pagina degli appunti serve ancora a hid.c */
    static unsigned char config[PAGINA];
    memcpy(config, a, (unsigned int)letti);
    if (!usb_hid_prova(d, config, (unsigned int)letti)) kprintf(" -- nessun driver\n");
}

/* --- il controllore --- */

/* Il controllore lo usa il BIOS: glielo chiediamo (capacita' estesa 1, "legacy"). */
static void chiedi_al_bios(struct xhci *hc) {
    unsigned int hcc = r32(hc->cap, CAP_HCC1);
    unsigned int pos = (hcc >> 16) << 2;
    for (int giri = 0; pos && giri < 64; giri++) {
        /* le capacita' estese possono stare lontano (sui PC Intel verso 32 KB): prima
         * di leggerne una, la sua pagina va mappata */
        if (!pag_mappa_dispositivo((indirizzo_t)hc->cap + pos, 8)) return;
        unsigned int v = r32(hc->cap, pos);
        if ((v & 0xFF) == 1) {
            if (v & (1u << 16)) {                       /* ce l'ha il BIOS */
                w32(hc->cap, pos, v | (1u << 24));      /* lo chiede il sistema */
                if (!aspetta_bit(hc->cap, pos, 1u << 16, 0, 1000))
                    kprintf("xhci: il BIOS non lascia il controllore\n");
            }
            /* niente piu' interruzioni "SMI" verso il BIOS; i bit di stato si cancellano con 1 */
            w32(hc->cap, pos + 4, (r32(hc->cap, pos + 4) & 0x000E1FEEu) | 0xE0000000u);
            return;
        }
        unsigned int dopo = (v >> 8) & 0xFF;
        if (!dopo) break;
        pos += dopo << 2;
    }
}

/* Sui PC Intel (dal 2012) le prese possono essere del controllore vecchio (EHCI) o di
 * questo: di serie le tiene l'EHCI. Le passiamo tutte qui (come fa Linux). */
static void prese_intel(const struct pci_scheda *s) {
    /* solo i chipset "Panther Point" e "Lynx Point": negli altri quei registri non ci sono */
    static const unsigned short modelli[] = { 0x1E31, 0x8C31, 0x9C31, 0x8CB1, 0x9CB1 };
    int e_uno = 0;
    for (unsigned int i = 0; i < sizeof modelli / sizeof modelli[0]; i++)
        if (s->device == modelli[i]) e_uno = 1;
    if (s->vendor != 0x8086 || !e_uno) return;
    pci_scrivi(s, 0xD8, pci_leggi(s, 0xDC));    /* USB 3: quelle che si possono accendere */
    pci_scrivi(s, 0xD0, pci_leggi(s, 0xD4));    /* USB 2: quelle che si possono spostare */
}

static int prepara(const struct pci_scheda *s) {
    if (n_controllori == MAX_CONTROLLORI) return 0;
    struct xhci *hc = &controllori[n_controllori];
    memset(hc, 0, sizeof *hc);

    indirizzo_t base = pci_bar_memoria(s, 0);
    if (!base || !pag_mappa_dispositivo(base, PAGINA)) return 0;
    pci_accendi(s);
    hc->cap = (volatile unsigned char *)base;
    unsigned int lung = r32(hc->cap, CAP_LUNGHEZZA) & 0xFF;   /* a 32 bit: certi ponti PCIe non leggono un byte */
    unsigned int hcs1 = r32(hc->cap, CAP_HCS1), hcs2 = r32(hc->cap, CAP_HCS2);
    unsigned int dboff = r32(hc->cap, CAP_DBOFF) & ~3u, rtsoff = r32(hc->cap, CAP_RTSOFF) & ~31u;
    hc->posti = hcs1 & 0xFF;
    hc->prese = hcs1 >> 24;
    hc->ctx = (r32(hc->cap, CAP_HCC1) & (1u << 2)) ? 64 : 32;
    if (hc->posti > MAX_DISPOSITIVI) hc->posti = MAX_DISPOSITIVI;
    /* bit 0: sa usare indirizzi a 64 bit. Se no, la RAM deve stare tutta sotto i 4 GB */
    if (!(r32(hc->cap, CAP_HCC1) & 1) && (unsigned long long)memoria_fine() > 0xFFFFFFFFull) {
        kprintf(" -- non vede la RAM oltre i 4 GB\n");
        return 0;
    }

    /* tutta la zona dei registri: fino al piu' lontano tra prese, campanelli, eventi */
    unsigned int fine = lung + OP_PORTSC(hc->prese + 1);
    if (dboff + 4 * 256 > fine) fine = dboff + 4 * 256;
    if (rtsoff + IR0 + 0x20 > fine) fine = rtsoff + IR0 + 0x20;
    if (!pag_mappa_dispositivo(base, fine)) return 0;
    hc->op = hc->cap + lung;
    hc->run = hc->cap + rtsoff;
    hc->campanelli = (volatile unsigned int *)(hc->cap + dboff);

    chiedi_al_bios(hc);
    prese_intel(s);

    /* fermo, poi reset */
    w32(hc->op, OP_USBCMD, r32(hc->op, OP_USBCMD) & ~CMD_RUN);
    if (!aspetta_bit(hc->op, OP_USBSTS, STS_FERMO, STS_FERMO, 100)) return 0;
    w32(hc->op, OP_USBCMD, CMD_RESET);
    aspetta_ms(1);                       /* certi Intel si bloccano se li si legge subito */
    if (!aspetta_bit(hc->op, OP_USBCMD, CMD_RESET, 0, 1000)) return 0;
    if (!aspetta_bit(hc->op, OP_USBSTS, STS_NON_PRONTO, 0, 1000)) return 0;
    if (!(r32(hc->op, OP_PAGESIZE) & 1)) return 0;          /* pagine da 4 KB: ci servono quelle */

    w32(hc->op, OP_CONFIG, hc->posti);

    /* l'elenco dei contesti, e le pagine di appunti che il controllore chiede per se' */
    indirizzo_t p = pagina_chiedi();
    if (!p) return 0;
    memset((void *)p, 0, PAGINA);
    hc->dcbaa = (unsigned long long *)p;
    unsigned int appunti = ((hcs2 >> 27) & 0x1F) | ((hcs2 >> 21) & 0x1F) << 5;
    if (appunti) {
        indirizzo_t elenco = pagina_chiedi();
        if (!elenco || appunti > PAGINA / 8) return 0;
        unsigned long long *e = (unsigned long long *)elenco;
        for (unsigned int i = 0; i < appunti; i++) {
            indirizzo_t a = pagina_chiedi();
            if (!a) return 0;
            memset((void *)a, 0, PAGINA);
            e[i] = a;
        }
        hc->dcbaa[0] = elenco;
    }
    w64(hc->op, OP_DCBAAP, (indirizzo_t)hc->dcbaa);

    /* l'anello dei comandi */
    if (!anello_nuovo(&hc->comandi)) return 0;
    w64(hc->op, OP_CRCR, (unsigned long long)indirizzo_di(&hc->comandi, 0) | 1);

    /* l'anello degli eventi: un solo segmento, descritto da una tabella di una riga.
     * Niente LINK qui: il controllore torna all'inizio da solo. */
    if (!anello_nuovo(&hc->eventi)) return 0;
    hc->eventi.trb[CASELLE - 1].p0 = hc->eventi.trb[CASELLE - 1].p1 = 0;
    indirizzo_t tabella = pagina_chiedi();
    if (!tabella) return 0;
    memset((void *)tabella, 0, PAGINA);
    unsigned int *riga = (unsigned int *)tabella;
    riga[0] = (unsigned int)indirizzo_di(&hc->eventi, 0);
    riga[1] = (unsigned int)((unsigned long long)indirizzo_di(&hc->eventi, 0) >> 32);
    riga[2] = CASELLE;
    w32(hc->run, IR0 + IR_ERSTSZ, 1);
    w64(hc->run, IR0 + IR_ERDP, indirizzo_di(&hc->eventi, 0));
    w64(hc->run, IR0 + IR_ERSTBA, tabella);
    w32(hc->run, IR0 + IR_IMAN, 0);                         /* interruzioni spente: polling */

    w32(hc->op, OP_USBCMD, CMD_RUN);
    if (!aspetta_bit(hc->op, OP_USBSTS, STS_FERMO, 0, 100)) return 0;
    hc->attivo = 1;
    n_controllori++;
    if (!lavoro_iscritto) {
        if (!timer_ogni_tick(controlla)) return 0;
        lavoro_iscritto = 1;
    }
    kprintf(" -- %u prese, %u posti\n", hc->prese, hc->posti);

    /* le prese: alimentate, poi il tempo di accorgersi di chi c'e' */
    for (unsigned int n = 1; n <= hc->prese; n++) {
        unsigned int v = r32(hc->op, OP_PORTSC(n));
        if (!(v & P_ALIMENTATA)) w32(hc->op, OP_PORTSC(n), (v & P_NEUTRO) | P_ALIMENTATA);
    }
    aspetta_ms(200);

    for (unsigned int n = 1; n <= hc->prese; n++) {
        unsigned int v = r32(hc->op, OP_PORTSC(n));
        w32(hc->op, OP_PORTSC(n), (v & P_NEUTRO) | P_CAMBI);     /* cancello i "cambiato" */
        if (!(v & P_COLLEGATO)) continue;
        if (!(v & P_ABILITATO)) {                 /* USB 2: la si resetta, poi e' abilitata */
            w32(hc->op, OP_PORTSC(n), (v & P_NEUTRO) | P_RESET);
            if (!aspetta_bit(hc->op, OP_PORTSC(n), P_RESET_FATTO, P_RESET_FATTO, 500)) continue;
            v = r32(hc->op, OP_PORTSC(n));
            w32(hc->op, OP_PORTSC(n), (v & P_NEUTRO) | P_CAMBI);
            aspetta_ms(10);                        /* il tempo di riprendersi, dice lo standard */
            v = r32(hc->op, OP_PORTSC(n));
            if (!(v & P_ABILITATO)) continue;
        }
        accogli(hc, n, P_VELOCITA(v));
    }
    return 1;
}

static const struct id_pci id_xhci[] = {
    { PCI_QUALSIASI, PCI_QUALSIASI, 0x0C0330 },     /* USB, interfaccia xHCI */
    { 0, 0, 0 }
};

static const struct driver driver_xhci = { "xhci", id_xhci, prepara };
DRIVER(driver_xhci);
