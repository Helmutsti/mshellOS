# Gli ingressi delle eccezioni (ISR = Interrupt Service Routine).
#
# Quando succede un'eccezione la CPU, da sola:
#   1. mette sulla pila EFLAGS, CS, EIP (dove stava lavorando)
#   2. per alcune eccezioni mette anche un "codice di errore"
#   3. salta all'indirizzo scritto nella riga della IDT
#
# Il C non sa partire cosi', ne' sa tornare con iret. Questi ingressi fanno da ponte:
# rendono la pila sempre uguale (codice di errore finto se la CPU non l'ha messo),
# ci aggiungono il numero dell'eccezione, e passano al pezzo comune.

# Eccezioni per cui la CPU NON mette un codice di errore: lo mettiamo noi (0).
.macro SENZA_ERRORE n
isr\n:
    push $0              # codice di errore finto
    push $\n             # numero dell'eccezione
    jmp isr_comune
.endm

# Eccezioni per cui la CPU mette gia' un codice di errore.
.macro CON_ERRORE n
isr\n:
    push $\n             # numero dell'eccezione
    jmp isr_comune
.endm

.section .text

SENZA_ERRORE 0     # divisione per zero
SENZA_ERRORE 1     # debug
SENZA_ERRORE 2     # NMI
SENZA_ERRORE 3     # breakpoint
SENZA_ERRORE 4     # overflow
SENZA_ERRORE 5     # fuori dai limiti
SENZA_ERRORE 6     # istruzione non valida
SENZA_ERRORE 7     # coprocessore assente
CON_ERRORE   8     # doppio errore
SENZA_ERRORE 9     # (storico)
CON_ERRORE   10    # TSS non valido
CON_ERRORE   11    # segmento assente
CON_ERRORE   12    # errore della pila
CON_ERRORE   13    # protezione generale
CON_ERRORE   14    # page fault
SENZA_ERRORE 15    # (riservata)
SENZA_ERRORE 16    # errore x87
CON_ERRORE   17    # allineamento
SENZA_ERRORE 18    # machine check
SENZA_ERRORE 19    # errore SSE
SENZA_ERRORE 20    # virtualizzazione
CON_ERRORE   21    # protezione del controllo
SENZA_ERRORE 22
SENZA_ERRORE 23
SENZA_ERRORE 24
SENZA_ERRORE 25
SENZA_ERRORE 26
SENZA_ERRORE 27
SENZA_ERRORE 28
CON_ERRORE   29
CON_ERRORE   30
SENZA_ERRORE 31

# Le interruzioni hardware (IRQ 0-15), che il PIC manda alle righe 32-47.
# Non hanno codice di errore: si comportano come SENZA_ERRORE.
SENZA_ERRORE 32    # IRQ 0  timer
SENZA_ERRORE 33    # IRQ 1  tastiera
SENZA_ERRORE 34    # IRQ 2  (collegamento al PIC secondario)
SENZA_ERRORE 35    # IRQ 3  porta seriale 2
SENZA_ERRORE 36    # IRQ 4  porta seriale 1
SENZA_ERRORE 37    # IRQ 5
SENZA_ERRORE 38    # IRQ 6  floppy
SENZA_ERRORE 39    # IRQ 7  porta parallela
SENZA_ERRORE 40    # IRQ 8  orologio (RTC)
SENZA_ERRORE 41    # IRQ 9
SENZA_ERRORE 42    # IRQ 10
SENZA_ERRORE 43    # IRQ 11
SENZA_ERRORE 44    # IRQ 12 mouse
SENZA_ERRORE 45    # IRQ 13 coprocessore
SENZA_ERRORE 46    # IRQ 14 disco principale
SENZA_ERRORE 47    # IRQ 15 disco secondario

# La porta delle chiamate di sistema: l'unica che un programma in ring 3 puo' usare
# (vedi chiamate.h). Se arriva dal ring 3, la CPU ha gia' cambiato pila: siamo sulla
# pila del kernel del programma, presa dalla TSS.
.global isr128
SENZA_ERRORE 128   # 0x80

# L'interruzione software con cui un compito cede il turno (cedi, in compiti.c).
.global isr129
SENZA_ERRORE 129   # 0x81

# Il pezzo comune: salva tutto, chiama il C, rimette tutto, torna.
isr_comune:
    pusha                    # salva EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI
    mov %ds, %ax
    push %eax                # salva il segmento dei dati

    mov $0x10, %ax           # usa i segmenti dati del kernel (riga 2 della GDT)
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    cld                      # il C si aspetta la "direzione" in avanti
    push %esp                # argomento: dove stanno i registri salvati
    call interruzione        # il gestore in C (idt.c)

    # IL CAMBIO DI CONTESTO. interruzione() restituisce in EAX dove ripescare i
    # registri. Di solito e' lo stesso posto (si torna al compito interrotto).
    # Se lo scheduler ha scelto un altro compito, e' la cima della SUA pila:
    # da qui in poi pop e iret lavorano sulla pila dell'altro compito.
    mov %eax, %esp

    pop %eax                 # rimette il segmento dei dati
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    popa                     # rimette i registri
    add $8, %esp             # toglie numero e codice di errore
    iret                     # torna dove la CPU era stata interrotta

# L'elenco degli indirizzi dei 48 ingressi, per riempire la IDT dal C.
.section .rodata
.global isr_tabella
isr_tabella:
    .long isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7      # eccezioni
    .long isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15
    .long isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
    .long isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    .long isr32, isr33, isr34, isr35, isr36, isr37, isr38, isr39     # IRQ 0-7
    .long isr40, isr41, isr42, isr43, isr44, isr45, isr46, isr47     # IRQ 8-15
