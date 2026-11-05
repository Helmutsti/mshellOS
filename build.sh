#!/bin/sh
# Compilazione di mshell. Gira DENTRO un container Docker alpine:3.24 (x86-64),
# con la cartella del progetto montata in /w. Lo usano sia run.ps1 (Windows) sia run.sh (Mac).
#
#   sh build.sh          compila init + programmi e crea build/initramfs.cpio.gz
#   sh build.sh disco    crea disco.img (ext4, 512 MB) con i programmi in /bin
#                        (che in mshell diventa /system/bin)
set -e
cd /w

# Versione del kernel di Alpine e moduli da mettere nell'archivio di avvio,
# nell'ordine in cui init li carica (prima le dipendenze).
KVER=6.18.60-0-virt
MODULI="lib/crc/crc16 fs/mbcache fs/jbd2/jbd2 fs/ext4/ext4 drivers/block/virtio_blk"

# Programmi di sistema che vanno sul disco in /system/bin.
# Nome del programma = nome del file in src/ (la shell, src/shell.c, diventa mshell).
PROGRAMMI="shutdown ls cat mkdir"

if [ "$1" = "disco" ]; then
    apk add -q e2fsprogs >/dev/null
    rm -rf /tmp/disco
    cp -r build/disco /tmp/disco
    chown -R 0:0 /tmp/disco
    chmod 755 /tmp/disco/bin /tmp/disco/bin/*
    rm -f disco.img
    truncate -s 512M disco.img
    mkfs.ext4 -q -L mshell -d /tmp/disco disco.img
    exit 0
fi

apk add -q gcc musl-dev cpio squashfs-tools >/dev/null
rm -rf build
mkdir -p build/rootfs/lib/modules build/disco/bin

# init: va nell'archivio di avvio
gcc -static -O2 -Wall -o build/rootfs/init src/init.c src/file_sicuro.c

# programmi di sistema: vanno sul disco
gcc -static -O2 -Wall -o build/disco/bin/mshell src/shell.c
for p in $PROGRAMMI; do
    gcc -static -O2 -Wall -o "build/disco/bin/$p" "src/$p.c"
done

# moduli del kernel, presi dall'archivio dei moduli di Alpine
unsquashfs -q -d /tmp/m kernel/modloop-virt >/dev/null
for m in $MODULI; do
    cp "/tmp/m/modules/$KVER/kernel/$m.ko" build/rootfs/lib/modules/
done

# archivio di avvio
cd build/rootfs
find . | cpio -o -H newc --quiet | gzip > ../initramfs.cpio.gz
