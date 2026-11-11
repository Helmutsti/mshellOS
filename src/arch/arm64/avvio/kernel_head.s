// Il primissimo codice del kernel su ARM64.
//
// Su ARM non c'e' BIOS e non c'e' GRUB: chi ci avvia (QEMU, UTM, U-Boot, il firmware del
// Raspberry Pi) carica il kernel in RAM e salta all'inizio, con la CPU gia' a 64 bit e la
// MMU spenta. In x0 lascia l'indirizzo del DEVICE TREE, la descrizione della macchina.
//
// IL FORMATO "IMAGE". Il kernel e' un file grezzo (build/arm64/kernel.img: i byte cosi'
// come vanno in memoria) che comincia con un'intestazione di 64 byte, la stessa di Linux:
// e' cosi' che QEMU, U-Boot e il Raspberry Pi riconoscono un kernel ARM64 e sanno dove
// metterlo. La prima voce e' un'istruzione (salta oltre l'intestazione); poi a che
// distanza dall'inizio della RAM va messo (0x80000), quanto e' grande, e la firma "ARM\x64".
//
// Qui si fa solo il minimo per poter chiamare il C:
//   1. se ci sono piu' CPU, lavora solo la prima: le altre aspettano;
//   2. ci si annota il device tree (x0), prima di usare x0 per altro;
//   3. se si e' a EL2 (l'hypervisor: succede sulle macchine vere), si scende a EL1;
//   4. una pila (il registro sp);
//   5. la zona .bss a zero (le variabili globali senza valore: il C le vuole a 0);
//   6. via, nel C.
// Su x86 era kernel_head.s con la firma Multiboot; qui la firma non serve.

.section .text.avvio
.global _start
_start:
    b       inizio                  // code0: si salta oltre l'intestazione
    .long   0                       // code1
    .quad   0x80000                 // text_offset: a 512 KB dall'inizio della RAM
    .quad   kernel_dimensione       // image_size: quanto occupa in memoria (con la .bss)
    .quad   0x2                     // flags: little-endian, pagine da 4 KB
    .quad   0, 0, 0                 // riservati
    .ascii  "ARM\x64"               // la firma
    .long   0                       // riservato

inizio:
    // 1. Chi sono? MPIDR_EL1 dice il numero di questa CPU (negli ultimi bit). Solo la 0
    //    prosegue; le altre si fermano (wfe: "aspetta un evento") per sempre.
    mrs     x1, mpidr_el1
    and     x1, x1, #0xFF
    cbnz    x1, ferma

    // 2. Il device tree: in x0. Si scrive in avvio_albero (nella .data, non nella .bss,
    //    che fra poco si azzera).
    ldr     x1, =avvio_albero
    str     x0, [x1]

    // 3. A che livello siamo? CurrentEL, bit 3-2.
    mrs     x1, CurrentEL
    lsr     x1, x1, #2
    ldr     x2, =avvio_livello
    str     x1, [x2]                // ci si annota da dove si e' partiti (per raccontarlo)
    cmp     x1, #2
    b.ne    a_el1

    // DA EL2 A EL1. Su ARM si scende di livello in un modo solo: "tornando" da
    // un'eccezione (eret) verso il livello di sotto. Prima si prepara EL1:
    //   HCR_EL2.RW (bit 31) = 1   : anche EL1 lavora a 64 bit
    //   CNTHCTL_EL2 bit 0 e 1 = 1 : EL1 puo' usare il timer generico e il contatore
    //   CNTVOFF_EL2 = 0           : il timer "virtuale" segna la stessa ora di quello vero
    //   ICC_SRE_EL2 = 0xF         : (solo se c'e' il GIC 3) EL1 puo' usare i registri del
    //                               GIC; se no, al primo uso la CPU darebbe errore
    //   SCTLR_EL1 = solo i bit "sempre a 1": MMU e cache spente, come a EL2
    //   SPSR_EL2 = 0x3C5          : si "torna" a EL1, con la sua pila (EL1h), interruzioni
    //                               spente (DAIF)
    //   ELR_EL2 = a_el1           : e si riparte da li'
    mov     x1, #(1 << 31)
    msr     hcr_el2, x1
    mrs     x1, cnthctl_el2
    orr     x1, x1, #3
    msr     cnthctl_el2, x1
    msr     cntvoff_el2, xzr
    mrs     x1, id_aa64pfr0_el1     // c'e' il GIC 3 nella CPU? (bit 27-24)
    ubfx    x1, x1, #24, #4
    cbz     x1, 1f
    mov     x1, #0xF
    msr     icc_sre_el2, x1
1:  ldr     x1, =0x30D00800
    msr     sctlr_el1, x1
    mov     x1, #0x3C5
    msr     spsr_el2, x1
    adr     x1, a_el1
    msr     elr_el2, x1
    isb
    eret

a_el1:
    // 4. La pila: cresce verso il basso, quindi si parte dalla cima.
    //    A EL1 ci sono due pile: SP_EL0 (dei programmi) e SP_EL1 (del kernel). Si sceglie
    //    SP_EL1 (spsel = 1): e' quella che la CPU usa quando arriva un'eccezione.
    msr     spsel, #1
    ldr     x1, =pila_cima
    mov     sp, x1

    // 5. La .bss a zero, 8 byte alla volta (linker.ld la allinea a 8).
    //    xzr e' il "registro zero": vale sempre 0.
    ldr     x1, =bss_inizio
    ldr     x2, =bss_fine
1:  cmp     x1, x2
    b.hs    2f
    str     xzr, [x1], #8          // scrive 0 in [x1], poi x1 += 8
    b       1b
2:
    // 6. Il C. Se tornasse (non deve), ci si ferma.
    bl      kernel_main

ferma:
    wfe
    b       ferma

// Quello che si annota l'avvio, per il C: nella .data, perche' la .bss si azzera dopo.
.section .data
.balign 8
.global avvio_albero
avvio_albero:
    .quad   0                       // l'indirizzo del device tree (x0 all'avvio)
.global avvio_livello
avvio_livello:
    .quad   0                       // il livello a cui si e' partiti (1 o 2)

// La pila dell'avvio: 16 KB nella .bss. Su ARM64 sp deve essere sempre multiplo di 16.
.section .bss
.balign 16
pila_fondo:
    .space  16384
.global pila_cima
pila_cima:
