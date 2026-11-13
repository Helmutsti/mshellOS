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
#                      con le chiamate di sistema di Linux e la libreria C musl. Dentro,
#                      le due librerie della grafica (LIBRERIE.md): gkit/ (le finestre, lo
#                      schermo e il disegno) e uikit/ (lo stile, i materiali e i componenti)
#   utente/avvio/      avvio, il programma dell'archivio di avvio (initramfs): carica i
#                      moduli del kernel, trova e monta le partizioni, fa partire init
#   utente/programmi/  i programmi del sistema (init, eventi, compositore, launcher, scrivania)
#   utente/applicazioni/  le applicazioni, una cartella ciascuna (i .c, manifest.toml e resources/)
#   utente/prove/      programmi che servono solo alle prove automatiche (test/): vanno
#                      solo in disco_prove.img, mai nel disco normale
#   file/              quello che si mette qui finisce nel disco, nella casa della persona
#                      (/data/users/1000/home, che i programmi vedono come /home)
#   risorse/           le risorse del sistema (caratteri, icone, sfondi), gia' pronte (le
#                      prepara strumenti/grafica/genera.py): vanno in /system/resources
#   strumenti/         programmi che girano qui, sul computer che compila (la grafica)
#   build/<cpu>/       il compilato (si puo' cancellare, viene rifatto):
#       linux          il kernel (per il PC: bzImage; per ARM: Image)
#       avvio.img      l'archivio di avvio (initramfs: cpio compresso con gzip)
#       BOOTX64.EFI    (solo PC) kernel + archivio + riga di comando in un solo programma
#                      UEFI (una "UKI", Unified Kernel Image)
#       riga_kernel.txt (solo ARM) la riga di comando del kernel, per QEMU -append
#       pacchetti/<id>.app  il pacchetto di ogni applicazione (uno ZIP), per questo processore
#       disco.img      IL DISCO, uno solo, con una tabella delle partizioni GPT e tre
#                      partizioni (vedi IL DISCO qui sotto)
#       disco_prove.img  il disco delle prove automatiche: lo stesso, con in piu' i
#                      programmi di utente/prove in /data/tests (vedi in fondo al passo 6)
#       disco_rete.img   per la prova della rete, in QEMU: il programma rete in /data/tests
#   build/scarico/     i pacchetti di Alpine scaricati, tenuti per la prossima volta
#
# IL DISCO. Una tabella GPT (Linux la legge da solo) con tre partizioni, ognuna con il suo
# nome nella tabella (il "PARTLABEL", che avvio usa per trovarle):
#   1. avvio   64 MB  la partizione EFI (FAT32): sul PC dentro \EFI\BOOT\BOOTX64.EFI, la
#                     UKI; su ARM e' vuota (QEMU carica kernel e archivio con -kernel e
#                     -initrd), ma c'e', perche' il disco sia fatto allo stesso modo
#   2. system  64 MB  ext4, montata da avvio in SOLA LETTURA come "/": il sistema
#   3. data    64 MB  ext4, montata da avvio in lettura e scrittura in /data: i dati
# Com'e' fatto il sistema che ne viene fuori:
#   /system                      (sola lettura) i programmi del sistema
#       init                     il primo programma (il processo 1)
#       services/<servizio>      i servizi: eventi, compositore, launcher, scrivania, network
#       config/<servizio>/       la configurazione predefinita dei servizi, da leggere (644):
#                                i file di utente/config/ (oggi network/network.conf)
#       apps/<app>/              le applicazioni del sistema: code (il programma),
#                                manifest.toml e resources/ (lo stesso contenuto del loro
#                                pacchetto .app, scompattato)
#       bin/                     i comandi (per ora nessuno)
#       lib/                     per le librerie condivise (per ora nessuna: vedi sotto)
#       resources/               le risorse del sistema: fonts/ (i caratteri), icons/ (le
#                                icone), wallpapers/ (gli sfondi). Sono i file di risorse/
#                                del repository; li leggono GKit e UIKit
#       firmware/                il firmware dei dispositivi (firmware_class.path)
#       modules/                 i moduli del kernel da caricare dal disco (per ora tutti
#                                quelli che servono stanno nell'archivio di avvio)
#   /data                        (lettura e scrittura)
#       apps/<app>/              le applicazioni installate dalla persona (all'inizio nessuna)
#       users/<uid>/home         la casa della persona: i suoi file (/home per i programmi)
#       users/<uid>/bin          i comandi della persona, cercati DOPO /system/bin (l'elenco
#                                dei posti dove cercarli, come il PATH, e' della shell:
#                                quando ci sara')
#       users/<uid>/appsdata/<app>   i dati di ogni applicazione
#       users/<uid>/preferences  le preferenze della persona
#       system/                  identita', utenti, permessi, applicazioni installate: solo
#                                del sistema (0, 700)
#       services/<servizio>      lo stato e i registri di ogni servizio, suoi
#   /etc/resolv.conf             un collegamento simbolico a /run/network/resolv.conf: i
#                                server dei nomi (DNS) per musl, che li cerca in /etc; il file
#                                vero lo scrive il servizio network (in /etc non si scrive)
#   /dev /proc /sys              di Linux (li monta avvio)
#   /run /tmp                    in memoria (tmpfs): i nomi dei socket stanno in /run/mshell,
#                                il DNS del servizio network in /run/network
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
    # efi-mkuki e systemd-efistub: la UKI
    ATTREZZI_CPU="efi-mkuki systemd-efistub"
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
# sullo schermo prima che arrivi il compositore. firmware_class.path: dove il kernel cerca
# il firmware dei dispositivi che lo chiedono (/system/firmware, nella partizione system).
RIGA_KERNEL="$CONSOLE vt.global_cursor_default=0 firmware_class.path=/system/firmware"

# I MODULI del kernel che servono (con quelli da cui dipendono, che si trovano da soli):
#   virtio_pci             il "bus" delle schede virtuali di QEMU (spesso gia' nel kernel)
#   virtio_blk, ext4       il disco e il formato delle partizioni system e data
#   virtio-gpu             la scheda video virtuale (lo schermo, con il DRM)
#   xhci-pci, usbhid,      il controllore USB 3, e tastiera e mouse USB ("HID")
#   hid-generic
#   evdev                  i file /dev/input/event*, da cui il servizio eventi li legge
#   virtio_net             la scheda di rete virtuale (virtio-net-pci in QEMU)
#   af_packet              i pacchetti "grezzi": servono per chiedere l'indirizzo (DHCP)
#                          quando la scheda non ne ha ancora uno
MODULI="virtio_pci virtio_blk ext4 virtio-gpu xhci-pci usbhid hid-generic evdev virtio_net af_packet"

# --- 0. gli attrezzi e i pacchetti di Alpine ---
SCARICO=/w/build/scarico
mkdir -p "$SCARICO/host" "$SCARICO/$ARCH"
# e2fsprogs: gli ext4 (mkfs.ext4); sfdisk: la tabella delle partizioni (GPT); mtools: la
# FAT della partizione di avvio, scritta senza montarla
apk add --cache-dir "$SCARICO/host" clang lld e2fsprogs sfdisk mtools zip $ATTREZZI_CPU >/dev/null

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
    --cache-dir "$SCARICO/$ARCH" add musl-dev linux-headers compiler-rt $PACCHETTI_KERNEL >/dev/null
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
    # compiler-rt: le funzioni del compilatore (es. i long double a 128 bit su ARM, che
    # printf e scanf di musl usano): senza, legare un programma che usa printf non riesce
    _rt=$(ls "$RADICE"/usr/lib/llvm*/lib/clang/*/lib/*/libclang_rt.builtins-*.a 2>/dev/null | head -1)
    ld.lld -static --gc-sections -s -o "$_uscita" "$LIBC/crt1.o" "$LIBC/crti.o" "$@" -L"$LIBC" -lc $_rt "$LIBC/crtn.o"
}

# --- 2. i programmi: la libreria, i programmi del sistema e le applicazioni ---
UTENTE="-Iutente/libreria"
# (anche GKit e UIKit, nelle loro cartelle: i nomi dei file .c sono tutti diversi)
for f in utente/libreria/*.c utente/libreria/gkit/*.c utente/libreria/uikit/*.c; do
    $CC $UTENTE -c "$f" -o "$B/oggetti/libreria/$(basename "$f" .c).o"
done
# Le librerie di altri (utente/libreria/esterni/, ognuna con la sua LICENSE), legate nella
# libreria come i nostri file (--gc-sections ne tiene solo quello che un programma usa):
#   miniz 3.0.2 (MIT), la release "amalgamata": leggere gli ZIP (i pacchetti .app).
#     Senza le funzioni sui FILE della libreria C e senza i nomi di zlib (pacchetto.c fa
#     le stesse scelte).
#   tomlc99 (MIT), commit 62ce93c del 4/11/2026: leggere il TOML (i manifesti).
#   stb_image 2.30 (pubblico dominio o MIT), commit f58f558: leggere le PNG (le icone delle
#     app), per immagine_carica di GKit. Solo PNG e senza FILE (esterni/stb/stb_image.c).
# Non sono nostre: i loro avvisi del compilatore si spengono (-w), per non coprire i nostri.
ESTERNI="-w -DMINIZ_NO_STDIO -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES"
$CC $ESTERNI -c utente/libreria/esterni/miniz/miniz.c -o "$B/oggetti/libreria/miniz.o"
$CC $ESTERNI -c utente/libreria/esterni/tomlc99/toml.c -o "$B/oggetti/libreria/toml.o"
$CC $ESTERNI -c utente/libreria/esterni/stb/stb_image.c -o "$B/oggetti/libreria/stb_image.o"
LIBRERIA="$B/oggetti/libreria/*.o"

# Mentre si compilano, i programmi si mettono negli ALBERI DELLE PARTIZIONI: due cartelle
# che sono gia' come saranno le partizioni system e data, con proprietari e permessi
# giusti (mkfs.ext4 le copia tali e quali, al passo 6). Stanno dentro il container, non in
# /w: la cartella condivisa con il computer che compila (Windows, macOS) non sa tenere i
# proprietari Unix. Il build gira come root (l'utente del container), quindi puo' dare
# ogni file a chi vuole.
# Le funzioni qui sotto parlano con i percorsi come li vedra' il sistema acceso: quelli
# sotto /data vanno nell'albero di data, tutti gli altri in quello di system (che e' "/").
ALBERO_SISTEMA=/tmp/albero-system
ALBERO_DATI=/tmp/albero-data
rm -rf "$ALBERO_SISTEMA" "$ALBERO_DATI"
for a in "$ALBERO_SISTEMA" "$ALBERO_DATI"; do
    mkdir -p "$a"
    chown 0:0 "$a"
    chmod 755 "$a"
done

# vero /percorso: dove sta, adesso, nell'albero della sua partizione
vero() {
    case "$1" in
        /data)   echo "$ALBERO_DATI" ;;
        /data/*) echo "$ALBERO_DATI/${1#/data/}" ;;
        *)       echo "$ALBERO_SISTEMA$1" ;;
    esac
}
# cartelle_per /a/b/c: crea le cartelle che mancano lungo il percorso (/a, /a/b), del
# sistema (uid 0), con 755
cartelle_per() {
    _parziale=""
    for _pezzo in $(echo "${1%/*}" | tr / ' '); do
        _parziale="$_parziale/$_pezzo"
        _qui=$(vero "$_parziale")
        if [ ! -d "$_qui" ]; then
            mkdir "$_qui"
            chown 0:0 "$_qui"
            chmod 755 "$_qui"
        fi
    done
}
# metti_file FILE /percorso PERMESSI: un file del sistema (uid 0)
metti_file() {
    cartelle_per "$2"
    cp "$1" "$(vero "$2")"
    chown 0:0 "$(vero "$2")"
    chmod "$3" "$(vero "$2")"
    printf '  %-40s %7s byte  %s\n' "$2" "$(wc -c < "$1")" "$3"
}
# metti_cartella /percorso UID PERMESSI: una cartella di qualcuno
metti_cartella() {
    cartelle_per "$1/"
    chown "$2:$2" "$(vero "$1")"
    chmod "$3" "$(vero "$1")"
    printf '  %-40s cartella  di %s, %s\n' "$1" "$2" "$3"
}

# --- la partizione system: "/", in sola lettura ---

# I posti dove si montano gli altri file system (li monta avvio): /dev (i dispositivi),
# /proc (i processi), /sys (il kernel e i driver), /run e /tmp (in memoria: in /run/mshell
# i nomi dei socket) e /data (la partizione data). Siccome "/" non si puo' scrivere, tutto
# quello che si scrive mentre il sistema gira finisce in uno di questi.
for c in /dev /proc /sys /run /tmp; do metti_cartella "$c" 0 755; done
mkdir "$ALBERO_SISTEMA/data"
chown 0:0 "$ALBERO_SISTEMA/data"
chmod 755 "$ALBERO_SISTEMA/data"

# Le cartelle del sistema che oggi sono vuote, ma hanno gia' il loro posto:
#   /system/bin        i comandi
#   /system/lib        le librerie condivise. Oggi non ce ne sono: i programmi sono
#                      statici (passo 1) e si portano dentro tutto. Quando ci saranno
#                      librerie condivise, qui andranno loro e il "caricatore" (ld.so)
#   /system/resources  le risorse del sistema: caratteri, icone, sfondi (le riempie il
#                      passo qui sotto, da risorse/)
#   /system/firmware   il firmware dei dispositivi (firmware_class.path, RIGA_KERNEL)
#   /system/modules    i moduli del kernel da caricare dal disco (oggi tutti quelli che
#                      servono stanno nell'archivio di avvio, passo 3)
for c in /system/bin /system/lib /system/resources/fonts /system/resources/icons \
         /system/resources/wallpapers /system/firmware /system/modules; do
    metti_cartella "$c" 0 755
done

# Le risorse del sistema: i file di risorse/ (preparati da strumenti/grafica/genera.py: i
# caratteri e le icone gia' disegnati, misura per misura, e lo sfondo) vanno tali e quali in
# /system/resources (fonts/, icons/, wallpapers/). Non sono dentro i programmi: GKit li
# legge dal disco (gkit/materiali_file.c) e UIKit carica i suoi la prima volta che servono
# (uikit/materiali.h). Sono di tutti da leggere: 644.
(cd risorse && find . -type f | sort) | while read -r f; do
    metti_file "risorse/${f#./}" "/system/resources/${f#./}" 644
done

# La configurazione predefinita dei servizi: i file di utente/config/ tali e quali in
# /system/config (oggi network/network.conf, letto dal servizio network). Di tutti da
# leggere: 644. Quella della persona, quando ci sara', stara' in /data.
(cd utente/config && find . -type f | sort) | while read -r f; do
    metti_file "utente/config/${f#./}" "/system/config/${f#./}" 644
done

# /etc/resolv.conf: musl ci cerca i server dei nomi (DNS), ma "/" e' in sola lettura. Come
# fa systemd-resolved, e' un collegamento simbolico a un file in memoria, che il servizio
# network scrive quando sa quali sono (/run/network/resolv.conf).
metti_cartella /etc 0 755
ln -s /run/network/resolv.conf "$(vero /etc/resolv.conf)"
printf '  %-40s -> /run/network/resolv.conf
' /etc/resolv.conf

SERVIZI=""
APPS=""

# I programmi del sistema (utente/programmi): init in /system, i servizi in
# /system/services/<servizio> (il programma si chiama come il servizio: cosi' lo si
# riconosce nell'elenco dei processi). Sono programmi: 755.
for f in utente/programmi/*.c; do
    nome=$(basename "$f" .c)
    $CC $UTENTE -c "$f" -o "$B/oggetti/$nome.o"
    lega "$B/oggetti/$nome" $LIBRERIA "$B/oggetti/$nome.o"
    case "$nome" in
        init) posto=/system/init ;;
        shell) posto=/system/bin/$nome ;;   # la shell di prova
        *)    posto=/system/services/$nome; SERVIZI="$SERVIZI $nome" ;;
    esac
    metti_file "$B/oggetti/$nome" "$posto" 755
done

# Le applicazioni (utente/applicazioni/<Nome>/). Per ognuna:
#   1. si compila il programma, code (i suoi .c legati insieme);
#   2. si prepara il suo PACCHETTO, una cartella con dentro (APPLICAZIONI.md, sezione 3):
#        manifest.toml  il manifesto, con "@PROCESSORE@" sostituito dal processore per cui
#                       si compila (x86_64 o arm64): il sorgente e' uno, i pacchetti uno per
#                       processore, e cosi' il manifesto non puo' sbagliare processore
#        code           il programma
#        resources/     le icone (la cartella resources/ dei sorgenti) e tutti gli altri file
#                       della cartella dei sorgenti che non sono .c, .h o il manifesto (per
#                       esempio doom1.wad)
#      e lo si chiude in uno ZIP: $B/pacchetti/<id>.app (le date fisse, per avere sempre lo
#      stesso file);
#   3. le app DEL SISTEMA si mettono scompattate in /system/apps/<id>/, con lo stesso
#      contenuto del pacchetto (code 755, il resto 644, le cartelle 755, tutto del sistema).
#      Le app di utente/prove/applicazioni/ (Esempio, Contatore) NON sono del sistema: si
#      compilano e si impacchettano come le altre, ma vanno solo nei dischi delle prove
#      (passo 6), mai in disco.img. In quei dischi quelle di DA_INSTALLARE hanno il pacchetto
#      sulla Scrivania della persona, per provare l'installazione (si apre con Files: il
#      launcher fa partire lo Store, che chiede se installarla); le altre sono scompattate
#      in /system/apps.
# L'id dell'app (nel manifesto) deve essere il nome della cartella dei sorgenti in minuscolo.
DA_INSTALLARE="contatore"
PACCHETTI="$B/pacchetti"
mkdir -p "$PACCHETTI"
PACCHETTI_ASSOLUTO=$(cd "$PACCHETTI" && pwd)
PROVE_APPS=""
for cartella in utente/applicazioni/*/ utente/prove/applicazioni/*/; do
    nome=$(basename "$cartella" | tr A-Z a-z)
    mkdir -p "$B/oggetti/applicazioni/$nome"
    oggetti=""
    for f in "$cartella"*.c; do
        o="$B/oggetti/applicazioni/$nome/$(basename "$f" .c).o"
        $CC $UTENTE -I"$cartella" -c "$f" -o "$o"
        oggetti="$oggetti $o"
    done
    lega "$B/oggetti/applicazioni/$nome/code" $LIBRERIA $oggetti

    manifesto="${cartella}manifest.toml"
    [ -f "$manifesto" ] || { echo "build.sh: manca $manifesto"; exit 1; }
    grep -q "^id = \"$nome\"" "$manifesto" || { echo "build.sh: in $manifesto l'id non e' \"$nome\""; exit 1; }
    grep -q '^processore = "@PROCESSORE@"' "$manifesto" ||
        { echo "build.sh: in $manifesto manca processore = \"@PROCESSORE@\""; exit 1; }
    pacchetto="$B/oggetti/pacchetti/$nome"
    mkdir -p "$pacchetto/resources"
    sed "s/@PROCESSORE@/$CPU/" "$manifesto" > "$pacchetto/manifest.toml"
    cp "$B/oggetti/applicazioni/$nome/code" "$pacchetto/code"
    for r in "$cartella"*; do
        case "$r" in *.c|*.h|*/manifest.toml) continue ;; esac
        [ -f "$r" ] && cp "$r" "$pacchetto/resources/"
    done
    [ -d "${cartella}resources" ] && cp -R "${cartella}resources/." "$pacchetto/resources/"
    find "$pacchetto" -type d -exec chmod 755 {} +
    find "$pacchetto" -type f -exec chmod 644 {} +
    chmod 755 "$pacchetto/code"
    find "$pacchetto" -exec touch -d "2026-01-01 00:00:00" {} +
    rm -f "$PACCHETTI/$nome.app"
    (cd "$pacchetto" && find manifest.toml code resources | sort | zip -X -q -D "$PACCHETTI_ASSOLUTO/$nome.app" -@)
    printf '  %-40s %7s byte  pacchetto\n' "$PACCHETTI/$nome.app" "$(wc -c < "$PACCHETTI/$nome.app")"

    case "$cartella" in
    utente/prove/*)
        # le app di prova: solo nei dischi delle prove (passo 6), mai in disco.img
        PROVE_APPS="$PROVE_APPS $nome"
        ;;
    *)
        (cd "$pacchetto" && find . -type f | sort) | while read -r f; do
            f=${f#./}
            permessi=644
            [ "$f" = code ] && permessi=755
            metti_file "$pacchetto/$f" "/system/apps/$nome/$f" $permessi
        done
        APPS="$APPS $nome"
        ;;
    esac
done
echo "  app di prova (solo nei dischi delle prove):$PROVE_APPS"

# --- la partizione data: /data, in lettura e scrittura ---

# Le applicazioni installate dalla persona: all'inizio nessuna. Le installa il launcher
# (pacchetto.c), come quelle di /system/apps (di 0, 755); il launcher e la scrivania le
# cercano qui dopo /system/apps.
metti_cartella /data/apps 0 755

# Le cose del solo sistema: chi sono gli utenti, i loro permessi, le applicazioni
# installate. Per ora vuota (gli utenti sono fissi: chiamate.h), ma nessun altro ci entra.
metti_cartella /data/system 0 700

# Lo stato e i registri dei servizi: una cartella ciascuno, sua. I servizi girano come
# sistema (uid 0: li fa partire init), quindi sono del sistema, e solo sue (700).
metti_cartella /data/services 0 755
for nome in $SERVIZI; do metti_cartella /data/services/$nome 0 700; done

# La persona (per ora una sola, 1000: le applicazioni girano con il suo uid). La sua
# cartella e' sua e di nessun altro (700), e dentro:
#   home              i suoi file; i programmi la vedono come /home (libreria.c). Dentro
#                     c'e' Scrivania, e quello che c'e' in file/ nel repository
#   bin               i suoi comandi, cercati dopo /system/bin (quando ci sara' una shell)
#   appsdata/<app>    i dati di ogni applicazione
#   preferences       le sue preferenze
UTENTE_UID=1000
CASA=/data/users/$UTENTE_UID
metti_cartella /data/users 0 755
for c in "$CASA" "$CASA/home" "$CASA/home/Scrivania" "$CASA/bin" "$CASA/appsdata" "$CASA/preferences"; do
    metti_cartella "$c" $UTENTE_UID 700
done
for nome in $APPS; do metti_cartella "$CASA/appsdata/$nome" $UTENTE_UID 700; done

# La cartella file/ del repository va nella casa: quello che ci si mette dentro si
# ritrova nel sistema (tranne il suo LEGGIMI.md, che parla della cartella). E' della
# persona: cartelle 755, file 644 (dentro la sua casa, che e' 700).
CASA_VERA=$(vero "$CASA/home")
for r in file/* file/.[!.]*; do
    if [ -e "$r" ] && [ "$r" != file/LEGGIMI.md ]; then
        cp -R "$r" "$CASA_VERA/"
        chown -R $UTENTE_UID:$UTENTE_UID "$CASA_VERA/$(basename "$r")"
        find "$CASA_VERA/$(basename "$r")" -type d -exec chmod 755 {} +
        find "$CASA_VERA/$(basename "$r")" -type f -exec chmod 644 {} +
        printf '  %-40s da file/\n' "$CASA/home/$(basename "$r")"
    fi
done
# Le date dei file della casa sono fisse (l'ultima sera prima dell'orologio delle prove,
# 1/1/2026: Files le scrive "ieri, 18:02"), non quelle della compilazione: cosi' quello
# che mostra Files e' sempre uguale, compilazione dopo compilazione.
find "$CASA_VERA" -exec touch -d "2025-12-31 18:02:00" {} +
echo "programmi pronti"

# I programmi delle prove (utente/prove): servono SOLO alle prove automatiche (test/), e
# nel disco normale non ci vanno. Per ora c'e' riempitivo, che dorme per sempre: in
# disco_prove.img (passo 6) ce ne sono RIEMPITIVI copie, e init le fa partire tutte.
mkdir -p "$B/oggetti/prove"
for f in utente/prove/*.c; do
    nome=$(basename "$f" .c)
    $CC $UTENTE -c "$f" -o "$B/oggetti/prove/$nome.o"
    lega "$B/oggetti/prove/$nome" $LIBRERIA "$B/oggetti/prove/$nome.o"
done
RIEMPITIVI=6

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

# --- 4. il kernel ---
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

# --- 5. (solo PC) il programma UEFI che avvia tutto ---
# All'accensione il firmware UEFI cerca un disco con una partizione EFI (FAT) e dentro il
# file \EFI\BOOT\BOOTX64.EFI. Ci mettiamo una UKI: un solo programma UEFI con dentro il
# kernel, l'archivio di avvio e la riga di comando. La fa efi-mkuki, che li attacca
# come "sezioni" al programmino UEFI di systemd che sa far partire Linux (systemd-stub).
# Su ARM QEMU carica il kernel e l'archivio da se' (-kernel, -initrd): la riga di comando
# gliela passa run.sh (-append), da riga_kernel.txt.
if [ "$CPU" = x86_64 ]; then
    printf 'NAME="mshell"\nID=mshell\nPRETTY_NAME="mshell (Linux %s)"\n' "$VERSIONE_K" > /tmp/os-release
    efi-mkuki -c "$RIGA_KERNEL" -k "$VERSIONE_K" -r /tmp/os-release \
        -S /usr/lib/systemd/boot/efi/linuxx64.efi.stub -o "$B/BOOTX64.EFI" "$B/linux" "$B/avvio.img"
    echo "$B/BOOTX64.EFI pronto"
else
    echo "$RIGA_KERNEL" > "$B/riga_kernel.txt"
fi

# --- 6. il disco: GPT con tre partizioni ---
# Un file grande come il disco, con dentro:
#   - la TABELLA DELLE PARTIZIONI GPT, all'inizio (e una copia in fondo). La scrive
#     sfdisk (di util-linux, gli attrezzi di Linux per le partizioni) da un piccolo
#     "copione": dove comincia ogni partizione e quanto e' grande (in settori da 512
#     byte), il tipo (U: partizione EFI; L: file system di Linux) e il NOME, che avvio
#     usa per trovarle (Linux lo mette in /sys/class/block/<partizione>/uevent, PARTNAME).
#   - le tre partizioni, ognuna formattata al suo posto: si dice a mkfs.ext4 e a mtools
#     da quale byte del file cominciare (offset), e non si tocca il resto.
# Ogni partizione comincia a un MB tondo (i dischi veri lo preferiscono). 64 MB ciascuna:
# tanti per i programmi e i file di oggi, pochi da copiare (il collaudo copia il disco a
# ogni prova). Il file e' "sparso": i pezzi mai scritti non occupano posto, dove il file
# system del computer che compila lo sa fare.
# Il formato del disco e' lo stesso per tutte le CPU; cambiano i programmi dentro.
MB=1048576
SETTORI_MB=2048                 # settori da 512 byte in un MB
INIZIO_AVVIO=1;    MISURA_AVVIO=64       # in MB
INIZIO_SISTEMA=65; MISURA_SISTEMA=64
INIZIO_DATI=129;   MISURA_DATI=64
TOTALE=194                      # l'ultimo MB tiene la copia della tabella
rm -f "$B/disco.img"
truncate -s ${TOTALE}M "$B/disco.img"
sfdisk -q "$B/disco.img" <<FINE
label: gpt
start=$((INIZIO_AVVIO * SETTORI_MB)), size=$((MISURA_AVVIO * SETTORI_MB)), type=U, name="avvio"
start=$((INIZIO_SISTEMA * SETTORI_MB)), size=$((MISURA_SISTEMA * SETTORI_MB)), type=L, name="system"
start=$((INIZIO_DATI * SETTORI_MB)), size=$((MISURA_DATI * SETTORI_MB)), type=L, name="data"
FINE

# 1. avvio: FAT32 (-F), come la vuole UEFI. mtools scrive nella FAT senza montarla
#    (mformat la crea, mmd fa le cartelle, mcopy copia i file); "disco.img@@<byte>" vuol
#    dire "la FAT che comincia a quel byte". -T: quanti settori.
AVVIO="$B/disco.img@@$((INIZIO_AVVIO * MB))"
mformat -i "$AVVIO" -F -T $((MISURA_AVVIO * SETTORI_MB)) -v AVVIO ::
mmd -i "$AVVIO" ::/EFI ::/EFI/BOOT
[ "$CPU" = x86_64 ] && mcopy -i "$AVVIO" "$B/BOOTX64.EFI" ::/EFI/BOOT/BOOTX64.EFI

# 2. system e 3. data: ext4. mkfs.ext4 (di e2fsprogs, gli attrezzi di Linux per ext2/3/4)
#    scrive un ext4 vuoto e, con -d, ci copia dentro l'albero, con proprietari, permessi e
#    date. -E offset: da quale byte del file; l'ultimo numero: quanti blocchi da 4096 byte.
#    -L: il nome del file system (lo stesso della partizione: si vede anche da li').
mkfs.ext4 -q -F -b 4096 -L system -E offset=$((INIZIO_SISTEMA * MB)) -d "$ALBERO_SISTEMA" \
    "$B/disco.img" $((MISURA_SISTEMA * MB / 4096))
mkfs.ext4 -q -F -b 4096 -L data -E offset=$((INIZIO_DATI * MB)) -d "$ALBERO_DATI" \
    "$B/disco.img" $((MISURA_DATI * MB / 4096))
echo "$B/disco.img pronto: GPT con avvio ($MISURA_AVVIO MB, FAT32), system ($MISURA_SISTEMA MB, ext4) e data ($MISURA_DATI MB, ext4)"

# IL DISCO DELLE PROVE: una copia di disco_app.img in cui la partizione data ha in piu' la
# cartella /data/tests, con i programmi delle prove (init fa partire tutto quello che c'e'
# li'). Le altre due partizioni sono le stesse; data si riscrive da capo, dallo stesso
# albero piu' la cartella. Lo usano solo le prove che lo chiedono (DISCO in test/prove/);
# acceso a mano (run.sh), il sistema usa sempre disco.img.
# IL DISCO DELLE APP DI PROVA (disco_app.img): come disco.img, ma con in piu' le app di
# prova (utente/prove/applicazioni): quelle di DA_INSTALLARE come .app
# sulla Scrivania della persona (suoi, 644: per provare l'installazione come la farebbe
# lei), le altre scompattate in /system/apps. Cambia anche system, quindi si riscrive.
# E' il disco delle prove che usano Esempio e Contatore (DISCO in test/prove/), senza i
# programmi di /data/tests; i due dischi qui sotto partono da lui.
for nome in $PROVE_APPS; do
    pacchetto="$B/oggetti/pacchetti/$nome"
    case " $DA_INSTALLARE " in
    *" $nome "*)
        cp "$PACCHETTI/$nome.app" "$CASA_VERA/Scrivania/$nome.app"
        chown $UTENTE_UID:$UTENTE_UID "$CASA_VERA/Scrivania/$nome.app"
        chmod 644 "$CASA_VERA/Scrivania/$nome.app"
        printf '  %-40s da installare (solo prove)\n' "$CASA/home/Scrivania/$nome.app"
        ;;
    *)
        (cd "$pacchetto" && find . -type f | sort) | while read -r f; do
            f=${f#./}
            permessi=644
            [ "$f" = code ] && permessi=755
            metti_file "$pacchetto/$f" "/system/apps/$nome/$f" $permessi >/dev/null
        done
        metti_cartella "$CASA/appsdata/$nome" $UTENTE_UID 700 >/dev/null
        ;;
    esac
done
# date fisse della casa, come sopra
find "$CASA_VERA" -exec touch -d "2025-12-31 18:02:00" {} +
cp "$B/disco.img" "$B/disco_app.img"
mkfs.ext4 -q -F -b 4096 -L system -E offset=$((INIZIO_SISTEMA * MB)) -d "$ALBERO_SISTEMA" "$B/disco_app.img" $((MISURA_SISTEMA * MB / 4096))
mkfs.ext4 -q -F -b 4096 -L data -E offset=$((INIZIO_DATI * MB)) -d "$ALBERO_DATI" "$B/disco_app.img" $((MISURA_DATI * MB / 4096))
echo "$B/disco_app.img pronto: come disco.img, con in piu' le app di prova:$PROVE_APPS"
cp "$B/disco_app.img" "$B/disco_prove.img"
metti_cartella /data/tests 0 755 >/dev/null
for i in $(seq $RIEMPITIVI); do
    metti_file "$B/oggetti/prove/riempitivo" "/data/tests/riempitivo-$i" 755 >/dev/null
done
mkfs.ext4 -q -F -b 4096 -L data -E offset=$((INIZIO_DATI * MB)) -d "$ALBERO_DATI"     "$B/disco_prove.img" $((MISURA_DATI * MB / 4096))
echo "$B/disco_prove.img pronto: con $RIEMPITIVI riempitivi in /data/tests"

# IL DISCO DELLE PROVE DEI PACCHETTI: come disco_prove.img, ma in /data/tests c'e' solo il
# programma pacchetti (utente/prove/pacchetti.c), che chiede al launcher di descrivere e
# installare contatore.app. Sta in un disco suo perche' installa davvero (in /data/apps):
# le altre prove non devono vederlo. Lo usa test/prove/pacchetti.py.
cp "$B/disco_app.img" "$B/disco_pacchetti.img"
rm -f "$(vero /data/tests)"/riempitivo-*
metti_file "$B/oggetti/prove/pacchetti" "/data/tests/pacchetti" 755 >/dev/null
mkfs.ext4 -q -F -b 4096 -L data -E offset=$((INIZIO_DATI * MB)) -d "$ALBERO_DATI" \
    "$B/disco_pacchetti.img" $((MISURA_DATI * MB / 4096))
echo "$B/disco_pacchetti.img pronto: con il programma pacchetti in /data/tests"

# IL DISCO DELLA PROVA DELLA RETE: come disco_pacchetti.img, ma in /data/tests c'e' solo il
# programma rete (utente/prove/rete.c), che risolve un nome e apre una connessione HTTP, e
# scrive sulla seriale come va. Serve QEMU con la scheda di rete (-nic user,...): si accende
# a mano, come spiegato in rete.c.
cp "$B/disco_app.img" "$B/disco_rete.img"
rm -f "$(vero /data/tests)"/pacchetti
metti_file "$B/oggetti/prove/rete" "/data/tests/rete" 755 >/dev/null
mkfs.ext4 -q -F -b 4096 -L data -E offset=$((INIZIO_DATI * MB)) -d "$ALBERO_DATI"     "$B/disco_rete.img" $((MISURA_DATI * MB / 4096))
rm -rf "$ALBERO_SISTEMA" "$ALBERO_DATI"
echo "$B/disco_rete.img pronto: con il programma rete in /data/tests"

rm -rf "$B/oggetti"
echo "versioni: $(apk --root "$RADICE" info -v 2>/dev/null | grep -E '^(linux-|musl-)' | tr '\n' ' ')"

# DA IMPLEMENTARE
#   - NUCLEO=lts sui PC veri: linux-lts ha i driver dell'hardware vero (dischi NVMe e
#     SATA, schede video Intel/AMD, tastiere...), ma quali moduli servano dipende dalla
#     macchina: invece di un elenco fisso (MODULI), avvio dovrebbe caricare i moduli
#     dei dispositivi che trova (i "modalias" in /sys, confrontati con modules.alias,
#     come fa udev). Anche il firmware (linux-firmware: centinaia di MB) andrebbe messo
#     nel disco, solo quello che serve, in /system/firmware (gia' detto al kernel con
#     firmware_class.path). Qui si installa linux-firmware-none (niente firmware).
#   - Un PC vero: disco.img si scrive cosi' com'e' su una chiavetta (o un disco); il
#     firmware UEFI trova la partizione avvio e la UKI. La partizione data andrebbe
#     allargata fino alla fine del disco, la prima volta che parte.
#   - Secure Boot: la UKI non e' firmata; su un PC vero va spento, o la UKI firmata.
