#!/bin/sh
# avvia-mac.sh: il gemello di avvia.ps1 per il Mac (arm64). Costruisce il sistema
# (costruisci.sh, nel container degli attrezzi: scarica Debian per ARM se l'elenco dei pacchetti
# e' cambiato, poi ricompila i programmi di mshell e rifa' il disco) e lo accende in QEMU.
# La finestra di QEMU mostra lo schermo (tty1); la shell dell'amministratore e' sulla seriale,
# qui nel terminale (Ctrl+A poi X spegne QEMU).
#
#   sh avvia-mac.sh            costruisce e accende
#   sh avvia-mac.sh --scarica  riscarica Debian da capo (butta il sistema salvato), poi come sopra
set -e
qui=$(cd "$(dirname "$0")" && pwd)

if [ "$1" = "--scarica" ]; then
    rm -f "$qui"/build/cache/radice-*.tar
fi

# Docker Desktop: se non e' acceso, si accende e si aspetta che risponda
if ! docker info >/dev/null 2>&1; then
    open -a Docker
    echo "aspetto Docker..."
    until docker info >/dev/null 2>&1; do sleep 2; done
fi

attrezzi=mshell-debian-attrezzi
if ! docker image inspect $attrezzi >/dev/null 2>&1; then
    docker build -q -t $attrezzi "$qui/strumenti" >/dev/null
fi
docker run --rm --privileged -v "$qui:/w" $attrezzi sh /w/costruisci.sh qemu-mac

# Il firmware UEFI per ARM (edk2, viene con QEMU di Homebrew) trova \EFI\BOOT\BOOTAA64.EFI
# nella partizione "avvio". hvf: il processore del Mac, senza imitazione.
# Sulle macchine ARM di QEMU non c'e' la tastiera dei PC: serve usb-kbd accanto a usb-tablet.
# virtio-gpu-pci: la scheda video virtio (virtio-vga e' solo per i PC).
firmware=$(brew --prefix qemu)/share/qemu/edk2-aarch64-code.fd
exec qemu-system-aarch64 -machine virt -accel hvf -cpu host -smp 4 -m 2048 -nodefaults \
    -drive "if=pflash,format=raw,readonly=on,file=$firmware" \
    -drive "if=none,id=disco,file=$qui/build/disco-qemu-mac.img,format=raw" \
    -device virtio-blk-pci,drive=disco,bootindex=1 \
    -device virtio-gpu-pci,xres=1280,yres=800 \
    -device qemu-xhci -device usb-kbd -device usb-tablet \
    -display cocoa -serial mon:stdio
