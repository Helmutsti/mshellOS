# Compila il kernel con Docker e lo avvia in QEMU (Windows).
# Il disco del kernel e' disco.img (16 MB): se manca viene creato vuoto.
$ErrorActionPreference = "Stop"
$qui = $PSScriptRoot

docker run --rm -v "${qui}:/w" alpine:3.24 sh /w/build.sh
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$disco = "$qui\disco.img"
if (-not (Test-Path $disco)) {
    $f = [IO.File]::Create($disco); $f.SetLength(16MB); $f.Close()
    Write-Host "disco.img creato (16 MB, tutto a zero)"
}

& "C:\Program Files\qemu\qemu-system-i386.exe" -kernel "$qui\build\kernel" `
    -drive "file=$disco,format=raw,if=ide,index=0"
