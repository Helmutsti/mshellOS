#!/bin/sh
# libvterm.sh ARCH RADICE SCARICO USCITA CC: la libreria libvterm (libvterm.a), compilata dai
# sorgenti ufficiali. La chiama programmi.mk (la regola di $(O)/libvterm/libvterm.a), quindi
# sia build.sh (disco) sia la sandbox, nel container degli attrezzi.
#
#   ARCH     il processore per il compilatore (x86_64, aarch64)
#   RADICE   la radice di Alpine per quel processore (musl, header)
#   SCARICO  dove tenere l'archivio dei sorgenti scaricato (build/scarico: resta da una
#            volta all'altra, come i pacchetti di Alpine e i sorgenti di curl)
#   USCITA   la cartella pronta: USCITA/libvterm.a e USCITA/include/ (vterm.h,
#            vterm_keycodes.h); accanto, USCITA/chiave
#   CC       il compilatore con gli argomenti dei nostri programmi (la variabile CC di
#            programmi.mk): cosi' la libreria si compila come tutto il resto
#
# COSA E'. libvterm e' la libreria di Neovim e Vim che fa il lavoro di un terminale: legge i
# byte che escono dal programma (le lettere e le sequenze di escape), tiene la griglia
# (lettera, colore e attributi di ogni casella, cursore, schermo alternativo) e traduce i
# tasti in quello che il programma si aspetta. Si lega solo nel Terminale.
#
# PERCHE' DAI SORGENTI. Alpine ha libvterm-static, ma e' un pacchetto della radice di un
# altro processore e puo' cambiare dentro la stessa versione: come per curl (curl.sh) si
# prende l'archivio ufficiale, la STESSA versione del pacchetto di Alpine 3.24 (0.3.3),
# scaricato da leonerd.org.uk e controllato con lo sha256 qui sotto (l'archivio e' lo stesso
# di Alpine: il suo sha512, nella ricetta APKBUILD di Alpine, coincide). Le tabelle dei
# caratteri (src/encoding/*.inc) sono gia' generate dentro l'archivio: niente perl.
# Per cambiare versione: VERSIONE e SHA256 insieme.
#
# SI RIFA' SOLO SE SERVE: la chiave e' questo file (versione, sha256) piu' il compilatore
# con i suoi argomenti piu' i pacchetti installati nella radice. Se non sono cambiati e la
# libreria c'e', non si fa niente.
set -e
ARCH=$1
RADICE=$2
SCARICO=$3
USCITA=$4
CC=$5
: "${ARCH:?}" "${RADICE:?}" "${SCARICO:?}" "${USCITA:?}" "${CC:?}"
# piu' sotto si compila in un'altra cartella: i percorsi dati diventano assoluti
case "$SCARICO" in /*) ;; *) SCARICO=$PWD/$SCARICO ;; esac
case "$USCITA" in /*) ;; *) USCITA=$PWD/$USCITA ;; esac

VERSIONE=0.3.3
SHA256=09156f43dd2128bd347cbeebe50d9a571d32c64e0cf18d211197946aff7226e0

CHIAVE="$(md5sum < "$0" | cut -d' ' -f1) $ARCH $CC $(md5sum < "$RADICE/lib/apk/db/installed" | cut -d' ' -f1)"
if [ -f "$USCITA/libvterm.a" ] && [ "$(cat "$USCITA/chiave" 2>/dev/null)" = "$CHIAVE" ]; then
    exit 0
fi

# I sorgenti: scaricati una volta, controllati ogni volta (un archivio rovinato o diverso si
# butta: al giro dopo si riscarica).
ARCHIVIO="$SCARICO/libvterm-$VERSIONE.tar.gz"
mkdir -p "$SCARICO"
if [ ! -f "$ARCHIVIO" ]; then
    wget -q -O "$ARCHIVIO.parziale" "https://www.leonerd.org.uk/code/libvterm/libvterm-$VERSIONE.tar.gz"
    mv "$ARCHIVIO.parziale" "$ARCHIVIO"
fi
if ! echo "$SHA256  $ARCHIVIO" | sha256sum -c -s; then
    rm -f "$ARCHIVIO"
    echo "libvterm.sh: l'archivio di libvterm $VERSIONE non ha lo sha256 atteso: buttato"
    exit 1
fi

# Si compila in /tmp, dentro il container (come curl.sh).
LAVORO=/tmp/libvterm-$ARCH
rm -rf "$LAVORO"
mkdir -p "$LAVORO"
tar -xzf "$ARCHIVIO" -C "$LAVORO"
cd "$LAVORO/libvterm-$VERSIONE"

echo "libvterm $VERSIONE: compilo ..."
# Non e' codice nostro: gli avvisi si spengono (-w), per non coprire i nostri. Lo standard e'
# quello del suo Makefile (C99); _DEFAULT_SOURCE per le funzioni di musl che usa.
for f in src/*.c; do
    $CC -w -std=c99 -D_DEFAULT_SOURCE -Iinclude -c "$f" -o "${f%.c}.o"
done
rm -f libvterm.a
ar rcs libvterm.a src/*.o

rm -rf "$USCITA"
mkdir -p "$USCITA/include"
cp libvterm.a "$USCITA/libvterm.a"
cp include/vterm.h include/vterm_keycodes.h "$USCITA/include/"
echo "$CHIAVE" > "$USCITA/chiave"
cd /
rm -rf "$LAVORO"
echo "libvterm $VERSIONE compilato ($(wc -c < "$USCITA/libvterm.a") byte)"
