# Compila tutto con Docker e avvia mshell in QEMU (Windows), sul PC a 64 bit.
#   build\x86_64\disco.img : il disco, uno solo, con una tabella delle partizioni GPT e tre
#                            partizioni (vedi build.sh): avvio (la partizione EFI, FAT32: dentro
#                            \EFI\BOOT\BOOTX64.EFI, il kernel Linux con l'archivio di avvio e la
#                            riga di comando, una UKI), system (ext4: "/", in sola lettura) e
#                            data (ext4: /data, i file della persona e lo stato dei servizi)
# LA SERIALE: nella finestra di QEMU, menu View (Visualizza) -> serial0: il registro dell'avvio e, in fondo, la shell di prova.
$ErrorActionPreference = "Stop"
$qui = $PSScriptRoot

# il container degli attrezzi (strumenti/compilazione/Dockerfile), preparato una volta sola
$attrezzi = "mshell-attrezzi-x86_64"
docker image inspect $attrezzi *> $null
if ($LASTEXITCODE -ne 0) {
    docker build -q -t $attrezzi "${qui}\strumenti\compilazione" | Out-Null
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
docker run --rm -v "${qui}:/w" $attrezzi sh /w/build.sh x86_64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# -machine pc -m 2048 : un PC con 2 GB di RAM (si puo' dare di piu': -m 8192), con un solo
#   processore: con -smp 4 QEMU su Windows (TCG con la finestra GTK o SDL) si blocca appena parte
# -drive if=pflash ... edk2-x86_64-code.fd : il firmware UEFI (OVMF, del progetto EDK2),
#   che viene con QEMU. Parte, trova nel disco la partizione EFI (avvio) e lancia
#   \EFI\BOOT\BOOTX64.EFI
# -drive disco.img + virtio-blk-pci : il disco (virtio: fatto per le macchine virtuali, con
#   il suo driver in Linux); bootindex=1: il primo (l'unico) da cui il firmware prova a partire
# -vga none -device virtio-vga : la scheda video virtio, a 1280x800
# -rtc base=localtime : l'orologio del PC virtuale segna l'ora di casa (non quella di Greenwich)
# -nic user,model=virtio-net-pci : la scheda di rete virtio. "user" e' la rete di QEMU
#   stessa: un router finto (10.0.2.2) che da' l'indirizzo con DHCP (10.0.2.15), risponde ai
#   nomi (DNS 10.0.2.3) ed esce su Internet attraverso il computer, senza configurare niente
# qemu-xhci, usb-kbd, usb-tablet : il controllore USB 3, con tastiera e tavoletta USB. La
#   tavoletta dice DOVE e' il puntatore: il mouse non va catturato, e il puntatore e'
#   sempre sotto quello di Windows
& "C:\Program Files\qemu\qemu-system-x86_64.exe" -machine pc -m 2048 -rtc base=localtime `
    -drive "if=pflash,format=raw,readonly=on,file=C:\Program Files\qemu\share\edk2-x86_64-code.fd" `
    -drive "if=none,id=disco,file=$qui\build\x86_64\disco.img,format=raw" `
    -device "virtio-blk-pci,drive=disco,bootindex=1" `
    -vga none -device "virtio-vga,xres=1280,yres=800" `
    -nic "user,model=virtio-net-pci" `
    -device qemu-xhci -device usb-kbd -device usb-tablet
