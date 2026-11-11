/* Il kernel: prepara tutto, nell'ordine giusto, lancia il primo programma
 * (/Sistema/init, letto dal disco) e poi diventa il compito "inattivo". Da li' in poi il
 * kernel lavora solo quando qualcuno lo chiama: un'interruzione o una chiamata di
 * sistema.
 *
 * Questa e' la parte NEUTRA, uguale per ogni CPU. Prima di arrivare qui, l'avvio della
 * CPU (arch: su x86 kernel_main, in arch/x86_64/avvio/avvio.c) ha gia' acceso lo schermo
 * e la console, preparato le tabelle della CPU e dato al magazziniere la RAM libera. */
#include "kernel.h"
#include "kmalloc.h"
#include "paginazione.h"
#include "disco.h"
#include "cache.h"
#include "vfs_operazioni.h"
#include "ext4.h"
#include "compiti.h"

void panico(const char *fmt, ...) {
    interruzioni_spegni();     /* niente piu' tick: nessun compito deve ripartire */
    va_list ap;
    va_start(ap, fmt);
    console_riprendi(0);       /* se lo schermo era di un programma, torna nostro */
    console_colore(ROSSO);
    kprintf("\nPANICO: ");
    vkprintf(fmt, ap);
    va_end(ap);
    cpu_ferma();
}

void titolo(const char *t) {
    console_colore(GIALLO);
    kprintf("\n%s\n", t);
    console_colore(GRIGIO);
}

void kernel_avvia(const struct schermo *s) {
    /* kmalloc sopra il magazziniere: si prende 1 MB consecutivo e lo divide a pezzi. */
    kmalloc_prepara();

    /* La paginazione: dopo il magazziniere, perche' le tabelle sono pagine sue.
     * Da qui ogni indirizzo passa dalla traduzione della CPU. */
    paginazione_prepara(s->indirizzo, s->passo * s->altezza);
    kprintf("Paginazione: accesa (%u tabelle), pagina 0 assente, codice in sola lettura\n",
            pag_tabelle());

    /* I dispositivi della CPU: le interruzioni, il timer, tastiera e mouse, il bus.
     * Quali sono dipende dalla macchina (arch). Alla fine le interruzioni sono accese. */
    arch_dispositivi_prepara();

    /* I dischi: quelli che i driver (ATA, virtio...) hanno trovato e iscritto. */
    for (int n = 0; n < disco_quanti(); n++) {
        struct dispositivo_blocchi *d = disco_numero(n);
        kprintf("Disco   : %s, %lu settori (%lu MB)\n", d->nome, (unsigned long)d->settori,
                (unsigned long)(d->settori * d->byte_settore >> 20));
    }
    if (!disco_quanti()) panico("nessun disco: non posso leggere /Sistema/init");

    /* IL FILE SYSTEM: "/" e' ext4 sul primo disco, tutto il disco (niente partizioni:
     * vedi disco.h), in sola lettura. Tra ext4 e il disco, la cache dei blocchi. */
    struct dispositivo_blocchi *sistema = cache_crea(disco_numero(0), CACHE_BLOCCHI);
    int r = vfs_monta("/", &tipo_ext4, sistema, 1);
    if (r < 0)
        panico("non posso montare / (ext4 sul disco %s): %s", sistema->nome, vfs_messaggio(r));
    kprintf("VFS     : / montato (ext4 su %s, sola lettura)\n", sistema->nome);

    /* Lo scheduler: da qui in poi questo codice e' il compito 0, "inattivo", e gli altri
     * compiti si alternano a ogni tick. */
    compiti_prepara();

    /* IL PRIMO PROGRAMMA. Come in Linux, il kernel lancia un solo programma,
     * /Sistema/init, e sara' lui a far nascere tutto il resto. Senza init non c'e'
     * sistema: panico. */
    const char *errore;
    struct compito *init = compito_crea_programma("/Sistema/init", UID_SISTEMA, &errore);
    if (!init) panico("non posso avviare /Sistema/init: %s", errore);

    kprintf("Compiti : scheduler acceso (inattivo, init)\n\n");

    /* Il compito inattivo: ferma la CPU fino alla prossima interruzione. Lo scheduler
     * torna qui solo quando nessun altro ha niente da fare. */
    for (;;)
        cpu_riposa();
}
