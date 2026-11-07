/* La IDT (Interrupt Descriptor Table).
 *
 * 256 righe da 8 byte. Ogni riga dice: "per l'evento N salta a questo indirizzo,
 * usando questo segmento di codice". Le righe 0-31 sono le eccezioni della CPU:
 * il loro significato lo decide Intel. Le altre sono libere (interruzioni
 * hardware e chiamate di sistema, piu' avanti).
 *
 * Una riga senza il bit "presente" fa scattare un errore se viene usata:
 * colleghiamo le 32 eccezioni (righe 0-31) e le 16 linee del PIC (righe 32-47). */
#include "kernel.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"

struct riga_idt {
    unsigned short indirizzo_basso;   /* bit 0-15 dell'indirizzo del gestore */
    unsigned short segmento;          /* selettore del codice nella GDT */
    unsigned char  zero;
    unsigned char  tipo;              /* presente, ring, tipo di porta */
    unsigned short indirizzo_alto;    /* bit 16-31 dell'indirizzo del gestore */
} __attribute__((packed));

struct registro_idt {
    unsigned short dimensione;
    unsigned int   indirizzo;
} __attribute__((packed));

/* tipo = presente (0x80) | ring 0 | "porta di interruzione a 32 bit" (0x0E).
 * Porta di interruzione: mentre il gestore gira, le interruzioni hardware sono spente. */
#define PORTA_KERNEL 0x8E

#define RIGHE 256
static struct riga_idt idt[RIGHE];
static struct registro_idt idtr;

#define INGRESSI 48
extern unsigned int isr_tabella[INGRESSI];   /* gli indirizzi degli ingressi, da isr.s */

static gestore_irq gestori[16];              /* chi chiamare per ogni linea del PIC */

static void collega(int n, unsigned int gestore) {
    idt[n].indirizzo_basso = gestore & 0xFFFF;
    idt[n].segmento        = SEL_KERNEL_CODICE;
    idt[n].zero            = 0;
    idt[n].tipo            = PORTA_KERNEL;
    idt[n].indirizzo_alto  = gestore >> 16;
}

void idt_prepara(void) {
    for (int n = 0; n < INGRESSI; n++)
        collega(n, isr_tabella[n]);

    idtr.dimensione = sizeof idt - 1;
    idtr.indirizzo  = (unsigned int)idt;
    __asm__ volatile("lidt %0" : : "m"(idtr));   /* "la IDT e' qui" */
}

void idt_mostra(void) {
    struct registro_idt letto;
    __asm__ volatile("sidt %0" : "=m"(letto));
    int collegate = 0;
    for (int n = 0; n < RIGHE; n++)
        if (idt[n].tipo & 0x80) collegate++;
    kprintf("La CPU usa la IDT a %p, %u righe, %d collegate (nostra: %p)\n",
            (void *)letto.indirizzo, (letto.dimensione + 1) / 8, collegate, (void *)idt);
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

static void eccezione(struct registri *r);

/* Il punto d'arrivo di TUTTE le righe collegate: lo chiama isr_comune. */
void interruzione(struct registri *r) {
    if (r->numero < 32) {
        eccezione(r);   /* un errore della CPU: non si torna */
        return;
    }

    /* Un'interruzione hardware: il dispositivo ha chiamato, lo serviamo e torniamo. */
    int irq = r->numero - IRQ_BASE;
    if (gestori[irq])
        gestori[irq](r);
    pic_ricevuto(irq);   /* senza questo il PIC non ci passa piu' niente */
}

static void eccezione(struct registri *r) {
    console_colore(ROSSO);
    kprintf("\nECCEZIONE %u: %s\n", r->numero, r->numero < 32 ? nomi[r->numero] : "?");
    console_colore(GRIGIO);
    kprintf("  dove : EIP=%08x  CS=%02x  EFLAGS=%08x  codice errore=%x\n",
            r->eip, r->cs, r->eflags, r->errore);
    kprintf("  reg. : EAX=%08x EBX=%08x ECX=%08x EDX=%08x\n", r->eax, r->ebx, r->ecx, r->edx);
    kprintf("         ESI=%08x EDI=%08x EBP=%08x\n", r->esi, r->edi, r->ebp);

    if (r->numero == 14) {   /* page fault: CR2 dice quale indirizzo ha dato problemi */
        unsigned int cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        kprintf("  indirizzo che ha causato il page fault: %08x\n", cr2);
    }

    /* Per ora nessuna eccezione si puo' riparare: ci fermiamo qui. */
    panico("sistema fermato dopo l'eccezione %u", r->numero);
}
