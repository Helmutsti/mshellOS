/* Il driver del disco ATA (IDE), in modalita' PIO.
 *
 * Il controller del disco principale si comanda da 8 porte, 0x1F0-0x1F7:
 *   0x1F0  dati: escono ed entrano i byte del settore, 2 alla volta
 *   0x1F1  errore (in lettura)
 *   0x1F2  quanti settori
 *   0x1F3  numero del settore, bit 0-7
 *   0x1F4  numero del settore, bit 8-15
 *   0x1F5  numero del settore, bit 16-23
 *   0x1F6  quale disco + numero del settore, bit 24-27
 *   0x1F7  scrivendo: il comando. Leggendo: lo stato
 *
 * I settori si indicano col loro numero, da 0 in su (LBA: Logical Block Address).
 * Con 28 bit di numero si arriva a 128 GB: per noi basta e avanza.
 *
 * Quando lo trova, si ISCRIVE allo sportello dei dischi (dispositivi/disco.h): mfs legge
 * e scrive passando da li', senza sapere che e' un disco ATA.
 *
 * Due semplificazioni:
 *   - PIO: i dati passano dalla CPU, 2 byte alla volta (niente DMA)
 *   - polling: si aspetta guardando lo stato in un ciclo (niente interruzione IRQ 14) */
#include "kernel.h"
#include "io.h"
#include "ata.h"
#include "disco.h"

#define DATI       0x1F0
#define CONTEGGIO  0x1F2
#define LBA_BASSO  0x1F3
#define LBA_MEDIO  0x1F4
#define LBA_ALTO   0x1F5
#define DISPOSITIVO 0x1F6
#define COMANDO    0x1F7   /* scrivendo */
#define STATO      0x1F7   /* leggendo */
#define STATO_ALT  0x3F6   /* lo stesso stato, ma leggerlo non "consuma" niente */

/* I bit dello stato */
#define OCCUPATO   0x80    /* BSY: il disco sta lavorando, non toccare niente */
#define GUASTO     0x20    /* DF: errore del dispositivo */
#define DATI_PRONTI 0x08   /* DRQ: il disco aspetta dati, o ne ha pronti */
#define ERRORE     0x01    /* ERR: l'ultimo comando e' fallito */

/* I comandi */
#define CMD_LEGGI     0x20
#define CMD_SCRIVI    0x30
#define CMD_SVUOTA    0xE7   /* svuota la cache: i dati vanno davvero sul disco */
#define CMD_IDENTIFICA 0xEC  /* "chi sei?": 512 byte di informazioni sul disco */

#define DISCO_PRINCIPALE 0xE0  /* bit 6 = numeri di settore (LBA), bit 4 = 0: il primo disco */
#define PAZIENZA 10000000      /* quanti giri di attesa prima di arrendersi */

static char modello[41];
static unsigned int settori;

/* Il disco vuole circa 400 nanosecondi per aggiornare lo stato dopo un comando:
 * leggere 4 volte la porta di stato e' il modo classico per farli passare. */
static void attesa_breve(void) {
    for (int i = 0; i < 4; i++) inb(STATO_ALT);
}

/* Aspetta che il disco non sia piu' occupato. */
static int aspetta_libero(void) {
    for (int i = 0; i < PAZIENZA; i++)
        if (!(inb(STATO) & OCCUPATO)) return 0;
    return -1;
}

/* Aspetta che il disco abbia i dati pronti (o che dica di aver sbagliato). */
static int aspetta_dati(void) {
    for (int i = 0; i < PAZIENZA; i++) {
        unsigned char s = inb(STATO);
        if (s & OCCUPATO) continue;
        if (s & (ERRORE | GUASTO)) return -1;
        if (s & DATI_PRONTI) return 0;
    }
    return -1;
}

/* Prepara un comando su un settore: quale disco, quale settore, quanti. */
static void prepara(unsigned int n) {
    outb(DISPOSITIVO, DISCO_PRINCIPALE | ((n >> 24) & 0x0F));
    outb(CONTEGGIO, 1);
    outb(LBA_BASSO, n & 0xFF);
    outb(LBA_MEDIO, (n >> 8) & 0xFF);
    outb(LBA_ALTO,  (n >> 16) & 0xFF);
}

static int leggi(unsigned int n, void *dati);
static int scrivi(unsigned int n, const void *dati);
static struct disco disco = { .modello = modello, .leggi = leggi, .scrivi = scrivi };

int ata_prepara(void) {
    outb(DISPOSITIVO, DISCO_PRINCIPALE);
    attesa_breve();
    outb(CONTEGGIO, 0); outb(LBA_BASSO, 0); outb(LBA_MEDIO, 0); outb(LBA_ALTO, 0);
    outb(COMANDO, CMD_IDENTIFICA);
    attesa_breve();

    if (inb(STATO) == 0) return 0;                      /* nessuno risponde: niente disco */
    if (aspetta_libero() < 0) return 0;
    if (inb(LBA_MEDIO) || inb(LBA_ALTO)) return 0;      /* risponde, ma non e' un disco ATA (es. CD) */
    if (aspetta_dati() < 0) return 0;

    /* La risposta: 256 parole da 2 byte. Ci interessano due cose. */
    unsigned short info[256];
    for (int i = 0; i < 256; i++) info[i] = inw(DATI);

    /* parole 27-46: il nome del modello, 40 caratteri, con i 2 byte di ogni parola scambiati */
    for (int i = 0; i < 20; i++) {
        modello[2 * i]     = info[27 + i] >> 8;
        modello[2 * i + 1] = info[27 + i] & 0xFF;
    }
    for (int i = 39; i >= 0 && modello[i] == ' '; i--) modello[i] = 0;   /* toglie gli spazi in fondo */

    /* parole 60-61: quanti settori ha il disco */
    settori = info[60] | ((unsigned int)info[61] << 16);
    disco.settori = settori;
    disco_iscrivi(&disco);   /* da qui e' il disco del sistema (disco.h) */
    return 1;
}

static int leggi(unsigned int n, void *dati) {
    if (n >= settori) return -1;
    if (aspetta_libero() < 0) return -1;

    prepara(n);
    outb(COMANDO, CMD_LEGGI);
    attesa_breve();
    if (aspetta_dati() < 0) return -1;

    unsigned short *p = dati;
    for (int i = 0; i < SETTORE / 2; i++)   /* 256 volte, 2 byte alla volta */
        p[i] = inw(DATI);
    return 0;
}

static int scrivi(unsigned int n, const void *dati) {
    if (n >= settori) return -1;
    if (aspetta_libero() < 0) return -1;

    prepara(n);
    outb(COMANDO, CMD_SCRIVI);
    attesa_breve();
    if (aspetta_dati() < 0) return -1;      /* il disco e' pronto a ricevere */

    const unsigned short *p = dati;
    for (int i = 0; i < SETTORE / 2; i++)
        outw(DATI, p[i]);

    /* il disco potrebbe tenere i dati nella sua cache: gli chiediamo di scriverli davvero */
    outb(COMANDO, CMD_SVUOTA);
    attesa_breve();
    if (aspetta_libero() < 0) return -1;
    return (inb(STATO) & (ERRORE | GUASTO)) ? -1 : 0;
}
