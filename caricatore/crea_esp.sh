#!/bin/sh
# Prepara il disco di avvio UEFI: una partizione FAT32 (la "partizione di sistema EFI")
# con dentro
#   \EFI\BOOT\BOOTX64.EFI   il caricatore (il posto dove ogni firmware UEFI guarda)
#   \mkernel\kernel         il kernel (ELF a 64 bit), che il caricatore legge
# Gira dentro il container alpine (come build.sh):
#   sh caricatore/crea_esp.sh <cartella_uscita> <kernel_elf> <esp.img>
# <cartella_uscita> e' quella passata a compila.sh (dove sta BOOTX64.EFI).
#
# L'immagine e' la sola partizione, senza tabella delle partizioni ("superfloppy"):
# il firmware la riconosce lo stesso, e per QEMU basta. mtools scrive nella FAT senza
# montarla (mformat la crea, mmd fa le cartelle, mcopy copia i file).
set -e
cd /w
OUT=${1:?uso: crea_esp.sh <cartella_uscita> <kernel_elf> <esp.img>}
KERNEL=${2:?manca il kernel}
ESP=${3:?manca il nome dell immagine}
command -v mformat >/dev/null || apk add --no-cache mtools >/dev/null

# 64 MB: la FAT32 vuole almeno ~33 MB; lo spazio in piu' resta per kernel piu' grandi
rm -f "$ESP"
dd if=/dev/zero of="$ESP" bs=1M count=64 2>/dev/null
mformat -i "$ESP" -F -v MKERNEL ::
mmd -i "$ESP" ::/EFI ::/EFI/BOOT ::/mkernel
mcopy -i "$ESP" "$OUT/BOOTX64.EFI" ::/EFI/BOOT/BOOTX64.EFI
mcopy -i "$ESP" "$KERNEL" ::/mkernel/kernel
echo "$ESP pronto"
