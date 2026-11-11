/* L'avvio sul PC (x86-64): dal caricatore UEFI al kernel neutro.
 *
 * kernel_head.s ha preparato la pila e copiato la CONSEGNA (comune/consegna.h): quello
 * che il caricatore ci ha lasciato, cioe' com'e' fatto lo schermo e la mappa della
 * memoria. Qui si fa solo la parte che dipende dal PC e dal caricatore, in quest'ordine:
 *   - le tabelle delle pagine, la GDT e la IDT nostre. PRIMA di tutto il resto: quelle
 *     del firmware stanno in pagine che per noi sono libere, e il magazziniere potrebbe
 *     darle a qualcuno mentre la CPU le sta ancora usando (kernel_head.s ha gia' lasciato
 *     le tabelle delle pagine del firmware per delle sue provvisorie, ma la GDT e' ancora
 *     quella del firmware);
 *   - la mappa della memoria della consegna, tradotta in un elenco di zone di RAM per il
 *     magazziniere;
 *   - lo schermo (la sua memoria si mappa adesso, nella zona dei dispositivi, senza cache);
 * poi si passa la mano al kernel neutro (kernel_avvia, in avvio/kernel.c).
 *
 * Qui c'e' anche arch_dispositivi_prepara, che il kernel neutro chiama quando e' il
 * momento delle interruzioni: sul PC sono il PIC, il timer PIT, tastiera e mouse PS/2,
 * e poi il bus PCI. */
#include "kernel.h"
#include "consegna.h"
#include "mmu.h"
#include "pagine.h"
#include "paginazione.h"
#include "pic.h"
#include "timer.h"
#include "tastiera.h"
#include "mouse.h"
#include "pci.h"
#include "driver.h"
#include "ata.h"

extern char kernel_start[], kernel_end[];   /* messi dal linker script */

/* La consegna, copiata da kernel_head.s: grande quanto dice lui (DIM_CONSEGNA). */
extern struct consegna consegna;
_Static_assert(sizeof(struct consegna) == 6208, "DIM_CONSEGNA in kernel_head.s va aggiornato");

/* Le zone di RAM della consegna, per il magazziniere: quelle libere e quella del kernel.
 * Anche il kernel sta nella RAM: contandolo, il totale e' la RAM della macchina (come su
 * ARM, dove la zona e' la RAM intera), e non cambia quando il kernel cresce. Le sue
 * pagine non finiscono a nessuno: pagine_prepara le rioccupa subito. */
static struct zona_ram zone[MAX_ZONE];

static int zone_ram(void) {
    int quante = 0;
    for (unsigned int i = 0; i < consegna.quante_zone && i < MAX_ZONE; i++) {
        const struct zona_consegna *z = &consegna.zone[i];
        if (z->tipo == ZONA_LIBERA || z->tipo == ZONA_KERNEL)
            zone[quante++] = (struct zona_ram){ z->inizio, z->inizio + z->byte };
    }
    return quante;
}

void kernel_main(void) {
    /* Se la firma non c'e', non ci ha avviato il nostro caricatore: non sappiamo niente
     * della macchina (neanche dov'e' lo schermo). Lo si dice sulla seriale e ci si ferma. */
    if (consegna.firma != CONSEGNA_FIRMA) {
        kprintf("mkernel: manca la consegna del caricatore (caricatore/): mi fermo\n");
        cpu_ferma();
    }
    kprintf("\nciao, sono mkernel sul PC (x86-64)\n");
    kprintf("Kernel  : %p -> %p  (%u KB)\n",
            kernel_start, kernel_end, (unsigned)(kernel_end - kernel_start) / 1024);

    /* 1. Le tabelle delle pagine nostre: da qui quelle provvisorie di kernel_head.s non
     *    servono piu', e la meta' bassa degli indirizzi e' vuota (sara' dei programmi). */
    unsigned long ram = mmu_prepara(&consegna);
    kprintf("Pagine  : RAM nella mappa diretta fino a %p (pagine da 2 MB, %u pezzi in pagine da 4 KB)\n",
            (void *)ram, mmu_pezzi_spezzati());

    /* 2. La GDT e la IDT nostre (la IDT usa il segmento di codice della GDT). */
    gdt_prepara();
    gdt_mostra();
    idt_prepara();
    idt_mostra();

    /* 3. Il magazziniere: la RAM secondo la consegna (le zone libere e il kernel, che lui
     *    stesso rioccupa); poi si rioccupa il primo MB (il vecchio BIOS, la memoria video:
     *    meglio non dare a nessuno). Le pagine del caricatore e del firmware (anche la
     *    consegna originale) da qui sono libere. */
    pagine_prepara(zone, zone_ram());
    pagine_proteggi(0, 0x100000);
    kprintf("Pagine  : %u libere = %u MB\n", pagine_libere(), pagine_libere() / 256);

    /* 4. Lo schermo: la memoria della scheda video, che il caricatore ci ha detto dove
     *    sta (di solito poco sopra la RAM bassa, a 2 o 3 GB). Si mappa come un
     *    dispositivo, poi la console ci scrive. */
    static struct schermo s;
    s = (struct schermo){
        .fisico = consegna.schermo,
        .passo = consegna.passo,
        .larghezza = consegna.larghezza, .altezza = consegna.altezza,
        .bpp = 32,
        .rosso = consegna.rosso, .verde = consegna.verde, .blu = consegna.blu,
    };
    if (consegna.schermo)
        s.indirizzo = pag_mappa_dispositivo(s.fisico, s.passo * s.altezza);
    if (!s.indirizzo) panico("non posso usare lo schermo a %p", (void *)s.fisico);
    if (!console_prepara(&s)) panico("lo schermo non e' a 32 bit per pixel");
    kprintf("Schermo : %ux%u, 32 bit per pixel, a %p (visto a %p)\n", s.larghezza, s.altezza,
            (void *)s.fisico, (void *)s.indirizzo);

    kernel_avvia(&s);   /* da qui, il kernel neutro: non torna */
}

void arch_dispositivi_prepara(void) {
    /* Le interruzioni hardware: centralino, metronomo, e poi si accende il campanello.
     * L'ordine conta: se si accendessero prima, il primo tick finirebbe su una riga
     * vuota della IDT (o, col PIC non riprogrammato, sulla riga 8: "doppio errore"). */
    titolo("Interruzioni");
    pic_prepara();
    timer_prepara(TIMER_HZ);
    int c_e_il_mouse = mouse_prepara();   /* prima della tastiera: lei poi svuota la porta */
    tastiera_prepara();
    interruzioni_accendi();   /* da qui la CPU ascolta */
    kprintf("PIC sulle righe %d-%d, timer a %u Hz, tastiera, %s, interruzioni accese\n",
            IRQ_BASE, IRQ_BASE + 15, TIMER_HZ, c_e_il_mouse ? "mouse" : "nessun mouse");

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
