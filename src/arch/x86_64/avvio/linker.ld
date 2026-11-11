/* La pianta del kernel x86-64.
 *
 * Il kernel e' un ELF a 64 bit: il caricatore UEFI (caricatore/) mette ogni pezzo
 * all'indirizzo FISICO scritto qui, e salta a _start. L'indirizzo virtuale e' lo stesso
 * (mappatura identica, come su ARM): 2 MB. Sotto ci sono il primo MB (memoria del BIOS
 * e della scheda video) e un po' di posto libero; sopra, da 8 MB, il firmware OVMF di
 * QEMU tiene una sua zona (ACPI NVS): il kernel, .bss compresa, deve finire prima.
 * Lo si controlla in fondo (ASSERT).
 *
 * I pezzi cominciano a pagina nuova (4 KB): la paginazione da' i permessi pagina per
 * pagina, e il codice e le costanti (sola lettura) non devono stare nella stessa pagina
 * dei dati.
 *
 * PHDRS: i pezzi da caricare li diciamo noi, due (codice e costanti; dati). Senza, il
 * linker potrebbe mettere fra le cose da caricare anche le intestazioni del file ELF,
 * a un indirizzo basso (fino alla pagina 0), che il caricatore riserverebbe al kernel. */
ENTRY(_start)

PHDRS {
    codice PT_LOAD FLAGS(5);   /* lettura ed esecuzione */
    dati   PT_LOAD FLAGS(6);   /* lettura e scrittura */
}

SECTIONS {
    . = 2M;
    kernel_start = .;

    .text   : ALIGN(4K) { *(.text.avvio) *(.text*) } :codice   /* _start per primo */
    .rodata : ALIGN(4K) { *(.rodata*) } :codice

    /* fin qui: codice e costanti, che non cambiano mai. Con la paginazione
     * queste pagine diventano di SOLA LETTURA. */
    sola_lettura_fine = ALIGN(4K);

    .data   : ALIGN(4K) { *(.data*) } :dati
    .bss    : ALIGN(4K) { *(COMMON) *(.bss*) } :dati

    kernel_end = ALIGN(4K);   /* primo indirizzo libero dopo il kernel */

    /* quello che non serve a far girare il kernel */
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}

ASSERT(kernel_end <= 8M, "il kernel e' troppo grande: arriva alla zona del firmware a 8 MB")
