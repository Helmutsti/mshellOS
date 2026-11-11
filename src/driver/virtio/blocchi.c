/* VIRTIO-BLK: il disco "pensato per le macchine virtuali".
 *
 * Immagina un MODULO da compilare. Per ogni cosa che vogliamo dal disco ne
 * riempiamo uno, fatto di tre pezzi:
 *
 *   1. la DOMANDA (16 byte, la legge il dispositivo):
 *        tipo      0 = leggi, 1 = scrivi, 4 = svuota la cache (flush)
 *        riservato non serve, a zero
 *        settore   quale settore da 512 byte (64 bit)
 *   2. lo SPAZIO PER LA RISPOSTA (512 byte): per una lettura ci scrive il
 *      dispositivo, per una scrittura ce li mettiamo noi e li legge lui.
 *      Per il flush questo pezzo non c'e'.
 *   3. la CASELLA "fatto/errore" (1 byte, la scrive il dispositivo):
 *      0 = tutto bene, altro = errore.
 *
 * I tre pezzi sono una catena nella coda 0. Diamo il modulo al dispositivo, suoniamo
 * il campanello e aspettiamo (polling: niente interruzioni) che lo riporti indietro.
 *
 * Tutto il modulo sta in una nostra pagina di appoggio: cosi' non importa dove stia il
 * buffer di chi ci chiama, copiamo da/verso quello. Una richiesta alla volta, con le
 * interruzioni spente, cosi' nessuno tocca la coda a meta'. Il kernel scrive e legge la
 * pagina dalla mappa diretta; nelle catene va il suo indirizzo fisico (il dispositivo
 * non conosce le mappe).
 *
 * LA CONFIGURAZIONE: i primi 8 byte sono la capacita' in settori da 512 byte. */
#include "kernel.h"
#include "pagine.h"
#include "driver.h"
#include "disco.h"
#include "virtio.h"

#define VIRTIO_BLK_F_FLUSH   (1ULL << 9)

#define TIPO_LEGGI    0
#define TIPO_SCRIVI   1
#define TIPO_FLUSH    4

#define GIRI_MASSIMI  50000000u     /* poi rinunciamo: il dispositivo non risponde */

/* Dove stanno i pezzi dentro la pagina di appoggio. */
#define POSTO_DOMANDA 0
#define POSTO_STATO   16
#define POSTO_DATI    512

struct domanda {
    unsigned int tipo;
    unsigned int riservato;
    unsigned int settore_basso;
    unsigned int settore_alto;
};

static struct virtio_dispositivo dev;
static struct virtq coda;
static unsigned char *pagina;       /* la pagina di appoggio, dove la vede il kernel */
static indirizzo_t pagina_fisica;   /* ... e dove la vede il dispositivo */
static int ha_flush;
static int trovato;

static const struct id_pci id_blocchi[] = {
    { 0x1af4, 0x1042, -1 },         /* virtio-blk moderno */
    /* "transizionale": come lo presenta QEMU (e UTM) di solito, per i sistemi vecchi.
     * Ha anche la parte moderna, che e' quella che usiamo: se non ce l'avesse,
     * virtio_prepara non troverebbe le sue capacita' e il driver direbbe di no. */
    { 0x1af4, 0x1001, -1 },
    { 0, 0, 0 }
};

/* Compila il modulo, lo manda e aspetta. 'dati' = 1 se c'e' il pezzo dei 512 byte,
 * 'scrive' = 1 se il dispositivo ci scrive sopra (lettura). 0 bene, -1 errore. */
static int richiesta(unsigned int tipo, unsigned int settore, int dati, int scrive) {
    struct virtq_pezzo pezzi[3];
    int n = 0;
    struct domanda *d = (struct domanda *)(pagina + POSTO_DOMANDA);
    volatile unsigned char *stato = pagina + POSTO_STATO;

    d->tipo = tipo;
    d->riservato = 0;
    d->settore_basso = settore;
    d->settore_alto = 0;
    *stato = 0xFF;                  /* "non ancora": il dispositivo la mettera' a 0 se va bene */

    pezzi[n].indirizzo = pagina_fisica + POSTO_DOMANDA;   /* al dispositivo: fisici */
    pezzi[n].byte = sizeof *d;
    pezzi[n].scrive_dispositivo = 0;
    n++;
    if (dati) {
        pezzi[n].indirizzo = pagina_fisica + POSTO_DATI;
        pezzi[n].byte = SETTORE;
        pezzi[n].scrive_dispositivo = scrive;
        n++;
    }
    pezzi[n].indirizzo = pagina_fisica + POSTO_STATO;
    pezzi[n].byte = 1;
    pezzi[n].scrive_dispositivo = 1;
    n++;

    int testa = virtq_aggiungi(&coda, pezzi, n);
    if (testa < 0) return -1;
    __sync_synchronize();
    virtq_notifica(&coda);

    for (unsigned int giri = 0; giri < GIRI_MASSIMI; giri++) {
        unsigned int byte;
        int id = virtq_raccogli(&coda, &byte);
        if (id == testa) {
            __sync_synchronize();
            return *stato == 0 ? 0 : -1;
        }
        /* un altro id non dovrebbe esistere: una richiesta alla volta */
    }
    return -1;
}

static int blocchi_leggi(unsigned int n, void *dati) {
    unsigned int s = interruzioni_spegni();
    int r = richiesta(TIPO_LEGGI, n, 1, 1);
    if (r == 0) memcpy(dati, pagina + POSTO_DATI, SETTORE);
    interruzioni_ripristina(s);
    return r;
}

static int blocchi_scrivi(unsigned int n, const void *dati) {
    unsigned int s = interruzioni_spegni();
    memcpy(pagina + POSTO_DATI, dati, SETTORE);
    int r = richiesta(TIPO_SCRIVI, n, 1, 0);
    /* Come l'ATA: dopo aver scritto chiediamo al disco di svuotare la cache. */
    if (r == 0 && ha_flush) r = richiesta(TIPO_FLUSH, 0, 0, 0);
    interruzioni_ripristina(s);
    return r;
}

static struct disco disco = { "virtio-blk", 0, blocchi_leggi, blocchi_scrivi };

static int prepara(const struct pci_scheda *s) {
    if (trovato) return 0;          /* un solo disco virtio */

    if (!virtio_prepara(&dev, s, VIRTIO_BLK_F_FLUSH)) return 0;
    if (!dev.config) return 0;
    ha_flush = (dev.caratteristiche & VIRTIO_BLK_F_FLUSH) != 0;

    /* La capacita': 64 bit little-endian, letta in due meta'. */
    unsigned int basso = virtio_config_leggi32(&dev, 0);
    unsigned int alto = virtio_config_leggi32(&dev, 4);
    disco.settori = alto ? 0xFFFFFFFFu : basso;

    if (!virtio_coda(&dev, 0, &coda)) return 0;
    indirizzo_t p = pagina_chiedi();
    if (!p) return 0;
    pagina_fisica = p;
    pagina = virt_da_fisico(p);
    memset(pagina, 0, PAGINA);

    virtio_pronto(&dev);
    trovato = 1;
    kprintf("\nvirtio-blk: %u settori (%u KB)", disco.settori, disco.settori / 2);
    disco_iscrivi(&disco);
    return 1;
}

static const struct driver driver_blocchi = { "virtio-blk", id_blocchi, prepara };
DRIVER(driver_blocchi);
