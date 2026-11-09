# Compila il kernel, i programmi e il disco con Docker e li avvia in QEMU (Windows).
# Il disco e' build\disco.img: build.sh lo rifa' a ogni compilazione, con dentro /init.
$ErrorActionPreference = "Stop"
$qui = $PSScriptRoot

docker run --rm -v "${qui}:/w" alpine:3.24 sh /w/build.sh
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& "C:\Program Files\qemu\qemu-system-i386.exe" -kernel "$qui\build\kernel" `
    -drive "file=$qui\build\disco.img,format=raw,if=ide,index=0"
