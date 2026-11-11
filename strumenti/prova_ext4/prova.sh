#!/bin/sh
# Le prove del driver ext4 (src/fs/ext4/). Gira dentro un container Docker alpine con
# la cartella del progetto montata in /w. Da Git Bash:
#
#   MSYS_NO_PATHCONV=1 docker run --rm --platform linux/amd64 -v "$(pwd -W):/w" \
#       alpine:3.24 sh /w/strumenti/prova_ext4/prova.sh
#
# (CASI="moderno-4k in-linea" per farne solo alcuni.)
#
# COSA FA:
#   1. compila il driver con le opzioni del kernel (x86_64 e arm64): nessun avviso;
#   2. compila prova_ext4 (il driver + un disco finto fatto con un file);
#   3. per ogni caso: prepara un albero di file (nomi lunghi, permessi e proprietari
#      diversi, date strane, collegamenti corti e lunghi, file sparsi e frammentati...),
#      ne fa un disco con "mkfs.ext4 -d" e controlla che il driver legga:
#        - le stesse informazioni (tipo, permessi, proprietario, gruppo, collegamenti,
#          dimensione, data, destinazione dei collegamenti) che dice il sistema;
#        - gli stessi byte, file per file (cmp);
#        - nessun avviso (i checksum devono tornare tutti);
#   4. qualche disco rovinato o con funzioni che non sappiamo leggere: il driver deve
#      accorgersene.
set -eu
cd /w
apk add --no-cache gcc musl-dev clang lld e2fsprogs e2fsprogs-extra findutils >/dev/null

L=/tmp/prova_ext4
rm -rf $L
mkdir -p $L
SBAGLI=0
sbaglio() { echo "  SBAGLIATO: $*"; SBAGLI=$((SBAGLI + 1)); }
bene() { echo "  ok: $*"; }

# --- 1. con le opzioni del kernel (come build.sh) ---
echo "== compilazione con le opzioni del kernel"
INCLUDE_COMUNI="$(find src -path src/arch -prune -o -type d -print | sed 's/^/-I/') -Icomune"
for cpu in x86_64 arm64; do
    case $cpu in
    x86_64) NUDO="--target=x86_64-elf -ffreestanding -O2 -Wall -Wextra -mno-red-zone -mno-sse -mno-sse2 -mno-mmx -mcmodel=kernel" ;;
    arm64)  NUDO="--target=aarch64-none-elf -ffreestanding -O2 -Wall -Wextra -mgeneral-regs-only -mstrict-align" ;;
    esac
    INCLUDE="$INCLUDE_COMUNI $(find src/arch/$cpu -type d | sed 's/^/-I/')"
    mkdir -p $L/$cpu
    for f in src/fs/ext4/*.c; do
        # shellcheck disable=SC2086
        if ! clang $NUDO $INCLUDE -Werror -fstack-usage -c "$f" -o "$L/$cpu/$(basename "$f" .c).o" 2>$L/avvisi; then
            cat $L/avvisi
            sbaglio "$cpu: $f non compila senza avvisi"
        fi
    done
    pila=$(cat $L/$cpu/*.su | awk -F'\t' '{ if ($2 + 0 > m) { m = $2 + 0; f = $1 } } END { print m " byte (" f ")" }')
    bene "$cpu compila senza avvisi; la funzione con la pila piu' grande: $pila"
done

# --- 2. il programma di prova ---
gcc -O2 -g -Wall -Wextra -Werror -Istrumenti/prova_ext4/finto -Isrc/fs -Isrc/fs/ext4 \
    -o $L/prova_ext4 strumenti/prova_ext4/prova_ext4.c src/fs/ext4/*.c
gcc -O2 -Wall -o $L/buchi strumenti/prova_ext4/buchi.c
P=$L/prova_ext4

# --- 3. l'albero dei file ---
# albero CARTELLA [grande] [sparso] [frammentato BLOCCO N]
albero() {
    T=$1; shift
    rm -rf "$T"; mkdir -p "$T"
    mkdir "$T/piccoli" "$T/vuota" "$T/permessi"
    for n in 0 1 2 59 60 61 100 1023 1024 1025 2047 2048 4095 4096 4097 8191 65539 1048583; do
        head -c $n /dev/urandom > "$T/piccoli/f_$n"
    done
    head -c 20971520 /dev/urandom > "$T/grande_20M"
    printf 'ciao\n' > "$T/ciao.txt"
    ln "$T/ciao.txt" "$T/piccoli/stesso_ciao"            # due nomi, un inode
    # nomi lunghi: 255 byte UTF-8 (127 "è" da 2 byte + "x")
    lungo=$(awk 'BEGIN { for (i = 0; i < 127; i++) printf "\303\250"; printf "x" }')
    printf 'nome lungo\n' > "$T/$lungo"
    corto=$(printf %s "$lungo" | head -c 200)
    mkdir "$T/d_$corto"
    printf 'dentro\n' > "$T/d_$corto/€uro#1"
    # collegamenti: corto (dentro l'inode), lungo (in un blocco), verso cartelle, assoluti,
    # a catena, rotti
    ln -s ciao.txt "$T/l_corto"
    ln -s "$(awk 'BEGIN { for (i = 0; i < 60; i++) printf "./" }')piccoli/f_4097" "$T/l_lungo"
    ln -s "$(awk 'BEGIN { for (i = 0; i < 150; i++) printf "x/" }')rotto" "$T/l_rotto_lungo"
    ln -s piccoli "$T/l_cartella"
    ln -s /piccoli/f_100 "$T/l_assoluto"
    ln -s l_corto "$T/l_catena"
    ln -s "$(awk 'BEGIN { for (i = 0; i < 59; i++) printf "a" }')" "$T/l_59"   # 59: l'ultimo corto
    ln -s "$(awk 'BEGIN { for (i = 0; i < 60; i++) printf "a" }')" "$T/l_60"   # 60: il primo lungo
    # permessi e proprietari (anche oltre i 16 bit), date prima del 1970 e dopo il 2038
    for modo in 0 400 640 644 755 1777 2711 4755 6750 7777; do
        f="$T/permessi/m_$modo"
        printf '%s\n' "$modo" > "$f"
        chmod "$modo" "$f"
    done
    chown 0:0 "$T/permessi/m_644"
    chown 1000:1000 "$T/permessi/m_640"
    chown 65535:65534 "$T/permessi/m_755"
    chown 70000:80000 "$T/permessi/m_400"
    chown 123456789:987654321 "$T/permessi/m_4755"
    chown -h 4242:4343 "$T/l_corto"
    chown 555:666 "$T/vuota"
    chmod 0700 "$T/vuota"
    touch -d @0 "$T/permessi/m_0"
    touch -d @-1000000000 "$T/permessi/m_2711"          # 1938
    case " $* " in *" senza_futuro "*) ;; *)   # gli inode da 128 byte si fermano al 2038
        touch -d @4102444800 "$T/permessi/m_1777"       # 2100
        touch -d @13569465600 "$T/permessi/m_6750"      # 2400
    esac
    mkfifo "$T/permessi/coda"
    # cartelle profonde
    p="$T/profonda"
    for k in $(seq 1 30); do p="$p/$k"; done
    mkdir -p "$p"
    printf 'in fondo\n' > "$p/fondo"
    for scelta in "$@"; do
        case $scelta in
        grande)
            # 5000 file in una cartella: ci vuole l'indice htree
            mkdir "$T/cinquemila"
            ( cd "$T/cinquemila" && seq 1 5000 | while read -r k; do echo "$k" > "file_numero_$k"; done )
            ;;
        collegati)
            # 3000 nomi lunghi per lo stesso file: una cartella di piu' blocchi, un inode
            mkdir "$T/collegati"
            echo uno > "$T/collegati/originale"
            ( cd "$T/collegati" && seq 1 3000 | while read -r k; do
                ln originale "un_nome_abbastanza_lungo_per_riempire_i_blocchi_della_cartella_$k"; done )
            ;;
        sparso)
            # 5 GiB quasi tutti buco, con dati in pochi posti
            truncate -s 5G "$T/sparso_5G"
            for off in 0 1 4097 123456789 1073741824 4294967296 5368709000; do
                printf 'dati a %s' "$off" | dd of="$T/sparso_5G" bs=1 seek=$off conv=notrunc 2>/dev/null
            done
            # e uno che finisce con un buco
            truncate -s 3M "$T/buco_in_fondo"
            printf 'inizio' | dd of="$T/buco_in_fondo" conv=notrunc 2>/dev/null
            ;;
        frammentato=*)
            # un blocco si' e uno no: migliaia di extent, albero profondo
            v=${scelta#frammentato=}
            $L/buchi "$T/frammentato" "${v%,*}" "${v#*,}"
            ;;
        esac
    done
}

# Le informazioni che il driver deve dare, prese dal sistema (GNU find):
# percorso tipo permessi uid gid collegamenti dimensione data [-> destinazione].
# Le cartelle: dimensione "-" (dipende dal formato), collegamenti = 2 + sottocartelle.
atteso() {
    ( cd "$1" && find . -mindepth 1 -printf '/%P %y %m %U %G %n %s %T@ %l\n' ) | awk '
        { sub(/\.[0-9]*$/, "", $8); if ($8 == "") $8 = 0 }   # via le frazioni di secondo
        $2 == "p" { $2 = "a" }
        $2 == "d" { $7 = "-"; d = $1; sub(/\/[^\/]*$/, "", d); figlie[d]++ }
        { righe[NR] = $0; tipo[NR] = $2; nome[NR] = $1 }
        END {
            for (i = 1; i <= NR; i++) {
                r = righe[i]
                if (tipo[i] == "d") { split(r, c, " "); c[6] = 2 + figlie[nome[i]]
                    r = c[1]; for (k = 2; k <= 8; k++) r = r " " c[k] }
                else if (tipo[i] == "l") { split(r, c, " "); r = c[1]; for (k = 2; k <= 8; k++) r = r " " c[k]; r = r " -> " c[9] }
                else { split(r, c, " "); r = c[1]; for (k = 2; k <= 8; k++) r = r " " c[k] }
                print r
            }
        }' | LC_ALL=C sort
}
letto() {
    awk '$1 == "/" || $1 ~ /^\/lost\+found/ { next } $2 == "d" { $7 = "-" } { print }' | LC_ALL=C sort
}

# controlla NOME IMMAGINE ALBERO [-s SETTORE]
controlla() {
    nome=$1; img=$2; T=$3; shift 3
    if ! e2fsck -fn "$img" >$L/fsck 2>&1; then
        cat $L/fsck; sbaglio "$nome: e2fsck dice che l'immagine e' rovinata"
    fi
    # le informazioni
    if ! $P "$@" "$img" elenca / >$L/elenco 2>$L/err; then
        cat $L/err; sbaglio "$nome: elenca"
    fi
    [ -s $L/err ] && { cat $L/err; sbaglio "$nome: avvisi su un disco sano"; }
    letto <$L/elenco >$L/elenco.letto
    atteso "$T" >$L/elenco.atteso
    if cmp -s $L/elenco.letto $L/elenco.atteso; then
        bene "$nome: informazioni di $(wc -l <$L/elenco.atteso) nodi uguali"
    else
        diff $L/elenco.atteso $L/elenco.letto | head -20
        sbaglio "$nome: informazioni diverse"
    fi
    # i contenuti
    rm -rf $L/fuori
    if ! $P "$@" "$img" estrai $L/fuori 2>$L/err; then cat $L/err; sbaglio "$nome: estrai"; fi
    diversi=0; quanti=0
    for f in $(cd "$T" && find . -type f | sed 's|^\./||' | grep -v ' ' ); do
        quanti=$((quanti + 1))
        cmp -s "$T/$f" "$L/fuori/$f" || { echo "  diverso: $f"; diversi=$((diversi + 1)); }
    done
    # (i nomi con lo spazio non ci sono; quelli lunghi UTF-8 si':)
    if [ $diversi = 0 ]; then bene "$nome: $quanti file uguali byte per byte"
    else sbaglio "$nome: $diversi file diversi"; fi
    # leggere passando per i collegamenti, e le loro destinazioni
    for l in l_corto=ciao.txt l_lungo=piccoli/f_4097 l_cartella/f_1025=piccoli/f_1025 \
             l_assoluto=piccoli/f_100 l_catena=ciao.txt; do
        if ! $P "$@" "$img" leggi "/${l%=*}" >$L/via 2>$L/err || ! cmp -s $L/via "$T/${l#*=}"; then
            cat $L/err; sbaglio "$nome: leggere attraverso $l"
        fi
    done
    for l in l_corto l_lungo l_rotto_lungo l_59 l_60 l_assoluto; do
        d=$($P "$@" "$img" destinazione "/$l" 2>&1)
        [ "$d" = "$(readlink "$T/$l")" ] || sbaglio "$nome: destinazione di $l: '$d'"
    done
    bene "$nome: collegamenti seguiti e letti"
}

# fai CASO OPZIONI_MKFS MEGA OPZIONI_ALBERO...
fai() {
    nome=$1; opz=$2; mega=$3; shift 3
    case " ${CASI:-tutti} " in *" tutti "*|*" $nome "*) ;; *) return 0 ;; esac
    echo "== $nome: mkfs.ext4 $opz"
    T=$L/albero_$nome
    albero "$T" "$@"
    img=$L/$nome.img
    rm -f "$img"
    # shellcheck disable=SC2086
    if ! mkfs.ext4 -q -F $opz -d "$T" "$img" ${mega}M >$L/mkfs 2>&1; then
        cat $L/mkfs; sbaglio "$nome: mkfs.ext4"; return 0
    fi
    # mkfs.ext4 -d ferma le date al 2038 (2^31 - 1): quelle dopo si scrivono con debugfs.
    # Sul disco: i 32 bit bassi (letti CON il segno) piu' 2 bit di "epoca" da 2^32
    # secondi in i_mtime_extra (la tabella "extra epoch bits" della documentazione).
    case " $* " in *" senza_futuro "*) ;; *)
        for coppia in m_1777=4102444800 m_6750=13569465600; do
            t=${coppia#*=}
            basso=$((t & 0xFFFFFFFF))
            con_segno=$basso; [ $basso -ge 2147483648 ] && con_segno=$((basso - 4294967296))
            epoca=$(((t - con_segno) >> 32))
            debugfs -w -R "sif /permessi/${coppia%=*} mtime $basso" "$img" >/dev/null 2>&1
            debugfs -w -R "sif /permessi/${coppia%=*} mtime_extra $epoca" "$img" >/dev/null 2>&1
        done
    esac
}

dumpe2fs_funzioni() { dumpe2fs -h "$1" 2>/dev/null | grep -i 'features' | sed 's/^/  /'; }

# profondita' massima dell'albero degli extent di un file (debugfs "ex": "Level Entries")
profondita() {
    debugfs -R "ex $2" "$1" 2>/dev/null | awk 'NR == 2 { m = $2 } END { print m + 0 }'   # " 0/ 3 ...": la radice, di 3 livelli
}

# --- i casi ---

# 1. le funzioni di oggi (quelle di mkfs.ext4 senza opzioni), blocchi da 4 KB
fai moderno-4k "-b 4096" 256 grande sparso frammentato=4096,3000 && [ -f $L/moderno-4k.img ] && {
    img=$L/moderno-4k.img
    e2fsck -fyD "$img" >/dev/null 2>&1 || true      # -D: indicizza le cartelle (htree)
    debugfs -w -R "fallocate /buco_in_fondo 10 200" "$img" >/dev/null 2>&1   # extent non scritti
    dumpe2fs_funzioni "$img"
    debugfs -R "htree /cinquemila" "$img" 2>/dev/null | grep -q "Root node dump" \
        && bene "cinquemila: la cartella ha l'indice htree" || sbaglio "cinquemila: niente htree"
    debugfs -R "ex /buco_in_fondo" "$img" 2>/dev/null | grep -qi uninit \
        && bene "buco_in_fondo: ha extent non scritti" || sbaglio "buco_in_fondo: niente extent non scritti"
    p=$(profondita "$img" /frammentato)
    [ "$p" -ge 2 ] && bene "frammentato: albero degli extent profondo $p" || sbaglio "frammentato: profondita' solo $p"
    controlla moderno-4k "$img" $L/albero_moderno-4k
    # lo stesso disco con settori da 4096 byte
    if $P -s 4096 "$img" estrai $L/fuori4096 2>$L/err && [ ! -s $L/err ] &&
       cmp -s $L/fuori4096/grande_20M $L/albero_moderno-4k/grande_20M &&
       cmp -s $L/fuori4096/frammentato $L/albero_moderno-4k/frammentato; then
        bene "moderno-4k: anche con settori da 4096 byte"
    else
        cat $L/err; sbaglio "moderno-4k con settori da 4096 byte"
    fi
    rm -rf $L/fuori4096
}

# 2. blocchi da 1 KB: tre livelli di extent (oltre 4*84*84 = 28224 extent)
fai moderno-1k "-b 1024" 160 frammentato=1024,30000 && [ -f $L/moderno-1k.img ] && {
    p=$(profondita $L/moderno-1k.img /frammentato)
    [ "$p" -ge 3 ] && bene "frammentato: albero degli extent profondo $p" || sbaglio "frammentato: profondita' solo $p"
    controlla moderno-1k $L/moderno-1k.img $L/albero_moderno-1k
}

# 3. blocchi da 64 KB (le lunghezze delle voci di cartella scritte in modo speciale)
fai blocchi-64k "-b 65536" 256 collegati && [ -f $L/blocchi-64k.img ] && {
    e2fsck -fyD $L/blocchi-64k.img >/dev/null 2>&1 || true
    controlla blocchi-64k $L/blocchi-64k.img $L/albero_blocchi-64k -s 4096
}

# 4. senza extent: la mappa dei blocchi (diretti, indiretti, doppi, tripli)
fai mappa-1k "-b 1024 -O ^extent,^64bit" 128 sparso frammentato=1024,5000 && [ -f $L/mappa-1k.img ] &&
    controlla mappa-1k $L/mappa-1k.img $L/albero_mappa-1k
fai mappa-4k "-O ^extent,^64bit" 128 grande && [ -f $L/mappa-4k.img ] && {
    e2fsck -fyD $L/mappa-4k.img >/dev/null 2>&1 || true
    controlla mappa-4k $L/mappa-4k.img $L/albero_mappa-4k
}

# 5. i dati in linea (file e cartelle piccoli dentro l'inode)
fai in-linea "-O inline_data" 64 && [ -f $L/in-linea.img ] && {
    debugfs -R "stat /piccoli/f_100" $L/in-linea.img 2>/dev/null | grep -q "Size of inline data" \
        && bene "in-linea: f_100 sta nell'inode" || sbaglio "in-linea: f_100 non e' in linea"
    debugfs -R "stat /vuota" $L/in-linea.img 2>/dev/null | grep -q "Size of inline data" \
        && bene "in-linea: la cartella vuota sta nell'inode" || sbaglio "in-linea: cartella non in linea"
    controlla in-linea $L/in-linea.img $L/albero_in-linea
}

# 6. dischi piu' vecchi: senza metadata_csum (checksum crc16 dei descrittori), meta_bg,
#    inode da 128 byte (niente date oltre il 2038: niente prova delle date), ext2
fai vecchio "-O ^metadata_csum,^64bit,uninit_bg,meta_bg,^resize_inode" 64 && [ -f $L/vecchio.img ] &&
    controlla vecchio $L/vecchio.img $L/albero_vecchio
fai ext2 "-t ext2 -I 128" 64 senza_futuro && [ -f $L/ext2.img ] &&
    controlla ext2 $L/ext2.img $L/albero_ext2
fai meta-bg-1k "-b 1024 -O meta_bg,^resize_inode" 300 && [ -f $L/meta-bg-1k.img ] &&
    controlla meta-bg-1k $L/meta-bg-1k.img $L/albero_meta-bg-1k

# --- 4. dischi che il driver deve rifiutare o segnalare ---
case " ${CASI:-tutti} " in *" tutti "*|*" rifiuti "*)
echo "== dischi rovinati o con funzioni sconosciute"
mkdir -p $L/poco && echo ciao > $L/poco/a
for f in casefold encrypt; do
    mkfs.ext4 -q -F -O $f -d $L/poco $L/rif.img 16M >/dev/null 2>&1
    set +e; $P $L/rif.img monta 2>$L/err; r=$?; set -e
    [ $r = 3 ] && grep -q "$f" $L/err && bene "$f: rifiutato ($(cat $L/err | head -1))" || sbaglio "$f: non rifiutato"
done
head -c 16777216 /dev/zero > $L/rif.img
set +e; $P $L/rif.img monta 2>$L/err; r=$?; set -e
[ $r = 3 ] && bene "disco vuoto: rifiutato" || sbaglio "disco vuoto: accettato"
mkfs.ext4 -q -F -d $L/poco $L/rif.img 16M
debugfs -w -R "feature needs_recovery" $L/rif.img >/dev/null 2>&1
set +e; $P $L/rif.img leggi /a >$L/via 2>$L/err; r=$?; set -e
[ $r = 0 ] && grep -q journal $L/err && cmp -s $L/via $L/poco/a && bene "needs_recovery: montato con l'avviso" || { cat $L/err; sbaglio "needs_recovery"; }
mkfs.ext4 -q -F -d $L/poco $L/rif.img 16M
printf '\125' | dd of=$L/rif.img bs=1 seek=$((1024 + 0x3FC)) conv=notrunc 2>/dev/null
set +e; $P $L/rif.img leggi /a >$L/via 2>$L/err; r=$?; set -e
[ $r = 0 ] && grep -q "superblocco" $L/err && cmp -s $L/via $L/poco/a && bene "checksum del superblocco rovinato: avviso" || { cat $L/err; sbaglio "checksum del superblocco"; }
# un inode rovinato (un byte dei suoi campi): avviso sul checksum, si legge lo stesso
mkfs.ext4 -q -F -d $L/poco $L/rif.img 16M
ino=$(debugfs -R "stat /a" $L/rif.img 2>/dev/null | awk '/^Inode:/ { print $2; exit }')
# per rovinarlo davvero si scrive dritti sul disco: posto dell'inode = tabella + (ino-1)*256
tab=$(dumpe2fs $L/rif.img 2>/dev/null | awk '/Inode table at/ { split($4, a, "-"); print a[1]; exit }')
bs=$(dumpe2fs -h $L/rif.img 2>/dev/null | awk '/^Block size:/ { print $3 }')
isz=$(dumpe2fs -h $L/rif.img 2>/dev/null | awk '/^Inode size:/ { print $3 }')
printf '\001' | dd of=$L/rif.img bs=1 seek=$((tab * bs + (ino - 1) * isz + 8)) conv=notrunc 2>/dev/null
set +e; $P $L/rif.img leggi /a >$L/via 2>$L/err; r=$?; set -e
[ $r = 0 ] && grep -q "inode $ino" $L/err && cmp -s $L/via $L/poco/a && bene "inode rovinato: avviso" || { cat $L/err; sbaglio "inode rovinato"; }
;; esac

echo
if [ $SBAGLI = 0 ]; then echo "TUTTE LE PROVE PASSATE"; else echo "PROVE SBAGLIATE: $SBAGLI"; exit 1; fi
