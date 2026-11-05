#!/bin/sh
# Avvio di mshell su macOS (Mac con chip Apple o Intel).
# Compila (in Docker, con build.sh), crea il disco se manca e avvia il kernel in QEMU.
#
# Serve:   Docker Desktop per Mac, e QEMU:  brew install qemu
# Avvio:   ./run.sh   (la prima volta: chmod +x run.sh)
#
# Schermo e tastiera della macchina (finestra QEMU) = console del sistema (tty0).
# Il terminale riceve solo una copia dei messaggi del kernel dalla porta seriale (ttyS0).
# Per spegnere: scrivi "shutdown" nella shell di mshell.
# Chiudere la finestra di QEMU equivale a staccare la corrente.
set -e
root="$(cd "$(dirname "$0")" && pwd)"

for programma in docker qemu-system-x86_64; do
    if ! command -v "$programma" >/dev/null 2>&1; then
        echo "Manca $programma. QEMU si installa con: brew install qemu" >&2
        exit 1
    fi
done

# mshell e' fatto per processori x86-64. Sui Mac con chip Apple (ARM) Docker deve
# compilare per x86-64 (--platform) e QEMU deve emulare il processore: piu' lento, ma funziona.
docker_run() {
    docker run --rm --platform linux/amd64 -v "$root:/w" alpine:3.24 "$@"
}
# Acceleratori in ordine di preferenza: QEMU prova il primo e, se non c'e', passa al secondo.
#   hvf = accelerazione del Mac (Hypervisor.framework), disponibile sui Mac Intel
#   tcg = emulazione del processore, funziona ovunque (Mac con chip Apple)
accel="-accel hvf -accel tcg"

# 1. Compila init, programmi e archivio di avvio
docker_run sh /w/build.sh

# 2. Il disco: un file sul Mac che per mshell e' un disco rigido ext4, montato su /system.
#    Viene creato solo se non esiste, cosi' i dati restano tra un avvio e l'altro.
#    Per aggiornare i programmi di sistema bisogna cancellare disco.img e ricrearlo.
if [ ! -f "$root/disco.img" ]; then
    docker_run sh /w/build.sh disco
fi

# 3. Avvia: kernel Alpine + initramfs + disco.
#    L'ultima console indicata (tty0) diventa /dev/console, cioe' quella del programma.
exec qemu-system-x86_64 \
    -m 256M -no-reboot -name mshell -serial stdio $accel \
    -kernel "$root/kernel/vmlinuz-virt" \
    -initrd "$root/build/initramfs.cpio.gz" \
    -drive "file=$root/disco.img,format=raw,if=virtio" \
    -append "console=ttyS0 console=tty0 panic=-1"
