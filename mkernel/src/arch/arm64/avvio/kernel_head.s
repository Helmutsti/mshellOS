// Il primissimo codice del kernel su ARM64.
//
// Su ARM non c'e' BIOS e non c'e' GRUB: QEMU (la macchina "virt") e UTM caricano il file
// del kernel in RAM, all'indirizzo scritto nel file (linker.ld), e saltano a _start.
// La CPU e' gia' a 64 bit, al livello EL1 (il "ring 0" di ARM), con la MMU spenta: ogni
// indirizzo e' un indirizzo fisico.
//
// Qui si fa solo il minimo per poter chiamare il C:
//   1. se ci sono piu' CPU, lavora solo la prima: le altre aspettano;
//   2. una pila (il registro sp);
//   3. la zona .bss a zero (le variabili globali senza valore: il C le vuole a 0);
//   4. via, nel C.
// Su x86 era kernel_head.s con la firma Multiboot; qui la firma non serve.

.section .text.avvio
.global _start
_start:
    // 1. Chi sono? MPIDR_EL1 dice il numero di questa CPU (negli ultimi bit). Solo la 0
    //    prosegue; le altre si fermano (wfe: "aspetta un evento") per sempre.
    mrs     x1, mpidr_el1
    and     x1, x1, #0xFF
    cbnz    x1, ferma

    // 2. La pila: cresce verso il basso, quindi si parte dalla cima.
    //    A EL1 ci sono due pile: SP_EL0 (dei programmi) e SP_EL1 (del kernel). Si sceglie
    //    SP_EL1 (spsel = 1): e' quella che la CPU usa quando arriva un'eccezione.
    msr     spsel, #1
    ldr     x1, =pila_cima
    mov     sp, x1

    // 3. La .bss a zero, 8 byte alla volta (linker.ld la allinea a 8).
    //    xzr e' il "registro zero": vale sempre 0.
    ldr     x1, =bss_inizio
    ldr     x2, =bss_fine
1:  cmp     x1, x2
    b.hs    2f
    str     xzr, [x1], #8          // scrive 0 in [x1], poi x1 += 8
    b       1b
2:
    // 4. Il C. Se tornasse (non deve), ci si ferma.
    bl      kernel_main

ferma:
    wfe
    b       ferma

// La pila dell'avvio: 16 KB nella .bss. Su ARM64 sp deve essere sempre multiplo di 16.
.section .bss
.balign 16
pila_fondo:
    .space  16384
.global pila_cima
pila_cima:
