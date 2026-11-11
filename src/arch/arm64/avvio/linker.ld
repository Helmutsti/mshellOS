/* La pianta del kernel ARM64.
 *
 * DOVE LO METTE CHI CI AVVIA. Sulla macchina "virt" di QEMU la RAM comincia a 0x40000000
 * (sotto ci sono i dispositivi: la seriale, il centralino delle interruzioni...). Il
 * kernel si mette un po' piu' su, a 0x40080000, come Linux (lo dice l'intestazione
 * "Image" di kernel_head.s: 512 KB dopo l'inizio della RAM). Su un'altra scheda la RAM
 * puo' cominciare altrove: il kernel non lo sa prima, e lo scopre all'avvio
 * (kernel_fisico).
 *
 * DOVE E' LEGATO. Gli indirizzi scritti nel codice sono quelli IN ALTO, da
 * KERNEL_IMMAGINE (src/memoria/pagine.h: i due numeri devono coincidere): e' li' che lo
 * vede la CPU dopo che kernel_head.s ha acceso la MMU. Il file "Image" e' grezzo (i byte
 * uno dopo l'altro), quindi l'indirizzo di legame non ci finisce dentro.
 * Prima di tutto c'e' .text.avvio, cioe' _start (kernel_head.s).
 *
 * I pezzi cominciano a pagina nuova (4 KB): la MMU da' i permessi pagina per pagina, e
 * il codice (si esegue, non si scrive), le costanti (solo lettura) e i dati (si
 * scrivono, non si eseguono) non devono stare nella stessa pagina. */
ENTRY(_start)

KERNEL_IMMAGINE = 0xFFFFFFFF80000000;

SECTIONS
{
    . = KERNEL_IMMAGINE;
    kernel_start = .;

    .text : {
        *(.text.avvio)
        *(.text*)
    }
    . = ALIGN(4096);
    codice_fine = .;

    .rodata : { *(.rodata*) }
    . = ALIGN(4096);
    costanti_fine = .;

    .data   : ALIGN(8) { *(.data*) }
    .bss    : ALIGN(4096) {
        bss_inizio = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(8);
        bss_fine = .;
    }

    kernel_end = .;
}
kernel_dimensione = kernel_end - kernel_start;   /* per l'intestazione "Image" */

/* mmu_avvio mappa il kernel con al massimo 4 tabelle L3, cioe' 8 MB (MAX_L3_KERNEL). */
ASSERT(kernel_dimensione <= 8M, "il kernel e' troppo grande: mmu.c ne mappa al massimo 8 MB")
