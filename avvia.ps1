# avvia.ps1: costruisce il sistema (costruisci.sh, nel container degli attrezzi) e lo accende
# in QEMU, sul PC Windows. La finestra di QEMU mostra lo schermo (tty1).
#
# IL COMPOSITORE PARTE DA SOLO: all'accensione greetd (il gestore dell'accesso) apre la sessione
# di utente1 senza chiedere niente e lancia mshell-compositore (disegno con OpenGL; in QEMU lo fa Mesa con il processore)
# (per ora senza app: solo lo sfondo e il puntatore). Sullo schermo, Alt+Esc
# chiude il compositore; allora greetd mostra l'accesso testuale (agreety). La shell
# dell'amministratore, gia' aperta, sta sulla seriale.
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

# -display gtk,zoom-to-fit=off: la finestra prende la misura dello schermo di mshell (con
# zoom-to-fit acceso, il predefinito, restava 853x575 e lo tagliava). Non SDL: la sua finestra
# si bloccava (QEMU smetteva di rispondere, anche al monitor).
# il firmware UEFI (OVMF, che viene con QEMU) trova \EFI\BOOT\BOOTX64.EFI nella partizione "avvio"
& "C:\Program Files\qemu\qemu-system-x86_64.exe" -machine q35 -nodefaults -m 2048 `
    -drive "if=pflash,format=raw,readonly=on,file=C:\Program Files\qemu\share\edk2-x86_64-code.fd" `
    -drive "if=none,id=disco,file=$qui\build\disco-qemu-pc.img,format=raw" `
    -device "virtio-blk-pci,drive=disco,bootindex=1" `
    -vga none -device "virtio-vga,xres=1280,yres=800" `
    -device qemu-xhci -device usb-tablet -display "gtk,zoom-to-fit=off" `
    -nic "user,model=virtio-net-pci" `
    -accel whpx -smp 4  # usb-tablet: il mouse (con virtio-tablet il compositore non lo riceve); whpx e 4 processori: il disegno con OpenGL imitato (SceneFX) arriva a circa 16 immagini al secondo (accettato dalla persona, 19/11/2026)
