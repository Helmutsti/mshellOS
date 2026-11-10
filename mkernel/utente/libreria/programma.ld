/* Come legare un programma in ring 3.
 *
 * Ogni programma e' un file ELF a se', compilato per vivere da 1 GB in su nella sua
 * mappa (PROGRAMMI_INIZIO in comune/chiamate.h: i due numeri devono coincidere).
 *
 * Tre parti con permessi diversi, e quindi tre pezzi nel file ELF:
 *   codice:     lettura ed esecuzione
 *   costanti:   solo lettura
 *   variabili:  lettura e scrittura (bss: variabili a zero, non stanno nel file)
 * I permessi si danno a pagine intere: ogni parte DEVE iniziare a pagina nuova, se no
 * due parti con permessi diversi dividerebbero la stessa pagina. Il kernel controlla
 * e rifiuta un file con un pezzo che non inizia a pagina nuova. */
ENTRY(_start)

SECTIONS {
    . = 0x40000000;

    .text   : ALIGN(4K) { *(.text*) }
    .rodata : ALIGN(4K) { *(.rodata*) }
    /* le variabili a pagina nuova con un ALIGN a parte: un programma senza .data
     * (tutte variabili a zero) la perderebbe, e la .bss finirebbe a meta' pagina */
    . = ALIGN(4K);
    .data   : { *(.data*) }
    .bss    : { *(COMMON) *(.bss*) }
}
