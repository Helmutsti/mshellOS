#!/bin/sh
# app_linux.sh ARCH CARTELLA SCARICO USCITA: l'albero linux/ di un'APP LINUX, cioe' di un
# programma di altri preso dai pacchetti di Alpine 3.24 con tutte le sue librerie. Lo chiama
# programmi.mk, nel container degli attrezzi (build.sh e la sandbox); il pacchetto (.app) lo
# fa poi impacchetta.sh, come per le nostre app.
#
#   ARCH      il processore per Alpine (x86_64, aarch64): si prendono sempre i pacchetti del
#             processore del computer, quello del container (niente emulazione)
#   CARTELLA  i sorgenti dell'app, utente/applicazioni_linux/<Nome>/ (con la barra in fondo)
#   SCARICO   dove restano i pacchetti di Alpine scaricati (build/scarico: lo stesso posto di
#             build.sh, SCARICO/ARCH)
#   USCITA    dove va l'albero: USCITA/linux/; accanto, USCITA/chiave
#
# PERCHE'. Le app Linux di altri (decisione della persona) portano con se' le loro librerie,
# dentro il loro pacchetto, come le app del Mac: nel sistema non c'e' una cartella di
# librerie condivise, e ogni app ha le sue, nelle versioni con cui e' stata fatta. Partono
# come un programma Linux qualsiasi: il kernel fa partire il caricatore di musl del SISTEMA
# (/lib/ld-musl-<arch>.so.1, un collegamento a /system/lib: come dyld sul Mac), che cerca le
# librerie nelle cartelle di LD_LIBRARY_PATH, cioe' nel pacchetto
# (<app>/linux/lib:<app>/linux/usr/lib: lo imposta il launcher; lo script prepara l'albero).
#
# COSA C'E' IN USCITA/linux/: un piccolo albero tipo Alpine con SOLO quello che serve.
#   (niente caricatore: e' del sistema, ed e' anche la libreria C, libc.musl-<arch>.so.1)
#   lib/, usr/lib/            le librerie: la CHIUSURA delle dipendenze, cioe' quelle che il
#                             programma chiede (NEEDED, letto con readelf), quelle che chiedono
#                             loro, e cosi' via, ognuna nella cartella dove Alpine la mette
#   usr/bin/... e usr/share/  i programmi e i dati dei pacchetti di alpine.txt
#   etc/...                   i file della cartella linux/ dei sorgenti (per Foot:
#                             etc/fonts/fonts.conf), messi sopra a tutto il resto
# NIENTE COLLEGAMENTI SIMBOLICI: lo ZIP del pacchetto e l'installazione non li sanno tenere.
# Dove Alpine ha un collegamento (libfoo.so.1 -> libfoo.so.1.2.3), nel pacchetto c'e' il
# file vero con il nome che il caricatore cerca, quello scritto in NEEDED (il SONAME).
# Niente documentazione, manuali, header, librerie statiche, icone e .desktop di Linux.
#
# ALPINE.TXT: i pacchetti, uno per riga ("#" commento). Due forme:
#   foot                                   tutto quello che serve del pacchetto: i
#                                          programmi (bin, sbin, usr/bin, usr/sbin,
#                                          usr/libexec) e i dati (usr/share, tranne doc,
#                                          man, info, icone, .desktop, completamenti e
#                                          simili). NON le librerie (le porta la chiusura)
#                                          e NON etc/ (le configurazioni le mette l'app,
#                                          nella sua cartella linux/)
#   ncurses-terminfo usr/share/terminfo/f/foot   solo quel file (o cartella) del pacchetto
# Le librerie non si scrivono mai: le trova lo script.
#
# COME SI PRENDONO I PACCHETTI. Con apk, in una radice temporanea (come la radice di
# build.sh): i repository di Alpine 3.24 (main e community, /etc/apk/repositories del
# container), le firme controllate con le chiavi di Alpine per il processore, senza far
# girare gli script d'installazione (--no-scripts: sono per un sistema Alpine vero). Gli
# archivi restano in SCARICO/ARCH.
#
# SI RIFA' SOLO SE SERVE: la chiave e' questo script, il processore, alpine.txt e i file
# della cartella linux/ dei sorgenti. Se non sono cambiati e l'albero c'e', non si fa niente
# (e non si va in rete: le versioni restano quelle di quando si e' fatto). Per prendere le
# versioni nuove di Alpine si cancella USCITA.
set -e
ARCH=$1
CARTELLA=$2
SCARICO=$3
USCITA=$4
: "${ARCH:?}" "${CARTELLA:?}" "${SCARICO:?}" "${USCITA:?}"
case "$SCARICO" in /*) ;; *) SCARICO=$PWD/$SCARICO ;; esac
case "$USCITA" in /*) ;; *) USCITA=$PWD/$USCITA ;; esac
case "$CARTELLA" in */) ;; *) CARTELLA=$CARTELLA/ ;; esac
ELENCO="${CARTELLA}alpine.txt"
SOPRA="${CARTELLA}linux"
[ -f "$ELENCO" ] || { echo "app_linux.sh: manca $ELENCO"; exit 1; }

# la chiave: i file della cartella linux/ dei sorgenti contano con il nome e il contenuto
sopra=""
if [ -d "$SOPRA" ]; then sopra=$(cd "$SOPRA" && find . -type f | sort | xargs md5sum | md5sum | cut -d' ' -f1); fi
CHIAVE="$(md5sum < "$0" | cut -d' ' -f1) $ARCH $(md5sum < "$ELENCO" | cut -d' ' -f1) $sopra"
if [ -d "$USCITA/linux" ] && [ "$(cat "$USCITA/chiave" 2>/dev/null)" = "$CHIAVE" ]; then
    exit 0
fi

# Le righe di alpine.txt, senza commenti e righe vuote: "pacchetto" o "pacchetto percorso".
righe=$(sed -e 's/#.*//' -e 's/[[:space:]]*$//' -e '/^[[:space:]]*$/d' "$ELENCO")
pacchetti=$(echo "$righe" | awk '{print $1}' | sort -u)

# --- 1. i pacchetti, in una radice temporanea ---
R=/tmp/app_linux-$(basename "$CARTELLA")-$ARCH
rm -rf "$R"
mkdir -p "$SCARICO/$ARCH"
# shellcheck disable=SC2086
apk --arch "$ARCH" --root "$R" --initdb --no-scripts --quiet \
    --repositories-file /etc/apk/repositories --keys-dir "/usr/share/apk/keys/$ARCH" \
    --cache-dir "$SCARICO/$ARCH" add $pacchetti >/dev/null

# L'albero si prepara accanto e prende il posto del vecchio solo alla fine: se qualcosa va
# storto, resta quello di prima (e la chiave vecchia, quindi si riprova al giro dopo).
T="$USCITA/linux.nuovo"
rm -rf "$T"
mkdir -p "$T"

# prendi PERCORSO: copia R/PERCORSO in T/PERCORSO, seguendo i collegamenti (il file vero)
prendi() {
    mkdir -p "$T/$(dirname "$1")"
    cp -RL "$R/$1" "$T/$1"
}

# --- 2. programmi e dati dei pacchetti di alpine.txt ---
echo "$righe" | while read -r pacchetto percorso; do
    contenuto=$(apk --root "$R" info -L "$pacchetto" 2>/dev/null | sed -e '/ contains:$/d' -e '/^$/d')
    [ -n "$contenuto" ] || { echo "app_linux.sh: il pacchetto $pacchetto non c'e' ($ELENCO)"; exit 1; }
    if [ -n "$percorso" ]; then
        # solo un file o una cartella, che deve essere proprio di quel pacchetto
        percorso=${percorso#/}
        echo "$contenuto" | grep -q -e "^$percorso\$" -e "^$percorso/" ||
            { echo "app_linux.sh: $percorso non e' nel pacchetto $pacchetto ($ELENCO)"; exit 1; }
        prendi "$percorso"
        continue
    fi
    echo "$contenuto" | while read -r f; do
        case "$f" in
        bin/*|sbin/*|usr/bin/*|usr/sbin/*|usr/libexec/*) ;;
        usr/share/doc/*|usr/share/man/*|usr/share/info/*|usr/share/licenses/*) continue ;;
        usr/share/applications/*|usr/share/icons/*|usr/share/pixmaps/*) continue ;;
        usr/share/metainfo/*|usr/share/appdata/*|usr/share/pkgconfig/*|usr/share/aclocal/*) continue ;;
        usr/share/bash-completion/*|usr/share/zsh/*|usr/share/fish/*|usr/share/gtk-doc/*) continue ;;
        usr/share/*) ;;
        *) continue ;;
        esac
        prendi "$f"
    done
done

# --- 3. le librerie: la chiusura delle dipendenze ---
# Il caricatore di musl NON va nel pacchetto: sta nel sistema (/system/lib, con il collegamento
# /lib/ld-musl-<arch>.so.1 che i programmi hanno scritto dentro; decisione della persona,
# 18/11/2026, come dyld sul Mac). E' anche la libreria C (libc.musl-<arch>.so.1).

# chiede FILE: le librerie che FILE chiede (le voci NEEDED), una per riga
chiede() {
    readelf -d "$1" 2>/dev/null | sed -n 's/.*(NEEDED).*\[\(.*\)\]$/\1/p'
}
# E' un programma o una libreria (ELF)? Gli altri file (dati, caratteri) non chiedono niente.
elf() {
    [ "$(head -c 4 "$1" | od -An -c | tr -d ' ')" = "177ELF" ]
}

# Si parte dai programmi copiati e si aggiunge una libreria alla volta: ogni libreria nuova
# entra in "da_guardare", e il giro finisce quando non ne arrivano altre.
da_guardare=$(find "$T" -type f | while read -r f; do if elf "$f"; then echo "$f"; fi; done)
while [ -n "$da_guardare" ]; do
    nuove=""
    for f in $da_guardare; do
        for nome in $(chiede "$f"); do
            case "$nome" in libc.musl-*) continue ;; esac     # e' il caricatore del sistema
            if [ -f "$T/lib/$nome" ] || [ -f "$T/usr/lib/$nome" ]; then continue; fi
            if [ -e "$R/lib/$nome" ]; then dove=lib
            elif [ -e "$R/usr/lib/$nome" ]; then dove=usr/lib
            else
                echo "app_linux.sh: $f chiede $nome, che non e' in nessun pacchetto installato"
                exit 1
            fi
            prendi "$dove/$nome"
            nuove="$nuove $T/$dove/$nome"
        done
    done
    da_guardare=$nuove
done

# --- 4. i file dell'app, sopra a tutto ---
if [ -d "$SOPRA" ]; then cp -R "$SOPRA/." "$T/"; fi

# --- 5. permessi e controlli ---
# cartelle 755, file 644; eseguibili (755) solo i programmi
find "$T" -type d -exec chmod 755 {} +
find "$T" -type f -exec chmod 644 {} +
for d in bin sbin usr/bin usr/sbin usr/libexec; do
    if [ -d "$T/$d" ]; then find "$T/$d" -type f -exec chmod 755 {} +; fi
done
if [ -n "$(find "$T" -type l)" ]; then
    echo "app_linux.sh: nell'albero sono rimasti collegamenti simbolici"; exit 1
fi

rm -rf "$USCITA/linux" "$R"
mv "$T" "$USCITA/linux"
echo "$CHIAVE" > "$USCITA/chiave"
printf '  %-40s %7s file  %s\n' "$USCITA/linux" "$(find "$USCITA/linux" -type f | wc -l)" "$(du -sh "$USCITA/linux" | cut -f1)"
