#!/bin/sh
# Compila tutto. Gira dentro un container Docker alpine con la cartella montata in /w:
#   docker run --rm --platform linux/amd64 -v "<questa cartella>:/w" alpine:3.24 sh /w/build.sh <cpu>
#   (run.sh e run.ps1 lo fanno da soli; da Git Bash su Windows: MSYS_NO_PATHCONV=1 e $(pwd -W))
#
#   sh build.sh        oppure  sh build.sh x86_64  : il PC a 64 bit (avviato da UEFI)
#                                                    ("x86" vale lo stesso)
#   sh build.sh arm64                              : ARM a 64 bit (QEMU "virt", UTM sul Mac)
#
# IL SISTEMA. Sotto c'e' il kernel Linux, preso cosi' com'e' dai pacchetti di Alpine Linux
# 3.24 (lo stesso container in cui gira questo script); sopra, dal processo numero 1 in
# su, solo i nostri programmi: niente programmi di una distribuzione, niente shell.
#
#   utente/libreria/   la libreria dei programmi (libreria.h): le nostre funzioni, fatte
#                      con le chiamate di sistema di Linux e la libreria C musl
#   utente/avvio/      avvio, il programma dell'archivio di avvio (initramfs): carica i
#                      moduli del kernel, monta il disco, fa partire init
#   utente/programmi/  i programmi del sistema (init, eventi, compositore, launcher, scrivania)
#   utente/applicazioni/  le applicazioni, una cartella ciascuna
#   file/              quello che si mette qui finisce nel disco, in /File
#   strumenti/         programmi che girano qui, sul computer che compila (la grafica)
#   build/<cpu>/       il compilato (si puo' cancellare, viene rifatto):
#       linux          il kernel (per il PC: bzImage; per ARM: Image)
#       avvio.img      l'archivio di avvio (initramfs: cpio compresso con gzip)
#       disco.img      il disco del sistema (ext4, etichetta "mshell")
#       esp.img        (solo PC) il disco di avvio UEFI: dentro \EFI\BOOT\BOOTX64.EFI,
#                      kernel + archivio + riga di comando in un solo programma UEFI
#                      (una "UKI", Unified Kernel Image)
#   build/scarico/     i pacchetti di Alpine scaricati, tenuti per la prossima volta
#
# LE VARIABILI D'AMBIENTE (tutte facoltative):
#   USCITA   un'altra cartella dove scrivere, per compilare in parallelo senza pestarsi i
#            piedi (per esempio USCITA=build/prova)
#   NUCLEO   quale kernel di Alpine: "virt" (linux-virt, se non si dice: piccolo, con i
#            driver delle macchine virtuali) o "lts" (linux-lts, con i driver dei PC veri,
#            piu' linux-firmware: vedi DA IMPLEMENTARE in fondo)
#   VERSIONE_LINUX  per fissare la versione del pacchetto del kernel (per esempio
#            6.18.63-r0): se non si dice, l'ultima di Alpine 3.24. Le versioni usate le
#            scrive il build alla fine, per poterle rifissare.
set -e
cd /w
CPU=${1:-x86_64}
[ "$CPU" = x86 ] && CPU=x86_64
NUCLEO=${NUCLEO:-virt}

# --- le cose di ogni CPU ---
# ARCH: il nome della CPU per Alpine e per il compilatore.
# CONSOLE: la seriale, dove il kernel e i programmi scrivono il registro dell'avvio
#   (ttyS0 sul PC, la PL011 ttyAMA0 su ARM). Non c'e' "console=tty0": il kernel non
#   scrive sullo schermo, che e' tutto del compositore.
case "$CPU" in
x86_64)
    ARCH=x86_64
    B=${USCITA:-build/x86_64}
    CONSOLE="console=ttyS0,115200"
    # mtools: il disco di avvio (FAT); efi-mkuki e systemd-efistub: la UKI
    ATTREZZI_CPU="mtools efi-mkuki systemd-efistub"
    ;;
arm64)
    ARCH=aarch64
    B=${USCITA:-build/arm64}
    CONSOLE="console=ttyAMA0"
    ATTREZZI_CPU=""
    ;;
*)
    echo "build.sh: CPU sconosciuta '$CPU' (x86_64 o arm64)"
    exit 1
    ;;
esac
case "$NUCLEO" in
virt) PACCHETTI_KERNEL="linux-virt" ;;
lts)  PACCHETTI_KERNEL="linux-lts linux-firmware-none" ;;
*)    echo "build.sh: NUCLEO sconosciuto '$NUCLEO' (virt o lts)"; exit 1 ;;
esac
[ -n "$VERSIONE_LINUX" ] && PACCHETTI_KERNEL="linux-$NUCLEO=$VERSIONE_LINUX ${PACCHETTI_KERNEL#linux-$NUCLEO}"

# La riga di comando del kernel. vt.global_cursor_default=0: niente cursore che lampeggia
# sullo schermo prima che arrivi il compositore.
RIGA_KERNEL="$CONSOLE vt.global_cursor_default=0"

# I MODULI del kernel che servono (con quelli da cui dipendono, che si trovano da soli):
#   virtio_pci             il "bus" delle schede virtuali di QEMU (spesso gia' nel kernel)
#   virtio_blk, ext4       il disco del sistema e il suo formato
#   virtio-gpu             la scheda video virtuale (lo schermo, con il DRM)
#   xhci-pci, usbhid,      il controllore USB 3, e tastiera e mouse USB ("HID")
#   hid-generic
#   evdev                  i file /dev/input/event*, da cui il servizio eventi li legge
MODULI="virtio_pci virtio_blk ext4 virtio-gpu xhci-pci usbhid hid-generic evdev"

# --- 0. gli attrezzi e i pacchetti di Alpine ---
SCARICO=/w/build/scarico
mkdir -p "$SCARICO/host" "$SCARICO/$ARCH"
apk add --cache-dir "$SCARICO/host" clang lld e2fsprogs $ATTREZZI_CPU >/dev/null

# LA RADICE DI ALPINE PER LA CPU: un albero con i pacchetti di Alpine PER QUELLA CPU
# (apk --arch, anche se il container e' x86_64), da cui si prendono:
#   - il kernel, i suoi moduli (linux-virt) e il firmware;
#   - musl-dev: la libreria C (libc.a) e i suoi header, da legare dentro i programmi;
#   - linux-headers: gli header del kernel (input, DRM...).
# Le firme dei pacchetti si controllano con le chiavi di Alpine per quella CPU.
# --no-scripts: gli script d'installazione non girano (sono per un'altra CPU, e non servono).
RADICE=/tmp/radice-$ARCH
rm -rf "$RADICE"
apk --arch "$ARCH" --root "$RADICE" --initdb --no-scripts \
    --repositories-file /etc/apk/repositories --keys-dir "/usr/share/apk/keys/$ARCH" \
    --cache-dir "$SCARICO/$ARCH" add musl-dev linux-headers $PACCHETTI_KERNEL >/dev/null
VERSIONE_K=$(ls "$RADICE/lib/modules")
MODULI_K="$RADICE/lib/modules/$VERSIONE_K"

# si ripulisce la cartella d'uscita (mai build/ intera: dentro ci sono le altre CPU)
[ "$B" = build ] && { echo "build.sh: USCITA=build cancellerebbe tutto"; exit 1; }
rm -rf "$B"
mkdir -p "$B/oggetti/libreria"

# --- 1. il compilatore ---
# clang compila per la CPU scelta (--target) con gli header e le librerie della radice di
# Alpine (--sysroot). I programmi sono STATICI: dentro ognuno c'e' tutto quello che gli
# serve della libreria C, e nel disco non serve un "caricatore" di librerie (ld.so).
#   -ffunction-sections -fdata-sections e --gc-sections: dentro va solo quello che si usa
#   -s: senza i nomi delle funzioni (programmi piu' piccoli)
# Si lega con ld.lld direttamente: i pezzi di avvio di musl (crt1.o, crti.o, crtn.o:
# chiamano main e poi exit), i nostri file, e la libreria C (-lc).
CC="clang --target=$ARCH-alpine-linux-musl --sysroot=$RADICE -O2 -Wall -Wextra -ffunction-sections -fdata-sections"
LIBC="$RADICE/usr/lib"
lega() {   # lega USCITA OGGETTI...
    _uscita=$1
    shift
    ld.lld -static --gc-sections -s -o "$_uscita" "$LIBC/crt1.o" "$LIBC/crti.o" "$@" -L"$LIBC" -lc "$LIBC/crtn.o"
}

# --- 2. i programmi: la libreria, i programmi del sistema e le applicazioni ---
UTENTE="-Iutente/libreria"
for f in utente/libreria/*.c; do
    $CC $UTENTE -c "$f" -o "$B/oggetti/libreria/$(basename "$f" .c).o"
done
LIBRERIA="$B/oggetti/libreria/*.o"

# Mentre si compilano, i programmi si mettono nell'ALBERO DEL DISCO: una cartella che e'
# gia' come sara' il disco, con proprietari e permessi giusti (mkfs.ext4 la copia tale e
# quale, al passo 4). Sta dentro il container, non in /w: la cartella condivisa con il
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

# I posti dove Linux monta i suoi file system speciali (li monta avvio): /dev (i
# dispositivi), /proc (i processi), /sys (il kernel e i driver), /run (in memoria: i
# nomi dei socket, in /run/mshell).
for c in /dev /proc /sys /run; do metti_cartella "$c" 0 755; done

APPS=""

# I programmi del sistema (utente/programmi): init in /Sistema, i servizi in
# /Sistema/Servizi. Sono programmi: 755.
for f in utente/programmi/*.c; do
    nome=$(basename "$f" .c)
    $CC $UTENTE -c "$f" -o "$B/oggetti/$nome.o"
    lega "$B/oggetti/$nome" $LIBRERIA "$B/oggetti/$nome.o"
    case "$nome" in
        init) posto=/Sistema/init ;;
        *)    posto=/Sistema/Servizi/$nome ;;
    esac
    metti_file "$B/oggetti/$nome" "$posto" 755
done

# Le applicazioni (utente/applicazioni/<Nome>/): ognuna diventa una cartella
# /Applicazioni/<Nome>/ con dentro il programma, chiamato come la cartella (i suoi .c
# legati insieme), e tutto il resto che c'e' nella cartella dei sorgenti: la
# descrizione e le risorse, da leggere (644).
for cartella in utente/applicazioni/*/; do
    nome=$(basename "$cartella")
    mkdir -p "$B/oggetti/applicazioni/$nome"
    oggetti=""
    for f in "$cartella"*.c; do
        o="$B/oggetti/applicazioni/$nome/$(basename "$f" .c).o"
        $CC $UTENTE -I"$cartella" -c "$f" -o "$o"
        oggetti="$oggetti $o"
    done
    lega "$B/oggetti/applicazioni/$nome/$nome" $LIBRERIA $oggetti
    metti_file "$B/oggetti/applicazioni/$nome/$nome" "/Applicazioni/$nome/$nome" 755
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
echo "programmi pronti"

# --- 3. l'archivio di avvio (initramfs) ---
# Un archivio cpio (formato "newc", quello che legge il kernel) compresso con gzip, con
# avvio come /init, i moduli e il loro ordine (vedi utente/avvio/avvio.c).
ARCHIVIO=/tmp/archivio-avvio
rm -rf "$ARCHIVIO"
mkdir -p "$ARCHIVIO/moduli" "$ARCHIVIO/dev" "$ARCHIVIO/proc" "$ARCHIVIO/sys"
$CC -c utente/avvio/avvio.c -o "$B/oggetti/avvio.o"
lega "$ARCHIVIO/init" "$B/oggetti/avvio.o"
mknod -m 600 "$ARCHIVIO/dev/console" c 5 1

# I moduli, nell'ordine giusto. modules.dep dice, per ogni modulo, da quali dipende
# ("kernel/fs/ext4/ext4.ko.gz: kernel/lib/crc/crc16.ko.gz ..."); metti_modulo mette
# prima, una volta sola, quelli da cui dipende, poi lui. Quelli gia' dentro il kernel
# (modules.builtin) non servono. I file di Alpine sono compressi (.ko.gz): il kernel li
# vuole interi, quindi si scompattano qui.
FATTI=" "
file_modulo() {   # file_modulo NOME: il suo file in modules.dep (con - o _, e' lo stesso)
    awk -F: -v n="$1" '{ f = $1; sub(/.*\//, "", f); sub(/\.ko.*/, "", f); gsub(/-/, "_", f)
                         m = n; gsub(/-/, "_", m); if (f == m) { print $1; exit } }' "$MODULI_K/modules.dep"
}
metti_modulo() {
    local _file _nome _dipende _base     # "local": metti_modulo chiama se' stessa
    _file=$(file_modulo "$1")
    if [ -z "$_file" ]; then
        _nome=$(echo "$1" | tr - _)
        grep -q "/$_nome.ko\|/$1.ko" "$MODULI_K/modules.builtin" || { echo "build.sh: il modulo $1 non c'e'"; exit 1; }
        return 0
    fi
    case "$FATTI" in *" $_file "*) return 0 ;; esac
    FATTI="$FATTI$_file "
    for _dipende in $(grep "^$_file:" "$MODULI_K/modules.dep" | cut -d: -f2); do
        metti_modulo "$(basename "$_dipende" | sed 's/\.ko.*//')"
    done
    _base=$(basename "$_file" | sed 's/\.ko.*//')
    case "$_file" in
        *.gz)  gzip -dc "$MODULI_K/$_file" > "$ARCHIVIO/moduli/$_base.ko" ;;
        *.xz)  xz -dc "$MODULI_K/$_file" > "$ARCHIVIO/moduli/$_base.ko" ;;
        *.zst) zstd -qdc "$MODULI_K/$_file" > "$ARCHIVIO/moduli/$_base.ko" ;;
        *)     cp "$MODULI_K/$_file" "$ARCHIVIO/moduli/$_base.ko" ;;
    esac
    echo "$_base" >> "$ARCHIVIO/moduli/elenco"
}
: > "$ARCHIVIO/moduli/elenco"
for m in $MODULI; do metti_modulo "$m"; done
(cd "$ARCHIVIO" && find . | sort | cpio -o -H newc 2>/dev/null) | gzip -9n > "$B/avvio.img"
echo "$B/avvio.img pronto: $(wc -l < "$ARCHIVIO/moduli/elenco") moduli ($(tr '\n' ' ' < "$ARCHIVIO/moduli/elenco"))"
rm -rf "$ARCHIVIO"

# --- 4. il disco: ext4, fatto con l'albero del disco ---
# mkfs.ext4 (di e2fsprogs, gli attrezzi di Linux per ext2/3/4) scrive un ext4 vuoto nel
# file e, con -d, ci copia dentro l'albero, con proprietari, permessi e date. Il formato
# del disco e' lo stesso per tutte le CPU; cambiano i programmi dentro.
# 64 MB: tanti per i programmi di oggi, pochi da copiare (il collaudo copia il disco a
# ogni prova). Il file e' "sparso": i pezzi mai scritti non occupano posto, dove il file
# system del computer che compila lo sa fare.
# -L mshell: il nome del disco, con cui lo trova avvio. Il formato occupa tutto il
# disco, senza tabella delle partizioni.
rm -f "$B/disco.img"
truncate -s 64M "$B/disco.img"
mkfs.ext4 -q -F -b 4096 -L mshell -d "$ALBERO" "$B/disco.img"
rm -rf "$ALBERO"
echo "$B/disco.img pronto"

# --- 5. il kernel ---
# Sul PC vmlinuz e' un "bzImage", che sa anche partire da UEFI da solo (l'"EFI stub").
# Su ARM64 Alpine lo da' compresso dentro un piccolo programma UEFI ("zboot": "MZ", poi
# "zimg", da dove a dove sta il kernel compresso, e "gzip"): per partire senza UEFI
# (QEMU -kernel, UTM) serve l'"Image" che c'e' dentro, quindi lo si tira fuori.
VMLINUZ="$RADICE/boot/vmlinuz-$NUCLEO"
if [ "$CPU" = arm64 ] && [ "$(dd if="$VMLINUZ" bs=1 skip=4 count=4 2>/dev/null)" = zimg ]; then
    numero_a() { od -An -tu4 -j "$1" -N4 "$VMLINUZ" | tr -d ' '; }
    tail -c +"$(($(numero_a 8) + 1))" "$VMLINUZ" | head -c "$(numero_a 12)" | gzip -dc > "$B/linux"
else
    cp "$VMLINUZ" "$B/linux"
fi
echo "$B/linux pronto: Linux $VERSIONE_K"

# --- 6. (solo PC) il disco di avvio UEFI ---
# All'accensione il firmware UEFI cerca un disco con una partizione FAT e dentro il file
# \EFI\BOOT\BOOTX64.EFI. Ci mettiamo una UKI: un solo programma UEFI con dentro il
# kernel, l'archivio di avvio e la riga di comando. La fa efi-mkuki, che li attacca
# come "sezioni" al programmino UEFI di systemd che sa far partire Linux (systemd-stub).
# Su un PC vero basta copiarlo su una chiavetta con una partizione FAT, allo stesso posto
# (vedi DA IMPLEMENTARE).
# L'immagine e' la sola partizione, senza tabella delle partizioni ("superfloppy"): il
# firmware la riconosce lo stesso, e per QEMU basta. mtools scrive nella FAT senza
# montarla (mformat la crea, mmd fa le cartelle, mcopy copia i file).
if [ "$CPU" = x86_64 ]; then
    printf 'NAME="mshell"\nID=mshell\nPRETTY_NAME="mshell (Linux %s)"\n' "$VERSIONE_K" > /tmp/os-release
    efi-mkuki -c "$RIGA_KERNEL" -k "$VERSIONE_K" -r /tmp/os-release \
        -S /usr/lib/systemd/boot/efi/linuxx64.efi.stub -o "$B/BOOTX64.EFI" "$B/linux" "$B/avvio.img"
    rm -f "$B/esp.img"
    truncate -s 64M "$B/esp.img"
    mformat -i "$B/esp.img" -F -v MSHELL ::
    mmd -i "$B/esp.img" ::/EFI ::/EFI/BOOT
    mcopy -i "$B/esp.img" "$B/BOOTX64.EFI" ::/EFI/BOOT/BOOTX64.EFI
    echo "$B/esp.img pronto"
else
    echo "$RIGA_KERNEL" > "$B/riga_kernel.txt"   # QEMU la passa con -append (run.sh)
fi

rm -rf "$B/oggetti"
echo "versioni: $(apk --root "$RADICE" info -v 2>/dev/null | grep -E '^(linux-|musl-)' | tr '\n' ' ')"

# DA IMPLEMENTARE
#   - NUCLEO=lts sui PC veri: linux-lts ha i driver dell'hardware vero (dischi NVMe e
#     SATA, schede video Intel/AMD, tastiere...), ma quali moduli servano dipende dalla
#     macchina: invece di un elenco fisso (MODULI), avvio dovrebbe caricare i moduli
#     dei dispositivi che trova (i "modalias" in /sys, confrontati con modules.alias,
#     come fa udev). Anche il firmware (linux-firmware: centinaia di MB) andrebbe messo
#     nel disco, solo quello che serve, in una cartella da dire al kernel con
#     firmware_class.path=. Qui si installa linux-firmware-none (niente firmware).
#   - Un disco unico per un PC vero: una tabella delle partizioni (GPT) con la partizione
#     EFI (la UKI) e quella del sistema (l'ext4 "mshell"), da scrivere su una chiavetta.
#     Oggi sono due immagini separate (esp.img e disco.img).
#   - Secure Boot: la UKI non e' firmata; su un PC vero va spento, o la UKI firmata.
