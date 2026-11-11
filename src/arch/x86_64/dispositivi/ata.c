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
 * DA IMPLEMENTARE: i comandi "LBA48" (0x24, 0x34), con numeri a 48 bit, per i dischi
 * piu' grandi.
 * Un comando legge o scrive fino a 256 settori di fila (nel registro del conteggio, 0
 * vuol dire 256): il disco li passa uno dopo l'altro, e prima di ognuno si aspetta che
 * sia pronto.
 *
 * Quando lo trova, si ISCRIVE allo sportello dei dischi (dispositivi/disco.h) come
 * "ata0": il file system legge e scrive passando da li', senza sapere che e' un disco
 * ATA.
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
#define MAX_PER_COMANDO 256    /* settori in un comando */
#define LIMITE_LBA28 (1ULL << 28)

static char modello[41];

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

/* Prepara un comando: quale disco, da quale settore, quanti (1..256; 256 si scrive 0). */
static void prepara(unsigned int n, unsigned int quanti) {
    outb(DISPOSITIVO, DISCO_PRINCIPALE | ((n >> 24) & 0x0F));
    outb(CONTEGGIO, quanti & 0xFF);
    outb(LBA_BASSO, n & 0xFF);
    outb(LBA_MEDIO, (n >> 8) & 0xFF);
    outb(LBA_ALTO,  (n >> 16) & 0xFF);
}

static int leggi(struct dispositivo_blocchi *d, unsigned long long primo, unsigned int quanti,
                 void *dove);
static int scrivi(struct dispositivo_blocchi *d, unsigned long long primo, unsigned int quanti,
                  const void *da);
static struct dispositivo_blocchi disco = {
    .nome = "ata0", .byte_settore = SETTORE, .leggi = leggi, .scrivi = scrivi,
};

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

    /* parole 60-61: quanti settori ha il disco (quelli che si raggiungono con 28 bit) */
    disco.settori = info[60] | ((unsigned int)info[61] << 16);
    kprintf("ATA     : %s\n", modello);
    disco_iscrivi(&disco);   /* da qui lo trova chi usa i dischi (disco.h) */
    return 1;
}

/* I settori da 'primo' in poi, MAX_PER_COMANDO alla volta. */
static int leggi(struct dispositivo_blocchi *d, unsigned long long primo, unsigned int quanti,
                 void *dove) {
    unsigned short *p = dove;
    (void)d;
    if (primo + quanti > disco.settori || primo + quanti > LIMITE_LBA28) return -1;
    while (quanti) {
        unsigned int n = quanti < MAX_PER_COMANDO ? quanti : MAX_PER_COMANDO;
        if (aspetta_libero() < 0) return -1;
        prepara((unsigned int)primo, n);
        outb(COMANDO, CMD_LEGGI);
        for (unsigned int s = 0; s < n; s++) {
            attesa_breve();
            if (aspetta_dati() < 0) return -1;      /* il prossimo settore e' pronto */
            for (int i = 0; i < SETTORE / 2; i++)   /* 256 volte, 2 byte alla volta */
                *p++ = inw(DATI);
        }
        primo += n;
        quanti -= n;
    }
    return 0;
}

static int scrivi(struct dispositivo_blocchi *d, unsigned long long primo, unsigned int quanti,
                  const void *da) {
    const unsigned short *p = da;
    (void)d;
    if (primo + quanti > disco.settori || primo + quanti > LIMITE_LBA28) return -1;
    while (quanti) {
        unsigned int n = quanti < MAX_PER_COMANDO ? quanti : MAX_PER_COMANDO;
        if (aspetta_libero() < 0) return -1;
        prepara((unsigned int)primo, n);
        outb(COMANDO, CMD_SCRIVI);
        for (unsigned int s = 0; s < n; s++) {
            attesa_breve();
            if (aspetta_dati() < 0) return -1;      /* il disco e' pronto a ricevere */
            for (int i = 0; i < SETTORE / 2; i++)
                outw(DATI, *p++);
        }
        primo += n;
        quanti -= n;
    }

    /* il disco potrebbe tenere i dati nella sua cache: gli chiediamo di scriverli davvero */
    if (aspetta_libero() < 0) return -1;
    outb(COMANDO, CMD_SVUOTA);
    attesa_breve();
    if (aspetta_libero() < 0) return -1;
    return (inb(STATO) & (ERRORE | GUASTO)) ? -1 : 0;
}
