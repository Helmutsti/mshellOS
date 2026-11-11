#!/bin/sh
# Compila tutto con Docker e avvia mkernel in QEMU (macOS / Linux).
#   ./run.sh         oppure  ./run.sh x86   : il PC, con GRUB, lo schermo e il disco
#   ./run.sh arm64                          : ARM a 64 bit (la seriale e' questo terminale)
#
# x86:
#   build/mkernel.iso : l'immagine avviabile, come un CD: GRUB accende la modalita'
#                       pixel (1280x800) e carica il kernel
#   build/disco.img   : il disco con il file system e i programmi (/Sistema/init e gli altri)
# arm64:
#   build/arm64/kernel.img : il kernel in formato "Image" (come Linux), che QEMU carica in
#                            RAM e avvia direttamente, passandogli il device tree
#   build/arm64/disco.img  : il disco, con i programmi compilati per ARM64
#
# I dispositivi virtio (la "libreria" di driver di mkernel li sa usare su tutte e due le CPU):
#   virtio-keyboard-pci, virtio-tablet-pci : tastiera e tavoletta. La tavoletta dice DOVE
#       e' il puntatore (non di quanto si e' mosso): il mouse non va catturato, e il
#       puntatore di mkernel e' sempre sotto quello del Mac
#   virtio-blk-pci : il disco (su ARM; su x86 il disco e' quello IDE di sempre)
set -e
qui="$(cd "$(dirname "$0")" && pwd)"
cpu="${1:-x86}"

docker run --rm --platform linux/amd64 -v "$qui:/w" alpine:3.24 sh /w/build.sh "$cpu"

if [ "$cpu" = arm64 ]; then
    # -M virt     : la macchina ARM "virtuale" di QEMU (seriale a 0x09000000, RAM da 0x40000000)
    # -kernel     : niente BIOS e niente GRUB: QEMU mette il kernel in RAM e ci salta
    # -serial stdio : la seriale e' questo terminale; -device ramfb: lo schermo
    # Su un Mac con chip Apple si usa la virtualizzazione (hvf): il kernel gira sulla CPU
    # vera, senza essere emulato. Altrove si emula un Cortex-A72.
    if [ "$(uname -s)" = Darwin ] && [ "$(sysctl -n kern.hv_support 2>/dev/null)" = 1 ] &&
       [ "$(uname -m)" = arm64 ]; then
        cpu_qemu="-accel hvf -cpu host"
    else
        cpu_qemu="-cpu cortex-a72"
    fi
    exec qemu-system-aarch64 -M virt $cpu_qemu -m 512 -rtc base=localtime \
        -kernel "$qui/build/arm64/kernel.img" -serial stdio \
        -device ramfb -display cocoa,zoom-to-fit=on \
        -drive if=none,id=disco,file="$qui/build/arm64/disco.img",format=raw \
        -device virtio-blk-pci,drive=disco \
        -device virtio-keyboard-pci -device virtio-tablet-pci
fi

# -cdrom + -boot d  : si parte dal "CD" con GRUB
# -drive ... if=ide : il disco.img e' il primo disco ATA, quello che legge il kernel
# -rtc base=localtime : l'orologio del PC virtuale segna l'ora di casa
# zoom-to-fit       : la finestra si puo' ingrandire
qemu-system-i386 \
    -cdrom "$qui/build/mkernel.iso" -boot d -rtc base=localtime \
    -drive file="$qui/build/disco.img",format=raw,if=ide,index=0 \
    -device virtio-keyboard-pci -device virtio-tablet-pci \
    -display cocoa,zoom-to-fit=on
