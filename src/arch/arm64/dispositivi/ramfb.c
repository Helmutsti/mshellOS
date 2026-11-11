/* Lo schermo "ramfb", e fw_cfg, il canale con cui lo si accende.
 *
 * RAMFB E' LO SCHERMO PIU' SEMPLICE CHE ESISTA. Il kernel prende un pezzo di RAM per i
 * pixel e dice a QEMU: "lo schermo e' qui, 1280x800, 4 byte per pixel". Da quel momento
 * QEMU mostra quella memoria in una finestra. Niente comandi, niente registri da
 * aggiornare: se il kernel cambia un byte, cambia un pixel. E' come appendere un
 * quadro al muro e dire a qualcuno "guarda quello": poi il quadro lo ridipingi tu.
 *
 * FW_CFG E' IL CANALE DI CONFIGURAZIONE DI QEMU. Un piccolo dispositivo in memoria
 * (MMIO, come la seriale) che contiene dei "file", ognuno con un nome e un numero.
 * Immagina un armadietto con dei cassetti etichettati: dici il numero del cassetto
 * (il "selettore"), poi leggi e il contenuto esce un byte alla volta. Su ARM sta dove dice
 * il device tree, nodo "qemu,fw-cfg-mmio" (di solito 0x09020000). I registri:
 *   +0x00  dati       (si legge: ogni lettura da' il byte successivo del file scelto)
 *   +0x08  selettore  (16 bit: il numero del file da leggere)
 *   +0x10  DMA        (64 bit: l'indirizzo di una richiesta; vedi sotto)
 * ATTENZIONE: i numeri di fw_cfg sono BIG-ENDIAN (il byte piu' grande per primo), mentre
 * l'ARM del kernel e' little-endian. Ogni numero va girato (__builtin_bswap).
 *
 * COME SI TROVA "etc/ramfb". Il file numero 0x19 e' l'elenco di tutti i file: un numero
 * (quanti sono) e poi, per ognuno, { dimensione, selettore, riservato, nome da 56 byte }.
 * Si cerca il nome, e si prende il suo selettore.
 *
 * COME SI SCRIVE (DMA). Il file etc/ramfb non si legge: si SCRIVE. Per scrivere si
 * prepara in RAM una "richiesta" { control, length, address }: "seleziona il file N e
 * scrivici 'length' byte presi da 'address'". Poi si da' l'indirizzo della richiesta al
 * registro DMA: prima la meta' alta, poi la bassa; e' la scrittura della bassa a far
 * partire tutto. Quando QEMU ha finito azzera control (se resta il bit 0, e' andata male).
 *
 * LA CACHE. I pixel stanno nella RAM e il kernel li scrive passando dalla cache della CPU.
 * QEMU li legge dalla memoria della macchina: con l'emulazione non c'e' differenza, con
 * la virtualizzazione (hvf) la cache e' quella vera, ma e' condivisa con QEMU perche'
 * sta sullo stesso processore, quindi di solito si vede tutto. Qui la richiesta DMA si
 * pulisce comunque dalla cache a mano, per non lasciare dubbi.
 *
 * DUE INDIRIZZI. QEMU legge la RAM della macchina direttamente (come un DMA): gli si danno
 * indirizzi FISICI (dei pixel e della richiesta). Il kernel invece scrive i pixel dalla
 * mappa diretta, e i registri di fw_cfg li vede nella zona dei dispositivi. */
#include "ramfb.h"
#include "albero.h"
#include "pagine.h"
#include "paginazione.h"

#define FWCFG_DATI        0x00
#define FWCFG_SELETTORE   0x08
#define FWCFG_DMA         0x10
#define FWCFG_ELENCO      0x19          /* il file con l'elenco dei file */

#define DMA_ERRORE        0x01
#define DMA_SELEZIONA     0x08
#define DMA_SCRIVI        0x10

#define FOURCC_XR24       0x34325258u   /* 'X','R','2','4': XRGB8888, un byte per colore */

/* La richiesta DMA: in RAM, allineata, tutta big-endian. */
struct richiesta_dma {
    unsigned int control;
    unsigned int length;
    unsigned long long address;
} __attribute__((aligned(16)));

/* Il contenuto di etc/ramfb (28 byte, big-endian, senza buchi). */
struct configurazione_ramfb {
    unsigned long long indirizzo;
    unsigned int fourcc;
    unsigned int flags;
    unsigned int larghezza;
    unsigned int altezza;
    unsigned int passo;
} __attribute__((packed, aligned(16)));
/* Attenzione: con aligned(16) sizeof farebbe 32; a QEMU ne vanno dati esattamente 28. */
#define BYTE_RAMFB 28

/* Statiche: devono restare in RAM per tutto il tempo del DMA. */
static struct richiesta_dma richiesta;
static struct configurazione_ramfb configurazione;

static unsigned long fwcfg;   /* dove il kernel vede fw_cfg (nella zona dei dispositivi) */

static int uguali(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

/* Spinge i dati dalla cache alla memoria vera, perche' chi legge senza passare dalla CPU
 * li veda (dc cvac = "clean by address to the point of coherency"). */
static void cache_pulisci(const void *p, unsigned long byte) {
    unsigned long a = (unsigned long)p & ~63UL;
    unsigned long fine = (unsigned long)p + byte;
    for (; a < fine; a += 64) __asm__ volatile("dc cvac, %0" :: "r"(a) : "memory");
    __asm__ volatile("dsb sy" ::: "memory");
}

static void seleziona(unsigned short numero) {
    *(volatile unsigned short *)(fwcfg + FWCFG_SELETTORE) = __builtin_bswap16(numero);
    __asm__ volatile("dsb sy" ::: "memory");
}

static unsigned char leggi_byte(void) {
    return *(volatile unsigned char *)(fwcfg + FWCFG_DATI);
}

static unsigned int leggi_be32(void) {
    unsigned int n = 0;
    for (int i = 0; i < 4; i++) n = (n << 8) | leggi_byte();
    return n;
}

static unsigned int leggi_be16(void) {
    unsigned int n = 0;
    for (int i = 0; i < 2; i++) n = (n << 8) | leggi_byte();
    return n;
}

/* Il selettore di un file, per nome. 0 se non c'e'. */
static unsigned int fwcfg_cerca(const char *nome) {
    seleziona(FWCFG_ELENCO);
    unsigned int quanti = leggi_be32();
    if (quanti > 1000) return 0;   /* roba senza senso: non e' fw_cfg */
    for (unsigned int i = 0; i < quanti; i++) {
        (void)leggi_be32();                       /* dimensione */
        unsigned int selettore = leggi_be16();
        (void)leggi_be16();                       /* riservato */
        char n[56];
        for (int k = 0; k < 56; k++) n[k] = (char)leggi_byte();
        n[55] = 0;
        if (uguali(n, nome)) return selettore;
    }
    return 0;
}

/* Scrive 'byte' byte, presi da 'dati', nel file 'selettore'. 1 se e' andata bene. */
static int fwcfg_scrivi(unsigned int selettore, const void *dati, unsigned int byte) {
    richiesta.control = __builtin_bswap32((selettore << 16) | DMA_SELEZIONA | DMA_SCRIVI);
    richiesta.length  = __builtin_bswap32(byte);
    richiesta.address = __builtin_bswap64(fisico_da_virt(dati));
    cache_pulisci(&richiesta, sizeof richiesta);

    unsigned long r = fisico_da_virt(&richiesta);   /* a QEMU: l'indirizzo fisico */
    volatile unsigned int *dma = (volatile unsigned int *)(fwcfg + FWCFG_DMA);
    dma[0] = __builtin_bswap32((unsigned int)(r >> 32));   /* meta' alta */
    dma[1] = __builtin_bswap32((unsigned int)r);           /* meta' bassa: parte */

    /* QEMU azzera control quando ha finito. Con un limite, per non restare qui per sempre. */
    for (unsigned long giri = 0; giri < 10000000UL; giri++) {
        unsigned int c = __builtin_bswap32(*(volatile unsigned int *)&richiesta.control);
        if (c == 0) return 1;
        if (c & DMA_ERRORE) return 0;
    }
    return 0;
}

int ramfb_prepara(struct schermo *s, unsigned int larghezza, unsigned int altezza) {
    unsigned long dimensione;
    unsigned long fisico;
    if (!albero_cerca("qemu,fw-cfg-mmio", &fisico, &dimensione)) return 0;
    fwcfg = pag_mappa_dispositivo(fisico, 0x20);   /* dati, selettore, DMA: 0x18 byte */
    if (!fwcfg) return 0;

    /* Il segno di fw_cfg: il file 0x19 non e' vuoto e c'e' etc/ramfb. */
    unsigned int selettore = fwcfg_cerca("etc/ramfb");
    if (!selettore) return 0;

    unsigned long passo = (unsigned long)larghezza * 4;
    unsigned long byte = passo * altezza;
    unsigned int pagine = (unsigned int)((byte + PAGINA - 1) / PAGINA);
    indirizzo_t pixel = pagine_chiedi_consecutive(pagine);
    if (!pixel) return 0;
    unsigned char *pixel_qui = virt_da_fisico(pixel);   /* li' li scrive il kernel */
    memset(pixel_qui, 0, byte);
    cache_pulisci(pixel_qui, byte);

    configurazione.indirizzo = __builtin_bswap64(pixel);
    configurazione.fourcc    = __builtin_bswap32(FOURCC_XR24);
    configurazione.flags     = 0;
    configurazione.larghezza = __builtin_bswap32(larghezza);
    configurazione.altezza   = __builtin_bswap32(altezza);
    configurazione.passo     = __builtin_bswap32((unsigned int)passo);
    cache_pulisci(&configurazione, sizeof configurazione);
    if (!fwcfg_scrivi(selettore, &configurazione, BYTE_RAMFB)) return 0;

    s->indirizzo = (indirizzo_t)pixel_qui;
    s->fisico = pixel;
    s->passo = (unsigned int)passo;
    s->larghezza = larghezza;
    s->altezza = altezza;
    s->bpp = 32;
    s->rosso = 16;
    s->verde = 8;
    s->blu = 0;
    return 1;
}
