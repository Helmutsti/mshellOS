/* La pianta del kernel x86-64.
 *
 * Il kernel e' un ELF a 64 bit con DUE indirizzi per ogni pezzo:
 *   - quello FISICO (LMA, "AT(...)"), dove il caricatore UEFI (caricatore/) lo mette nella
 *     RAM: da 2 MB. Sotto ci sono il primo MB (memoria del BIOS e della scheda video) e un
 *     po' di posto libero; sopra, da 8 MB, il firmware OVMF di QEMU tiene una sua zona
 *     (ACPI NVS): il kernel, .bss compresa, deve finire prima. Lo si controlla in fondo
 *     (ASSERT);
 *   - quello VIRTUALE (VMA), dove il kernel e' legato e dove lo vede la CPU dopo l'avvio:
 *     KERNEL_IMMAGINE + 2 MB, negli ultimi 2 GB degli indirizzi (la pianta e' in
 *     src/memoria/pagine.h: i due numeri devono coincidere). Il codice si compila con
 *     -mcmodel=kernel (build.sh): ogni indirizzo del kernel sta in 32 bit "con il segno".
 * Il caricatore salta al punto d'ingresso tradotto in fisico; kernel_head.s fa le
 * tabelle delle pagine e salta in alto.
 *
 * I pezzi cominciano a pagina nuova (4 KB): la paginazione da' i permessi pagina per
 * pagina, e il codice e le costanti (sola lettura) non devono stare nella stessa pagina
 * dei dati.
 *
 * PHDRS: i pezzi da caricare li diciamo noi, due (codice e costanti; dati). Senza, il
 * linker potrebbe mettere fra le cose da caricare anche le intestazioni del file ELF,
 * a un indirizzo basso (fino alla pagina 0), che il caricatore riserverebbe al kernel. */
ENTRY(_start)

KERNEL_IMMAGINE = 0xFFFFFFFF80000000;

PHDRS {
    codice PT_LOAD FLAGS(5);   /* lettura ed esecuzione */
    dati   PT_LOAD FLAGS(6);   /* lettura e scrittura */
}

SECTIONS {
    . = KERNEL_IMMAGINE + 2M;
    kernel_start = .;

    .text   : AT(ADDR(.text) - KERNEL_IMMAGINE) { *(.text.avvio) *(.text*) } :codice   /* _start per primo */
    . = ALIGN(4K);
    .rodata : AT(ADDR(.rodata) - KERNEL_IMMAGINE) { *(.rodata*) } :codice

    /* fin qui: codice e costanti, che non cambiano mai. Con la paginazione
     * queste pagine diventano di SOLA LETTURA. */
    . = ALIGN(4K);
    sola_lettura_fine = .;

    .data   : AT(ADDR(.data) - KERNEL_IMMAGINE) { *(.data*) } :dati
    . = ALIGN(4K);
    .bss    : AT(ADDR(.bss) - KERNEL_IMMAGINE) { *(COMMON) *(.bss*) } :dati

    kernel_end = ALIGN(4K);   /* primo indirizzo libero dopo il kernel */

    /* quello che non serve a far girare il kernel */
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}

ASSERT(kernel_end - KERNEL_IMMAGINE <= 8M, "il kernel e' troppo grande: arriva alla zona del firmware a 8 MB")
