#!/bin/sh
# Compila tutto con Docker e avvia mshell in QEMU (macOS / Linux).
#   ./run.sh         oppure  ./run.sh x86_64 : il PC a 64 bit, avviato da UEFI
#   ./run.sh arm64                           : ARM a 64 bit (la seriale e' questo terminale)
#
# Il disco e' uno solo, build/<cpu>/disco.img, con una tabella delle partizioni GPT e tre
# partizioni (vedi build.sh): avvio (la partizione EFI, FAT32), system (ext4: "/", in sola
# lettura) e data (ext4: /data, i file della persona e lo stato dei servizi).
# x86_64:
#   il firmware UEFI trova nel disco la partizione avvio e lancia \EFI\BOOT\BOOTX64.EFI:
#   il kernel Linux con l'archivio di avvio e la riga di comando (una UKI)
# arm64:
#   build/arm64/linux      : il kernel Linux in formato "Image", che QEMU carica in RAM e
#                            avvia direttamente, passandogli il device tree
#   build/arm64/avvio.img  : l'archivio di avvio (initramfs), che QEMU mette in RAM accanto
#   build/arm64/disco.img  : il disco (la partizione avvio c'e', ma qui non si usa)
#
# I dispositivi sono quelli "virtio" (fatti apposta per le macchine virtuali, con i loro
# driver in Linux) e l'USB, come su un computer vero:
#   virtio-blk-pci : il disco
#   virtio-vga / virtio-gpu-pci : la scheda video, a 1280x800 (sul PC "-vga" perche'
#       anche il firmware UEFI ci sa disegnare)
#   qemu-xhci : il controllore USB 3
#   usb-kbd, usb-tablet : tastiera e tavoletta USB. La tavoletta dice DOVE e' il
#       puntatore (non di quanto si e' mosso): il mouse non va catturato, e il
#       puntatore di mshell e' sempre sotto quello del Mac
# LA SERIALE (x86_64): nella finestra di QEMU, menu View (Visualizza) -> serial0: il registro
#   dell'avvio e, con il disco di sviluppo (SVILUPPO=1, vedi build.sh e STATO.md sezione 3), la shell.
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
    exec qemu-system-aarch64 -M virt $cpu_qemu -m 512 -smp 4 -rtc base=localtime \
        -kernel "$qui/build/arm64/linux" -initrd "$qui/build/arm64/avvio.img" \
        -append "$(cat "$qui/build/arm64/riga_kernel.txt")" -serial stdio \
        -device virtio-gpu-pci,xres=1280,yres=800 -display cocoa,zoom-to-fit=on \
        -drive if=none,id=disco,file="$qui/build/arm64/disco.img",format=raw \
        -device virtio-blk-pci,drive=disco \
        -device qemu-xhci -device usb-kbd -device usb-tablet
fi

# -machine pc -m 2048 -smp 4 : un PC con 2 GB di RAM (si puo' dare di piu': -m 8192) e 4
#   processori: compositore e app lavorano insieme invece di darsi il turno su uno solo
# -drive if=pflash ... : il firmware UEFI (OVMF, del progetto EDK2), che viene con QEMU
#                        (Homebrew lo mette in share/qemu). Parte, trova nel disco la
#                        partizione EFI (avvio) e lancia \EFI\BOOT\BOOTX64.EFI, cioe' Linux
# bootindex=1          : il disco e' il primo (l'unico) da cui il firmware prova a partire
# -rtc base=localtime  : l'orologio del PC virtuale segna l'ora di casa
# zoom-to-fit          : la finestra si puo' ingrandire
firmware="$(brew --prefix qemu 2>/dev/null)/share/qemu/edk2-x86_64-code.fd"
[ -f "$firmware" ] || firmware=/usr/share/qemu/edk2-x86_64-code.fd   # Linux
exec qemu-system-x86_64 -machine pc -m 2048 -smp 4 -rtc base=localtime \
    -drive if=pflash,format=raw,readonly=on,file="$firmware" \
    -drive if=none,id=disco,file="$qui/build/x86_64/disco.img",format=raw \
    -device virtio-blk-pci,drive=disco,bootindex=1 \
    -vga none -device virtio-vga,xres=1280,yres=800 \
    -device qemu-xhci -device usb-kbd -device usb-tablet \
    -display cocoa,zoom-to-fit=on
