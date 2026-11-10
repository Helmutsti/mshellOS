// LA TABELLA DEI VETTORI: dove salta la CPU quando succede qualcosa.
//
// Su x86 la IDT e' un elenco di INDIRIZZI. Su ARM e' un blocco di CODICE: 16 caselle da
// 128 byte, e la CPU salta direttamente dentro la casella giusta. L'indirizzo del blocco
// va nel registro VBAR_EL1 (allineato a 2 KB). Le caselle, a gruppi di quattro:
//
//                                       sincrona  IRQ    FIQ    SError
//   dal kernel, con la pila SP_EL0       0x000    0x080  0x100  0x180   (non la usiamo)
//   dal kernel, con la pila SP_EL1       0x200    0x280  0x300  0x380   <- il kernel
//   da un programma a 64 bit (EL0)       0x400    0x480  0x500  0x580   <- i programmi
//   da un programma a 32 bit             0x600    0x680  0x700  0x780   (non ci sono)
//
//   SINCRONA: causata dall'istruzione stessa (un errore, o svc: la chiamata di sistema)
//   IRQ:      un'interruzione normale, dal centralino (GIC)
//   FIQ:      un'interruzione "veloce" (non la usiamo)
//   SError:   un errore di sistema (per esempio, la memoria che risponde male)
//
// Ogni casella fa la stessa cosa: fa posto sulla pila, salva x0 e x1, mette in x1 il suo
// numero (0-15) e salta a 'comune', che salva il resto e chiama il C:
//     struct registri *eccezione(struct registri *r, unsigned long tipo)
// Il C restituisce i registri DA RIPRENDERE: di solito gli stessi, ma lo scheduler potra'
// dare quelli di un altro compito (come su x86, dove restituisce un altro esp).

.equ DIM_REGISTRI, 272          // struct registri: x0-x30, elr, spsr, sp_el0

.macro CASELLA numero
.balign 0x80
    sub     sp, sp, #DIM_REGISTRI
    stp     x0, x1, [sp, #0]
    mov     x1, #\numero
    b       comune
.endm

.section .text
.balign 0x800
.global tabella_vettori
tabella_vettori:
    CASELLA 0
    CASELLA 1
    CASELLA 2
    CASELLA 3
    CASELLA 4
    CASELLA 5
    CASELLA 6
    CASELLA 7
    CASELLA 8
    CASELLA 9
    CASELLA 10
    CASELLA 11
    CASELLA 12
    CASELLA 13
    CASELLA 14
    CASELLA 15

comune:
    // il resto dei registri normali (x0 e x1 li ha gia' salvati la casella)
    stp     x2, x3, [sp, #16]
    stp     x4, x5, [sp, #32]
    stp     x6, x7, [sp, #48]
    stp     x8, x9, [sp, #64]
    stp     x10, x11, [sp, #80]
    stp     x12, x13, [sp, #96]
    stp     x14, x15, [sp, #112]
    stp     x16, x17, [sp, #128]
    stp     x18, x19, [sp, #144]
    stp     x20, x21, [sp, #160]
    stp     x22, x23, [sp, #176]
    stp     x24, x25, [sp, #192]
    stp     x26, x27, [sp, #208]
    stp     x28, x29, [sp, #224]
    str     x30, [sp, #240]
    // dove riprendere, lo stato di prima, la pila dei programmi
    mrs     x2, elr_el1
    mrs     x3, spsr_el1
    stp     x2, x3, [sp, #248]
    mrs     x2, sp_el0
    str     x2, [sp, #264]

    mov     x0, sp                  // primo argomento: i registri salvati
    bl      eccezione               // (x1, il secondo, e' il numero della casella)
    mov     sp, x0                  // i registri da riprendere (forse di un altro)

    ldp     x2, x3, [sp, #248]
    msr     elr_el1, x2
    msr     spsr_el1, x3
    ldr     x2, [sp, #264]
    msr     sp_el0, x2
    ldp     x0, x1, [sp, #0]
    ldp     x2, x3, [sp, #16]
    ldp     x4, x5, [sp, #32]
    ldp     x6, x7, [sp, #48]
    ldp     x8, x9, [sp, #64]
    ldp     x10, x11, [sp, #80]
    ldp     x12, x13, [sp, #96]
    ldp     x14, x15, [sp, #112]
    ldp     x16, x17, [sp, #128]
    ldp     x18, x19, [sp, #144]
    ldp     x20, x21, [sp, #160]
    ldp     x22, x23, [sp, #176]
    ldp     x24, x25, [sp, #192]
    ldp     x26, x27, [sp, #208]
    ldp     x28, x29, [sp, #224]
    ldr     x30, [sp, #240]
    add     sp, sp, #DIM_REGISTRI
    eret                            // torna: a elr, con lo stato di spsr (l'iret di ARM)
