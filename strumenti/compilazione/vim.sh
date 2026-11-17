#!/bin/sh
# vim.sh ARCH RADICE SCARICO USCITA CC: l'editor vim (/system/bin/vim) nella versione piu'
# piccola, compilato dai sorgenti ufficiali, con le voci terminfo che gli servono. Lo chiama
# programmi.mk (la regola di $(O)/vim/vim), quindi sia build.sh (disco) sia la sandbox, nel
# container degli attrezzi.
#
#   ARCH     il processore per il compilatore (x86_64, aarch64)
#   RADICE   la radice di Alpine per quel processore (musl, compiler-rt, ncurses)
#   SCARICO  dove tenere l'archivio dei sorgenti scaricato (build/scarico: resta da una
#            volta all'altra, come i pacchetti di Alpine e i sorgenti di curl)
#   USCITA   la cartella pronta: USCITA/vim (il programma), USCITA/defaults.vim (da mettere
#            in /system/share/vim/vim92) e USCITA/terminfo/<l>/<nome> (le descrizioni dei
#            terminali, da mettere in /etc/terminfo); accanto, USCITA/chiave
#   CC       il compilatore con gli argomenti dei nostri programmi (la variabile CC di
#            programmi.mk): cosi' vim si compila come tutto il resto
#
# PERCHE' DAI SORGENTI. Il pacchetto vim di Alpine e' legato alle librerie condivise (e nel
# sistema non c'e' il caricatore dinamico) ed e' la versione "huge", con i linguaggi di
# script. Si prendono quindi i sorgenti ufficiali, la STESSA versione del pacchetto di Alpine
# 3.24 (9.2.1352): l'archivio della versione su github.com/vim/vim, lo stesso che scarica
# Alpine (il suo sha512, nella ricetta APKBUILD di Alpine, coincide), controllato con lo
# sha256 qui sotto. Per cambiare versione: VERSIONE e SHA256 insieme.
#
# COME SI COMPILA. Con il compilatore e gli argomenti dei nostri programmi (CC) e il
# configure di vim:
#   - --with-features=tiny: la versione piu' piccola (come "vim-tiny" di Debian): niente
#     evidenziazione della sintassi, niente linguaggio di vim (:let, i plugin), niente
#     finestre divise in piu' di quanto serve, niente aiuto (:help vuole i file di runtime,
#     che non mettiamo). Le lettere UTF-8 ci sono sempre (vim 9 le ha in ogni versione);
#   - niente grafica (GUI, X, Wayland), niente traduzioni (NLS), niente linguaggi di script,
#     niente mouse della console (gpm), niente ACL/SELinux/attributi estesi, niente suoni,
#     niente cifratura (libsodium), niente canali e niente terminale dentro vim;
#   - LA LIBRERIA DEL TERMINALE. vim chiede al sistema come si parla al terminale (le
#     sequenze per muovere il cursore, i colori, i codici dei tasti) con tgetent/tgetstr, e
#     il suo configure si ferma se non trova una libreria che le ha: vim non sa fare a meno di
#     una libreria del terminale. Si usa ncurses di Alpine (ncurses-static nella radice),
#     legata dentro il programma: di ncurses vim usa solo la parte "terminfo" (leggere la
#     descrizione di un terminale), non le finestre di curses;
#   - le DESCRIZIONI DEI TERMINALI (terminfo): ncurses le legge da file, uno per terminale,
#     nelle cartelle scritte dentro la libreria quando Alpine l'ha compilata
#     (/etc/terminfo:/usr/share/terminfo:...), oppure dalla variabile TERMINFO. Ai programmi
#     arriva un ambiente minimo, senza TERMINFO: le voci si mettono quindi in /etc/terminfo,
#     la prima cartella che ncurses cerca, che sta nella partizione system (in sola lettura,
#     come /etc/ssl/certs per curl). Solo quelle che servono (sotto, TERMINALI), copiate dalla
#     radice di Alpine (ncurses-terminfo-base);
#   - --prefix=/system: dove vim cerca i suoi file: $VIM=/system/share/vim (il vimrc di
#     sistema, $VIM/vimrc: per scelta non c'e') e il "runtime" $VIMRUNTIME=$VIM/vim92. Del
#     runtime si mette UN SOLO file, defaults.vim, tale e quale dall'archivio: sono le
#     impostazioni di fabbrica di vim, che vim stesso legge quando la persona non ha un suo
#     ~/.vimrc (senza, vim parte con l'errore E1187 e chiede Invio a ogni avvio). Non e' un
#     vimrc di sistema: chi ha un ~/.vimrc non lo legge. Va in
#     /system/share/vim/vim92/defaults.vim;
#   - PROGRAMMA STATICO, come curl: -static, con lld e compiler-rt della radice, senza
#     gcc; --gc-sections e -s come i nostri programmi.
#
# SI RIFA' SOLO SE SERVE: la chiave e' questo file (versione, sha256, argomenti, terminali)
# piu' il compilatore con i suoi argomenti piu' i pacchetti installati nella radice. Se non
# sono cambiati e il programma c'e', non si fa niente.
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

VERSIONE=9.2.1352
SHA256=f491db14f8ef04f474e57dd7759347601442843163a57604dacd9132475b34e2

# I terminali di cui si mette la descrizione (la loro cartella in /etc/terminfo e' la prima
# lettera del nome):
#   xterm-256color  quello che la shell mette nella variabile TERM dei comandi (il Terminale,
#                   con libvterm, parla come xterm; la seriale di QEMU la si guarda da un
#                   terminale che capisce xterm)
#   xterm           lo stesso, per chi dice TERM=xterm
#   vt100           un terminale seriale qualsiasi, il minimo che tutti capiscono
#   linux           la console del kernel sullo schermo (il terminale di Linux)
TERMINALI="xterm-256color xterm vt100 linux"

CHIAVE="$(md5sum < "$0" | cut -d' ' -f1) $ARCH $CC $(md5sum < "$RADICE/lib/apk/db/installed" | cut -d' ' -f1)"
if [ -f "$USCITA/vim" ] && [ "$(cat "$USCITA/chiave" 2>/dev/null)" = "$CHIAVE" ]; then
    exit 0
fi

# I sorgenti: scaricati una volta, controllati ogni volta (un archivio rovinato o diverso si
# butta: al giro dopo si riscarica).
ARCHIVIO="$SCARICO/vim-$VERSIONE.tar.gz"
mkdir -p "$SCARICO"
if [ ! -f "$ARCHIVIO" ]; then
    wget -q -O "$ARCHIVIO.parziale" "https://github.com/vim/vim/archive/refs/tags/v$VERSIONE/vim-$VERSIONE.tar.gz"
    mv "$ARCHIVIO.parziale" "$ARCHIVIO"
fi
if ! echo "$SHA256  $ARCHIVIO" | sha256sum -c -s; then
    rm -f "$ARCHIVIO"
    echo "vim.sh: l'archivio di vim $VERSIONE non ha lo sha256 atteso: buttato"
    exit 1
fi

# Si compila in /tmp, dentro il container (come curl.sh): della cartella dei sorgenti si
# estraggono solo src/ e runtime/defaults.vim (il resto del runtime, i manuali e le prove
# non servono).
LAVORO=/tmp/vim-$ARCH
rm -rf "$LAVORO"
mkdir -p "$LAVORO"
tar -xzf "$ARCHIVIO" -C "$LAVORO" "vim-$VERSIONE/src" "vim-$VERSIONE/runtime/defaults.vim"
cd "$LAVORO/vim-$VERSIONE/src"

# compiler-rt della radice: la cartella "risorse" di clang e' quella che contiene lib/
RT=$(ls "$RADICE"/usr/lib/llvm*/lib/clang/*/lib/*/libclang_rt.builtins-*.a | head -n 1)
RISORSE=${RT%/lib/*}
REGISTRO=$LAVORO/registro.txt

echo "vim $VERSIONE: compilo ..."
# Non e' codice nostro: gli avvisi si spengono (-w), per non coprire i nostri. Il configure
# prova anche a far girare dei piccoli programmi (per esempio cosa risponde tgetent a un
# terminale che non c'e'): sono statici e per lo stesso processore, girano nel container.
if ! ./configure \
        CC="$CC -w" \
        LDFLAGS="-static -fuse-ld=lld -rtlib=compiler-rt -unwindlib=none -resource-dir=$RISORSE -Wl,--gc-sections -s" \
        --prefix=/system \
        --with-features=tiny \
        --enable-gui=no --without-x --without-wayland \
        --disable-nls --disable-acl --disable-gpm --disable-sysmouse \
        --disable-selinux --disable-smack --disable-xattr \
        --disable-canberra --disable-libsodium --disable-netbeans --disable-channel \
        --disable-terminal --disable-xsmp --disable-xsmp-interact \
        --disable-luainterp --disable-perlinterp --disable-pythoninterp \
        --disable-python3interp --disable-rubyinterp --disable-tclinterp \
        --disable-mzschemeinterp \
        --with-tlib=ncursesw \
        >"$REGISTRO" 2>&1; then
    tail -n 20 "$REGISTRO"
    echo "vim.sh: configure non riuscito (il resto in auto/config.log)"
    exit 1
fi
if ! make -j"$(nproc)" vim >>"$REGISTRO" 2>&1; then
    tail -n 30 "$REGISTRO"
    echo "vim.sh: la compilazione di vim non e' riuscita"
    exit 1
fi

# Deve essere statico: un programma con l'intestazione INTERP (il caricatore dinamico) nel
# sistema non partirebbe.
if readelf -l vim | grep -q INTERP; then
    echo "vim.sh: vim e' venuto legato alle librerie condivise, non statico"
    exit 1
fi

rm -rf "$USCITA"
mkdir -p "$USCITA"
cp vim "$USCITA/vim"
cp ../runtime/defaults.vim "$USCITA/defaults.vim"
# le descrizioni dei terminali, dalla radice (Alpine le tiene in /etc/terminfo)
for t in $TERMINALI; do
    l=$(printf '%s' "$t" | cut -c1)
    if [ ! -f "$RADICE/etc/terminfo/$l/$t" ]; then
        echo "vim.sh: nella radice manca la descrizione del terminale $t"
        exit 1
    fi
    mkdir -p "$USCITA/terminfo/$l"
    cp "$RADICE/etc/terminfo/$l/$t" "$USCITA/terminfo/$l/$t"
done
echo "$CHIAVE" > "$USCITA/chiave"
cd /
rm -rf "$LAVORO"
echo "vim $VERSIONE compilato ($(wc -c < "$USCITA/vim") byte)"
