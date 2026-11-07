#!/bin/sh
# Compila il kernel. Gira dentro un container Docker alpine con la cartella montata in /w.
#   src/    il codice
#   build/  il compilato (si puo' cancellare, viene rifatto)
set -e
cd /w
apk add --no-cache clang lld >/dev/null
mkdir -p build

# --target=i686-elf : codice x86 a 32 bit per una macchina nuda (nessun sistema sotto)
# -ffreestanding    : niente libreria standard, niente main()
# -mno-sse ...      : niente istruzioni SSE/MMX. Il compilatore le userebbe per velocizzare
#                     i cicli, ma la CPU le accetta solo dopo che il kernel le ha attivate:
#                     senza questo, la prima che incontra manda in crash la macchina.
CFLAGS="--target=i686-elf -ffreestanding -O2 -Wall -Wextra -mno-sse -mno-sse2 -mno-mmx"

rm -f build/*.o   # si riparte puliti: niente .o vecchi di file cancellati

# assembla ogni .s e compila ogni .c di src/, poi li lega insieme
for f in src/*.s; do
    clang --target=i686-elf -c "$f" -o "build/$(basename "$f" .s).o"
done
for f in src/*.c; do
    clang $CFLAGS -c "$f" -o "build/$(basename "$f" .c).o"
done
# l'ordine dei .o non conta: e' linker.ld a mettere la firma Multiboot per prima
ld.lld -T src/linker.ld -o build/kernel build/*.o

echo "build/kernel pronto"
