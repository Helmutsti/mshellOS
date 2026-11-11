/* La IDT (Interrupt Descriptor Table).
 *
 * 256 righe da 16 byte (a 64 bit: l'indirizzo del gestore e' lungo 8 byte, e non ci sta
 * piu' in una riga da 8 come a 32 bit). Ogni riga dice: "per l'evento N salta a questo
 * indirizzo, usando questo segmento di codice". Le righe 0-31 sono le eccezioni della
 * CPU: il loro significato lo decide Intel. Le altre sono libere (interruzioni hardware e
 * chiamate di sistema).
 *
 * Una riga senza il bit "presente" fa scattare un errore se viene usata:
 * colleghiamo le 32 eccezioni (righe 0-31) e le 16 linee del PIC (righe 32-47). */
#include "kernel.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "compiti.h"
#include "dinamica.h"
#include "chiamate.h"

struct riga_idt {
    unsigned short indirizzo_basso;   /* bit 0-15 dell'indirizzo del gestore */
    unsigned short segmento;          /* selettore del codice nella GDT */
    unsigned char  pila_speciale;     /* IST: 0 = la pila solita (non ne usiamo altre) */
    unsigned char  tipo;              /* presente, ring, tipo di porta */
    unsigned short indirizzo_medio;   /* bit 16-31 */
    unsigned int   indirizzo_alto;    /* bit 32-63 */
    unsigned int   zero;
} __attribute__((packed));

struct registro_idt {
    unsigned short dimensione;
    unsigned long  indirizzo;
} __attribute__((packed));

/* tipo = presente (0x80) | ring 0 | "porta di interruzione" (0x0E).
 * Porta di interruzione: mentre il gestore gira, le interruzioni hardware sono spente. */
#define PORTA_KERNEL 0x8E
/* Come sopra, ma ring 3 (0x60): l'istruzione "int" la puo' usare anche un programma.
 * Le altre righe restano ring 0: se un programma prova "int" su una di quelle, la CPU
 * lo ferma (protezione generale). */
#define PORTA_PROGRAMMI 0xEE

#define RIGHE 256
static struct riga_idt idt[RIGHE];
static struct registro_idt idtr;

#define INGRESSI 48
extern unsigned long isr_tabella[INGRESSI];   /* gli indirizzi degli ingressi, da isr.s */

static gestore_irq gestori[16];               /* chi chiamare per ogni linea del PIC */

static void collega(int n, unsigned long gestore, unsigned char tipo) {
    idt[n].indirizzo_basso = gestore & 0xFFFF;
    idt[n].segmento        = SEL_KERNEL_CODICE;
    idt[n].pila_speciale   = 0;
    idt[n].tipo            = tipo;
    idt[n].indirizzo_medio = (gestore >> 16) & 0xFFFF;
    idt[n].indirizzo_alto  = gestore >> 32;
    idt[n].zero            = 0;
}

extern void isr128(void), isr129(void);
indirizzo_t chiamata_di_sistema(struct registri *r);   /* chiamate.c */

void idt_prepara(void) {
    for (int n = 0; n < INGRESSI; n++)
        collega(n, isr_tabella[n], PORTA_KERNEL);
    collega(INT_CEDI, (unsigned long)isr129, PORTA_KERNEL);         /* cedi(): un compito passa il turno */
    collega(INT_SISTEMA, (unsigned long)isr128, PORTA_PROGRAMMI);   /* la porta per i programmi */

    idtr.dimensione = sizeof idt - 1;
    idtr.indirizzo  = (unsigned long)idt;
    __asm__ volatile("lidt %0" : : "m"(idtr));   /* "la IDT e' qui" */
}

void idt_mostra(void) {
    struct registro_idt letto;
    __asm__ volatile("sidt %0" : "=m"(letto));
    int collegate = 0;
    for (int n = 0; n < RIGHE; n++)
        if (idt[n].tipo & 0x80) collegate++;
    kprintf("IDT     : %s a %p, %u righe, %d collegate\n",
            letto.indirizzo == (unsigned long)idt ? "la nostra" : "NON la nostra",
            (void *)letto.indirizzo, (letto.dimensione + 1) / 16, collegate);
}

/* --- il gestore in C: lo chiama isr_comune --- */

static const char *nomi[32] = {
    "divisione per zero", "debug", "NMI", "breakpoint",
    "overflow", "fuori dai limiti", "istruzione non valida", "coprocessore assente",
    "doppio errore", "(storico)", "TSS non valido", "segmento assente",
    "errore della pila", "protezione generale", "page fault", "(riservata)",
    "errore x87", "allineamento", "machine check", "errore SSE",
    "virtualizzazione", "protezione del controllo", "(riservata)", "(riservata)",
    "(riservata)", "(riservata)", "(riservata)", "(riservata)",
    "(riservata)", "(riservata)", "(riservata)", "(riservata)",
};

void irq_collega(int irq, gestore_irq gestore) {
    gestori[irq] = gestore;
}

static indirizzo_t eccezione(struct registri *r);

/* Il punto d'arrivo di TUTTE le righe collegate: lo chiama isr_comune.
 * Restituisce l'RSP da cui isr_comune ripeschera' i registri: quello di 'r' per
 * tornare al compito interrotto, o quello di un altro compito scelto dallo scheduler. */
indirizzo_t interruzione(struct registri *r) {
    if (r->numero < 32)
        return eccezione(r);   /* un errore della CPU */

    /* un programma chiede un servizio al kernel */
    if (r->numero == INT_SISTEMA)
        return chiamata_di_sistema(r);

    /* cedi(): il compito vuole passare il turno */
    if (r->numero == INT_CEDI)
        return scheduler((indirizzo_t)r);

    /* Un'interruzione hardware: il dispositivo ha chiamato, lo serviamo. */
    int irq = (int)r->numero - IRQ_BASE;
    if (gestori[irq])
        gestori[irq](r);
    pic_ricevuto(irq);   /* senza questo il PIC non ci passa piu' niente */

    /* Il timer decide i turni. Per le altre interruzioni (es. un tasto) si cambia
     * compito solo se stava girando l'inattivo: chi e' stato appena svegliato
     * riparte subito, senza aspettare il prossimo tick. */
    if (irq == IRQ_TIMER || scheduler_sta_riposando())
        return scheduler((indirizzo_t)r);
    return (indirizzo_t)r;
}

static unsigned long leggi_cr2(void) {
    unsigned long cr2;
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
    return cr2;
}

/* Il page fault: CR2 dice QUALE indirizzo, il codice di errore dice COSA si voleva fare.
 *   bit 0: 0 = la pagina non c'e'          1 = c'e', ma il permesso manca
 *   bit 1: 0 = in lettura                   1 = in scrittura
 *   bit 2: 0 = mentre girava il kernel      1 = mentre girava un programma (ring 3) */
static void spiega_page_fault(unsigned long errore) {
    unsigned long cr2 = leggi_cr2();
    kprintf("  %s all'indirizzo %p: %s (%s)\n",
            errore & 2 ? "scrittura" : "lettura", (void *)cr2,
            errore & 1 ? "permesso negato" : "pagina assente",
            errore & 4 ? "ring 3" : "ring 0");
    if (cr2 < 4096)
        kprintf("  (e' la pagina 0: quasi certamente un puntatore nullo)\n");
}

static indirizzo_t eccezione(struct registri *r) {
    /* Un page fault su una pagina che non c'e' (bit 0 a zero) potrebbe non essere un
     * errore: se l'indirizzo e' nella zona dinamica, sotto il confine, la pagina era
     * solo promessa. La si da' adesso, e si torna all'istruzione che aveva fallito:
     * stavolta la pagina c'e'. Vale anche per il kernel che tocca la memoria di un
     * programma durante una chiamata di sistema. */
    if (r->numero == 14 && !(r->errore & 1) && memoria_promessa(leggi_cr2()))
        return (indirizzo_t)r;

    console_colore(ROSSO);
    kprintf("\nECCEZIONE %u: %s\n", (unsigned int)r->numero, nomi[r->numero]);
    console_colore(GRIGIO);
    if (r->numero == 14) spiega_page_fault(r->errore);
    kprintf("  dove : RIP=%p  CS=%02lx  RFLAGS=%08lx  codice errore=%lx\n",
            (void *)r->rip, r->cs, r->rflags, r->errore);
    kprintf("  reg. : RAX=%p RBX=%p RCX=%p RDX=%p\n",
            (void *)r->rax, (void *)r->rbx, (void *)r->rcx, (void *)r->rdx);
    kprintf("         RSI=%p RDI=%p RBP=%p RSP=%p\n",
            (void *)r->rsi, (void *)r->rdi, (void *)r->rbp, (void *)r->rsp);

    /* Se ha sbagliato un compito con una pila sua (non l'inattivo, non il kernel prima
     * dello scheduler), si chiude SOLO LUI e il sistema continua: lo scheduler passa
     * a un altro compito e liberera' questo. E' quello che fanno i sistemi veri con
     * un programma che sbaglia ("Segmentation fault"). */
    struct compito *c = compito_attuale();
    if (c && c->pila && c->lucchetti)   /* la risorsa che teneva e' rimasta a meta' */
        panico("il compito %d ha sbagliato mentre teneva un lucchetto", c->numero);
    if (c && c->pila) {
        console_colore(GIALLO);
        kprintf("  Il compito %d (%s) e' stato chiuso. Il sistema continua.\n", c->numero, c->nome);
        console_colore(GRIGIO);
        c->stato = FINITO;
        return scheduler((indirizzo_t)r);
    }

    /* Ha sbagliato il kernel stesso (o l'inattivo): non c'e' nessuno a cui tornare. */
    panico("sistema fermato dopo l'eccezione %u", (unsigned int)r->numero);
}
