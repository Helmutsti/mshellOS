/* VIRTIO-INPUT: tastiere, mouse e tavolette "pensati per le macchine virtuali".
 *
 * Il dispositivo ci manda EVENTI, come l'"evdev" di Linux. Ogni evento e' un pacchetto
 * di 8 byte con tre numeri:
 *
 *   type   che genere di cosa e' successo
 *            EV_SYN (0)  "fine del gruppo": tutti gli eventi di prima vanno insieme
 *            EV_KEY (1)  un tasto o un pulsante: value 1 premuto, 0 rilasciato,
 *                        2 ripetizione automatica (il tasto e' ancora giu')
 *            EV_REL (2)  uno spostamento (mouse): code REL_X 0 / REL_Y 1, value = di quanto
 *            EV_ABS (3)  una posizione (tavoletta): code ABS_X 0 / ABS_Y 1, value = dove
 *   code   quale tasto / quale asse. I tasti della tastiera sono i KEY_... di Linux;
 *          i pulsanti del mouse sono BTN_LEFT 0x110, BTN_RIGHT 0x111, BTN_MIDDLE 0x112
 *   value  il valore
 *
 * Esempio: muovere la tavoletta manda ABS_X, ABS_Y e poi SYN_REPORT. Noi aspettiamo il
 * SYN_REPORT e mandiamo UN evento solo di puntatore: cosi' x e y arrivano insieme.
 *
 * LA CODA DEGLI EVENTI e' la numero 0. Il kernel riempie la cassetta di buffer vuoti da
 * 8 byte, "tutti per il dispositivo" (ci scrive lui). Quando succede qualcosa, il
 * dispositivo scrive l'evento in un buffer e lo mette nell'anello "usato". A ogni tick
 * guardiamo li': leggiamo l'evento, e ridiamo il buffer al dispositivo.
 *
 * LA CONFIGURAZIONE: si scrive "select" (byte 0) e "subsel" (byte 1), e si legge "size"
 * (byte 2): quanti byte di risposta, che stanno dal byte 8. Con select 0x01 la risposta
 * e' il NOME ("QEMU Virtio Tablet"); con 0x12 e subsel = asse e' la scheda dell'asse
 * (minimo e massimo). Se size e' 0, quell'asse non c'e': il dispositivo e' relativo. */
#include "kernel.h"
#include "pagine.h"
#include "driver.h"
#include "timer.h"
#include "eventi.h"
#include "tastiera_it.h"
#include "virtio.h"

#define VIRTIO_INPUT_DEBUG 0     /* 1: stampa ogni evento (solo per provare) */

#define CFG_SELECT       0
#define CFG_SUBSEL       1
#define CFG_SIZE         2
#define CFG_DATI         8
#define CFG_ID_NAME      0x01
#define CFG_ABS_INFO     0x12

#define EV_SYN 0
#define EV_KEY 1
#define EV_REL 2
#define EV_ABS 3

#define ABS_X 0
#define ABS_Y 1
#define REL_X 0
#define REL_Y 1
#define BTN_LEFT   0x110
#define BTN_RIGHT  0x111
#define BTN_MIDDLE 0x112

#define MAX_INPUT   4      /* quanti dispositivi input insieme (tastiera + tavoletta + ...) */
#define BUFFER      64     /* quanti buffer da 8 byte (stanno in una pagina) */

struct evento_virtio {
    unsigned short tipo;
    unsigned short codice;
    unsigned int valore;
};

struct input {
    struct virtio_dispositivo d;
    struct virtq coda;
    struct evento_virtio *buffer;       /* BUFFER eventi: il numero del descrittore = il posto */
    int assoluto;                       /* 1: tavoletta (posizioni), 0: mouse (spostamenti) */
    int minimo[2], massimo[2];          /* gli estremi degli assi, per scalare */
    int pos[2];                         /* la posizione, gia' scalata a 0..PUNTATORE_MAX */
    int spostamento[2];                 /* lo spostamento accumulato (mouse) */
    unsigned int pulsanti;
    int cambiato;                       /* dall'ultimo SYN_REPORT e' cambiato il puntatore? */
};

static struct input input[MAX_INPUT];
static int quanti;
static int lavoro_iscritto;

static const struct id_pci id_input[] = {
    { 0x1af4, 0x1052, -1 },     /* virtio-input (transitional non esiste: solo "moderno") */
    { 0, 0, 0 }
};

/* Legge dalla configurazione: dice select/subsel e ritorna quanti byte di risposta ci sono. */
static unsigned int chiedi(struct input *in, unsigned char select, unsigned char subsel) {
    virtio_config_scrivi8(&in->d, CFG_SELECT, select);
    virtio_config_scrivi8(&in->d, CFG_SUBSEL, subsel);
    __sync_synchronize();
    return virtio_config_leggi8(&in->d, CFG_SIZE);
}

/* Un valore dell'asse, dal minimo/massimo alla scala 0..PUNTATORE_MAX. Con numeri a 32
 * bit: (v - min) <= 32767 per max <= 65535 circa, e 32767*32767 sta in 32 bit. */
static int scala(struct input *in, int asse, int valore) {
    int minimo = in->minimo[asse], massimo = in->massimo[asse];
    if (valore < minimo) valore = minimo;
    if (valore > massimo) valore = massimo;
    unsigned int campo = massimo - minimo;
    unsigned int dist = valore - minimo;
    while (campo > 65535) { campo >>= 1; dist >>= 1; }    /* assi molto grandi: si stringe */
    if (campo == 0) return 0;
    return (int)(dist * PUNTATORE_MAX / campo);
}

/* Rimette in coda il buffer numero 'id': e' di nuovo vuoto, per il dispositivo. */
static int rida_buffer(struct input *in, int id) {
    struct virtq_pezzo p = { (indirizzo_t)&in->buffer[id], sizeof(struct evento_virtio), 1 };
    return virtq_aggiungi(&in->coda, &p, 1) >= 0;
}

static void gestisci(struct input *in, const struct evento_virtio *e) {
#if VIRTIO_INPUT_DEBUG
    kprintf("virtio-input: tipo %u codice %u valore %d\n", e->tipo, e->codice, (int)e->valore);
#endif
    switch (e->tipo) {
    case EV_KEY:
        if (e->codice < 0x100) {
            tastiera_tasto(e->codice, e->valore != 0);    /* 2 = ripetizione: ancora premuto */
        } else {
            unsigned int bit = e->codice == BTN_LEFT ? PULSANTE_SINISTRO
                             : e->codice == BTN_RIGHT ? PULSANTE_DESTRO
                             : e->codice == BTN_MIDDLE ? PULSANTE_CENTRALE : 0;
            if (!bit) break;
            if (e->valore) in->pulsanti |= bit; else in->pulsanti &= ~bit;
            in->cambiato = 1;
        }
        break;
    case EV_ABS:
        if (e->codice == ABS_X || e->codice == ABS_Y) {
            in->pos[e->codice] = scala(in, e->codice, (int)e->valore);
            in->cambiato = 1;
        }
        break;
    case EV_REL:
        if (e->codice == REL_X || e->codice == REL_Y) {
            in->spostamento[e->codice] += (int)e->valore;
            in->cambiato = 1;
        }
        break;
    case EV_SYN:                      /* SYN_REPORT (code 0): il gruppo e' completo */
        if (e->codice == 0 && in->cambiato) {
            struct evento ev;
            memset(&ev, 0, sizeof ev);
            if (in->assoluto) {
                ev.tipo = EVENTO_PUNTATORE;
                ev.dx = in->pos[0];
                ev.dy = in->pos[1];
            } else {
                ev.tipo = EVENTO_MOUSE;
                ev.dx = in->spostamento[0];
                ev.dy = in->spostamento[1];
            }
            ev.pulsanti = in->pulsanti;
            eventi_metti(&ev);
            in->spostamento[0] = in->spostamento[1] = 0;
            in->cambiato = 0;
        }
        break;
    }
}

/* A ogni tick, dentro l'interruzione del timer: corta, senza aspettare. */
static void controlla(void) {
    for (int i = 0; i < quanti; i++) {
        struct input *in = &input[i];
        int rimessi = 0, id;
        unsigned int byte;
        while ((id = virtq_raccogli(&in->coda, &byte)) >= 0) {
            if (id < BUFFER) {
                struct evento_virtio e = in->buffer[id];     /* una copia: il buffer torna al dispositivo */
                gestisci(in, &e);
                rimessi += rida_buffer(in, id);
            }
        }
        if (rimessi) virtq_notifica(&in->coda);               /* un solo campanello per giro */
    }
}

static int prepara(const struct pci_scheda *s) {
    if (quanti == MAX_INPUT) return 0;
    struct input *in = &input[quanti];
    memset(in, 0, sizeof *in);

    if (!virtio_prepara(&in->d, s, 0)) return 0;
    if (!in->d.config) return 0;

    /* Il nome. */
    char nome[129];
    unsigned int n = chiedi(in, CFG_ID_NAME, 0);
    if (n > 128) n = 128;
    for (unsigned int i = 0; i < n; i++) nome[i] = virtio_config_leggi8(&in->d, CFG_DATI + i);
    nome[n] = 0;
    kprintf("\nvirtio-input: %s", nome);

    /* Gli assi assoluti: se ABS_X ha una scheda, e' una tavoletta. */
    if (chiedi(in, CFG_ABS_INFO, ABS_X) >= 8) {
        in->assoluto = 1;
        for (int asse = 0; asse < 2; asse++) {
            if (chiedi(in, CFG_ABS_INFO, asse) < 8) { in->minimo[asse] = 0; in->massimo[asse] = PUNTATORE_MAX; continue; }
            in->minimo[asse] = (int)virtio_config_leggi32(&in->d, CFG_DATI);
            in->massimo[asse] = (int)virtio_config_leggi32(&in->d, CFG_DATI + 4);
            if (in->massimo[asse] <= in->minimo[asse]) { in->minimo[asse] = 0; in->massimo[asse] = PUNTATORE_MAX; }
        }
        kprintf(" (assoluto, x %d..%d, y %d..%d)", in->minimo[0], in->massimo[0], in->minimo[1], in->massimo[1]);
    }

    /* La coda degli eventi, riempita di buffer vuoti. */
    if (!virtio_coda(&in->d, 0, &in->coda)) return 0;
    indirizzo_t pagina = pagina_chiedi();
    if (!pagina) return 0;
    memset((void *)pagina, 0, PAGINA);
    in->buffer = (struct evento_virtio *)pagina;
    for (int i = 0; i < BUFFER && i < in->coda.dimensione; i++)
        if (!rida_buffer(in, i)) break;

    virtio_pronto(&in->d);
    virtq_notifica(&in->coda);

    quanti++;
    if (!lavoro_iscritto) {
        if (!timer_ogni_tick(controlla)) return 0;
        lavoro_iscritto = 1;
    }
    return 1;
}

static const struct driver driver_input = { "virtio-input", id_input, prepara };
DRIVER(driver_input);
