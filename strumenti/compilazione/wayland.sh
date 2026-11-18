#!/bin/sh
# wayland.sh ARCH RADICE SCARICO USCITA CC: le librerie di Wayland (libwayland-server.a per il
# compositore, libwayland-client.a per i client di prova), compilate dai sorgenti ufficiali.
# La chiama programmi.mk (la regola di $(O)/wayland/libwayland-server.a), quindi sia build.sh
# (disco) sia la sandbox, nel container degli attrezzi.
#
#   ARCH     il processore per il compilatore (x86_64, aarch64)
#   RADICE   la radice di Alpine per quel processore (musl, header, libffi.a)
#   SCARICO  dove tenere gli archivi dei sorgenti scaricati (build/scarico)
#   USCITA   la cartella pronta: USCITA/libwayland-server.a, USCITA/libwayland-client.a e
#            USCITA/include/ (le intestazioni, quelle generate comprese); accanto, USCITA/chiave
#   CC       il compilatore con gli argomenti dei nostri programmi (la variabile CC di
#            programmi.mk): cosi' le librerie si compilano come tutto il resto
#
# COSA E'. Wayland e' il protocollo con cui un programma grafico dice al compositore "ecco la
# mia finestra" (un foglio di memoria condivisa piu' i messaggi su un socket). libwayland-server
# e' la parte che sta DENTRO il compositore (legge il socket, tiene gli oggetti, chiama le
# nostre funzioni); libwayland-client e' quella dei programmi (oggi solo il client di prova:
# domani GKit). Wayland dentro il compositore, non un ponte esterno: SISTEMA.md, "Wayland".
#
# PERCHE' DAI SORGENTI. Alpine ha wayland-libs-server, ma e' un pacchetto condiviso (.so) di un
# altro processore: noi vogliamo le librerie STATICHE per il nostro musl. Come per curl e
# libvterm si prende l'archivio ufficiale con la STESSA versione di Alpine 3.24 (wayland 1.25.0,
# wayland-protocols 1.48), scaricato da gitlab.freedesktop.org e controllato con lo sha256 qui
# sotto. Per cambiare versione: VERSIONE e SHA256 insieme.
#
# COME SI COMPILA, senza meson (non serve altro che il compilatore):
#   1. wayland-scanner, il programma che da' un file XML dei protocolli (protocol/wayland.xml,
#      xdg-shell.xml) fa le intestazioni e il codice C. Gira SOLO durante la compilazione: e'
#      dello stesso processore del sistema (si compila sempre nativi) e legato statico a
#      expat-static della radice. Non va nel disco.
#   2. con lo scanner: wayland-server-protocol.h e wayland-client-protocol.h, wayland-protocol.c,
#      e lo stesso per xdg-shell (stable, xdg-shell.xml);
#   3. i sorgenti delle librerie con il compilatore del sistema (musl, --sysroot della radice);
#      la libreria C ha tutto quello che serve (accept4, memfd_create, gettid, mremap, ...):
#      il config.h che meson genererebbe e' qui sotto, scritto a mano;
#   4. libffi (libffi.a, pacchetto libffi-dev nella radice) la lega il programma: wayland
#      la usa per chiamare le nostre funzioni dai messaggi (wl_closure_invoke).
#
# SI RIFA' SOLO SE SERVE: la chiave e' questo file piu' il compilatore con i suoi argomenti
# piu' i pacchetti installati nella radice.
set -e
ARCH=$1
RADICE=$2
SCARICO=$3
USCITA=$4
CC=$5
: "${ARCH:?}" "${RADICE:?}" "${SCARICO:?}" "${USCITA:?}" "${CC:?}"
case "$SCARICO" in /*) ;; *) SCARICO=$PWD/$SCARICO ;; esac
case "$USCITA" in /*) ;; *) USCITA=$PWD/$USCITA ;; esac

VERSIONE=1.25.0
SHA256=c065f040afdff3177680600f249727e41a1afc22fccf27222f15f5306faa1f03
PROTOCOLLI=1.48
SHA256_PROTOCOLLI=398036ac0eb6484982ddbde7ff86848d753231f9cdeeae983f06b52946625aa1

CHIAVE="$(md5sum < "$0" | cut -d' ' -f1) $ARCH $CC $(md5sum < "$RADICE/lib/apk/db/installed" | cut -d' ' -f1)"
if [ -f "$USCITA/libwayland-server.a" ] && [ "$(cat "$USCITA/chiave" 2>/dev/null)" = "$CHIAVE" ]; then
    exit 0
fi

# I sorgenti: scaricati una volta, controllati ogni volta (come curl.sh e libvterm.sh).
scarica() {   # scarica NOME URL SHA256
    mkdir -p "$SCARICO"
    if [ ! -f "$SCARICO/$1" ]; then
        wget -q -O "$SCARICO/$1.parziale" "$2"
        mv "$SCARICO/$1.parziale" "$SCARICO/$1"
    fi
    if ! echo "$3  $SCARICO/$1" | sha256sum -c -s; then
        rm -f "$SCARICO/$1"
        echo "wayland.sh: l'archivio $1 non ha lo sha256 atteso: buttato"
        exit 1
    fi
}
scarica "wayland-$VERSIONE.tar.xz" \
    "https://gitlab.freedesktop.org/wayland/wayland/-/releases/$VERSIONE/downloads/wayland-$VERSIONE.tar.xz" "$SHA256"
scarica "wayland-protocols-$PROTOCOLLI.tar.xz" \
    "https://gitlab.freedesktop.org/wayland/wayland-protocols/-/releases/$PROTOCOLLI/downloads/wayland-protocols-$PROTOCOLLI.tar.xz" \
    "$SHA256_PROTOCOLLI"

LAVORO=/tmp/wayland-$ARCH
rm -rf "$LAVORO"
mkdir -p "$LAVORO"
tar -xJf "$SCARICO/wayland-$VERSIONE.tar.xz" -C "$LAVORO"
tar -xJf "$SCARICO/wayland-protocols-$PROTOCOLLI.tar.xz" -C "$LAVORO" \
    "wayland-protocols-$PROTOCOLLI/stable/xdg-shell/xdg-shell.xml"
XDG="$LAVORO/wayland-protocols-$PROTOCOLLI/stable/xdg-shell/xdg-shell.xml"
cd "$LAVORO/wayland-$VERSIONE"

echo "wayland $VERSIONE: compilo ..."
# il config.h di meson, a mano: tutto quello che musl ha. Non c'e' sys/ucred.h (e' di BSD).
cat > config.h <<FINE
#define PACKAGE "wayland"
#define PACKAGE_VERSION "$VERSIONE"
#define HAVE_SYS_PRCTL_H 1
#define HAVE_ACCEPT4 1
#define HAVE_MKOSTEMP 1
#define HAVE_POSIX_FALLOCATE 1
#define HAVE_PRCTL 1
#define HAVE_MEMFD_CREATE 1
#define HAVE_MREMAP 1
#define HAVE_STRNDUP 1
#define HAVE_GETTID 1
#define HAVE_XUCRED_CR_PID 0
#define HAVE_BROKEN_MSG_CMSG_CLOEXEC 0
FINE
MAGGIORE=${VERSIONE%%.*}; RESTO=${VERSIONE#*.}; MINORE=${RESTO%%.*}; MICRO=${RESTO#*.}
sed -e "s/@WAYLAND_VERSION@/$VERSIONE/" -e "s/@WAYLAND_VERSION_MAJOR@/$MAGGIORE/" \
    -e "s/@WAYLAND_VERSION_MINOR@/$MINORE/" -e "s/@WAYLAND_VERSION_MICRO@/$MICRO/" \
    src/wayland-version.h.in > src/wayland-version.h

# 1. lo scanner. Il container e il sistema hanno lo stesso processore (si compila sempre nativo),
# quindi lo scanner si compila con lo stesso compilatore del sistema e si lega STATICO come i
# nostri programmi (come "lega" di programmi.mk: i pezzi di avvio di musl, expat-static della
# radice, compiler-rt): gira nel container, e non serve nessun compilatore "da host".
LIBC=$RADICE/usr/lib
RT=$(ls "$RADICE"/usr/lib/llvm*/lib/clang/*/lib/*/libclang_rt.builtins-*.a | head -n 1)
$CC -w -include config.h -Isrc -Iprotocol -c src/scanner.c -o scanner.o
$CC -w -include config.h -Isrc -Iprotocol -c src/wayland-util.c -o util.o
ld.lld -static --gc-sections -o wayland-scanner "$LIBC/crt1.o" "$LIBC/crti.o" scanner.o util.o \
    -L"$LIBC" -lexpat -lc "$RT" "$LIBC/crtn.o"

# 2. le intestazioni e il codice dei protocolli
mkdir -p gen
S=./wayland-scanner
$S -s server-header protocol/wayland.xml gen/wayland-server-protocol.h
$S -s client-header protocol/wayland.xml gen/wayland-client-protocol.h
$S -s public-code   protocol/wayland.xml gen/wayland-protocol.c
$S -s server-header "$XDG" gen/xdg-shell-server-protocol.h
$S -s client-header "$XDG" gen/xdg-shell-client-protocol.h
$S -s public-code   "$XDG" gen/xdg-shell-protocol.c

# 3. le librerie. Non e' codice nostro: gli avvisi si spengono (-w). -D_POSIX_C_SOURCE come
# meson; gli header e il codice generato si vedono da gen/ e src/.
mkdir -p o
COMPILA="$CC -w -std=c99 -D_POSIX_C_SOURCE=200809L -include config.h -I. -Isrc -Igen -Iprotocol"
for f in src/connection.c src/wayland-os.c src/wayland-util.c src/wayland-server.c \
         src/wayland-shm.c src/event-loop.c src/wayland-client.c gen/wayland-protocol.c \
         gen/xdg-shell-protocol.c; do
    $COMPILA -c "$f" -o "o/$(basename "$f" .c).o"
done
COMUNI="o/connection.o o/wayland-os.o o/wayland-util.o o/wayland-protocol.o o/xdg-shell-protocol.o"
rm -f libwayland-server.a libwayland-client.a
ar rcs libwayland-server.a $COMUNI o/wayland-server.o o/wayland-shm.o o/event-loop.o
ar rcs libwayland-client.a $COMUNI o/wayland-client.o

rm -rf "$USCITA"
mkdir -p "$USCITA/include"
cp libwayland-server.a libwayland-client.a "$USCITA/"
cp src/wayland-server.h src/wayland-server-core.h src/wayland-client.h src/wayland-client-core.h \
   src/wayland-util.h src/wayland-version.h gen/*.h "$USCITA/include/"
echo "$CHIAVE" > "$USCITA/chiave"
cd /
rm -rf "$LAVORO"
echo "wayland $VERSIONE compilato ($(wc -c < "$USCITA/libwayland-server.a") byte il server)"
