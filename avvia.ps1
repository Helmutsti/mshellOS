# avvia.ps1: costruisce il sistema (costruisci.sh, nel container degli attrezzi) e lo accende
# in QEMU, sul PC Windows. La finestra di QEMU mostra lo schermo (tty1), con la shell
# dell'amministratore gia' aperta.
$ErrorActionPreference = "Stop"
$qui = $PSScriptRoot

$attrezzi = "mshell-debian-attrezzi"
docker image inspect $attrezzi *> $null
if ($LASTEXITCODE -ne 0) {
    docker build -q -t $attrezzi "${qui}\strumenti" | Out-Null
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
docker run --rm --privileged -v "${qui}:/w" $attrezzi sh /w/costruisci.sh qemu-pc
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# il firmware UEFI (OVMF, che viene con QEMU) trova \EFI\BOOT\BOOTX64.EFI nella partizione "avvio"
& "C:\Program Files\qemu\qemu-system-x86_64.exe" -machine pc -nodefaults -m 2048 `
    -drive "if=pflash,format=raw,readonly=on,file=C:\Program Files\qemu\share\edk2-x86_64-code.fd" `
    -drive "if=none,id=disco,file=$qui\build\disco-qemu-pc.img,format=raw" `
    -device "virtio-blk-pci,drive=disco,bootindex=1" `
    -vga none -device "virtio-vga,xres=1280,yres=800"
