#!/bin/sh
# costruisci.sh: COSTRUISCE IL SISTEMA, da zero, nel container degli attrezzi
# (strumenti/Dockerfile; lo chiama avvia.ps1). Il risultato e' build/disco-<macchina>.img.
#
#   1. mmdebstrap crea in RADICE un Debian 13 con SOLO i pacchetti obbligatori ("essential") e
#      quelli di pacchetti.txt (approvati dalla persona), piu' le librerie che servono;
#   2. l'archivio d'avvio (initramfs) con dracut degli attrezzi, con i moduli del kernel del sistema;
#   3. le impostazioni: il nome del computer, la shell sulla console senza login;
#   4. si tolgono i pacchetti "da terminale" e quelli "per installare" (decisione della persona):
#      si cancellano i loro file, poi il registro dei pacchetti;
#   5. il kernel tutto-in-uno (UKI): kernel + initramfs + riga di comando in un solo file .EFI;
#   6. il disco: partizione "avvio" (FAT32, con \EFI\BOOT\BOOTX64.EFI) e "system" (ext4).
# Gira come root nel container (docker run --privileged: mmdebstrap monta /proc nella radice).
# L'argomento e' la MACCHINA (macchine/<nome>.conf): dice cosa serve a quella macchina.
set -e
cd /w
MACCHINA=${1:-qemu-pc}
. "macchine/$MACCHINA.conf"
B=/w/build
RADICE=/tmp/radice
mkdir -p "$B"
rm -rf "$RADICE"

# --- 1. il sistema dall'elenco ---
# SI RIFA' SOLO SE CAMBIA L'ELENCO: il sistema appena creato si tiene in un archivio
# (build/cache/radice-<impronta>.tar), con l'impronta di pacchetti.txt e di questo script. Se
# l'archivio c'e', si riparte da li' (secondi) invece di riscaricare e reinstallare (minuti).
PACCHETTI=$(sed 's/#.*//' pacchetti.txt | tr -s ' \t\n' '\n' | grep -v '^$' | paste -sd, -)
IMPRONTA=$(cat pacchetti.txt strumenti/Dockerfile | sha256sum | cut -c1-16)
ARCHIVIO="$B/cache/radice-$IMPRONTA.tar"
if [ ! -f "$ARCHIVIO" ]; then
    echo "1. mmdebstrap: essential + $PACCHETTI"
    mkdir -p "$B/cache"
    rm -f "$B/cache/radice-"*.tar
    mmdebstrap --quiet --mode=root --variant=essential --include="$PACCHETTI" \
        trixie "$B/cache/parziale.tar" http://deb.debian.org/debian
    mv "$B/cache/parziale.tar" "$ARCHIVIO"
else
    echo "1. il sistema dall'archivio (l'elenco non e' cambiato)"
fi
mkdir -p "$RADICE"
tar -xf "$ARCHIVIO" -C "$RADICE"

# --- 2. l'archivio d'avvio ---
KVER=$(ls "$RADICE/lib/modules")
echo "2. initramfs per il kernel $KVER"
# solo i driver della macchina (DRIVER_AVVIO), non tutti quelli di Debian
dracut --quiet --force --no-hostonly --kver "$KVER" --kmoddir "$RADICE/lib/modules/$KVER" \
    --drivers "$DRIVER_AVVIO" --filesystems ext4 "$B/initrd.img"

# --- 3. le impostazioni ---
echo "3. impostazioni"
echo mshell > "$RADICE/etc/hostname"
: > "$RADICE/etc/fstab"
# LA SHELL SULLA CONSOLE, SENZA LOGIN (decisione della persona): un servizio che apre /bin/sh
# come amministratore sul terminale dato (%I: tty1 e' lo schermo, ttyS0 la seriale), e lo
# riapre se si chiude. I getty (che chiederebbero nome e password con login) sono spenti.
S="$RADICE/etc/systemd/system"
cat > "$S/console-shell@.service" <<'FINE'
[Unit]
Description=Shell dell'amministratore su %I, senza login
After=systemd-user-sessions.service

[Service]
ExecStart=/bin/sh
StandardInput=tty
StandardOutput=tty
TTYPath=/dev/%I
TTYReset=yes
TTYVHangup=yes
Restart=always
WorkingDirectory=/root
Environment=HOME=/root TERM=linux

[Install]
WantedBy=multi-user.target
FINE
mkdir -p "$S/multi-user.target.wants"
for t in tty1 ttyS0; do ln -sf ../console-shell@.service "$S/multi-user.target.wants/console-shell@$t.service"; done
ln -sf /dev/null "$S/getty@.service"
ln -sf /dev/null "$S/serial-getty@.service"
ln -sf /dev/null "$S/getty-static.service"   # di Debian: i getty su tty2-tty6
# I SERVIZI CHE NON SERVONO (decisione della persona): il sistema si ricostruisce da capo a
# ogni modifica, quindi quello che rifanno dopo un aggiornamento si fa qui, una volta sola
# (l'elenco delle librerie, le spiegazioni dei messaggi del registro).
chroot "$RADICE" /sbin/ldconfig
journalctl --root="$RADICE" --update-catalog
for u in ldconfig.service systemd-journal-catalog-update.service systemd-update-done.service \
         systemd-sysext.socket systemd-sysext.service systemd-initctl.socket systemd-initctl.service \
         systemd-creds.socket; do
    ln -sf /dev/null "$S/$u"
done

# --- 4. via i pacchetti "da terminale" e "per installare" ---
DA_TERMINALE="bash grep sed findutils diffutils tar gzip hostname bsdutils ncurses-base ncurses-bin debianutils login"
PER_INSTALLARE="dpkg init-system-helpers perl-base sysvinit-utils mawk"
D="$RADICE/var/lib/dpkg"
dpkg-query --admindir="$D" -W -f '${Package} ${Installed-Size}\n' | sort > "$B/pacchetti-tutti.txt"
echo "4. tolgo: $DA_TERMINALE $PER_INSTALLARE"
for p in $DA_TERMINALE $PER_INSTALLARE; do
    elenco=$(ls "$D/info/$p.list" "$D/info/$p:"*.list 2>/dev/null | head -n 1)
    [ -n "$elenco" ] || continue
    while read -r f; do
        if [ -L "$RADICE$f" ] || [ -f "$RADICE$f" ]; then rm -f "$RADICE$f"; fi
    done < "$elenco"
done
# quello che resta, con la misura (KB): il "cosa c'e' dentro" del sistema
grep -v -w -E "^($(echo $DA_TERMINALE $PER_INSTALLARE | tr ' ' '|'))" "$B/pacchetti-tutti.txt" > "$B/pacchetti-sistema.txt"
rm -rf "$D" "$RADICE/var/cache/apt" "$RADICE/var/log/"*.log
# il kernel e' nel file tutto-in-uno: nella radice non serve
cp "$RADICE/boot/vmlinuz-$KVER" "$B/vmlinuz"
rm -rf "$RADICE/boot/"* "$RADICE/vmlinuz" "$RADICE/vmlinuz.old" "$RADICE/initrd.img" "$RADICE/initrd.img.old"

# --- 5. il kernel tutto-in-uno ---
echo "5. UKI"
ukify build --linux "$B/vmlinuz" --initrd "$B/initrd.img" \
    --cmdline "root=PARTLABEL=system rw console=tty1 console=ttyS0 $RIGA_KERNEL" \
    --output "$B/BOOTX64.EFI" >/dev/null

# --- 6. il disco ---
MB=1048576
SETTORI_MB=2048
MISURA_AVVIO=64
MISURA_SISTEMA=$(( $(du -sm "$RADICE" | cut -f1) * 13 / 10 + 64 ))
INIZIO_SISTEMA=$((1 + MISURA_AVVIO))
TOTALE=$((INIZIO_SISTEMA + MISURA_SISTEMA + 1))
echo "6. disco: avvio $MISURA_AVVIO MB, system $MISURA_SISTEMA MB"
rm -f "$B/disco-$MACCHINA.img"
truncate -s ${TOTALE}M "$B/disco-$MACCHINA.img"
sfdisk -q "$B/disco-$MACCHINA.img" <<FINE
label: gpt
start=$SETTORI_MB, size=$((MISURA_AVVIO * SETTORI_MB)), type=U, name="avvio"
start=$((INIZIO_SISTEMA * SETTORI_MB)), size=$((MISURA_SISTEMA * SETTORI_MB)), type=L, name="system"
FINE
AVVIO="$B/disco-$MACCHINA.img@@$MB"
mformat -i "$AVVIO" -F -T $((MISURA_AVVIO * SETTORI_MB)) -v AVVIO ::
mmd -i "$AVVIO" ::/EFI ::/EFI/BOOT
mcopy -i "$AVVIO" "$B/BOOTX64.EFI" ::/EFI/BOOT/BOOTX64.EFI
mkfs.ext4 -q -F -b 4096 -L system -E offset=$((INIZIO_SISTEMA * MB)) -d "$RADICE" \
    "$B/disco-$MACCHINA.img" $((MISURA_SISTEMA * MB / 4096))
echo "pronto: build/disco-$MACCHINA.img ($(wc -l < "$B/pacchetti-sistema.txt") pacchetti nel sistema, $(du -sm "$RADICE" | cut -f1) MB)"
