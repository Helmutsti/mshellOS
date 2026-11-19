#!/bin/sh
# aggiorna-disco.sh: AGGIORNA I PROGRAMMI DI MSHELL DENTRO UN DISCO GIA' COSTRUITO, senza rifare
# il sistema (circa 1 minuto invece di 5-7). Gira nel container degli attrezzi, con --privileged
# (monta la partizione del disco):
#   sh strumenti/aggiorna-disco.sh [disco]      (predefinito: build/disco-qemu-pc.img)
# Compila i programmi (strumenti/compila.sh) in una radice a parte, apre la partizione "system"
# del disco e ci copia sopra quei file, e solo quelli. Vale finche' l'elenco dei pacchetti e il
# resto del sistema non cambiano: allora serve la costruzione completa (costruisci.sh).
# Il disco non deve essere acceso in QEMU mentre si aggiorna.
set -e
cd /w
DISCO=${1:-build/disco-qemu-pc.img}
NUOVI=/tmp/nuovi
rm -rf "$NUOVI"
sh strumenti/compila.sh "$NUOVI"
# dove comincia la partizione "system" (in byte), dalla tabella delle partizioni
INIZIO=$(sfdisk -J "$DISCO" | python3 -c 'import json,sys; t=json.load(sys.stdin)["partitiontable"]; p=[p for p in t["partitions"] if p.get("name")=="system"][0]; print(p["start"]*t.get("sectorsize",512))')
M=/tmp/sistema
mkdir -p "$M"
mount -o loop,offset="$INIZIO" "$DISCO" "$M"
cp -a "$NUOVI/." "$M/"
umount "$M"
echo "aggiornato $DISCO:"; (cd "$NUOVI" && find . -type f | sed 's#^\.#  #')
