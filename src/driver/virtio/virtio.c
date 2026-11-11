/* VIRTIO: dispositivi "pensati per le macchine virtuali".
 *
 * Un disco, una tastiera o uno schermo finti potrebbero fingere di essere hardware vero
 * (un disco IDE, una tastiera PS/2): ma allora il kernel ospite farebbe un sacco di
 * gesti lenti, e il programma che finge (QEMU, UTM, un cloud) dovrebbe indovinarli tutti.
 * VIRTIO e' un accordo: "visto che siamo entrambi software, parliamo in un modo semplice
 * e veloce". E' lo STESSO in QEMU, in UTM, nel cloud, su x86 e su ARM: un driver solo.
 *
 * COME SI PRESENTA: sul bus PCI, con vendor 0x1af4. Ma le sue "porte" non sono a posti
 * fissi: la scheda ha delle CAPACITA' PCI di tipo 0x09, e ognuna dice "questa parte sta
 * nel BAR numero B, a distanza O dall'inizio, ed e' lunga L". Le parti sono quattro:
 *
 *   1 configurazione COMUNE   stato, caratteristiche, e come si preparano le code
 *   2 NOTIFICA                il campanello: si scrive qui per dire "ho roba per te"
 *   3 ISR                     il motivo dell'interruzione (noi non usiamo interruzioni)
 *   4 configurazione del DISPOSITIVO   cose sue (la capienza di un disco, il nome di una tastiera)
 *
 * LA STRETTA DI MANO, passo per passo, con il registro "stato":
 *
 *   0                    reset: "dimentica tutto"
 *   ACKNOWLEDGE (1)      "ti ho visto"
 *   DRIVER (2)           "so come si guida"
 *   ... si leggono le caratteristiche offerte e si scrivono quelle che accettiamo.
 *       VERSION_1 e' obbligatoria: vuol dire "parliamo virtio 1.x"
 *   FEATURES_OK (8)      "ho scelto". Si rilegge lo stato: se il bit e' sparito, il
 *                        dispositivo ha rifiutato la scelta
 *   ... si preparano le code
 *   DRIVER_OK (4)        "sono pronto, parti"
 *
 * LA CODA "SPLIT": come si passano le cose avanti e indietro. Tre tabelle in memoria,
 * che il dispositivo legge e scrive da solo (e' un "bus master", vede la RAM):
 *
 *   - la tabella dei DESCRITTORI: i PACCHI. Ognuno dice "a questo indirizzo, lungo tanti
 *     byte, e il dispositivo puo' solo leggerlo / ci deve scrivere". Un pacco puo' avere
 *     un "prossimo": si fa una catena (per un disco: la richiesta, i dati, la risposta).
 *   - l'anello DISPONIBILE: la CASSETTA dove il kernel lascia i pacchi per il dispositivo.
 *     Si scrive il numero della testa della catena nell'anello, poi si alza "idx".
 *   - l'anello USATO: dove il dispositivo RESTITUISCE i pacchi finiti (numero della testa
 *     e quanti byte ha scritto), e alza il suo "idx".
 *
 * Ognuno alza solo il proprio idx e legge quello dell'altro. L'ORDINE conta: prima si
 * scrive il contenuto, POI si alza idx, altrimenti l'altro vedrebbe un pacco a meta'.
 * Per questo c'e' __sync_synchronize() (una "barriera": tutto quello scritto prima e'
 * davvero in memoria prima di quello dopo).
 *
 * DUE INDIRIZZI PER OGNI COSA. Il dispositivo legge e scrive la RAM da solo (DMA) e non
 * sa niente delle mappe: gli si danno indirizzi FISICI (le tabelle della coda, i pezzi
 * delle catene). Il kernel invece le stesse pagine le vede nella mappa diretta
 * (virt_da_fisico, pagine.h), e i registri della scheda nella zona dei dispositivi.
 *
 * Noi non usiamo le interruzioni: a ogni tick il driver guarda l'anello usato ("polling").
 * Quindi non servono ne' ISR ne' MSI-X. */
#include "kernel.h"
#include "pagine.h"
#include "paginazione.h"
#include "virtio.h"

#define CAP_VENDOR       0x09      /* il tipo delle capacita' virtio */
#define CAP_COMUNE       1
#define CAP_NOTIFICA     2
#define CAP_ISR          3
#define CAP_DISPOSITIVO  4

/* Lo stato. */
#define ST_ACKNOWLEDGE   1
#define ST_DRIVER        2
#define ST_DRIVER_OK     4
#define ST_FEATURES_OK   8
#define ST_FALLITO       0x80

/* Dove stanno i campi nella configurazione comune. */
#define C_DEV_FEAT_SEL   0
#define C_DEV_FEAT       4
#define C_DRV_FEAT_SEL   8
#define C_DRV_FEAT       12
#define C_STATO          20
#define C_VERSIONE       21
#define C_CODA_SEL       22
#define C_CODA_DIM       24
#define C_CODA_ABILITA   28
#define C_CODA_NOTIFICA  30
#define C_CODA_DESC      32
#define C_CODA_DISP      40
#define C_CODA_USATO     48

#define DESC_PROSSIMO    1     /* il pacco ha un "prossimo" */
#define DESC_SCRIVE      2     /* ci scrive il dispositivo */

#define CODA_MAX         64    /* una pagina basta, e per noi e' piu' che abbastanza */

/* --- accessi ai registri: larghezza esatta, volatile --- */
static unsigned char r8(volatile unsigned char *base, unsigned int p) {
    return *(volatile unsigned char *)(base + p);
}
static unsigned short r16(volatile unsigned char *base, unsigned int p) {
    return *(volatile unsigned short *)(base + p);
}
static unsigned int r32(volatile unsigned char *base, unsigned int p) {
    return *(volatile unsigned int *)(base + p);
}
static void w8(volatile unsigned char *base, unsigned int p, unsigned char v) {
    *(volatile unsigned char *)(base + p) = v;
}
static void w16(volatile unsigned char *base, unsigned int p, unsigned short v) {
    *(volatile unsigned short *)(base + p) = v;
}
static void w32(volatile unsigned char *base, unsigned int p, unsigned int v) {
    *(volatile unsigned int *)(base + p) = v;
}
/* Un campo a 64 bit si scrive in due metà da 32: prima la bassa. */
static void w64(volatile unsigned char *base, unsigned int p, unsigned long long v) {
    w32(base, p, (unsigned int)v);
    w32(base, p + 4, (unsigned int)(v >> 32));
}

/* Un byte della scheda anagrafica (la scheda si legge a 4 byte alla volta). */
static unsigned int cap_byte(const struct pci_scheda *s, unsigned char pos, unsigned int i) {
    return (pci_leggi(s, (pos + i) & 0xFC) >> (((pos + i) & 3) * 8)) & 0xFF;
}
static unsigned int cap_u32(const struct pci_scheda *s, unsigned char pos, unsigned int i) {
    return pci_leggi(s, pos + i);    /* pos e' multiplo di 4, e i pure */
}

/* Dalla capacita' alla memoria: il BAR, piu' la distanza (un indirizzo fisico). Il pezzo
 * si mappa nella zona dei dispositivi: il puntatore e' dove lo vede il kernel. */
static volatile unsigned char *mappa_parte(const struct pci_scheda *s, unsigned char pos,
                                           unsigned int *lunghezza) {
    unsigned int bar = cap_byte(s, pos, 4);
    unsigned int distanza = cap_u32(s, pos, 8);
    unsigned int lung = cap_u32(s, pos, 12);
    if (bar > 5) return 0;
    indirizzo_t base = pci_bar_memoria(s, bar);
    if (!base) return 0;
    indirizzo_t virt = pag_mappa_dispositivo(base + distanza, lung);
    if (!virt) return 0;
    if (lunghezza) *lunghezza = lung;
    return (volatile unsigned char *)virt;
}

int virtio_prepara(struct virtio_dispositivo *d, const struct pci_scheda *s,
                   unsigned long long volute) {
    memset(d, 0, sizeof *d);
    d->scheda = s;

    /* 1. Le capacita': dove sta ogni parte. */
    unsigned char pos_notifica = 0;
    for (unsigned char p = pci_capacita(s, CAP_VENDOR, 0); p; p = pci_capacita(s, CAP_VENDOR, p)) {
        unsigned int tipo = cap_byte(s, p, 3);
        if (tipo == CAP_COMUNE && !d->comune) d->comune = mappa_parte(s, p, 0);
        else if (tipo == CAP_ISR && !d->isr) d->isr = mappa_parte(s, p, 0);
        else if (tipo == CAP_DISPOSITIVO && !d->config) d->config = mappa_parte(s, p, &d->config_byte);
        else if (tipo == CAP_NOTIFICA && !d->notifica) {
            d->notifica = mappa_parte(s, p, 0);
            d->moltiplicatore = cap_u32(s, p, 16);
            pos_notifica = p;
        }
    }
    /* La config del dispositivo puo' mancare (alcuni non ne hanno), le altre no. */
    if (!d->comune || !d->notifica || !pos_notifica) return 0;

    pci_accendi(s);   /* risponde sulla memoria, e legge/scrive la RAM da sola */
    volatile unsigned char *c = d->comune;

    /* 2. Reset: scrivo 0 e aspetto che lo stato torni 0 (con un limite). */
    w8(c, C_STATO, 0);
    for (int giri = 0; r8(c, C_STATO) != 0; giri++)
        if (giri > 1000000) return 0;

    w8(c, C_STATO, ST_ACKNOWLEDGE);
    w8(c, C_STATO, ST_ACKNOWLEDGE | ST_DRIVER);

    /* 3. Le caratteristiche: 64 bit, letti a 32 alla volta. Scelgo VERSION_1 e quelle
     * volute che il dispositivo offre. */
    w32(c, C_DEV_FEAT_SEL, 0);
    unsigned long long offerte = r32(c, C_DEV_FEAT);
    w32(c, C_DEV_FEAT_SEL, 1);
    offerte |= (unsigned long long)r32(c, C_DEV_FEAT) << 32;
    if (!(offerte & VIRTIO_F_VERSION_1)) {     /* e' un dispositivo "vecchio": non lo guidiamo */
        w8(c, C_STATO, ST_FALLITO);
        return 0;
    }
    unsigned long long scelte = VIRTIO_F_VERSION_1 | (volute & offerte);
    w32(c, C_DRV_FEAT_SEL, 0);
    w32(c, C_DRV_FEAT, (unsigned int)scelte);
    w32(c, C_DRV_FEAT_SEL, 1);
    w32(c, C_DRV_FEAT, (unsigned int)(scelte >> 32));
    d->caratteristiche = scelte;

    /* 4. FEATURES_OK, e controllo che resti acceso. */
    w8(c, C_STATO, ST_ACKNOWLEDGE | ST_DRIVER | ST_FEATURES_OK);
    if (!(r8(c, C_STATO) & ST_FEATURES_OK)) {
        w8(c, C_STATO, ST_FALLITO);
        return 0;
    }
    return 1;
}

int virtio_coda(struct virtio_dispositivo *d, unsigned short numero, struct virtq *q) {
    volatile unsigned char *c = d->comune;
    w16(c, C_CODA_SEL, numero);
    unsigned short dim = r16(c, C_CODA_DIM);
    if (dim == 0) return 0;                    /* questa coda non esiste */
    if (dim > CODA_MAX) dim = CODA_MAX;
    /* La dimensione deve essere una potenza di 2 (lo e' sempre; qui lo e' anche dopo il taglio). */

    /* Una pagina azzerata, divisa in tre:
     *   da 0      i descrittori (16 byte l'uno)
     *   da 1024   l'anello disponibile (4 byte + 2 per voce)
     *   da 2048   l'anello usato (4 byte + 8 per voce)  */
    indirizzo_t pagina = pagina_chiedi();   /* fisico: e' quello che si da' al dispositivo */
    if (!pagina) return 0;
    unsigned char *qui = virt_da_fisico(pagina);   /* ... e qui la vede il kernel */
    memset(qui, 0, PAGINA);

    q->dimensione = dim;
    q->numero = numero;
    q->descrittori = (struct virtq_descrittore *)qui;
    q->disponibile = (volatile struct virtq_disponibile *)(qui + 1024);
    q->usato = (volatile struct virtq_usato *)(qui + 2048);
    q->disponibile_idx = 0;
    q->usato_visti = 0;
    /* Tutti i descrittori sono liberi: una lista, ognuno punta al successivo. */
    for (unsigned short i = 0; i < dim; i++) q->descrittori[i].prossimo = i + 1;
    q->libero = 0;
    q->liberi = dim;

    w16(c, C_CODA_DIM, dim);
    w64(c, C_CODA_DESC, pagina);
    w64(c, C_CODA_DISP, pagina + 1024);
    w64(c, C_CODA_USATO, pagina + 2048);
    unsigned short off = r16(c, C_CODA_NOTIFICA);
    q->campanello = (volatile unsigned short *)(d->notifica + (unsigned int)off * d->moltiplicatore);
    __sync_synchronize();
    w16(c, C_CODA_ABILITA, 1);
    return 1;
}

void virtio_pronto(struct virtio_dispositivo *d) {
    w8(d->comune, C_STATO, ST_ACKNOWLEDGE | ST_DRIVER | ST_FEATURES_OK | ST_DRIVER_OK);
}

int virtq_aggiungi(struct virtq *q, const struct virtq_pezzo *pezzi, int n) {
    if (n < 1 || n > q->liberi) return -1;

    /* Prendo n descrittori dalla lista dei liberi e li riempio: ognuno punta al
     * successivo della catena (l'ultimo no). */
    unsigned short testa = q->libero;
    unsigned short corrente = testa;
    for (int i = 0; i < n; i++) {
        struct virtq_descrittore *ds = &q->descrittori[corrente];
        unsigned short dopo = ds->prossimo;      /* il prossimo libero, prima di riscriverlo */
        ds->indirizzo = (unsigned long long)pezzi[i].indirizzo;
        ds->byte = pezzi[i].byte;
        ds->flags = pezzi[i].scrive_dispositivo ? DESC_SCRIVE : 0;
        if (i < n - 1) {
            ds->flags |= DESC_PROSSIMO;
            ds->prossimo = dopo;                 /* la catena segue la lista: il successivo e' 'dopo' */
        }
        q->libero = dopo;
        corrente = dopo;
    }
    q->liberi -= n;

    /* Nella cassetta: scrivo la testa, e SOLO DOPO alzo idx. */
    q->disponibile->anello[q->disponibile_idx % q->dimensione] = testa;
    __sync_synchronize();
    q->disponibile_idx++;
    q->disponibile->idx = q->disponibile_idx;
    __sync_synchronize();
    return testa;
}

void virtq_notifica(struct virtq *q) {
    __sync_synchronize();
    *q->campanello = q->numero;
}

int virtq_raccogli(struct virtq *q, unsigned int *byte) {
    if (q->usato->idx == q->usato_visti) return -1;      /* niente di nuovo */
    __sync_synchronize();                                 /* prima idx, poi il contenuto */
    volatile struct virtq_usato_voce *v = &q->usato->anello[q->usato_visti % q->dimensione];
    unsigned int id = v->id;
    if (byte) *byte = v->byte;
    q->usato_visti++;
    if (id >= q->dimensione) return -1;                   /* il dispositivo ha sbagliato: lo ignoro */

    /* Restituisco la catena alla lista dei liberi: la scorro fino all'ultimo pezzo. */
    unsigned short corrente = id;
    int pezzi = 1;
    while ((q->descrittori[corrente].flags & DESC_PROSSIMO) && pezzi < q->dimensione) {
        corrente = q->descrittori[corrente].prossimo;
        pezzi++;
    }
    q->descrittori[corrente].flags = 0;
    q->descrittori[corrente].prossimo = q->libero;        /* l'ultimo pezzo punta al vecchio primo libero */
    q->libero = id;
    q->liberi += pezzi;
    return id;
}

unsigned char virtio_config_leggi8(const struct virtio_dispositivo *d, unsigned int posto) {
    return r8(d->config, posto);
}
unsigned short virtio_config_leggi16(const struct virtio_dispositivo *d, unsigned int posto) {
    return r16(d->config, posto);
}
unsigned int virtio_config_leggi32(const struct virtio_dispositivo *d, unsigned int posto) {
    return r32(d->config, posto);
}
void virtio_config_scrivi8(const struct virtio_dispositivo *d, unsigned int posto,
                           unsigned char valore) {
    w8(d->config, posto, valore);
}
unsigned char virtio_config_versione(const struct virtio_dispositivo *d) {
    return r8(d->comune, C_VERSIONE);
}
