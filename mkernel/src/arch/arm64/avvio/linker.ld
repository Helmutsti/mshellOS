/* La pianta del kernel ARM64.
 *
 * Sulla macchina "virt" di QEMU la RAM comincia a 0x40000000 (sotto ci sono i
 * dispositivi: la seriale, il centralino delle interruzioni...). Il kernel si mette un
 * po' piu' su, a 0x40080000, come Linux: i primi KB di RAM QEMU li usa per la
 * descrizione della macchina (il "device tree").
 * Prima di tutto c'e' .text.avvio, cioe' _start (kernel_head.s).
 *
 * I pezzi cominciano a pagina nuova (4 KB): la MMU da' i permessi pagina per pagina, e
 * il codice (si esegue, non si scrive), le costanti (solo lettura) e i dati (si
 * scrivono, non si eseguono) non devono stare nella stessa pagina. */
ENTRY(_start)

SECTIONS
{
    . = 0x40080000;
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
