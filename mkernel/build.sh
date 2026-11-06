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
CFLAGS="--target=i686-elf -ffreestanding -O2 -Wall -Wextra"

clang --target=i686-elf -c src/kernel_head.s -o build/kernel_head.o
clang $CFLAGS -c src/kernel.c -o build/kernel.o
ld.lld -T src/linker.ld -o build/kernel build/kernel_head.o build/kernel.o

echo "build/kernel pronto"
