# Compila tutto con Docker e avvia mkernel in QEMU (Windows).
#   build\mkernel.iso : l'immagine avviabile, come un CD: GRUB accende la modalita'
#                       pixel (800x600) e carica il kernel
#   build\disco.img   : il disco con il file system e i programmi (/init, /shell)
$ErrorActionPreference = "Stop"
$qui = $PSScriptRoot

docker run --rm -v "${qui}:/w" alpine:3.24 sh /w/build.sh
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# -cdrom + -boot d : si parte dal "CD" con GRUB
# -drive ... if=ide,index=0 : il disco.img e' il primo disco ATA, quello che legge il kernel
& "C:\Program Files\qemu\qemu-system-i386.exe" -cdrom "$qui\build\mkernel.iso" -boot d `
    -drive "file=$qui\build\disco.img,format=raw,if=ide,index=0"
