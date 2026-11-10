#!/bin/sh
# Compila tutto con Docker e avvia mkernel in QEMU (macOS / Linux).
#   ./run.sh
#   build/mkernel.iso : l'immagine avviabile, come un CD: GRUB accende la modalita'
#                       pixel (1280x800) e carica il kernel
#   build/disco.img   : il disco con il file system e i programmi (/Sistema/init e gli altri)
set -e
qui="$(cd "$(dirname "$0")" && pwd)"

docker run --rm --platform linux/amd64 -v "$qui:/w" alpine:3.24 sh /w/build.sh

# -cdrom + -boot d  : si parte dal "CD" con GRUB
# -drive ... if=ide : il disco.img e' il primo disco ATA, quello che legge il kernel
# zoom-to-fit       : la finestra si puo' ingrandire
qemu-system-i386 \
    -cdrom "$qui/build/mkernel.iso" -boot d \
    -drive file="$qui/build/disco.img",format=raw,if=ide,index=0 \
    -display cocoa,zoom-to-fit=on
