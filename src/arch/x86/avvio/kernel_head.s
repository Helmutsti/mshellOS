# Punto di ingresso del kernel.
# Il caricatore di avvio (GRUB) cerca la firma Multiboot,
# carica il file in memoria e salta a _start.

.set MAGIC,    0x1BADB002          # firma Multiboot
.set FLAGS,    7                   # bit 0: moduli allineati, bit 1: mappa della memoria,
                                   # bit 2: accendimi la modalita' pixel (campi in fondo)
.set CHECKSUM, -(MAGIC + FLAGS)    # MAGIC + FLAGS + CHECKSUM deve fare 0

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM
.long 0, 0, 0, 0, 0                # indirizzi di caricamento: non servono (il bit 16 e' spento)
.long 0                            # modalita' video: 0 = pixel (1 sarebbe testo)
.long 1280, 800, 32                # larghezza, altezza, bit per pixel

# Lo stack: 16 KB riservati, il C ne ha bisogno per funzionare.
.section .bss
.align 16
.global stack_top                # serve allo scheduler: e' la pila dell'inattivo
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
