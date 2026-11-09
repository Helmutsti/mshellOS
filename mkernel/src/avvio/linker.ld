/* Dove mettere ogni parte del kernel in memoria. */
ENTRY(_start)

SECTIONS {
    . = 1M;   /* sotto 1 MB ci sono zone del BIOS e della scheda video */
    kernel_start = .;

    /* I programmi (src/programmi, compilati come prog_*.o) sono esclusi da qui:
     * vanno nella zona dei programmi, in fondo. */
    .text   : ALIGN(4K) { *(.multiboot) *(EXCLUDE_FILE(*prog_*.o) .text*) }   /* la firma per prima */
    .rodata : ALIGN(4K) { *(EXCLUDE_FILE(*prog_*.o) .rodata*) }

    /* fin qui: codice e costanti, che non cambiano mai. Con la paginazione
     * queste pagine diventano di SOLA LETTURA. */
    sola_lettura_fine = ALIGN(4K);

    .data   : ALIGN(4K) { *(EXCLUDE_FILE(*prog_*.o) .data*) }
    .bss    : ALIGN(4K) { *(EXCLUDE_FILE(*prog_*.o) COMMON) *(EXCLUDE_FILE(*prog_*.o) .bss*) }

    /* LA ZONA DEI PROGRAMMI. Due indirizzi diversi per la stessa roba:
     *
     *   indirizzo VIRTUALE (quello scritto nel codice dei programmi): 1 GB in su.
     *     Il linker compila i programmi COME SE girassero li'. E' dove il programma
     *     se li trovera' nella sua mappa.
     *   indirizzo di CARICAMENTO (AT): subito dopo il kernel, in RAM vera. Li' li mette
     *     il caricatore all'avvio, insieme al kernel: e' la copia "originale", che
     *     nessuno esegue. Quando un programma nasce, il kernel ne copia le pagine in
     *     pagine nuove, solo sue, e le mette nella sua mappa al loro indirizzo virtuale.
     *
     * Le costanti PROGRAMMI_INIZIO (paginazione.h) e 0x40000000 qui devono coincidere. */
    utente_carica = ALIGN(4K);
    .utente_codice 0x40000000 : AT(utente_carica) {
        utente_codice_inizio = .;
        *prog_*.o(.text* .rodata*)
    }
    .utente_dati ALIGN(4K) : AT(utente_carica + (ADDR(.utente_dati) - ADDR(.utente_codice))) {
        utente_dati_inizio = .;
        *prog_*.o(.data*)
        utente_bss_inizio = .;            /* da qui: variabili a zero, il kernel le azzera */
        *prog_*.o(COMMON .bss*)
    }
    utente_fine = ALIGN(4K);

    /* primo indirizzo libero dopo il kernel: dopo la copia originale dei programmi */
    kernel_end = ALIGN(utente_carica + (utente_fine - utente_codice_inizio), 4K);
}
