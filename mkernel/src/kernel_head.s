# Punto di ingresso del kernel.
# Il caricatore di avvio (qui QEMU) cerca la firma Multiboot,
# carica il file in memoria e salta a _start.

.set MAGIC,    0x1BADB002          # firma Multiboot
.set FLAGS,    3                   # bit 0: moduli allineati, bit 1: mappa della memoria
.set CHECKSUM, -(MAGIC + FLAGS)    # MAGIC + FLAGS + CHECKSUM deve fare 0

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

# Lo stack: 16 KB riservati, il C ne ha bisogno per funzionare.
.section .bss
.align 16
stack_bottom:
.skip 16384
stack_top:

.section .text
.global _start
_start:
    mov $stack_top, %esp     # lo stack cresce verso il basso: si parte dalla cima
    push %ebx                # 2o argomento: puntatore alle info Multiboot
    push %eax                # 1o argomento: numero magico del bootloader
    call kernel_main         # salto al codice C (kernel_main(magic, info))
    cli                      # se il C ritorna: spegni le interruzioni...
1:  hlt                      # ...e ferma il processore
    jmp 1b
