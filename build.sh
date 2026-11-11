#!/bin/sh
# Compila tutto. Gira dentro un container Docker alpine con la cartella montata in /w.
#   sh build.sh        oppure  sh build.sh x86   : il PC (avviato da GRUB)
#   sh build.sh arm64                             : ARM a 64 bit (QEMU "virt", UTM sul Mac)
#
#   src/       il kernel, una cartella per argomento (avvio, memoria, compiti, driver...).
#              Quello che dipende dalla CPU sta in src/arch/<cpu>/ (x86, arm64); il resto
#              e' uguale per tutte. Ogni CPU prende il resto + SOLO la sua cartella
#   utente/    i programmi: la libreria, i programmi del sistema, le applicazioni
#   comune/    quello che kernel e programmi devono vedere uguale (le chiamate di sistema)
#   strumenti/ programmi che girano qui, sul computer che compila (crea_disco)
#   build/     il compilato (si puo' cancellare, viene rifatto):
#              x86:   build/kernel, build/utente/..., build/disco.img, build/mkernel.iso
#              arm64: build/arm64/kernel.img, build/arm64/utente/..., build/arm64/disco.img
#
# USCITA (variabile d'ambiente): un'altra cartella dove scrivere, per compilare in
# parallelo senza pestarsi i piedi (per esempio USCITA=build/prova).
set -e
cd /w
CPU=${1:-x86}

# --- le opzioni di ogni CPU ---
# -ffreestanding : niente libreria standard, niente main() obbligatorio
# x86:   --target=i686-elf: codice x86 a 32 bit per una macchina nuda.
#        -mno-sse ...: niente istruzioni SSE/MMX. Il compilatore le userebbe per
#        velocizzare i cicli, ma la CPU le accetta solo dopo che il kernel le ha attivate:
#        senza questo, la prima che incontra manda in crash la macchina.
# arm64: --target=aarch64-none-elf: codice ARM a 64 bit per una macchina nuda.
#        -mgeneral-regs-only: lo stesso per ARM (niente SIMD/virgola mobile).
#        -mstrict-align: niente letture "storte" (un intero a meta' tra due indirizzi):
#        con la MMU spenta ARM non le accetta.
# INIZIO_PROGRAMMI: dove vivono i programmi nella loro mappa (PROGRAMMI_INIZIO in
# comune/chiamate.h: i due numeri devono coincidere).
case "$CPU" in
x86)
    B=${USCITA:-build}
    NUDO="--target=i686-elf -ffreestanding -O2 -Wall -Wextra -mno-sse -mno-sse2 -mno-mmx"
    INIZIO_PROGRAMMI=0x40000000
    # grub-bios e xorriso servono a grub-mkrescue, che prepara il CD avviabile
    apk add --no-cache clang lld gcc musl-dev grub grub-bios xorriso >/dev/null
    ;;
arm64)
    B=${USCITA:-build/arm64}
    NUDO="--target=aarch64-none-elf -ffreestanding -O2 -Wall -Wextra -mgeneral-regs-only -mstrict-align"
    INIZIO_PROGRAMMI=0x8040000000
    apk add --no-cache clang lld gcc musl-dev >/dev/null
    ;;
*)
    echo "build.sh: CPU sconosciuta '$CPU' (x86 o arm64)"
    exit 1
    ;;
esac

# si ripulisce la cartella d'uscita (in build/, quelle delle altre compilazioni restano)
mkdir -p "$B"
if [ "$B" = build ]; then
    find build -mindepth 1 -maxdepth 1 ! -name 'arm64*' -exec rm -rf {} +
else
    rm -rf "$B"
fi
mkdir -p "$B/kernel.o" "$B/utente/libreria"

# --- 1. il kernel ---
# Le cartelle e i file del kernel: tutto src/ tranne src/arch, piu' src/arch/<cpu>.
# -I...: gli header si cercano in tutte quelle cartelle (e in comune/), cosi' basta
#        scrivere #include "pagine.h" ovunque si trovi il file
CARTELLE="$(find src -path src/arch -prune -o -type d -print) $(find src/arch/$CPU -type d)"
INCLUDE="$(for d in $CARTELLE; do echo "-I$d"; done) -Icomune"
FILE_S="$(find src -path src/arch -prune -o -name '*.s' -print) $(find src/arch/$CPU -name '*.s')"
FILE_C="$(find src -path src/arch -prune -o -name '*.c' -print) $(find src/arch/$CPU -name '*.c')"
for f in $FILE_S; do
    clang ${NUDO%% *} -c "$f" -o "$B/kernel.o/$(basename "$f" .s)_s.o"
done
for f in $FILE_C; do
    clang $NUDO $INCLUDE -c "$f" -o "$B/kernel.o/$(basename "$f" .c).o"
done
if [ "$CPU" = x86 ]; then
    # l'ordine dei .o non conta: e' linker.ld a mettere la firma Multiboot per prima
    ld.lld -T src/arch/x86/avvio/linker.ld -o "$B/kernel" "$B"/kernel.o/*.o
    echo "$B/kernel pronto"
else
    # due forme dello stesso kernel: ELF (con i nomi delle funzioni, per guardarci dentro)
    # e "Image", il file grezzo che si avvia (QEMU, UTM, U-Boot, il Raspberry Pi)
    ld.lld -T src/arch/arm64/avvio/linker.ld -o "$B/kernel" "$B"/kernel.o/*.o
    ld.lld -T src/arch/arm64/avvio/linker.ld --oformat=binary -o "$B/kernel.img" "$B"/kernel.o/*.o
    echo "$B/kernel.img pronto"
fi

# --- 2. i programmi: i programmi del sistema e le applicazioni, ognuno un ELF a se' ---
# Ognuno e' legato con i pezzi della libreria che non sono negli header: avvio.o (che
# contiene _start: chiama main, poi esci), stringhe.o (le funzioni sulla memoria che il
# compilatore puo' chiamare da solo) e memoria.o (malloc e free); e con programma.ld (che
# lo mette a INIZIO_PROGRAMMI, dove vivono i programmi nella loro mappa).
UTENTE="-Iutente/libreria -Icomune"
LEGA="ld.lld -T utente/libreria/programma.ld --defsym=INIZIO_PROGRAMMI=$INIZIO_PROGRAMMI"
for f in utente/libreria/*.c; do
    clang $NUDO $UTENTE -c "$f" -o "$B/utente/libreria/$(basename "$f" .c).o"
done
SUL_DISCO=""
APPS=""

# I programmi del sistema (utente/programmi): init in /Sistema, i servizi in
# /Sistema/Servizi.
for f in utente/programmi/*.c; do
    nome=$(basename "$f" .c)
    clang $NUDO $UTENTE -c "$f" -o "$B/utente/$nome.o"
    $LEGA -o "$B/utente/$nome" "$B"/utente/libreria/*.o "$B/utente/$nome.o"
    case "$nome" in
        init) posto=/Sistema/init ;;
        *)    posto=/Sistema/Servizi/$nome ;;
    esac
    SUL_DISCO="$SUL_DISCO $posto=$B/utente/$nome"
done

# Le applicazioni (utente/applicazioni/<Nome>/): ognuna diventa una cartella
# /Applicazioni/<Nome>/ con dentro il programma, chiamato come la cartella (i suoi .c
# legati insieme), e tutto il resto che c'e' nella cartella dei sorgenti: la
# descrizione e le risorse, da leggere (644).
for cartella in utente/applicazioni/*/; do
    nome=$(basename "$cartella")
    mkdir -p "$B/utente/applicazioni/$nome"
    oggetti=""
    for f in "$cartella"*.c; do
        o="$B/utente/applicazioni/$nome/$(basename "$f" .c).o"
        clang $NUDO $UTENTE -I"$cartella" -c "$f" -o "$o"
        oggetti="$oggetti $o"
    done
    $LEGA -o "$B/utente/applicazioni/$nome/$nome" "$B"/utente/libreria/*.o $oggetti
    SUL_DISCO="$SUL_DISCO /Applicazioni/$nome/$nome=$B/utente/applicazioni/$nome/$nome"
    for r in "$cartella"*; do
        case "$r" in *.c|*.h) continue ;; esac
        [ -f "$r" ] && SUL_DISCO="$SUL_DISCO /Applicazioni/$nome/$(basename "$r")=$r:644"
    done
    APPS="$APPS $nome"
done

# L'utente (per ora uno solo, 1000): la sua cartella e' sua e di nessun altro (700). Le
# applicazioni la vedono come /home: dentro, Apps/<Nome> per i dati di ognuna e Scrivania.
UTENTE_UID=1000
SUL_DISCO="$SUL_DISCO +/Utenti:0:755 +/Utenti/$UTENTE_UID:$UTENTE_UID:700"
SUL_DISCO="$SUL_DISCO +/Utenti/$UTENTE_UID/Apps:$UTENTE_UID:700 +/Utenti/$UTENTE_UID/Scrivania:$UTENTE_UID:700"
for nome in $APPS; do
    SUL_DISCO="$SUL_DISCO +/Utenti/$UTENTE_UID/Apps/$nome:$UTENTE_UID:700"
done
echo "$B/utente pronto"

# --- 3. il disco: un mfs con dentro i programmi ---
# crea_disco gira qui (gcc normale, non per mkernel) ed e' compilato con lo stesso
# src/fs/mfs.c del kernel: strumenti/finto/ sostituisce le parti del kernel che mancano.
# Il formato del disco e' lo stesso per tutte le CPU; cambiano i programmi dentro.
gcc -O2 -Wall -Istrumenti/finto -Isrc/dispositivi -Isrc/fs \
    -o "$B/crea_disco" strumenti/crea_disco.c src/fs/mfs.c
"./$B/crea_disco" "$B/disco.img" 32768 $SUL_DISCO   # 32768 settori = 16 MB
echo "$B/disco.img pronto"

# --- 4. (solo x86) il CD avviabile: GRUB, che accende la modalita' pixel e carica il kernel ---
# GRUB si avvia per primo, legge grub.cfg, trova la firma Multiboot nel kernel (con la
# richiesta 1280x800 a 32 bit), accende lo schermo e salta a _start. Su ARM non serve:
# chi avvia carica direttamente kernel.img.
if [ "$CPU" = x86 ]; then
    mkdir -p "$B/iso/boot/grub"
    cp "$B/kernel" "$B/iso/boot/kernel"
    cat > "$B/iso/boot/grub/grub.cfg" <<'FINE'
set timeout=0
insmod all_video
menuentry "mkernel" {
    multiboot /boot/kernel
    boot
}
FINE
    grub-mkrescue -o "$B/mkernel.iso" "$B/iso" 2>/dev/null
    echo "$B/mkernel.iso pronto"
fi
