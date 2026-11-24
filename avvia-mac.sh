#!/bin/sh
# avvia-mac.sh: il gemello di avvia.ps1 per il Mac (arm64). Costruisce il sistema
# (costruisci.sh, nel container degli attrezzi: scarica Debian per ARM se l'elenco dei pacchetti
# e' cambiato, poi ricompila i programmi di mshell e rifa' il disco) e lo accende in UTM, nella
# macchina "mshell" (vedi sotto). La finestra di UTM mostra lo schermo (tty1).
#
#   sh avvia-mac.sh            costruisce e accende
#   sh avvia-mac.sh --scarica  riscarica Debian da capo (butta il sistema salvato), poi come sopra
set -e
qui=$(cd "$(dirname "$0")" && pwd)

if [ "$1" = "--scarica" ]; then
    rm -f "$qui"/build/cache/radice-*.tar
fi

# Docker Desktop: se non e' acceso, si accende e si aspetta che risponda
if ! docker info >/dev/null 2>&1; then
    open -a Docker
    echo "aspetto Docker..."
    until docker info >/dev/null 2>&1; do sleep 2; done
fi

attrezzi=mshell-debian-attrezzi
if ! docker image inspect $attrezzi >/dev/null 2>&1; then
    docker build -q -t $attrezzi "$qui/strumenti" >/dev/null
fi
docker run --rm --privileged -v "$qui:/w" $attrezzi sh /w/costruisci.sh qemu-mac

# SI ACCENDE IN UTM (decisione della persona, 20/11/2026: per provare i driver della scheda video).
# UTM con il motore QEMU e la scheda video "virtio-gpu-gl-pci": la macchina ha una scheda video
# virtio con il 3D (VirGL), che UTM disegna con la scheda video vera del Mac (ANGLE su Metal).
# Nel sistema la usano il driver virtio_gpu del kernel e quello virgl di Mesa. (Con il motore
# Apple Virtualization la scheda video di Linux non ha il 3D; con QEMU di Homebrew neppure.)
# La macchina "mshell" si crea la prima volta; poi a ogni costruzione si rimette dentro il disco
# nuovo (UTM tiene il disco dentro il pacchetto della macchina, una sua copia).
vm=mshell
disco="$HOME/Library/Containers/com.utmapp.UTM/Data/Documents/$vm.utm/Data/disco-qemu-mac.img"
utmctl=/Applications/UTM.app/Contents/MacOS/utmctl
pgrep -xq UTM || { open -a UTM; sleep 3; }
if [ ! -d "$(dirname "$disco")" ]; then
    osascript <<FINE >/dev/null
tell application "UTM"
    make new virtual machine with properties {backend:qemu, configuration:{name:"$vm", architecture:"aarch64", machine:"virt", memory:2048, cpu cores:4, hypervisor:true, uefi:true, drives:{{interface:VirtIO, raw:true, source:(POSIX file "$qui/build/disco-qemu-mac.img")}}, displays:{{hardware:"virtio-gpu-gl-pci"}}, network interfaces:{{hardware:"virtio-net-pci", mode:shared}}}}
end tell
FINE
else
    # spenta, si mette il disco nuovo al posto del vecchio (copia istantanea: clonefile di APFS)
    $utmctl stop --kill $vm >/dev/null 2>&1 || true
    while ! $utmctl status $vm | grep -q stopped; do sleep 1; done
    rm -f "$disco"
    cp -c "$qui/build/disco-qemu-mac.img" "$disco"
fi
$utmctl start $vm
