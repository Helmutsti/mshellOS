#!/bin/sh
# curl.sh ARCH RADICE SCARICO USCITA: il comando curl (/system/bin/curl), compilato dai
# sorgenti ufficiali. Lo chiama build.sh (passo 1 e 2), nel container degli attrezzi.
#
#   ARCH     il processore per il compilatore (x86_64, aarch64)
#   RADICE   la radice di Alpine per quel processore (musl, compiler-rt, mbedTLS)
#   SCARICO  dove tenere l'archivio dei sorgenti scaricato (build/scarico: resta da una
#            volta all'altra, come i pacchetti di Alpine)
#   USCITA   il programma pronto (build/<cpu>/curl); accanto, USCITA.chiave
#
# PERCHE' DAI SORGENTI. Alpine non ha un curl statico pronto: il suo pacchetto curl e'
# legato alle librerie condivise (e nel sistema non c'e' il caricatore dinamico), e
# curl-static contiene solo la libreria libcurl.a, non il programma. Il programma curl sta
# solo nei sorgenti ufficiali di curl, quindi si prendono quelli: la STESSA versione del
# pacchetto di Alpine 3.24, scaricata da curl.se e controllata con lo sha256 qui sotto
# (l'archivio e' lo stesso che usa Alpine: il suo sha512, nella ricetta APKBUILD di
# Alpine, coincide). Per cambiare versione: VERSIONE e SHA256 insieme.
#
# COME SI COMPILA. Con lo stesso compilatore dei nostri programmi (clang, per il processore,
# con la radice di Alpine come --sysroot: programmi.mk) e il configure di curl:
#   - HTTPS con mbedTLS, la libreria di Alpine (mbedtls-static, nella radice), legata
#     dentro il programma. I certificati di cui ci si fida sono quelli di Alpine, nel posto
#     solito di Linux, /etc/ssl/certs/ca-certificates.crt (build.sh ce li mette): curl li
#     cerca li' da solo e controlla sempre il certificato del server (nome e date), a meno
#     che chi lo lancia non dica -k;
#   - nessun'altra libreria: niente zlib, brotli, zstd (le risposte compresse), nghttp2
#     (HTTP/2), libpsl, libidn2 (i nomi con lettere non inglesi), libssh2, LDAP. Restano i
#     protocolli che curl fa da se' (http, https, ftp, ...);
#   - niente manuale dentro il programma (curl --manual): resta l'aiuto, curl --help;
#   - PROGRAMMA STATICO: "curl_LDFLAGS=-all-static" e' il modo di curl (con libtool) di
#     chiedere un programma senza librerie condivise. Il clang driver lega con i pezzi di
#     avvio di musl e con compiler-rt della radice (-rtlib=compiler-rt, e -resource-dir
#     per trovarlo nella radice e non in quella del container): niente gcc;
#   - -ffunction-sections -fdata-sections e --gc-sections, -s: come i nostri programmi,
#     dentro solo quello che si usa e senza i nomi delle funzioni.
#
# SI RIFA' SOLO SE SERVE: la chiave e' questo file (versione, sha256, argomenti) piu' i
# pacchetti installati nella radice (come la firma di programmi.mk). Se non sono cambiati
# e il programma c'e', non si fa niente.
set -e
ARCH=$1
RADICE=$2
SCARICO=$3
USCITA=$4
: "${ARCH:?}" "${RADICE:?}" "${SCARICO:?}" "${USCITA:?}"
# piu' sotto si compila in un'altra cartella: i percorsi dati diventano assoluti
case "$SCARICO" in /*) ;; *) SCARICO=$PWD/$SCARICO ;; esac
case "$USCITA" in /*) ;; *) USCITA=$PWD/$USCITA ;; esac

VERSIONE=8.23.0
SHA256=8dadd7d2536a314f982ad05a651b713f426bf7d02cc2c76cffb9796f2218e6c5

CHIAVE="$(md5sum < "$0" | cut -d' ' -f1) $ARCH $(md5sum < "$RADICE/lib/apk/db/installed" | cut -d' ' -f1)"
if [ -f "$USCITA" ] && [ "$(cat "$USCITA.chiave" 2>/dev/null)" = "$CHIAVE" ]; then
    exit 0
fi

# I sorgenti: scaricati una volta, controllati ogni volta (un archivio rovinato o diverso si
# butta: al giro dopo si riscarica).
ARCHIVIO="$SCARICO/curl-$VERSIONE.tar.xz"
mkdir -p "$SCARICO"
if [ ! -f "$ARCHIVIO" ]; then
    wget -q -O "$ARCHIVIO.parziale" "https://curl.se/download/curl-$VERSIONE.tar.xz"
    mv "$ARCHIVIO.parziale" "$ARCHIVIO"
fi
if ! echo "$SHA256  $ARCHIVIO" | sha256sum -c -s; then
    rm -f "$ARCHIVIO"
    echo "curl.sh: l'archivio di curl $VERSIONE non ha lo sha256 atteso: buttato"
    exit 1
fi

# Si compila in /tmp, dentro il container (la cartella condivisa con il computer e' lenta e
# non serve tenere gli oggetti: curl cambia di rado).
LAVORO=/tmp/curl-$ARCH
rm -rf "$LAVORO"
mkdir -p "$LAVORO"
tar -xJf "$ARCHIVIO" -C "$LAVORO"
cd "$LAVORO/curl-$VERSIONE"

# compiler-rt della radice: la cartella "risorse" di clang e' quella che contiene lib/
RT=$(ls "$RADICE"/usr/lib/llvm*/lib/clang/*/lib/*/libclang_rt.builtins-*.a | head -n 1)
RISORSE=${RT%/lib/*}
REGISTRO=$LAVORO/registro.txt

echo "curl $VERSIONE: compilo ..."
if ! ./configure \
        CC="clang --target=$ARCH-alpine-linux-musl --sysroot=$RADICE" \
        CFLAGS="-O2 -ffunction-sections -fdata-sections" \
        LDFLAGS="-static -fuse-ld=lld -rtlib=compiler-rt -unwindlib=none -resource-dir=$RISORSE -Wl,--gc-sections -s" \
        --host="$ARCH-alpine-linux-musl" --disable-shared --enable-static \
        --with-mbedtls \
        --without-zlib --without-brotli --without-zstd --without-nghttp2 \
        --without-libpsl --without-libidn2 --without-libssh2 --disable-ldap \
        --disable-docs --disable-manual \
        --with-ca-bundle=/etc/ssl/certs/ca-certificates.crt --without-ca-path \
        >"$REGISTRO" 2>&1; then
    tail -n 20 "$REGISTRO"
    echo "curl.sh: configure non riuscito (il resto in config.log)"
    exit 1
fi
if ! make -j"$(nproc)" curl_LDFLAGS=-all-static >>"$REGISTRO" 2>&1; then
    tail -n 30 "$REGISTRO"
    echo "curl.sh: la compilazione di curl non e' riuscita"
    exit 1
fi

# Deve essere statico: un programma con l'intestazione INTERP (il caricatore dinamico) nel
# sistema non partirebbe.
if readelf -l src/curl | grep -q INTERP; then
    echo "curl.sh: curl e' venuto legato alle librerie condivise, non statico"
    exit 1
fi

mkdir -p "$(dirname "$USCITA")"
cp src/curl "$USCITA"
echo "$CHIAVE" > "$USCITA.chiave"
cd /
rm -rf "$LAVORO"
echo "curl $VERSIONE compilato ($(wc -c < "$USCITA") byte)"
