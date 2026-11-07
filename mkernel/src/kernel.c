/* Il kernel: prepara GDT e magazziniere delle pagine, e li controlla. */
#include <stdarg.h>
#include "kernel.h"
#include "multiboot.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "timer.h"
#include "tastiera.h"
#include "pagine.h"
#include "kmalloc.h"

extern char kernel_start[], kernel_end[];   /* messi dal linker script */

void panico(const char *fmt, ...) {
    /* kprintf non accetta una va_list: stampiamo il formato con al massimo un argomento */
    va_list ap;
    va_start(ap, fmt);
    console_colore(ROSSO);
    kprintf("\nPANICO: ");
    kprintf(fmt, va_arg(ap, unsigned int));
    va_end(ap);
    for (;;) __asm__ volatile("cli; hlt");
}

static int falliti;

/* Stampa solo i controlli falliti: quelli riusciti si contano e basta. */
static void controlla(int vero, const char *cosa) {
    if (vero) return;
    falliti++;
    console_colore(ROSSO);
    kprintf("  [NO!] %s\n", cosa);
    console_colore(GRIGIO);
}

static void titolo(const char *t) {
    console_colore(GIALLO);
    kprintf("\n%s\n", t);
    console_colore(GRIGIO);
}

/* Il magazziniere: chiede, scrive, restituisce, e controlla che tutto torni. */
static void prova_pagine(void) {
    unsigned int all_inizio = pagine_libere();
    unsigned int a = pagina_chiedi(), b = pagina_chiedi(), c = pagina_chiedi();

    controlla(a >= (unsigned int)kernel_end, "a e' dopo la fine del kernel");
    controlla(a % PAGINA == 0 && b % PAGINA == 0 && c % PAGINA == 0, "sono allineate a 4 KB");
    controlla(a != b && b != c && a != c, "sono tutte diverse");

    unsigned int *dentro = (unsigned int *)a;
    dentro[0] = 0xCAFE;
    dentro[1023] = 0xBEEF;
    controlla(dentro[0] == 0xCAFE && dentro[1023] == 0xBEEF, "ci si puo' scrivere e rileggere");

    pagina_restituisci(b);
    unsigned int d = pagina_chiedi();
    controlla(d == b, "una pagina restituita torna disponibile");

    pagina_restituisci(a);
    pagina_restituisci(c);
    pagina_restituisci(d);
    controlla(pagine_libere() == all_inizio, "restituite tutte: il conto torna");
}

/* kmalloc: tre pezzi, un buco nel mezzo, il buco riusato, tutto restituito. */
static void prova_kmalloc(void) {
    unsigned int blocchi, liberi_inizio, liberi;
    kmalloc_conta(&blocchi, &liberi_inizio);

    char *a = kmalloc(10), *b = kmalloc(100), *c = kmalloc(1000);
    controlla(a && b && c, "kmalloc ha dato i tre pezzi");
    controlla((unsigned int)a % 8 == 0 && (unsigned int)b % 8 == 0 && (unsigned int)c % 8 == 0,
              "i pezzi sono allineati a 8");
    controlla(a + 10 <= b && b + 100 <= c, "i pezzi non si sovrappongono");

    /* ognuno scrive nel suo pezzo: se si sovrapponessero, si rovinerebbero a vicenda */
    for (int i = 0; i < 10; i++)   a[i] = 'a';
    for (int i = 0; i < 100; i++)  b[i] = 'b';
    for (int i = 0; i < 1000; i++) c[i] = 'c';
    controlla(a[9] == 'a' && b[0] == 'b' && b[99] == 'b' && c[0] == 'c', "ognuno tiene il suo");

    kfree(b);                         /* un buco nel mezzo */
    kprintf("Dopo kmalloc(10), kmalloc(100), kmalloc(1000) e kfree del secondo:\n");
    kmalloc_mostra();

    char *d = kmalloc(50);
    controlla(d == b, "kmalloc(50) riusa il buco lasciato dal secondo");

    char *grande = kmalloc(2 * 1024 * 1024);   /* piu' della zona iniziale: deve crescere */
    controlla(grande != 0, "un pezzo da 2 MB fa crescere il quaderno");

    kfree(a); kfree(c); kfree(d); kfree(grande);
    kmalloc_conta(&blocchi, &liberi);
    controlla(liberi >= liberi_inizio, "tutto restituito: i blocchi liberi si sono fusi");
    kprintf("Alla fine: %u blocco/i, %u KB liberi (all'inizio %u KB)\n",
            blocchi, liberi / 1024, liberi_inizio / 1024);
}

void kernel_main(unsigned int magic, struct multiboot_info *info) {
    console_pulisci();
    console_colore(BIANCO);
    kprintf("mkernel\n");
    console_colore(GRIGIO);

    if (magic != MULTIBOOT_MAGIC_BOOT)
        panico("non sono stato caricato da un bootloader Multiboot (eax=%x)", magic);
    if (!(info->flags & MB_FLAG_MMAP))
        panico("il bootloader non ha dato la mappa della memoria");

    kprintf("Kernel  : %p -> %p  (%u KB)\n",
            kernel_start, kernel_end, (unsigned)(kernel_end - kernel_start) / 1024);

    /* La GDT per prima: quella del bootloader potrebbe stare in memoria che
     * il magazziniere sta per considerare libera. */
    gdt_prepara();
    gdt_mostra(0);

    /* La IDT subito dopo: le sue righe usano il segmento di codice della GDT. */
    idt_prepara();
    idt_mostra();

    pagine_prepara(info);
    kprintf("Pagine  : %u libere = %u KB\n", pagine_libere(), pagine_libere() * 4);
    prova_pagine();

    /* kmalloc sopra il magazziniere: si prende 1 MB consecutivo e lo divide a pezzi. */
    titolo("kmalloc");
    kmalloc_prepara();
    prova_kmalloc();

    console_colore(falliti ? ROSSO : VERDE);
    kprintf("\n%s\n", falliti ? "Qualche controllo e' fallito." : "Tutti i controlli sono passati.");

    /* Le interruzioni hardware: centralino, metronomo, e poi si accende il campanello.
     * L'ordine conta: se si accendessero prima, il primo tick finirebbe su una riga
     * vuota della IDT (o, col PIC non riprogrammato, sulla riga 8: "doppio errore"). */
    titolo("Interruzioni");
    pic_prepara();
    timer_prepara(100);
    tastiera_prepara();
    __asm__ volatile("sti");   /* accende il flag delle interruzioni: da qui la CPU ascolta */
    kprintf("PIC sulle righe %d-%d, timer a 100 Hz, tastiera, interruzioni accese\n",
            IRQ_BASE, IRQ_BASE + 15);

    console_colore(BIANCO);
    kprintf("\nScrivi qualcosa (tastiera italiana):\n> ");
    console_colore(GRIGIO);

    /* Il ciclo principale: la CPU dorme (hlt) finche' un'interruzione non la sveglia.
     * Al risveglio: stampa i tasti arrivati e, se e' passato un secondo, aggiorna l'orologio. */
    unsigned int ultimo = (unsigned int)-1;
    for (;;) {
        __asm__ volatile("hlt");

        char c;
        while ((c = tastiera_leggi())) {
            console_putc(c);
            if (c == '\n') kprintf("> ");
        }

        unsigned int secondi = timer_tick() / 100;
        if (secondi != ultimo) {
            ultimo = secondi;
            /* l'orologio in alto a destra: ci si sposta, si scrive, si torna dov'eravamo */
            int r, col;
            console_dove(&r, &col);
            unsigned char vecchio = console_colore(CIANO);
            console_vai(0, 62);
            kprintf("[ %02u:%02u:%02u ]", secondi / 3600, secondi / 60 % 60, secondi % 60);
            console_colore(vecchio);
            console_vai(r, col);
        }
    }
}
