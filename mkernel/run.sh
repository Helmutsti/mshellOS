#!/bin/sh
# Compila il kernel, i programmi e il disco con Docker e li avvia in QEMU (macOS / Linux).
#   ./run.sh
# Il disco e' build/disco.img: build.sh lo rifa' a ogni compilazione, con dentro /init.
set -e
qui="$(cd "$(dirname "$0")" && pwd)"

docker run --rm --platform linux/amd64 -v "$qui:/w" alpine:3.24 sh /w/build.sh

# -drive ... if=ide : il file disco.img diventa il primo disco ATA della macchina
# zoom-to-fit       : la finestra si puo' ingrandire
qemu-system-i386 \
    -kernel "$qui/build/kernel" \
    -drive file="$qui/build/disco.img",format=raw,if=ide,index=0 \
    -display cocoa,zoom-to-fit=on
