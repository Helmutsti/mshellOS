/* Il kernel: prepara tutto, nell'ordine giusto, lancia il primo programma (/init, in
 * ring 3, letto dal disco) e poi diventa il compito "inattivo". Da li' in poi il kernel
 * lavora solo quando qualcuno lo chiama: un'interruzione o una chiamata di sistema. */
#include "kernel.h"
#include "multiboot.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "timer.h"
#include "tastiera.h"
#include "pagine.h"
#include "kmalloc.h"
#include "paginazione.h"
#include "disco.h"
#include "mfs.h"
#include "compiti.h"

extern char kernel_start[], kernel_end[];   /* messi dal linker script */

void panico(const char *fmt, ...) {
    __asm__ volatile("cli");   /* niente piu' tick: nessun compito deve ripartire */
    va_list ap;
    va_start(ap, fmt);
    console_colore(ROSSO);
    kprintf("\nPANICO: ");
    vkprintf(fmt, ap);
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
    console_prepara();

    if (magic != MULTIBOOT_MAGIC_BOOT)
        panico("non sono stato caricato da un bootloader Multiboot (eax=%x)", magic);
    if (!(info->flags & MB_FLAG_MMAP))
        panico("il bootloader non ha dato la mappa della memoria");

    kprintf("Kernel  : %p -> %p  (%u KB)\n",
            kernel_start, kernel_end, (unsigned)(kernel_end - kernel_start) / 1024);

    /* La GDT per prima: quella del bootloader potrebbe stare in memoria che
     * il magazziniere sta per considerare libera. */
    gdt_prepara();
    gdt_mostra();

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
    console_colore(GRIGIO);

    /* La paginazione: dopo il magazziniere, perche' le tabelle sono pagine sue.
     * Da qui ogni indirizzo passa dalla traduzione della CPU. */
    paginazione_prepara();
    kprintf("Paginazione: accesa (%u tabelle), pagina 0 assente, codice in sola lettura\n",
            pag_tabelle());

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

    /* Il disco: si chiede al controller "chi sei?". Con polling, quindi non servono interruzioni. */
    int c_e_il_disco = disco_prepara();
    if (c_e_il_disco)
        kprintf("Disco   : %s, %u settori (%u KB)\n",
                disco_modello(), disco_settori(), disco_settori() / 2);
    else
        kprintf("Disco   : nessuno\n");

    /* Il file system: si legge il superblocco e si controlla il numero magico. */
    if (!c_e_il_disco) panico("nessun disco: non posso leggere /init");
    if (!mfs_monta()) panico("il disco non ha mfs: non posso leggere /init");
    kprintf("mfs     : montato\n");

    /* Lo scheduler: da qui in poi questo codice e' il compito 0, "inattivo", e gli altri
     * compiti si alternano a ogni tick. */
    compiti_prepara();

    /* IL PRIMO PROGRAMMA. Come in Linux, il kernel lancia un solo programma, /init, e
     * sara' lui a far nascere tutto il resto. Senza init non c'e' sistema: panico. */
    const char *errore;
    struct compito *init = compito_crea_programma("/init", &errore);
    if (!init) panico("non posso avviare /init: %s", errore);

    kprintf("Compiti : scheduler acceso (inattivo, init)\n\n");

    /* Il compito inattivo: ferma la CPU fino alla prossima interruzione. Lo scheduler
     * torna qui solo quando nessun altro ha niente da fare. */
    for (;;)
        __asm__ volatile("hlt");
}
