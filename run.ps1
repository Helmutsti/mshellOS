# Compila tutto con Docker e avvia mkernel in QEMU (Windows), sul PC a 64 bit.
#   build\x86_64\esp.img   : il disco di avvio (FAT, come la "partizione EFI" di un PC):
#                            dentro il caricatore UEFI e il kernel
#   build\x86_64\disco.img : il disco con il file system e i programmi (/Sistema/init e gli altri)
$ErrorActionPreference = "Stop"
$qui = $PSScriptRoot

docker run --rm -v "${qui}:/w" alpine:3.24 sh /w/build.sh x86_64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# -machine pc -m 2048 : un PC con 2 GB di RAM (si puo' dare di piu': -m 8192)
# -drive if=pflash ... edk2-x86_64-code.fd : il firmware UEFI (OVMF, del progetto EDK2),
#   che viene con QEMU. Parte, trova il disco di avvio e lancia \EFI\BOOT\BOOTX64.EFI
# -drive esp.img ... index=2 : il disco di avvio, sul secondo canale IDE
# -drive disco.img ... index=0 : il primo disco ATA, quello che legge il kernel
# -rtc base=localtime : l'orologio del PC virtuale segna l'ora di casa (non quella di Greenwich)
# qemu-xhci, usb-kbd, usb-tablet : il controllore USB 3, con tastiera e tavoletta USB (lo
#   stesso driver di un PC vero). La tavoletta dice DOVE e' il puntatore: il mouse non va
#   catturato, e il puntatore e' sempre sotto quello di Windows
& "C:\Program Files\qemu\qemu-system-x86_64.exe" -machine pc -m 2048 -rtc base=localtime `
    -drive "if=pflash,format=raw,readonly=on,file=C:\Program Files\qemu\share\edk2-x86_64-code.fd" `
    -drive "file=$qui\build\x86_64\esp.img,format=raw,if=ide,index=2" `
    -drive "file=$qui\build\x86_64\disco.img,format=raw,if=ide,index=0" `
    -device qemu-xhci -device usb-kbd -device usb-tablet
