# Compila tutto con Docker e avvia mkernel in QEMU (Windows).
#   build\mkernel.iso : l'immagine avviabile, come un CD: GRUB accende la modalita'
#                       pixel (1280x800) e carica il kernel
#   build\disco.img   : il disco con il file system e i programmi (/Sistema/init e gli altri)
$ErrorActionPreference = "Stop"
$qui = $PSScriptRoot

docker run --rm -v "${qui}:/w" alpine:3.24 sh /w/build.sh
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# -cdrom + -boot d : si parte dal "CD" con GRUB
# -drive ... if=ide,index=0 : il disco.img e' il primo disco ATA, quello che legge il kernel
# -rtc base=localtime : l'orologio del PC virtuale segna l'ora di casa (non quella di Greenwich)
# virtio-keyboard-pci, virtio-tablet-pci : tastiera e tavoletta virtio. La tavoletta dice DOVE
#   e' il puntatore: il mouse non va catturato, e il puntatore e' sempre sotto quello di Windows
& "C:\Program Files\qemu\qemu-system-i386.exe" -cdrom "$qui\build\mkernel.iso" -boot d -rtc base=localtime `
    -drive "file=$qui\build\disco.img,format=raw,if=ide,index=0" `
    -device virtio-keyboard-pci -device virtio-tablet-pci
