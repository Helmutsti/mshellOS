/* I CHECKSUM DI EXT4: numeri calcolati dai byte di un pezzo del disco e scritti accanto
 * a lui. Se rileggendo il numero non torna, il pezzo si e' rovinato (o l'ha scritto
 * qualcuno a meta').
 *
 * Il CRC (controllo a ridondanza ciclica) tratta i byte come un lunghissimo numero in
 * base 2 e ne fa il resto della divisione per un numero fisso, il POLINOMIO (in
 * aritmetica "senza riporti": la sottrazione e' uno XOR). Un errore di pochi bit cambia
 * sempre il resto.
 *
 *   - crc32c (il polinomio 0x1EDC6F41 di Castagnoli, scritto al contrario 0x82F63B78
 *     perche' i bit si prendono dal basso): quello di metadata_csum, per superblocco,
 *     descrittori, inode, nodi degli extent, blocchi delle cartelle;
 *   - crc16 (0x8005, al contrario 0xA001): il vecchio checksum dei soli descrittori
 *     dei gruppi (gdt_csum, detto anche uninit_bg), sui dischi senza metadata_csum.
 *
 * Il modo semplice e' un bit alla volta: per ogni bit, se quello basso e' 1 si fa lo XOR
 * col polinomio. Piu' svelto: per ognuno dei 256 valori di un byte si calcolano una
 * volta sola gli 8 passi (una TABELLA da 1 KB), e poi si fa un passo per byte. La
 * tabella del crc32c si prepara alla prima chiamata (due compiti che la preparano
 * insieme scrivono gli stessi numeri: nessun danno). Il crc16 serve poco (una volta per
 * gruppo al montaggio): va bene bit per bit. */
#include "ext4_interno.h"

static unsigned int tabella32[256];
static int tabella32_pronta;

static void prepara_tabella32(void) {
    for (unsigned int i = 0; i < 256; i++) {
        unsigned int c = i;
        for (int b = 0; b < 8; b++) c = (c & 1) ? (c >> 1) ^ 0x82F63B78u : c >> 1;
        tabella32[i] = c;
    }
    tabella32_pronta = 1;
}

unsigned int ext4_crc32c(unsigned int crc, const void *dati, unsigned int byte) {
    const unsigned char *p = dati;
    if (!tabella32_pronta) prepara_tabella32();
    while (byte--) crc = tabella32[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return crc;
}

unsigned int ext4_crc16(unsigned int crc, const void *dati, unsigned int byte) {
    const unsigned char *p = dati;
    crc &= 0xFFFF;
    while (byte--) {
        crc ^= *p++;
        for (int b = 0; b < 8; b++) crc = (crc & 1) ? (crc >> 1) ^ 0xA001u : crc >> 1;
    }
    return crc;
}
