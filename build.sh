#!/bin/sh
# Compila tutto. Gira dentro un container Docker alpine con la cartella montata in /w.
#   sh build.sh        oppure  sh build.sh x86_64  : il PC a 64 bit (avviato da UEFI)
#                                                    ("x86" vale lo stesso)
#   sh build.sh arm64                              : ARM a 64 bit (QEMU "virt", UTM sul Mac)
#
#   src/        il kernel, una cartella per argomento (avvio, memoria, compiti, driver...).
#               Quello che dipende dalla CPU sta in src/arch/<cpu>/ (x86_64, arm64); il
#               resto e' uguale per tutte. Ogni CPU prende il resto + SOLO la sua cartella
#   utente/     i programmi: la libreria, i programmi del sistema, le applicazioni
#   comune/     quello che kernel e programmi (e il caricatore) devono vedere uguale
#   caricatore/ il caricatore UEFI del PC: il programma che il firmware fa partire, e
#               che carica il kernel
#   strumenti/  programmi che girano qui, sul computer che compila (crea_disco)
#   build/      il compilato (si puo' cancellare, viene rifatto), una cartella per CPU:
#               x86_64: build/x86_64/kernel, .../utente/..., .../disco.img, .../esp.img
#               arm64:  build/arm64/kernel.img, .../utente/..., .../disco.img
#
# USCITA (variabile d'ambiente): un'altra cartella dove scrivere, per compilare in
# parallelo senza pestarsi i piedi (per esempio USCITA=build/prova).
set -e
cd /w
CPU=${1:-x86_64}
[ "$CPU" = x86 ] && CPU=x86_64

# --- le opzioni di ogni CPU ---
# -ffreestanding : niente libreria standard, niente main() obbligatorio
# x86_64: --target=x86_64-elf: codice x86 a 64 bit per una macchina nuda.
#         -mno-sse ...: niente istruzioni SSE/MMX. Il compilatore le userebbe per
#         velocizzare i cicli, ma i loro registri il kernel non li salva quando cambia
#         compito: due compiti se li pesterebbero.
#         -mno-red-zone: il C a 64 bit puo' usare 128 byte SOTTO la cima della pila senza
#         spostarla ("zona rossa"). Nel kernel un'interruzione scrive proprio li': vietato.
#         IL MODELLO DI CODICE. Il kernel sta a 2 MB: il modello normale ("small", tutto
#         nei primi 2 GB, indirizzi da 32 bit dentro le istruzioni) va bene. I programmi
#         stanno a 127,5 TB, dove un indirizzo non sta in 32 bit: si compilano
#         "indipendenti dalla posizione" (-fPIE), cioe' ogni variabile si trova contando
#         da dove sta l'istruzione (RIP + distanza, fino a 2 GB: il programma e' tutto
#         dentro 1 GB). Il linker li mette lo stesso al loro indirizzo fisso.
#         (L'alternativa, -mcmodel=large, scrive ogni indirizzo per intero, 8 byte, e fa
#         ogni chiamata passando da un registro: codice piu' lungo e piu' lento.)
# arm64:  --target=aarch64-none-elf: codice ARM a 64 bit per una macchina nuda.
#         -mgeneral-regs-only: lo stesso di -mno-sse, per ARM (niente SIMD/virgola mobile).
#         -mstrict-align: niente letture "storte" (un intero a meta' tra due indirizzi):
#         con la MMU spenta ARM non le accetta.
# INIZIO_PROGRAMMI: dove vivono i programmi nella loro mappa (PROGRAMMI_INIZIO in
# comune/chiamate.h: i due numeri devono coincidere). Uguale per le due CPU.
INIZIO_PROGRAMMI=0x7F8040000000
case "$CPU" in
x86_64)
    B=${USCITA:-build/x86_64}
    NUDO="--target=x86_64-elf -ffreestanding -O2 -Wall -Wextra -mno-red-zone -mno-sse -mno-sse2 -mno-mmx"
    PROGRAMMI="-fPIE"
    # mtools serve a crea_esp.sh, che prepara il disco di avvio (FAT)
    apk add --no-cache clang lld gcc musl-dev mtools >/dev/null
    ;;
arm64)
    B=${USCITA:-build/arm64}
    NUDO="--target=aarch64-none-elf -ffreestanding -O2 -Wall -Wextra -mgeneral-regs-only -mstrict-align"
    PROGRAMMI=""
    apk add --no-cache clang lld gcc musl-dev >/dev/null
    ;;
*)
    echo "build.sh: CPU sconosciuta '$CPU' (x86_64 o arm64)"
    exit 1
    ;;
esac

# si ripulisce la cartella d'uscita (mai build/ intera: dentro ci sono le altre CPU)
[ "$B" = build ] && { echo "build.sh: USCITA=build cancellerebbe tutto"; exit 1; }
rm -rf "$B"
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
if [ "$CPU" = x86_64 ]; then
    # un ELF a 64 bit: il caricatore UEFI lo legge e mette i pezzi dove dice linker.ld
    ld.lld -T src/arch/x86_64/avvio/linker.ld -o "$B/kernel" "$B"/kernel.o/*.o
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
# Sul PC si compilano con -fPIE (vedi sopra: il modello di codice).
UTENTE="$PROGRAMMI -Iutente/libreria -Icomune"
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

# --- 4. (solo x86_64) il disco di avvio: il caricatore UEFI e il kernel ---
# All'accensione il firmware UEFI cerca un disco con una partizione FAT e dentro il file
# \EFI\BOOT\BOOTX64.EFI: e' il nostro caricatore (caricatore/), che legge \mkernel\kernel,
# chiede lo schermo al firmware e salta al kernel. Su ARM non serve: chi avvia carica
# direttamente kernel.img.
if [ "$CPU" = x86_64 ]; then
    sh caricatore/compila.sh "$B"
    sh caricatore/crea_esp.sh "$B" "$B/kernel" "$B/esp.img"
fi
