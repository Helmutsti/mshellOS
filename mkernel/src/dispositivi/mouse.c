/* Il mouse PS/2.
 *
 * Il mouse e' attaccato allo stesso controller della tastiera, l'8042, come "secondo
 * dispositivo". Parlano tutti e due dalla porta 0x60: i suoi byte si distinguono perche'
 * arrivano con un'altra interruzione, IRQ 12 invece di IRQ 1.
 *
 * All'accensione il mouse e' spento. Per accenderlo:
 *   1. si dice al controller di attivare il secondo dispositivo;
 *   2. si accende, nella configurazione del controller, l'interruzione del mouse;
 *   3. si parla al mouse (ogni byte per lui va preceduto da 0xD4 al controller):
 *      "impostazioni di fabbrica", poi "comincia a mandare". Lui risponde 0xFA, ricevuto.
 *
 * Da li' in poi, a ogni movimento o clic, manda un PACCHETTO di 3 byte, uno per IRQ:
 *   byte 0: i pulsanti (bit 0 sinistro, 1 destro, 2 centrale), il bit 3 sempre a 1,
 *           il segno di x (bit 4) e di y (bit 5), "troppo veloce" (bit 6 e 7)
 *   byte 1: lo spostamento in x (gli 8 bit bassi; il segno e' nel byte 0)
 *   byte 2: lo spostamento in y, positivo VERSO L'ALTO (sullo schermo e' il contrario)
 * Il mouse non sa dov'e' il puntatore: dice solo di quanto si e' mosso. */
#include "io.h"
#include "pic.h"
#include "idt.h"
#include "eventi.h"
#include "mouse.h"

#define PORTA_DATI  0x60
#define PORTA_STATO 0x64   /* in lettura: lo stato del controller; in scrittura: i comandi */

#define STATO_PIENO 0x01   /* c'e' un byte da leggere sulla porta dei dati */
#define STATO_OCCUPATO 0x02 /* il controller non ha ancora preso l'ultimo byte scritto */
#define STATO_DAL_MOUSE 0x20 /* il byte da leggere viene dal mouse, non dalla tastiera */

#define CTRL_ATTIVA_MOUSE  0xA8
#define CTRL_LEGGI_CONFIG  0x20
#define CTRL_SCRIVI_CONFIG 0x60
#define CTRL_AL_MOUSE      0xD4   /* il prossimo byte e' per il mouse, non per il controller */
#define CONFIG_IRQ_MOUSE   0x02
#define CONFIG_MOUSE_SPENTO 0x20

#define MOUSE_FABBRICA 0xF6
#define MOUSE_MANDA    0xF4
#define MOUSE_RICEVUTO 0xFA

#define PACCHETTO_UNO  0x08   /* il bit 3 del primo byte e' sempre acceso */
#define PACCHETTO_TROPPO 0xC0 /* "troppo veloce": lo spostamento non e' affidabile */

/* Attese con un limite: un controller che non risponde non deve bloccare l'avvio. */
static int puo_scrivere(void) {
    for (int i = 0; i < 100000; i++) if (!(inb(PORTA_STATO) & STATO_OCCUPATO)) return 1;
    return 0;
}
static int puo_leggere(void) {
    for (int i = 0; i < 100000; i++) if (inb(PORTA_STATO) & STATO_PIENO) return 1;
    return 0;
}

static void al_controller(unsigned char comando) { if (puo_scrivere()) outb(PORTA_STATO, comando); }
static void dati(unsigned char byte)              { if (puo_scrivere()) outb(PORTA_DATI, byte); }

/* Un byte per il mouse; 1 se ha risposto "ricevuto". */
static int al_mouse(unsigned char byte) {
    al_controller(CTRL_AL_MOUSE);
    dati(byte);
    return puo_leggere() && inb(PORTA_DATI) == MOUSE_RICEVUTO;
}

/* --- il gestore: un byte per interruzione, tre fanno un pacchetto --- */

static unsigned char pacchetto[3];
static int quanti;

static void al_mouse_irq(struct registri *r) {
    (void)r;
    /* PRIMA SI CHIEDE SE C'E' DAVVERO UN BYTE. Durante l'accensione il mouse risponde
     * "ricevuto" ai nostri comandi, e ogni risposta suona IRQ 12: la linea e' ancora
     * chiusa, ma il PIC se lo ricorda e la consegna appena si accendono le interruzioni.
     * A quel punto la risposta l'abbiamo gia' letta, e la porta dei dati ripete l'ultimo
     * byte (0xFA): preso per l'inizio di un pacchetto, sfaserebbe i primi movimenti. */
    unsigned char stato = inb(PORTA_STATO);
    if (!(stato & STATO_PIENO) || !(stato & STATO_DAL_MOUSE)) return;
    unsigned char b = inb(PORTA_DATI);

    /* Se si perde un byte, i pacchetti si sfasano. Il primo byte ha sempre il bit 3:
     * un "primo byte" senza e' un byte di mezzo, e lo si salta per ritrovare il passo. */
    if (quanti == 0 && !(b & PACCHETTO_UNO)) return;
    pacchetto[quanti++] = b;
    if (quanti < 3) return;
    quanti = 0;

    unsigned char primo = pacchetto[0];
    if (primo & PACCHETTO_TROPPO) return;
    /* 9 bit con segno: gli 8 bassi nel byte, il nono (il segno) nel byte 0 */
    int dx = pacchetto[1] - ((primo & 0x10) ? 256 : 0);
    int dy = pacchetto[2] - ((primo & 0x20) ? 256 : 0);

    struct evento e = {
        .tipo = EVENTO_MOUSE,
        .dx = dx,
        .dy = -dy,   /* il mouse conta in su, lo schermo in giu' */
        .pulsanti = primo & (PULSANTE_SINISTRO | PULSANTE_DESTRO | PULSANTE_CENTRALE),
    };
    eventi_metti(&e);
}

int mouse_prepara(void) {
    al_controller(CTRL_ATTIVA_MOUSE);

    al_controller(CTRL_LEGGI_CONFIG);
    if (!puo_leggere()) return 0;
    unsigned char config = inb(PORTA_DATI);
    config |= CONFIG_IRQ_MOUSE;
    config &= ~CONFIG_MOUSE_SPENTO;
    al_controller(CTRL_SCRIVI_CONFIG);
    dati(config);

    if (!al_mouse(MOUSE_FABBRICA) || !al_mouse(MOUSE_MANDA)) return 0;

    irq_collega(IRQ_MOUSE, al_mouse_irq);
    pic_apri(IRQ_MOUSE);
    return 1;
}
