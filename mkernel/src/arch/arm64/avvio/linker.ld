/* La pianta del kernel ARM64.
 *
 * Sulla macchina "virt" di QEMU la RAM comincia a 0x40000000 (sotto ci sono i
 * dispositivi: la seriale, il centralino delle interruzioni...). Il kernel si mette un
 * po' piu' su, a 0x40080000, come Linux: i primi KB di RAM QEMU li usa per la
 * descrizione della macchina (il "device tree").
 * Prima di tutto c'e' .text.avvio, cioe' _start (kernel_head.s). */
ENTRY(_start)

SECTIONS
{
    . = 0x40080000;
    kernel_start = .;

    .text : {
        *(.text.avvio)
        *(.text*)
    }
    .rodata : ALIGN(8) { *(.rodata*) }
    .data   : ALIGN(8) { *(.data*) }
    .bss    : ALIGN(16) {
        bss_inizio = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(8);
        bss_fine = .;
    }

    kernel_end = .;
}
