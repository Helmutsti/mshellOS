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
#   strumenti/  programmi che girano qui, sul computer che compila
#   file/       quello che si mette qui finisce nel disco, in /File
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
#         IL MODELLO DI CODICE: come si scrive un indirizzo dentro un'istruzione. I
#         programmi stanno nei primi 2 GB (da 4 MB, chiamate.h): il modello normale
#         ("small": ogni indirizzo sta in 32 bit) va bene. Il kernel sta negli ULTIMI
#         2 GB (KERNEL_IMMAGINE, src/memoria/pagine.h): -mcmodel=kernel, come Linux, cioe'
#         indirizzi da 32 bit "con il segno" (0xFFFFFFFF80000000 si scrive 0x80000000 e
#         la CPU lo allunga ripetendo il bit piu' alto).
#         (L'alternativa, -mcmodel=large, scrive ogni indirizzo per intero, 8 byte, e fa
#         ogni chiamata passando da un registro: codice piu' lungo e piu' lento.)
# arm64:  --target=aarch64-none-elf: codice ARM a 64 bit per una macchina nuda.
#         -mgeneral-regs-only: lo stesso di -mno-sse, per ARM (niente SIMD/virgola mobile).
#         -mstrict-align: niente letture "storte" (un intero a meta' tra due indirizzi):
#         con la MMU spenta ARM non le accetta.
#         Il modello di codice va bene quello normale per tutti: su ARM ogni variabile si
#         trova contando dalla posizione dell'istruzione (adrp, fino a 4 GB), quindi il
#         kernel in alto e i programmi in basso si compilano allo stesso modo.
# INIZIO_PROGRAMMI: dove vivono i programmi nella loro mappa (PROGRAMMI_INIZIO in
# comune/chiamate.h: i due numeri devono coincidere). Uguale per le due CPU.
INIZIO_PROGRAMMI=0x400000
case "$CPU" in
x86_64)
    B=${USCITA:-build/x86_64}
    NUDO="--target=x86_64-elf -ffreestanding -O2 -Wall -Wextra -mno-red-zone -mno-sse -mno-sse2 -mno-mmx"
    KERNEL="-mcmodel=kernel"
    # mtools serve a crea_esp.sh, che prepara il disco di avvio (FAT); e2fsprogs (mkfs.ext4)
    # prepara il disco del sistema
    apk add --no-cache clang lld mtools e2fsprogs >/dev/null
    ;;
arm64)
    B=${USCITA:-build/arm64}
    NUDO="--target=aarch64-none-elf -ffreestanding -O2 -Wall -Wextra -mgeneral-regs-only -mstrict-align"
    KERNEL=""
    apk add --no-cache clang lld e2fsprogs >/dev/null   # e2fsprogs: mkfs.ext4
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
    clang $NUDO $KERNEL $INCLUDE -c "$f" -o "$B/kernel.o/$(basename "$f" .c).o"
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
UTENTE="-Iutente/libreria -Icomune"
LEGA="ld.lld -T utente/libreria/programma.ld --defsym=INIZIO_PROGRAMMI=$INIZIO_PROGRAMMI"
for f in utente/libreria/*.c; do
    clang $NUDO $UTENTE -c "$f" -o "$B/utente/libreria/$(basename "$f" .c).o"
done

# Mentre si compilano, i programmi si mettono nell'ALBERO DEL DISCO: una cartella che e'
# gia' come sara' il disco, con proprietari e permessi giusti (mkfs.ext4 la copia tale e
# quale, al passo 3). Sta dentro il container, non in /w: la cartella condivisa con il
# computer che compila (Windows, macOS) non sa tenere i proprietari Unix. Il build gira
# come root (l'utente del container), quindi puo' dare ogni file a chi vuole.
ALBERO=/tmp/albero-disco
rm -rf "$ALBERO"
mkdir -p "$ALBERO"
chown 0:0 "$ALBERO"
chmod 755 "$ALBERO"

# cartelle_per /a/b/c: crea le cartelle che mancano lungo il percorso (/a, /a/b), del
# sistema (uid 0), con 755
cartelle_per() {
    _parziale=""
    for _pezzo in $(echo "${1%/*}" | tr / ' '); do
        _parziale="$_parziale/$_pezzo"
        if [ ! -d "$ALBERO$_parziale" ]; then
            mkdir "$ALBERO$_parziale"
            chown 0:0 "$ALBERO$_parziale"
            chmod 755 "$ALBERO$_parziale"
        fi
    done
}
# metti_file FILE /percorso PERMESSI: un file del sistema (uid 0)
metti_file() {
    cartelle_per "$2"
    cp "$1" "$ALBERO$2"
    chown 0:0 "$ALBERO$2"
    chmod "$3" "$ALBERO$2"
    printf '  %-40s %7s byte  %s\n' "$2" "$(wc -c < "$1")" "$3"
}
# metti_cartella /percorso UID PERMESSI: una cartella di qualcuno
metti_cartella() {
    cartelle_per "$1/"
    chown "$2:$2" "$ALBERO$1"
    chmod "$3" "$ALBERO$1"
    printf '  %-40s cartella  di %s, %s\n' "$1" "$2" "$3"
}

APPS=""

# I programmi del sistema (utente/programmi): init in /Sistema, i servizi in
# /Sistema/Servizi. Sono programmi: 755.
for f in utente/programmi/*.c; do
    nome=$(basename "$f" .c)
    clang $NUDO $UTENTE -c "$f" -o "$B/utente/$nome.o"
    $LEGA -o "$B/utente/$nome" "$B"/utente/libreria/*.o "$B/utente/$nome.o"
    case "$nome" in
        init) posto=/Sistema/init ;;
        *)    posto=/Sistema/Servizi/$nome ;;
    esac
    metti_file "$B/utente/$nome" "$posto" 755
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
    metti_file "$B/utente/applicazioni/$nome/$nome" "/Applicazioni/$nome/$nome" 755
    for r in "$cartella"*; do
        case "$r" in *.c|*.h) continue ;; esac
        [ -f "$r" ] && metti_file "$r" "/Applicazioni/$nome/$(basename "$r")" 644
    done
    APPS="$APPS $nome"
done

# L'utente (per ora uno solo, 1000): la sua cartella e' sua e di nessun altro (700). Le
# applicazioni la vedono come /home: dentro, Apps/<Nome> per i dati di ognuna e Scrivania.
UTENTE_UID=1000
metti_cartella /Utenti 0 755
metti_cartella /Utenti/$UTENTE_UID $UTENTE_UID 700
metti_cartella /Utenti/$UTENTE_UID/Apps $UTENTE_UID 700
metti_cartella /Utenti/$UTENTE_UID/Scrivania $UTENTE_UID 700
for nome in $APPS; do
    metti_cartella /Utenti/$UTENTE_UID/Apps/$nome $UTENTE_UID 700
done

# La cartella file/ del repository diventa /File: quello che ci si mette dentro si
# ritrova nel sistema (tranne il suo LEGGIMI.md, che parla della cartella). Del sistema,
# da leggere per tutti: cartelle 755, file 644.
metti_cartella /File 0 755
for r in file/* file/.[!.]*; do
    if [ -e "$r" ] && [ "$r" != file/LEGGIMI.md ]; then cp -R "$r" "$ALBERO/File/"; fi
done
chown -R 0:0 "$ALBERO/File"
find "$ALBERO/File" -type d -exec chmod 755 {} +
find "$ALBERO/File" -type f -exec chmod 644 {} +
echo "$B/utente pronto"

# --- 3. il disco: ext4, fatto con l'albero del disco ---
# mkfs.ext4 (di e2fsprogs, gli attrezzi di Linux per ext2/3/4) scrive un ext4 vuoto nel
# file e, con -d, ci copia dentro l'albero, con proprietari, permessi e date. Il formato
# del disco e' lo stesso per tutte le CPU; cambiano i programmi dentro.
# 64 MB: tanti per i programmi di oggi, pochi da copiare (il collaudo copia il disco a
# ogni prova). Il file e' "sparso": i pezzi mai scritti non occupano posto, dove il file
# system del computer che compila lo sa fare.
# -b 4096: blocchi da 4 KB, come quelli della cache dei blocchi del kernel (sotto i
# 512 MB mkfs sceglierebbe blocchi da 1 KB). -L: il nome del disco.
# Il formato occupa tutto il disco, senza tabella delle partizioni (vedi
# src/dispositivi/disco.h).
rm -f "$B/disco.img"
truncate -s 64M "$B/disco.img"
mkfs.ext4 -q -F -b 4096 -L mkernel -d "$ALBERO" "$B/disco.img"
rm -rf "$ALBERO"
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
