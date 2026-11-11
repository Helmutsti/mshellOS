#!/bin/sh
# Compila il caricatore UEFI: <cartella_uscita>/BOOTX64.EFI
# Gira dentro il container alpine, con il repo montato in /w (come build.sh):
#   sh caricatore/compila.sh build
#
# Il formato. Il firmware UEFI vuole programmi PE32+ (il formato degli .exe di Windows),
# "applicazione EFI". Per questo si compila per x86_64-windows e si lega con lld-link
# (il linker di LLVM per il formato Microsoft), senza nessuna libreria.
#   -ffreestanding      : niente libreria standard
#   -fshort-wchar       : L"..." a 16 bit, come i testi UEFI
#   -mno-red-zone       : il firmware puo' interromperci in ogni momento e scrivere
#                         sotto la pila: niente "zona rossa"
#   -fno-stack-protector, -mno-stack-arg-probe : niente chiamate a funzioni che non ci sono
#                         (__security_cookie, __chkstk)
#   -subsystem:efi_application -entry:efi_main : il tipo di programma e da dove parte
set -e
cd /w
OUT=${1:?uso: compila.sh <cartella_uscita>}
command -v clang >/dev/null && command -v lld-link >/dev/null ||
    apk add --no-cache clang lld >/dev/null
mkdir -p "$OUT/caricatore.o"

clang --target=x86_64-unknown-windows -ffreestanding -fshort-wchar -mno-red-zone \
    -fno-stack-protector -mno-stack-arg-probe -O2 -Wall -Wextra \
    -Icomune -c caricatore/caricatore.c -o "$OUT/caricatore.o/caricatore.o"
lld-link -subsystem:efi_application -entry:efi_main -nodefaultlib \
    -out:"$OUT/BOOTX64.EFI" "$OUT/caricatore.o/caricatore.o"
echo "$OUT/BOOTX64.EFI pronto"
