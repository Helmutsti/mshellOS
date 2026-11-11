/* LA CONSEGNA: il biglietto che il caricatore UEFI (caricatore/) lascia al kernel x86-64.
 *
 * Sul PC il firmware UEFI fa partire il caricatore, gia' a 64 bit. Il caricatore legge
 * il kernel dal disco di avvio, chiede al firmware lo schermo, la mappa della memoria e
 * dove sono le tabelle ACPI, li scrive qui, chiude i servizi del firmware
 * (ExitBootServices) e salta al kernel. Da quel momento il firmware non c'e' piu': tutto
 * quello che il kernel sa della macchina e' scritto in questo biglietto.
 *
 * COME SI SALTA AL KERNEL. Il punto d'ingresso e' quello dell'ELF del kernel, chiamato
 * con la convenzione System V (quella di Linux): il primo argomento, in RDI, e' il
 * puntatore alla consegna. Interruzioni spente. La paginazione e' ancora quella del
 * firmware (mappatura identica: indirizzo = indirizzo fisico); la pila e' ancora
 * quella del caricatore: il kernel deve passare SUBITO alla sua pila e alle sue tabelle.
 * Il kernel e' legato in alto (KERNEL_IMMAGINE, src/memoria/pagine.h) ma messo nella RAM
 * agli indirizzi fisici del suo ELF: il caricatore salta al punto d'ingresso tradotto in
 * fisico, e sara' il kernel a portarsi in alto (kernel_head.s).
 *
 * DOVE STA IL BIGLIETTO. In memoria del caricatore (EfiLoaderData), che nella mappa qui
 * sotto risulta LIBERA: il kernel deve COPIARLO nelle sue variabili prima di usare
 * qualsiasi pagina libera. */
#ifndef CONSEGNA_H
#define CONSEGNA_H

#define CONSEGNA_FIRMA 0x4D4B45524E454C31ULL   /* "MKERNEL1" */

/* I tipi di una zona di memoria. */
#define ZONA_LIBERA      1   /* RAM che il kernel puo' usare (anche quella che il
                              * firmware e il caricatore usavano per se') */
#define ZONA_RISERVATA   2   /* non toccare (firmware, dispositivi, buchi...) */
#define ZONA_ACPI        3   /* tabelle ACPI: si leggono, poi si potranno liberare */
#define ZONA_ACPI_NVS    4   /* memoria del firmware per ACPI: non toccare mai */
#define ZONA_KERNEL      5   /* il kernel caricato: non e' libera, ma e' RAM (conta
                              * nel totale) */

#define MAX_ZONE 256

struct zona_consegna {
    unsigned long long inizio;     /* indirizzo fisico, multiplo di 4096 */
    unsigned long long byte;
    unsigned int tipo;             /* ZONA_... */
    unsigned int riservato;
};

struct consegna {
    unsigned long long firma;      /* CONSEGNA_FIRMA */

    /* Lo schermo (UEFI GOP), gia' in modalita' pixel: 32 bit per pixel. */
    unsigned long long schermo;    /* indirizzo fisico della memoria dei pixel */
    unsigned int larghezza, altezza;
    unsigned int passo;            /* byte per riga (puo' essere piu' di larghezza * 4) */
    unsigned char rosso, verde, blu;   /* di quanti bit e' spostato ogni colore nel pixel
                                        * (es. 16, 8, 0 per BGRX) */
    unsigned char riservato1;

    /* La RAM e il resto, zona per zona, in ordine di indirizzo, senza sovrapposizioni. */
    unsigned int quante_zone;
    unsigned int riservato2;
    struct zona_consegna zone[MAX_ZONE];

    /* La tabella RSDP di ACPI (versione 2 se c'e', se no 1); 0 se non c'e'. */
    unsigned long long acpi_rsdp;

    /* Dove il caricatore ha messo il kernel (anche nella mappa come ZONA_KERNEL). */
    unsigned long long kernel_inizio, kernel_fine;
};

#endif
