/* L'avvio su ARM64: da chi ci ha caricato (QEMU, UTM, un firmware) al kernel neutro.
 *
 * kernel_head.s ha preparato la pila (ed e' sceso da EL2 se serviva) e salta qui. Qui si
 * fa solo la parte che dipende da ARM:
 *   - ci si presenta sulla seriale (livello, CPU);
 *   - la tabella dei vettori (le eccezioni);
 *   - il device tree: dove sono la RAM, la seriale, il GIC...;
 *   - la MMU: kernel_head.s l'ha gia' accesa, con il kernel in alto e una mappa
 *     provvisoria in basso (virtuale = fisico: cosi' qui si leggono il device tree e si
 *     scrive sulla seriale al loro indirizzo fisico). Qui si aggiunge la mappa diretta
 *     della RAM (mmu_prepara), si danno al device tree e alla seriale i loro posti in alto,
 *     e si toglie la mappa provvisoria: da li' la meta' bassa e' solo dei programmi.
 *     Poi due prove che dice di no;
 *   - la RAM al magazziniere, proteggendo il device tree;
 * poi si passa la mano al kernel neutro (kernel_avvia, in avvio/kernel.c), come su x86.
 *
 * Qui c'e' anche arch_dispositivi_prepara, che il kernel neutro chiama quando e' il
 * momento delle interruzioni: su ARM sono il GIC e il timer generico, poi il bus PCI.
 *
 * Due registri che x86 non ha:
 *   CurrentEL : a che livello sta girando la CPU (bit 3-2): 0 programmi, 1 kernel,
 *               2 hypervisor, 3 firmware. Il kernel deve essere a EL1.
 *   MIDR_EL1  : chi ha fatto la CPU (bit 31-24: 0x41 = ARM, 0x61 = Apple) e che modello e'.
 * Si leggono con l'istruzione mrs ("dal registro di sistema a un registro normale"). */
#include "kernel.h"
#include "seriale.h"
#include "eccezioni.h"
#include "gic.h"
#include "timer.h"
#include "mmu.h"
#include "albero.h"
#include "pagine.h"
#include "pci.h"
#include "driver.h"
#include "ramfb.h"
#include "paginazione.h"

extern char kernel_start[], kernel_end[];   /* da linker.ld */
extern unsigned long avvio_albero, avvio_livello;   /* da kernel_head.s */

/* IL DEVICE TREE: dove l'ha lasciato chi ci ha avviato (x0). Se x0 era 0 (QEMU con un
 * kernel ELF), si guarda all'inizio della RAM, dove QEMU lo mette in quel caso. Lo si
 * riconosce dal numero magico 0xd00dfeed (scritto "big-endian": byte d0 0d fe ed). */
#define MAGICO_ALBERO 0xd00dfeedU
static int e_un_albero(unsigned long a) {
    if (!a) return 0;
    const volatile unsigned char *b = (const volatile unsigned char *)a;
    unsigned int m = (unsigned int)b[0] << 24 | (unsigned int)b[1] << 16 | (unsigned int)b[2] << 8 | b[3];
    return m == MAGICO_ALBERO;
}

static unsigned long trova_albero(void) {
    if (e_un_albero(avvio_albero)) return avvio_albero;
    if (e_un_albero(0x40000000UL)) return 0x40000000UL;
    return 0;
}

static unsigned long leggi_livello(void) {
    unsigned long v;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(v));
    return (v >> 2) & 3;
}

static unsigned long leggi_midr(void) {
    unsigned long v;
    __asm__ volatile("mrs %0, midr_el1" : "=r"(v));
    return v;
}

static const char *costruttore(unsigned long midr) {
    switch ((midr >> 24) & 0xFF) {
    case 0x41: return "ARM";
    case 0x61: return "Apple";
    default:   return "un altro";
    }
}

static void numero(unsigned long n) {
    char cifre[24];
    int i = 0;
    do { cifre[i++] = (char)('0' + n % 10); n /= 10; } while (n);
    while (i) seriale_scrivi(cifre[--i]);
}

/* Un indirizzo che il compilatore non puo' prevedere (cosi' non "aggiusta" la prova). */
static volatile unsigned long *nascosto(unsigned long a) {
    __asm__ volatile("" : "+r"(a));
    return (volatile unsigned long *)a;
}

static void racconta_prova(const char *cosa) {
    unsigned long dove, esr;
    seriale_testo(cosa);
    if (prova_esito(&dove, &esr)) {
        seriale_testo(": NO, dice la MMU (");
        seriale_testo(((esr & 0x3F) >> 2) == 3 ? "permesso negato" :
                      ((esr & 0x3F) >> 2) == 1 ? "pagina assente" : "errore");
        seriale_testo(", indirizzo ");
        seriale_esadecimale(dove);
        seriale_testo(")\n");
    } else {
        seriale_testo(": e' passato! (la MMU non protegge)\n");
    }
}

void kernel_main(void) {
    seriale_testo("\nciao, sono mkernel su ARM64\n");
    seriale_testo("Livello : EL");
    seriale_scrivi((char)('0' + leggi_livello()));
    seriale_testo(leggi_livello() == 1 ? " (il kernel: giusto)" : " (non e' EL1!)");
    seriale_testo(avvio_livello == 2 ? ", partito a EL2 e sceso\n" : ", partito gia' li'\n");
    unsigned long albero = trova_albero();
    seriale_testo("Albero  : ");
    if (albero) {
        seriale_testo("device tree a ");
        seriale_esadecimale(albero);
        seriale_testo(albero == avvio_albero ? " (da x0)\n" : " (inizio della RAM)\n");
    } else {
        seriale_testo("nessun device tree: uso gli indirizzi di QEMU \"virt\"\n");
    }
    /* Dal device tree: la RAM e la seriale (da qui la si usa all'indirizzo che dice lui:
     * per ora quello fisico, nella mappa provvisoria). */
    unsigned long seriale = 0x09000000UL;   /* quella di "virt", se il device tree non c'e' */
    unsigned long dove, quanto;
    if (albero && albero_prepara((const void *)albero)) {
        if (albero_memoria(&dove, &quanto)) {
            seriale_testo("RAM     : da ");
            seriale_esadecimale(dove);
            seriale_testo(", ");
            numero(quanto >> 20);
            seriale_testo(" MB\n");
        }
        if (albero_cerca("arm,pl011", &dove, &quanto)) {
            seriale = dove;
            seriale_usa(dove);
            seriale_testo("Seriale : PL011 a ");
            seriale_esadecimale(dove);
            seriale_testo(" (dal device tree)\n");
        }
    }
    unsigned long midr = leggi_midr();
    seriale_testo("CPU     : MIDR ");
    seriale_esadecimale(midr);
    seriale_testo(", fatta da ");
    seriale_testo(costruttore(midr));
    seriale_testo("\nKernel  : ");
    seriale_esadecimale((unsigned long)kernel_start);
    seriale_testo(" -> ");
    seriale_esadecimale((unsigned long)kernel_end);
    seriale_testo(", nella RAM a ");
    seriale_esadecimale(kernel_fisico);
    seriale_scrivi('\n');

    /* Le eccezioni: da qui un errore stampa il panico invece di bloccare tutto in silenzio. */
    vettori_prepara();
    seriale_testo("Vettori : caricati (VBAR_EL1)\n");

    /* La MMU: con la MMU spenta, ARM tratta tutto come un dispositivo, senza cache;
     * accesa, la RAM e' memoria normale, con la cache. (In una macchina virtuale la
     * differenza non si vede: la RAM dell'ospite la tiene in cache chi la ospita.) */
    mmu_prepara();
    seriale_testo("MMU     : accesa (4 livelli, pagine da 4 KB), cache accese, la RAM in alto\n");

    /* Il device tree e la seriale in alto: il device tree sta nella RAM, quindi nella
     * mappa diretta; la seriale nella zona dei dispositivi (ci vuole il magazziniere,
     * per le tabelle: lo si prepara prima, sotto). */
    unsigned long albero_fisico = albero;
    if (albero) {
        albero = (unsigned long)virt_da_fisico(albero_fisico);
        albero_prepara((const void *)albero);
    }

    /* Il magazziniere: la RAM che dice il device tree (se non c'e', quella di "virt");
     * poi si rioccupa cio' che e' gia' usato: il device tree stesso (lo si legge ancora). */
    static struct zona_ram zona;
    unsigned long ram, ram_byte;
    if (!albero_memoria(&ram, &ram_byte)) { ram = 0x40000000UL; ram_byte = 128UL << 20; }
    zona = (struct zona_ram){ ram, ram + ram_byte };
    pagine_prepara(&zona, 1);
    if (albero) pagine_proteggi(albero_fisico, albero_fisico + albero_dimensione());

    /* La seriale nella zona dei dispositivi; poi via la mappa provvisoria. */
    indirizzo_t seriale_alta = pag_mappa_dispositivo(seriale, 0x1000);
    if (!seriale_alta) panico("non posso mappare la seriale");
    seriale_usa(seriale_alta);
    pag_usa_mappa(0);
    kprintf("Pagine  : %u libere = %u KB; la meta' bassa ora e' vuota\n",
            pagine_libere(), pagine_libere() * 4);

    /* Le prove: la MMU deve dire di no. */
    prova_errore_atteso();
    *nascosto((unsigned long)kernel_start) = 0;   /* scrivere nel codice del kernel */
    racconta_prova("Prova   : scrivere nel codice del kernel");
    prova_errore_atteso();
    (void)*nascosto(0x10);                         /* leggere vicino a NULL */
    racconta_prova("Prova   : leggere l'indirizzo 0x10");

    /* Lo schermo: ramfb (QEMU, UTM). Se non c'e', la console scrive solo sulla seriale. */
    static struct schermo schermo;
    if (ramfb_prepara(&schermo, 1280, 800) && console_prepara(&schermo))
        kprintf("Schermo : %ux%u, ramfb a %p\n", schermo.larghezza, schermo.altezza,
                (void *)schermo.fisico);
    else
        kprintf("Schermo : nessuno (solo la seriale)\n");
    kernel_avvia(&schermo);   /* da qui, il kernel neutro: non torna */
}

void arch_dispositivi_prepara(void) {
    /* Il centralino e il metronomo, poi si aprono le interruzioni. L'ordine conta, come
     * su x86: prima chi le gestisce, poi le si lascia arrivare. */
    titolo("Interruzioni");
    gic_prepara();
    timer_prepara(TIMER_HZ);
    unsigned long frequenza;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(frequenza));
    interruzioni_accendi();   /* il rubinetto I di DAIF: da qui arrivano le IRQ */
    kprintf("GIC versione %d (distributore a %p), timer generico a %u Hz (%u colpi al secondo), "
            "interruzioni accese\n", gic_versione(), (void *)gic_distributore(), TIMER_HZ,
            (unsigned int)frequenza);

    /* Il bus PCI e la libreria dei driver. */
    titolo("PCI");
    int schede = pci_prepara();
    if (schede) {
        kprintf("%d schede:\n", schede);
        pci_mostra();
        driver_avvia_pci();
    } else {
        kprintf("nessun bus PCI (per ora)\n");
    }
}
