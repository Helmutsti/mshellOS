# Avvio di mshell su Windows.
# Compila (in Docker, con build.sh), crea il disco se manca e avvia il kernel in QEMU.
# Schermo e tastiera della macchina (finestra QEMU) = console del sistema (tty0).
# Il terminale riceve solo una copia dei messaggi del kernel dalla porta seriale (ttyS0).
#
# Per spegnere: scrivi "shutdown" nella shell di mshell.
# In alternativa Ctrl+Alt+Canc, che pero' Windows tiene per se': si manda dal menu di QEMU
#   View > compatmonitor0, poi scrivi:  sendkey ctrl-alt-delete  e premi Invio.
# Chiudere la finestra di QEMU equivale a staccare la corrente.
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$qemu = 'C:\Program Files\qemu'
$docker = @('run', '--rm', '--platform', 'linux/amd64', '-v', "${root}:/w", 'alpine:3.24')

# 1. Compila init, programmi e archivio di avvio
docker @docker sh /w/build.sh
if ($LASTEXITCODE -ne 0) { throw 'Compilazione fallita' }

# 2. Il disco: un file su Windows che per mshell e' un disco rigido ext4, montato su /system.
#    Viene creato solo se non esiste, cosi' i dati restano tra un avvio e l'altro.
#    Alla creazione ci vengono messi i programmi di sistema in /system/bin.
#    Per aggiornarli bisogna cancellare disco.img e ricrearlo.
if (-not (Test-Path "$root\disco.img")) {
  docker @docker sh /w/build.sh disco
  if ($LASTEXITCODE -ne 0) { throw 'Creazione del disco fallita' }
}

# 3. Avvia: kernel Alpine + initramfs + disco.
#    L'ultima console indicata (tty0) diventa /dev/console, cioe' quella del programma.
& "$qemu\qemu-system-x86_64.exe" `
  -m 256M -no-reboot -name mshell -serial stdio `
  -kernel "$root\kernel\vmlinuz-virt" `
  -initrd "$root\build\initramfs.cpio.gz" `
  -drive "file=$root\disco.img,format=raw,if=virtio" `
  -append 'console=ttyS0 console=tty0 panic=-1'
