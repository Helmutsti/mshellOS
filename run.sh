#!/bin/sh
# Compila tutto con Docker e avvia mshell in QEMU (macOS / Linux).
#   ./run.sh         oppure  ./run.sh x86_64 : il PC a 64 bit, avviato da UEFI
#   ./run.sh arm64                           : ARM a 64 bit (la seriale e' questo terminale)
#
# x86_64:
#   build/x86_64/esp.img   : il disco di avvio (FAT, come la "partizione EFI" di un PC):
#                            dentro \EFI\BOOT\BOOTX64.EFI, il kernel Linux con l'archivio
#                            di avvio e la riga di comando (una UKI)
#   build/x86_64/disco.img : il disco del sistema (ext4 "mshell": /Sistema/init e gli altri)
# arm64:
#   build/arm64/linux      : il kernel Linux in formato "Image", che QEMU carica in RAM e
#                            avvia direttamente, passandogli il device tree
#   build/arm64/avvio.img  : l'archivio di avvio (initramfs), che QEMU mette in RAM accanto
#   build/arm64/disco.img  : il disco, con i programmi compilati per ARM64
#
# I dispositivi sono quelli "virtio" (fatti apposta per le macchine virtuali, con i loro
# driver in Linux) e l'USB, come su un computer vero:
#   virtio-blk-pci : il disco del sistema
#   virtio-vga / virtio-gpu-pci : la scheda video, a 1280x800 (sul PC "-vga" perche'
#       anche il firmware UEFI ci sa disegnare)
#   qemu-xhci : il controllore USB 3
#   usb-kbd, usb-tablet : tastiera e tavoletta USB. La tavoletta dice DOVE e' il
#       puntatore (non di quanto si e' mosso): il mouse non va catturato, e il
#       puntatore di mshell e' sempre sotto quello del Mac
set -e
qui="$(cd "$(dirname "$0")" && pwd)"
cpu="${1:-x86_64}"
[ "$cpu" = x86 ] && cpu=x86_64

docker run --rm --platform linux/amd64 -v "$qui:/w" alpine:3.24 sh /w/build.sh "$cpu"

if [ "$cpu" = arm64 ]; then
    # -M virt     : la macchina ARM "virtuale" di QEMU
    # -kernel, -initrd, -append : niente firmware: QEMU mette in RAM il kernel e
    #               l'archivio di avvio e salta al kernel, con la sua riga di comando
    # -serial stdio : la seriale e' questo terminale
    # Su un Mac con chip Apple si usa la virtualizzazione (hvf): il kernel gira sulla CPU
    # vera, senza essere emulato. Altrove si emula un Cortex-A72.
    if [ "$(uname -s)" = Darwin ] && [ "$(sysctl -n kern.hv_support 2>/dev/null)" = 1 ] &&
       [ "$(uname -m)" = arm64 ]; then
        cpu_qemu="-accel hvf -cpu host"
    else
        cpu_qemu="-cpu cortex-a72"
    fi
    exec qemu-system-aarch64 -M virt $cpu_qemu -m 512 -rtc base=localtime \
        -kernel "$qui/build/arm64/linux" -initrd "$qui/build/arm64/avvio.img" \
        -append "$(cat "$qui/build/arm64/riga_kernel.txt")" -serial stdio \
        -device virtio-gpu-pci,xres=1280,yres=800 -display cocoa,zoom-to-fit=on \
        -drive if=none,id=disco,file="$qui/build/arm64/disco.img",format=raw \
        -device virtio-blk-pci,drive=disco \
        -device qemu-xhci -device usb-kbd -device usb-tablet
fi

# -machine pc -m 2048 : un PC con 2 GB di RAM (si puo' dare di piu': -m 8192)
# -drive if=pflash ... : il firmware UEFI (OVMF, del progetto EDK2), che viene con QEMU
#                        (Homebrew lo mette in share/qemu). Parte, trova il disco di avvio
#                        e lancia \EFI\BOOT\BOOTX64.EFI, cioe' Linux
# esp.img ... index=2  : il disco di avvio, sul secondo canale IDE (lo legge solo il firmware)
# -rtc base=localtime  : l'orologio del PC virtuale segna l'ora di casa
# zoom-to-fit          : la finestra si puo' ingrandire
firmware="$(brew --prefix qemu 2>/dev/null)/share/qemu/edk2-x86_64-code.fd"
[ -f "$firmware" ] || firmware=/usr/share/qemu/edk2-x86_64-code.fd   # Linux
exec qemu-system-x86_64 -machine pc -m 2048 -rtc base=localtime \
    -drive if=pflash,format=raw,readonly=on,file="$firmware" \
    -drive file="$qui/build/x86_64/esp.img",format=raw,if=ide,index=2 \
    -drive if=none,id=disco,file="$qui/build/x86_64/disco.img",format=raw \
    -device virtio-blk-pci,drive=disco \
    -vga none -device virtio-vga,xres=1280,yres=800 \
    -device qemu-xhci -device usb-kbd -device usb-tablet \
    -display cocoa,zoom-to-fit=on
