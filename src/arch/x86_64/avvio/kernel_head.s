# Il primissimo codice del kernel sul PC (x86-64).
#
# Il caricatore UEFI (caricatore/) ha messo il kernel in memoria agli indirizzi FISICI
# scritti nell'ELF (linker.ld: da 2 MB) e salta qui, a _start, gia' a 64 bit, con le
# interruzioni spente. In RDI c'e' il puntatore alla CONSEGNA (comune/consegna.h): lo
# schermo, la mappa della memoria, le tabelle ACPI.
#
# DOVE SIAMO. Il kernel e' legato IN ALTO, a KERNEL_IMMAGINE + 2 MB (la pianta e' in
# src/memoria/pagine.h), ma adesso gira dove l'ha messo il caricatore, a 2 MB: le
# tabelle delle pagine sono ancora quelle del firmware (virtuale = fisico). Finche' non
# si salta in alto, qui si usano solo indirizzi RELATIVI all'istruzione (xxx(%rip)):
# valgono ovunque il codice stia davvero.
#
# Tutto quello che abbiamo intorno e' ancora del firmware e del caricatore: la pila, le
# tabelle delle pagine, la GDT, la IDT, e la consegna stessa. E stanno in memoria che la
# mappa chiama LIBERA: appena il magazziniere comincia a dare pagine, potrebbero essere
# riscritte. Qui si fa il minimo:
#   1. la consegna copiata in una variabile nostra (consegna, nella .bss), e ci si annota
#      dove sta il kernel nella RAM (kernel_fisico, per pagine.h);
#   2. delle tabelle delle pagine PROVVISORIE: il primo GB di RAM visto due volte, dov'e'
#      (per l'istruzione dopo il cambio di tabelle) e in alto (dove il kernel e' legato);
#   3. il salto in alto, la nostra pila (16 KB nella .bss), e via nel C (kernel_main, in
#      avvio.c), che fa subito le tabelle vere (senza gli indirizzi bassi), la GDT e la
#      IDT, prima di toccare qualsiasi pagina libera.
# La .bss e' gia' a zero: l'ha azzerata il caricatore (la parte dei pezzi ELF che non sta
# nel file).

# La grandezza della consegna, in byte: deve essere sizeof(struct consegna) (avvio.c
# lo controlla con _Static_assert).
.set DIM_CONSEGNA, 6208

# Una riga di tabella: presente (bit 0) e scrivibile (bit 1); 0x80 = pagina da 2 MB.
.set RIGA, 0x3
.set GRANDE, 0x83

.section .text.avvio
.global _start
_start:
    cli                          # (lo sono gia': per sicurezza)
    cld                          # le copie (rep movsq) vanno in avanti

    # 1. La consegna: RSI = da dove (RDI del caricatore), RDI = dove, RCX = quanti
    #    pezzi da 8 byte. rep movsq copia RCX volte 8 byte da [RSI] a [RDI].
    mov     %rdi, %rsi
    lea     consegna(%rip), %rdi
    mov     $(DIM_CONSEGNA / 8), %rcx
    rep movsq
    #    Dove sta il kernel nella RAM: qui, prima del salto, l'indirizzo di kernel_start
    #    calcolato da RIP e' proprio quello fisico.
    lea     kernel_start(%rip), %rax
    mov     %rax, kernel_fisico(%rip)

    # 2. Le tabelle provvisorie (nella .bss: a zero = righe vuote).
    #    La PD: 512 pagine da 2 MB, il primo GB fisico (riga i -> i * 2 MB).
    lea     avvio_pd(%rip), %rdi
    mov     $GRANDE, %eax
    xor     %ecx, %ecx
1:  mov     %rax, (%rdi,%rcx,8)
    add     $0x200000, %rax
    inc     %ecx
    cmp     $512, %ecx
    jb      1b
    #    La stessa PD da due PDPT: in basso (riga 0: da 0) e in alto (riga 510: da
    #    KERNEL_IMMAGINE, 0xFFFFFFFF80000000, cioe' riga 511 della PML4, riga 510 della
    #    PDPT). Nelle righe vanno gli indirizzi fisici: quelli che da' RIP adesso.
    lea     avvio_pd(%rip), %rax
    or      $RIGA, %rax
    lea     avvio_pdpt_basso(%rip), %rdx
    mov     %rax, (%rdx)
    lea     avvio_pdpt_alto(%rip), %rdx
    mov     %rax, 510*8(%rdx)
    lea     avvio_pml4(%rip), %rdx
    lea     avvio_pdpt_basso(%rip), %rax
    or      $RIGA, %rax
    mov     %rax, (%rdx)
    lea     avvio_pdpt_alto(%rip), %rax
    or      $RIGA, %rax
    mov     %rax, 511*8(%rdx)
    mov     %rdx, %cr3               # da qui le tabelle sono le nostre (provvisorie)

    # 3. In alto: un salto a un indirizzo intero (64 bit), non relativo.
    movabs  $in_alto, %rax
    jmp     *%rax
in_alto:
    lea     pila_cima(%rip), %rsp    # ora RIP e' in alto: la pila pure
    call    kernel_main              # se tornasse (non deve), ci si ferma
1:  cli
    hlt
    jmp     1b

# Dove sta il kernel nella RAM (vedi pagine.h).
.section .data
.balign 8
.global kernel_fisico
kernel_fisico:
    .quad   0

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

# Le tabelle provvisorie: una pagina ciascuna. Dopo mmu_prepara non servono piu'.
.balign 4096
avvio_pml4:
    .skip   4096
avvio_pdpt_basso:
    .skip   4096
avvio_pdpt_alto:
    .skip   4096
avvio_pd:
    .skip   4096
