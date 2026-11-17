#!/bin/sh
# Crea il disco finito di mshell per il Mac con chip Apple (arm64), pronto per QEMU:
# build/arm64/disco.img, con accanto build/arm64/linux e build/arm64/avvio.img (su ARM QEMU
# parte da questi due, senza firmware: vedi run.sh). E' l'uscita del repository (non va su
# GitHub). Solo il disco: niente dischi delle prove.
set -e
qui="$(cd "$(dirname "$0")" && pwd)"
attrezzi=$(sh "$qui/strumenti/compilazione/attrezzi.sh")
docker run --rm -e SOLO_DISCO=1 -v "$qui:/w" "$attrezzi" sh /w/build.sh arm64
