#!/bin/sh
# La sandbox: compila (solo quello che e' cambiato) e prepara il sistema da mettere
# nell'immagine Docker. Gira nel container degli attrezzi (sandbox.py lo fa da solo):
#   docker run --rm -v <repo>:/w -v mshell-sandbox-oggetti:/o mshell-sandbox-attrezzi \
#       sh /w/strumenti/sandbox/compila.sh
#
# 1. make (strumenti/sandbox/Makefile): i programmi con il compilatore e gli argomenti di
#    build.sh; gli oggetti restano nel volume /o, quindi la seconda volta si rifa' solo
#    quello che e' cambiato.
# 2. L'ALBERO, come i due alberi delle partizioni di build.sh (passo 2) messi insieme: "/"
#    con /system, e /data. Stessi percorsi, proprietari e permessi; in piu' /.sandbox (il
#    programma avvio della sandbox e i programmi delle prove, che avvio mette in /data/tests
#    se la prova li chiede). Se build.sh cambia come mette i file, va cambiato anche qui.
# 3. build/sandbox/radice.tar (l'albero, con i proprietari) e il Dockerfile dell'immagine
#    (FROM scratch: dentro c'e' solo questo albero). L'immagine la fa sandbox.py.
set -e
cd /w
# CPU e ARCH arrivano da sandbox.py (il processore del computer: arm64 o x86_64)
: "${CPU:?}" "${ARCH:?}"
O=/o
make -s -f strumenti/sandbox/Makefile -j"$(nproc)" O=$O ARCH=$ARCH

A=/tmp/albero
rm -rf "$A"
mkdir -p "$A"
chmod 755 "$A"

# vero, cartelle_per, metti_file, metti_cartella: come in build.sh, su un albero solo
vero() { echo "$A$1"; }
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
metti_file() {
    cartelle_per "$2"
    cp "$1" "$(vero "$2")"
    chown 0:0 "$(vero "$2")"
    chmod "$3" "$(vero "$2")"
}
metti_cartella() {
    cartelle_per "$1/"
    chown "$2:$2" "$(vero "$1")"
    chmod "$3" "$(vero "$1")"
}

# --- system ---
for c in /dev /proc /sys /run /tmp /data; do metti_cartella "$c" 0 755; done
# mkfs.ext4 mette lost+found nella radice di ogni partizione (system e data)
metti_cartella /lost+found 0 700
for c in /system/bin /system/lib /system/resources/fonts /system/resources/icons \
         /system/resources/wallpapers /system/firmware /system/modules; do
    metti_cartella "$c" 0 755
done
# le risorse del sistema (font, icone, sfondi), come build.sh
(cd risorse && find . -type f | sort) | while read -r f; do
    metti_file "risorse/${f#./}" "/system/resources/${f#./}" 644
done
metti_file $O/bin/init /system/init 755
SERVIZI="compositore eventi launcher scrivania network"
for nome in $SERVIZI; do metti_file "$O/bin/$nome" "/system/services/$nome" 755; done
# la configurazione predefinita dei servizi, come build.sh. Il collegamento
# /etc/resolv.conf invece no: nel container quel file lo mette Docker. Il servizio network
# qui non tocca niente: Docker non gli da' il permesso di cambiare le schede, e lui lo dice
# e resta fermo (la rete del container la prepara Docker).
(cd utente/config && find . -type f | sort) | while read -r f; do
    metti_file "utente/config/${f#./}" "/system/config/${f#./}" 644
done
# i comandi, come build.sh: la shell di prova e quelli di utente/comandi
for nome in shell; do metti_file "$O/bin/$nome" "/system/bin/$nome" 755; done
for f in utente/comandi/*.c; do
    nome=$(basename "$f" .c)
    metti_file "$O/bin/comandi/$nome" "/system/bin/$nome" 755
done

# le applicazioni, come build.sh: il pacchetto (manifesto, code, resources); quelle di
# DA_INSTALLARE solo come .app sulla Scrivania, le altre scompattate in /system/apps.
# Ci sono anche Esempio e Contatore (utente/prove/applicazioni): la sandbox e' un ambiente
# di prova, come disco_prove.img.
DA_INSTALLARE="contatore"
APPS=""
SCRIVANIA_PACCHETTI=""
P=/tmp/pacchetti
rm -rf "$P"
for cartella in utente/applicazioni/*/ utente/prove/applicazioni/*/; do
    nome=$(basename "$cartella" | tr A-Z a-z)
    pacchetto="$P/$nome"
    mkdir -p "$pacchetto/resources"
    sed "s/@PROCESSORE@/$CPU/" "${cartella}manifest.toml" > "$pacchetto/manifest.toml"
    cp "$O/app/$nome/code" "$pacchetto/code"
    for r in "$cartella"*; do
        case "$r" in *.c|*.h|*/manifest.toml) continue ;; esac
        [ -f "$r" ] && cp "$r" "$pacchetto/resources/"
    done
    [ -d "${cartella}resources" ] && cp -R "${cartella}resources/." "$pacchetto/resources/"
    find "$pacchetto" -type d -exec chmod 755 {} +
    find "$pacchetto" -type f -exec chmod 644 {} +
    chmod 755 "$pacchetto/code"
    find "$pacchetto" -exec touch -d "2026-01-01 00:00:00" {} +
    case " $DA_INSTALLARE " in
    *" $nome "*)
        (cd "$pacchetto" && find manifest.toml code resources | sort | zip -X -q -D "$P/$nome.app" -@)
        SCRIVANIA_PACCHETTI="$SCRIVANIA_PACCHETTI $nome"
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

# --- data ---
metti_cartella /data/lost+found 0 700
metti_cartella /data/apps 0 755
metti_cartella /data/system 0 700
metti_cartella /data/services 0 755
for nome in $SERVIZI; do metti_cartella /data/services/$nome 0 700; done
UTENTE_UID=1000
CASA=/data/users/$UTENTE_UID
metti_cartella /data/users 0 755
for c in "$CASA" "$CASA/home" "$CASA/home/Scrivania" "$CASA/bin" "$CASA/appsdata" "$CASA/preferences"; do
    metti_cartella "$c" $UTENTE_UID 700
done
for nome in $APPS; do metti_cartella "$CASA/appsdata/$nome" $UTENTE_UID 700; done
CASA_VERA=$(vero "$CASA/home")
for r in file/* file/.[!.]*; do
    if [ -e "$r" ] && [ "$r" != file/LEGGIMI.md ]; then
        cp -R "$r" "$CASA_VERA/"
        chown -R $UTENTE_UID:$UTENTE_UID "$CASA_VERA/$(basename "$r")"
        find "$CASA_VERA/$(basename "$r")" -type d -exec chmod 755 {} +
        find "$CASA_VERA/$(basename "$r")" -type f -exec chmod 644 {} +
    fi
done
for nome in $SCRIVANIA_PACCHETTI; do
    cp "$P/$nome.app" "$CASA_VERA/Scrivania/$nome.app"
    chown $UTENTE_UID:$UTENTE_UID "$CASA_VERA/Scrivania/$nome.app"
    chmod 644 "$CASA_VERA/Scrivania/$nome.app"
done
# (le date della casa le sposta avvio della sandbox, a ogni accensione)
find "$CASA_VERA" -exec touch -d "2025-12-31 18:02:00" {} +

# --- solo della sandbox ---
metti_file $O/bin/avvio /.sandbox/avvio 755
metti_file $O/bin/prove/riempitivo /.sandbox/prove/riempitivo 755
metti_file $O/bin/prove/pacchetti /.sandbox/prove/pacchetti 755
metti_file $O/bin/prove/messaggi /.sandbox/prove/messaggi 755

mkdir -p /w/build/sandbox
(cd "$A" && tar -cf /w/build/sandbox/radice.tar --numeric-owner .)
cat > /w/build/sandbox/Dockerfile <<'FINE'
# fatto da strumenti/sandbox/compila.sh: il sistema di mshell, e nient'altro
FROM scratch
ADD radice.tar /
ENTRYPOINT ["/.sandbox/avvio"]
FINE
rm -rf "$A" "$P"
echo "sandbox: radice.tar pronto ($(wc -c < /w/build/sandbox/radice.tar) byte)"
