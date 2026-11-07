/* Dove mettere ogni parte del kernel in memoria. */
ENTRY(_start)

SECTIONS {
    . = 1M;   /* sotto 1 MB ci sono zone del BIOS e della scheda video */
    kernel_start = .;

    .text   : ALIGN(4K) { *(.multiboot) *(.text) }   /* la firma per prima */
    .rodata : ALIGN(4K) { *(.rodata) }
    .data   : ALIGN(4K) { *(.data) }
    .bss    : ALIGN(4K) { *(COMMON) *(.bss) }

    kernel_end = ALIGN(4K);   /* primo indirizzo libero dopo il kernel */
}
