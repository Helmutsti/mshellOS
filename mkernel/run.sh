#!/bin/sh
# Compila tutto con Docker e avvia mkernel in QEMU (macOS / Linux).
#   ./run.sh         oppure  ./run.sh x86   : il PC, con GRUB, lo schermo e il disco
#   ./run.sh arm64                          : ARM a 64 bit, sulla seriale (per ora)
#
# x86:
#   build/mkernel.iso : l'immagine avviabile, come un CD: GRUB accende la modalita'
#                       pixel (1280x800) e carica il kernel
#   build/disco.img   : il disco con il file system e i programmi (/Sistema/init e gli altri)
# arm64:
#   build/arm64/kernel : il kernel, che QEMU carica in RAM e avvia direttamente
set -e
qui="$(cd "$(dirname "$0")" && pwd)"
cpu="${1:-x86}"

docker run --rm --platform linux/amd64 -v "$qui:/w" alpine:3.24 sh /w/build.sh "$cpu"

if [ "$cpu" = arm64 ]; then
    # -M virt     : la macchina ARM "virtuale" di QEMU (seriale a 0x09000000, RAM da 0x40000000)
    # -kernel     : niente BIOS e niente GRUB: QEMU mette il kernel in RAM e ci salta
    # -nographic  : niente finestra: la seriale e' questo terminale (per uscire: ctrl-a, poi x)
    # Su un Mac con chip Apple si usa la virtualizzazione (hvf): il kernel gira sulla CPU
    # vera, senza essere emulato. Altrove si emula un Cortex-A72.
    if [ "$(uname -s)" = Darwin ] && [ "$(sysctl -n kern.hv_support 2>/dev/null)" = 1 ] &&
       [ "$(uname -m)" = arm64 ]; then
        cpu_qemu="-accel hvf -cpu host"
    else
        cpu_qemu="-cpu cortex-a72"
    fi
    exec qemu-system-aarch64 -M virt $cpu_qemu -nographic -kernel "$qui/build/arm64/kernel"
fi

# -cdrom + -boot d  : si parte dal "CD" con GRUB
# -drive ... if=ide : il disco.img e' il primo disco ATA, quello che legge il kernel
# -rtc base=localtime : l'orologio del PC virtuale segna l'ora di casa
# zoom-to-fit       : la finestra si puo' ingrandire
qemu-system-i386 \
    -cdrom "$qui/build/mkernel.iso" -boot d -rtc base=localtime \
    -drive file="$qui/build/disco.img",format=raw,if=ide,index=0 \
    -display cocoa,zoom-to-fit=on
