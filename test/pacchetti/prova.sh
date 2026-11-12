#!/bin/sh
# Le prove della libreria dei pacchetti (pacchetto.c), sul computer che compila, senza QEMU.
# Gira nello stesso container di build.sh:
#   docker run --rm --platform linux/amd64 -v "<cartella del progetto>:/w" alpine:3.24 sh /w/test/pacchetti/prova.sh
#   (da Git Bash su Windows: MSYS_NO_PATHCONV=1 e $(pwd -W))
# Prepara i pacchetti di prova (fai_pacchetti.py), compila prova.c con descrizione.c,
# pacchetto.c, miniz e tomlc99 come li compila build.sh, e la fa girare (come root: i file
# installati devono essere del sistema).
set -e
cd /w
apk add clang lld gcc musl-dev python3 >/dev/null
T=/tmp/pacchetti-prova
rm -rf "$T"
mkdir -p "$T"
python3 test/pacchetti/fai_pacchetti.py "$T/app"
CC="clang -O2 -Wall -Wextra -Iutente/libreria"
ESTERNI="-w -DMINIZ_NO_STDIO -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES"
$CC $ESTERNI -c utente/libreria/esterni/miniz/miniz.c -o "$T/miniz.o"
$CC $ESTERNI -c utente/libreria/esterni/tomlc99/toml.c -o "$T/toml.o"
$CC -c utente/libreria/pacchetto.c -o "$T/pacchetto.o"
$CC -c utente/libreria/descrizione.c -o "$T/descrizione.o"
$CC -c test/pacchetti/prova.c -o "$T/prova.o"
clang -fuse-ld=lld -o "$T/prova" "$T"/*.o
# con i pacchetti veri di build.sh, se sono gia' stati fatti per x86_64
if [ -d build/x86_64/pacchetti ]; then "$T/prova" "$T/app" build/x86_64/pacchetti; else "$T/prova" "$T/app"; fi
