/* L'avvio su x86: da GRUB al kernel neutro.
 *
 * kernel_head.s prepara la pila e salta qui, con quello che GRUB ci ha lasciato
 * (Multiboot): com'e' fatto lo schermo e la mappa della memoria. Qui si fa solo la
 * parte che dipende da x86 e da GRUB:
 *   - lo schermo, dalle informazioni Multiboot;
 *   - la GDT e la IDT, le tabelle della CPU;
 *   - la mappa della memoria di GRUB, tradotta in un elenco di zone di RAM per il
 *     magazziniere;
 * poi si passa la mano al kernel neutro (kernel_avvia, in avvio/kernel.c).
 *
 * Qui c'e' anche arch_dispositivi_prepara, che il kernel neutro chiama quando e' il
 * momento delle interruzioni: su x86 sono il PIC, il timer PIT, tastiera e mouse PS/2,
 * e poi il bus PCI. */
#include "kernel.h"
#include "multiboot.h"
#include "pagine.h"
#include "pic.h"
#include "timer.h"
#include "tastiera.h"
#include "mouse.h"
#include "pci.h"
#include "driver.h"
#include "ata.h"

extern char kernel_start[], kernel_end[];   /* messi dal linker script */

/* Senza modalita' pixel non abbiamo dove scrivere: e' rimasta accesa la vecchia
 * modalita' testo (succede se il kernel non e' stato avviato da GRUB, per esempio con
 * "qemu -kernel", che la modalita' pixel non la sa accendere). Si lascia un messaggio
 * direttamente nella memoria del testo, a 0xB8000, e ci si ferma. */
static void senza_schermo(void) {
    static const char messaggio[] = "mkernel: serve la modalita' pixel. Avvialo con GRUB (run.ps1).";
    volatile unsigned short *testo = (unsigned short *)0xB8000;
    for (int i = 0; messaggio[i]; i++)
        testo[i] = (unsigned char)messaggio[i] | (((NERO << 4) | ROSSO) << 8);
    cpu_ferma();
}

/* La mappa della memoria di GRUB, tradotta: solo le zone libere. GRUB ne da' di solito
 * una decina; oltre MAX_ZONE si ignorano (si perde un po' di RAM, non si rompe niente). */
#define MAX_ZONE 64
static struct zona_ram zone[MAX_ZONE];

static int zone_da_grub(const struct multiboot_info *info) {
    int quante = 0;
    unsigned int a = info->mmap_addr, fine = info->mmap_addr + info->mmap_length;
    while (a < fine && quante < MAX_ZONE) {
        const struct multiboot_mmap *r = (const struct multiboot_mmap *)a;
        a += r->size + 4;   /* 'size' non conta se stesso */
        if (r->type != MB_MMAP_LIBERA) continue;
        zone[quante++] = (struct zona_ram){ r->base, r->base + r->len };
    }
    return quante;
}

void kernel_main(unsigned int magic, struct multiboot_info *info) {
    /* Lo schermo per primo, per poter scrivere tutto il resto (anche i panici). */
    if (magic != MULTIBOOT_MAGIC_BOOT || !(info->flags & MB_FLAG_SCHERMO) ||
        info->framebuffer_type != MB_SCHERMO_RGB || (info->framebuffer_addr >> 32))
        senza_schermo();
    static struct schermo s;
    s = (struct schermo){
        .indirizzo = (unsigned int)info->framebuffer_addr,
        .passo = info->framebuffer_pitch,
        .larghezza = info->framebuffer_width, .altezza = info->framebuffer_height,
        .bpp = info->framebuffer_bpp,
        .rosso = info->rosso_posizione, .verde = info->verde_posizione, .blu = info->blu_posizione,
    };
    if (!console_prepara(&s)) senza_schermo();
    kprintf("Schermo : %ux%u, %u bit per pixel, a %p\n", s.larghezza, s.altezza, s.bpp,
            (void *)s.indirizzo);
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

    /* Il magazziniere: le zone libere secondo GRUB; poi si rioccupa cio' che e' gia'
     * usato: il primo MB (BIOS, memoria video), le informazioni di GRUB e la sua mappa. */
    pagine_prepara(zone, zone_da_grub(info));
    pagine_proteggi(0, 0x100000);
    pagine_proteggi((unsigned int)info, (unsigned int)info + sizeof *info);
    pagine_proteggi(info->mmap_addr, info->mmap_addr + info->mmap_length);
    kprintf("Pagine  : %u libere = %u KB\n", pagine_libere(), pagine_libere() * 4);

    kernel_avvia(&s);   /* da qui, il kernel neutro: non torna */
}

void arch_dispositivi_prepara(void) {
    /* Le interruzioni hardware: centralino, metronomo, e poi si accende il campanello.
     * L'ordine conta: se si accendessero prima, il primo tick finirebbe su una riga
     * vuota della IDT (o, col PIC non riprogrammato, sulla riga 8: "doppio errore"). */
    titolo("Interruzioni");
    pic_prepara();
    timer_prepara(100);
    int c_e_il_mouse = mouse_prepara();   /* prima della tastiera: lei poi svuota la porta */
    tastiera_prepara();
    interruzioni_accendi();   /* da qui la CPU ascolta */
    kprintf("PIC sulle righe %d-%d, timer a 100 Hz, tastiera, %s, interruzioni accese\n",
            IRQ_BASE, IRQ_BASE + 15, c_e_il_mouse ? "mouse" : "nessun mouse");

    /* Il bus PCI: si bussa a tutte le stanze e si stampa chi c'e'. Da qui i driver
     * delle schede moderne (USB, ...) trovano la loro. */
    titolo("PCI");
    kprintf("%d schede:\n", pci_prepara());
    pci_mostra();

    /* Il disco IDE per primo: e' quello con il sistema (se c'e' anche un disco virtio,
     * si iscrive dopo e non conta). Poi la libreria dei driver: per ogni scheda, chi la
     * sa gestire. */
    ata_prepara();
    driver_avvia_pci();
}
