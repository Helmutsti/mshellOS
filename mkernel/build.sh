#!/bin/sh
# Compila tutto. Gira dentro un container Docker alpine con la cartella montata in /w.
#   src/       il kernel (ring 0), una cartella per argomento (avvio, cpu, memoria, ...)
#   utente/    l'area utente (ring 3): la libreria e i programmi
#   comune/    quello che kernel e programmi devono vedere uguale (le chiamate di sistema)
#   strumenti/ programmi che girano qui, sul computer che compila (crea_disco)
#   build/     il compilato (si puo' cancellare, viene rifatto):
#              build/kernel, build/utente/<programma>, build/disco.img,
#              build/mkernel.iso (l'immagine avviabile: GRUB + kernel)
set -e
cd /w
# grub-bios e xorriso servono a grub-mkrescue, che prepara l'immagine avviabile
apk add --no-cache clang lld gcc musl-dev grub grub-bios xorriso >/dev/null
rm -rf build
mkdir -p build/kernel.o build/utente

# Le opzioni per il codice che gira su mkernel (kernel e programmi):
# --target=i686-elf : codice x86 a 32 bit per una macchina nuda (nessun sistema sotto)
# -ffreestanding    : niente libreria standard, niente main() obbligatorio
# -mno-sse ...      : niente istruzioni SSE/MMX. Il compilatore le userebbe per velocizzare
#                     i cicli, ma la CPU le accetta solo dopo che il kernel le ha attivate:
#                     senza questo, la prima che incontra manda in crash la macchina.
NUDO="--target=i686-elf -ffreestanding -O2 -Wall -Wextra -mno-sse -mno-sse2 -mno-mmx"

# --- 1. il kernel ---
# -I...: gli header si cercano in tutte le cartelle di src/ (e in comune/), cosi' basta
#        scrivere #include "pagine.h" ovunque si trovi il file
INCLUDE="$(find src -type d | sed 's/^/-I/') -Icomune"
for f in $(find src -name '*.s'); do
    clang --target=i686-elf -c "$f" -o "build/kernel.o/$(basename "$f" .s).o"
done
for f in $(find src -name '*.c'); do
    clang $NUDO $INCLUDE -c "$f" -o "build/kernel.o/$(basename "$f" .c).o"
done
# l'ordine dei .o non conta: e' linker.ld a mettere la firma Multiboot per prima
ld.lld -T src/avvio/linker.ld -o build/kernel build/kernel.o/*.o
echo "build/kernel pronto"

# --- 2. l'area utente: ogni programma e' un file ELF a se' ---
# Ognuno e' legato con avvio.o (che contiene _start: chiama main, poi esci) e con
# programma.ld (che lo mette a 1 GB, dove vivono i programmi nella loro mappa).
UTENTE="-Iutente/libreria -Icomune"
clang $NUDO $UTENTE -c utente/libreria/avvio.c -o build/utente/avvio.o
SUL_DISCO=""
for f in utente/programmi/*.c; do
    nome=$(basename "$f" .c)
    clang $NUDO $UTENTE -c "$f" -o "build/utente/$nome.o"
    ld.lld -T utente/libreria/programma.ld -o "build/utente/$nome" \
        build/utente/avvio.o "build/utente/$nome.o"
    SUL_DISCO="$SUL_DISCO /$nome=build/utente/$nome"
done
echo "build/utente pronto"

# --- 3. il disco: un mfs con dentro i programmi ---
# crea_disco gira qui (gcc normale, non i686-elf) ed e' compilato con lo stesso
# src/fs/mfs.c del kernel: strumenti/finto/ sostituisce le parti del kernel che mancano.
gcc -O2 -Wall -Istrumenti/finto -Isrc/dispositivi -Isrc/fs \
    -o build/crea_disco strumenti/crea_disco.c src/fs/mfs.c
./build/crea_disco build/disco.img 32768 $SUL_DISCO   # 32768 settori = 16 MB
echo "build/disco.img pronto"

# --- 4. l'immagine avviabile: GRUB, che accende la modalita' pixel e carica il kernel ---
# Come un CD: GRUB si avvia per primo, legge grub.cfg, trova la firma Multiboot nel
# kernel (con la richiesta 800x600 a 32 bit), accende lo schermo e salta a _start.
mkdir -p build/iso/boot/grub
cp build/kernel build/iso/boot/kernel
cat > build/iso/boot/grub/grub.cfg <<'FINE'
set timeout=0
insmod all_video
menuentry "mkernel" {
    multiboot /boot/kernel
    boot
}
FINE
grub-mkrescue -o build/mkernel.iso build/iso 2>/dev/null
echo "build/mkernel.iso pronto"
