# Il primissimo codice del kernel sul PC (x86-64).
#
# Il caricatore UEFI (caricatore/) ha messo il kernel in memoria agli indirizzi scritti
# nell'ELF (linker.ld) e salta qui, a _start, gia' a 64 bit, con le interruzioni spente.
# In RDI c'e' il puntatore alla CONSEGNA (comune/consegna.h): lo schermo, la mappa
# della memoria, le tabelle ACPI.
#
# Tutto quello che abbiamo intorno e' ancora del firmware e del caricatore: la pila, le
# tabelle delle pagine, la GDT, la IDT, e la consegna stessa. E stanno in memoria che la
# mappa chiama LIBERA: appena il magazziniere comincia a dare pagine, potrebbero essere
# riscritte. Qui si fa il minimo:
#   1. la nostra pila (16 KB nella .bss);
#   2. la consegna copiata in una variabile nostra (consegna, nella .bss);
#   3. via, nel C (kernel_main, in avvio.c), che fa subito le tabelle delle pagine, la
#      GDT e la IDT, prima di toccare qualsiasi pagina libera.
# La .bss e' gia' a zero: l'ha azzerata il caricatore (la parte dei pezzi ELF che non sta
# nel file).

# La grandezza della consegna, in byte: deve essere sizeof(struct consegna) (avvio.c
# lo controlla con _Static_assert).
.set DIM_CONSEGNA, 6208

.section .text.avvio
.global _start
_start:
    cli                          # (lo sono gia': per sicurezza)
    cld                          # le copie (rep movsq) vanno in avanti

    # 1. La pila: cresce verso il basso, quindi si parte dalla cima.
    lea     pila_cima(%rip), %rsp

    # 2. La consegna: RSI = da dove (RDI del caricatore), RDI = dove, RCX = quanti
    #    pezzi da 8 byte. rep movsq copia RCX volte 8 byte da [RSI] a [RDI].
    mov     %rdi, %rsi
    lea     consegna(%rip), %rdi
    mov     $(DIM_CONSEGNA / 8), %rcx
    rep movsq

    # 3. Il C. Se tornasse (non deve), ci si ferma.
    call    kernel_main
1:  cli
    hlt
    jmp     1b

# La consegna, copiata: la legge avvio.c.

.section .bss
.balign 16
.global consegna
consegna:
    .skip   DIM_CONSEGNA

# La pila dell'avvio: 16 KB. Diventa la pila del compito inattivo.
.balign 16
pila_fondo:
    .skip   16384
.global pila_cima
pila_cima:
