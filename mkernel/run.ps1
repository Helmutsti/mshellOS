# Compila il kernel con Docker e lo avvia in QEMU.
$ErrorActionPreference = "Stop"
$qui = $PSScriptRoot

docker run --rm -v "${qui}:/w" alpine:3.24 sh /w/build.sh
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& "C:\Program Files\qemu\qemu-system-i386.exe" -kernel "$qui\build\kernel"
