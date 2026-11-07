#!/bin/sh
# Compila il kernel con Docker e lo avvia in QEMU (macOS / Linux).
#   ./run.sh
# Il disco del kernel e' disco.img (16 MB): se manca viene creato vuoto.
# Per ripartire da un disco pulito basta cancellarlo.
set -e
qui="$(cd "$(dirname "$0")" && pwd)"

docker run --rm --platform linux/amd64 -v "$qui:/w" alpine:3.24 sh /w/build.sh

if [ ! -f "$qui/disco.img" ]; then
    dd if=/dev/zero of="$qui/disco.img" bs=1m count=16 2>/dev/null
    echo "disco.img creato (16 MB, tutto a zero)"
fi

# -drive ... if=ide : il file disco.img diventa il primo disco ATA della macchina
# zoom-to-fit       : la finestra si puo' ingrandire
qemu-system-i386 \
    -kernel "$qui/build/kernel" \
    -drive file="$qui/disco.img",format=raw,if=ide,index=0 \
    -display cocoa,zoom-to-fit=on
