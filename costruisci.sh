#!/bin/sh
# costruisci.sh: COSTRUISCE IL SISTEMA, da zero, nel container degli attrezzi
# (strumenti/Dockerfile; lo chiamano avvia.ps1 sul PC e avvia-mac.sh sul Mac). Il risultato e' build/disco-<macchina>.img.
#
#   1. mmdebstrap crea in RADICE un Debian 13 con SOLO i pacchetti obbligatori ("essential") e
#      quelli di pacchetti.txt (approvati dalla persona), piu' le librerie che servono;
#   2. l'archivio d'avvio (initramfs) con dracut degli attrezzi, con i moduli del kernel del sistema;
#   3. le impostazioni: il nome del computer, l'accesso automatico (greetd sullo schermo,
#      l'amministratore sulla seriale);
#      poi si compilano i programmi di mshell (utente/) e si mettono in /usr/bin;
#      lo sfondo, il puntatore e il carattere (risorse/) si preparano in /usr/share;
#   4. si tolgono i pacchetti "da terminale" e quelli "per installare" (decisione della persona):
#      si cancellano i loro file, poi il registro dei pacchetti;
#   5. il kernel tutto-in-uno (UKI): kernel + initramfs + riga di comando in un solo file .EFI;
#   6. il disco: partizione "avvio" (FAT32, con \EFI\BOOT\<FILE_AVVIO>) e "system" (ext4).
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
# il kernel non e' in pacchetti.txt: dipende dal processore, lo dice la macchina (KERNEL)
PACCHETTI=$( (sed 's/#.*//' pacchetti.txt; echo "$KERNEL $FIRMWARE") | tr -s ' \t\n' '\n' | grep -v '^$' | paste -sd, -)
IMPRONTA=$( (cat pacchetti.txt strumenti/Dockerfile; echo "$KERNEL $FIRMWARE") | sha256sum | cut -c1-16)
ARCHIVIO="$B/cache/radice-$IMPRONTA.tar"
if [ ! -f "$ARCHIVIO" ]; then
    echo "1. mmdebstrap: essential + $PACCHETTI"
    mkdir -p "$B/cache"
    rm -f "$B/cache/radice-"*.tar
    mmdebstrap --quiet --mode=root --variant=essential --components="main non-free-firmware" --include="$PACCHETTI" \
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
# L'ACCESSO SULLA SERIALE (SERIALE della macchina: ttyS0 sul PC, ttyAMA0 sul Mac; per le prove): agetty, il programma che apre la console, con
# --autologin root: fa partire login, che passa da PAM, e PAM (pam_systemd) registra la
# SESSIONE presso logind. Cosi' la shell e' una sessione vera: allo spegnimento si chiude bene,
# ha la sua cartella /run/user/0. La shell di root e' /bin/sh (dash): bash non c'e'.
# Lo schermo (tty1) e' di greetd: vedi piu' sotto, "LA SCRIVANIA".
S="$RADICE/etc/systemd/system"
mkdir -p "$S/serial-getty@$SERIALE.service.d"
cat > "$S/serial-getty@$SERIALE.service.d/accesso-automatico.conf" <<'FINE'
[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin root --keep-baud 115200,57600,38400,9600 - $TERM
FINE
sed -i 's#^\(root:[^:]*:[^:]*:[^:]*:[^:]*:[^:]*:\)/bin/bash$#\1/bin/sh#' "$RADICE/etc/passwd"
ln -sf /dev/null "$S/getty-static.service"   # di Debian: i getty su tty2-tty6
# LA SCRIVANIA PARTE DA SOLA, COME PERSONA NORMALE (decisione della persona). Il gestore
# dell'accesso e' greetd (la strada standard, la stessa che usera' la schermata di accesso):
# all'accensione apre una sessione di utente1 senza chiedere niente (initial_session). Passa da
# PAM come login, quindi logind registra la sessione su seat0. Quando la sessione finisce,
# greetd mostra agreety, l'accesso testuale del pacchetto (default_session, con l'utente _greetd
# creato dal pacchetto).
# greetd prende tty1 (Debian gli da' tty7): getty@tty1 non si accende piu' (via il suo
# collegamento in getty.target) e, come nel greetd originale, i due si escludono.
# Il pacchetto stesso abilita greetd come display-manager.service (lo vuole graphical.target).
#
# LA SCRIVANIA E' UN SERVIZIO DI SYSTEMD (la persona, 27/11/2026; come GNOME e niri). Prima la
# lanciava greetd direttamente, e systemd non sapeva quando la sessione grafica finiva: i
# portali delle app (che sono fatti per spegnersi con la sessione grafica, PartOf=
# graphical-session.target) restavano accesi senza schermo e si bloccavano, e dopo ogni
# riavvio della scrivania app e barra aspettavano 25 secondi a ogni apertura. Ora:
#   1. greetd lancia mshell-sessione (qui sotto), che passa a systemd dell'utente le
#      impostazioni della sessione (quale sessione e', la lingua) e gli chiede di avviare la
#      scrivania (mshell.service), aspettando finche' e' accesa;
#   2. systemd la avvia; quando e' pronta (systemd-notify nel comando "-s", NotifyAccess=all)
#      comincia la sessione grafica (graphical-session.target, legato con BindsTo);
#   3. quando si chiude, per un'uscita o per un errore, mshell-sessione chiude la sessione
#      grafica (mshell-fine.target, come niri-shutdown.target di niri): systemd spegne i
#      portali; poi toglie l'indirizzo dello schermo e finisce, e greetd vede la sessione
#      chiusa. Se invece si chiude la sessione di greetd (systemctl stop greetd, come fa la
#      prova veloce), mshell-sessione chiude la sessione grafica allo stesso modo, e con lei la
#      scrivania, prima di uscire.
#      La chiusura e' "senza eccezioni" (--job-mode=replace-irreversibly): un'app che perde lo
#      schermo, spegnendosi, richiama il portale, che chiede di riaccendere la sua parte
#      grafica; senza, quella richiesta rimasta a meta' blocca la chiusura (systemd: "transaction
#      is destructive") e i portali restano accesi senza schermo.
# Il compositore chiede schermo e tastiera a logind (libseat): da servizio e' fuori dalla
# sessione di greetd, e logind la riconosce dalla variabile XDG_SESSION_ID che gli passa
# mshell-sessione (come fanno GNOME e niri). utente1 non ha bisogno di gruppi speciali.
cat > "$RADICE/etc/greetd/config.toml" <<'FINE'
# fatto da costruisci.sh
[terminal]
vt = 1

[default_session]
command = "/usr/sbin/agreety --cmd /bin/sh"
user = "_greetd"

[initial_session]
command = "/usr/bin/mshell-sessione"
user = "utente1"
FINE
cat > "$RADICE/usr/bin/mshell-sessione" <<'FINE'
#!/bin/sh
# mshell-sessione (fatto da costruisci.sh): lo lancia greetd all'accesso; avvia la scrivania
# come servizio di systemd dell'utente e aspetta che finisca (spiegato in costruisci.sh).
# chiude la sessione grafica: i portali, poi la scrivania (legata alla sessione con BindsTo)
fine() {
	systemctl --user start --job-mode=replace-irreversibly mshell-fine.target
}
trap fine TERM HUP INT
systemctl --user reset-failed mshell.service graphical-session.target 2>/dev/null
systemctl --user import-environment XDG_SESSION_ID XDG_SEAT XDG_VTNR XDG_SESSION_TYPE XDG_SESSION_CLASS LANG
# in sottofondo, perche' "wait" si interrompe quando arriva un segnale e la scrivania si ferma
systemctl --user --wait start mshell.service &
wait $!
wait $!
fine
systemctl --user unset-environment WAYLAND_DISPLAY XDG_SESSION_ID XDG_SEAT XDG_VTNR XDG_SESSION_TYPE XDG_SESSION_CLASS
FINE
chmod 755 "$RADICE/usr/bin/mshell-sessione"
# LA SCRIVANIA PER SYSTEMD. Le variabili:
# WLR_RENDERER_ALLOW_SOFTWARE=1: il compositore disegna con OpenGL ES 2 (SceneFX, per gli effetti:
# utente/compositore/effetti.c); dove non c'e' una scheda video vera (QEMU) OpenGL lo fa Mesa con
# il processore (llvmpipe), e senza questa variabile il compositore si rifiuterebbe di partire.
# LP_NUM_THREADS=0: llvmpipe disegna tutto il fotogramma prima di restituirlo al compositore,
# invece che in suoi thread a parte. Serve perche' SceneFX 0.2 non dice a nessuno quando il
# disegno e' finito (non fa le "fence" di wlroots): con una scheda video vera ci pensa il driver,
# con llvmpipe no, e la scheda video di QEMU mostrava fotogrammi a meta' o vecchi (un menu che
# non compare finche' non si muove qualcos'altro). Con una scheda video vera non conta.
# XKB_DEFAULT_LAYOUT=it: LA TASTIERA ITALIANA (decisione della persona). Il compositore chiede a
# xkbcommon la mappa dei tasti senza dire quale: xkbcommon allora la prende da queste variabili
# (e i file da xkb-data).
# VARIABILI_COMPOSITORE: quelle in piu' che chiede la macchina (macchine/<nome>.conf).
# "-s ...": il compositore, appena pronto, fa tre cose:
#   1. dice ai servizi dell'utente dov'e' lo schermo (WAYLAND_DISPLAY): i portali e le app
#      lanciate da systemd lo trovano;
#   2. dice a systemd che e' pronto: comincia la sessione grafica, i portali possono partire;
#   3. fa partire la barra di stato.
mkdir -p "$RADICE/usr/lib/systemd/user"
cat > "$RADICE/usr/lib/systemd/user/mshell.service" <<FINE
# fatto da costruisci.sh: la scrivania di mshell
[Unit]
Description=mshell, la scrivania
BindsTo=graphical-session.target
Before=graphical-session.target
Wants=graphical-session-pre.target
After=graphical-session-pre.target

[Service]
Type=notify
NotifyAccess=all
Environment=WLR_RENDERER_ALLOW_SOFTWARE=1 LP_NUM_THREADS=0 XKB_DEFAULT_LAYOUT=it ${VARIABILI_COMPOSITORE}
ExecStart=/usr/bin/mshell-compositore -s "systemctl --user import-environment WAYLAND_DISPLAY; systemd-notify --ready; exec mshell-barra"
ExecStopPost=/usr/bin/systemctl --user unset-environment WAYLAND_DISPLAY
FINE
cat > "$RADICE/usr/lib/systemd/user/mshell-fine.target" <<'FINE'
# fatto da costruisci.sh: la fine della sessione grafica (la avvia mshell-sessione)
[Unit]
Description=mshell, fine della sessione grafica
DefaultDependencies=no
StopWhenUnneeded=yes
Conflicts=graphical-session.target graphical-session-pre.target
After=graphical-session.target graphical-session-pre.target
FINE
rm -f "$S/getty.target.wants/getty@tty1.service"
mkdir -p "$S/greetd.service.d"
cat > "$S/greetd.service.d/tty1.conf" <<'FINE'
[Unit]
After=getty@tty1.service
Conflicts=getty@tty1.service
FINE
# LA RETE (la persona, 24/11/2026: "quello che c'e' gia' nel modo piu' semplice possibile, niente
# pacchetti aggiuntivi"): systemd-networkd, che e' dentro systemd, accende ogni scheda via cavo e
# chiede l'indirizzo con DHCP. I NOMI (flathub.org -> indirizzo): senza systemd-resolved (un
# pacchetto in piu') nessuno scrive /etc/resolv.conf, e /etc e' in sola lettura: lo si scrive qui
# una volta, con due server pubblici (Quad9 e Cloudflare). Il Wi-Fi verra' dopo.
mkdir -p "$RADICE/etc/systemd/network"
cat > "$RADICE/etc/systemd/network/20-cavo.network" <<'FINE'
# fatto da costruisci.sh: ogni scheda via cavo prende l'indirizzo con DHCP
[Match]
Type=ether

[Network]
DHCP=yes
FINE
rm -f "$RADICE/etc/resolv.conf"
cat > "$RADICE/etc/resolv.conf" <<'FINE'
# fatto da costruisci.sh: i server dei nomi (il sistema e' in sola lettura)
nameserver 9.9.9.9
nameserver 1.1.1.1
FINE
systemctl --root="$RADICE" enable systemd-networkd.service >/dev/null 2>&1
# IL WI-FI (la persona, 27/11/2026: iwd). iwd si collega alla rete (comando iwctl); l'indirizzo lo
# chiede systemd-networkd con DHCP, come per il cavo (iwd lascia fare a lui: e' il suo modo normale).
cat > "$RADICE/etc/systemd/network/30-wifi.network" <<'FINE'
# fatto da costruisci.sh: ogni scheda Wi-Fi collegata da iwd prende l'indirizzo con DHCP
[Match]
Type=wlan

[Network]
DHCP=yes
FINE
systemctl --root="$RADICE" enable iwd.service >/dev/null 2>&1
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

# --- 3b. i programmi di mshell (utente/): li compila strumenti/compila.sh (dice quali sono), lo
# stesso script delle prove veloci (strumenti/LEGGIMI-prove.md) ---
sh strumenti/compila.sh "$RADICE"

# --- 3c. lo sfondo e il puntatore (risorse/), preparati qui con gli attrezzi ---
# LO SFONDO: la foto di risorse/sfondi/ portata alla misura dello schermo della macchina
# (SCHERMO_LARGHEZZA x SCHERMO_ALTEZZA, in macchine/<nome>.conf), in pixel pronti: il formato
# e' descritto in strumenti/sfondo.py. Il compositore lo legge all'avvio da
# /usr/share/mshell/sfondo (i dati fissi dei programmi stanno in /usr/share/<programma>).
echo "3c. sfondo ${SCHERMO_LARGHEZZA}x${SCHERMO_ALTEZZA} e puntatore"
mkdir -p "$RADICE/usr/share/mshell"
python3 strumenti/sfondo.py risorse/sfondi/samurai.jpg "$SCHERMO_LARGHEZZA" "$SCHERMO_ALTEZZA"     "$RADICE/usr/share/mshell/sfondo"
# IL FUSO ORARIO: Roma (deciso dalla persona il 19/11/2026). Si copia solo la regola di Roma
# dagli attrezzi in /etc/localtime (2,6 KB), senza il pacchetto tzdata con tutti i fusi del
# mondo: la leggono systemd e GLib (l'orologio della barra). Ora legale compresa.
rm -f "$RADICE/etc/localtime"
cp /usr/share/zoneinfo/Europe/Rome "$RADICE/etc/localtime"
echo "Europe/Rome" > "$RADICE/etc/timezone"
# LA LINGUA: italiano (la persona, 27/11/2026). Due pezzi:
#   1. le regole dell'italiano (date, numeri, ordine alfabetico) preparate da localedef degli
#      attrezzi (i sorgenti vengono dal pacchetto locales, che nel sistema non entra) nella
#      cartella standard /usr/lib/locale, accanto a C.utf8 che c'e' gia';
#   2. /etc/locale.conf dice a tutti di usarle: lo leggono systemd (per i servizi del sistema e
#      dell'utente, come il portale) e PAM all'accesso (/etc/default/locale e' un collegamento a
#      questo file), quindi la sessione di greetd, il compositore, la barra e le app.
# Le traduzioni dei programmi (GTK, portale, systemd...) ci sono gia': i pacchetti le portano.
# Le app Flatpak prendono la lingua dalla sessione; le loro traduzioni le scaricano a parte
# (vedi "FLATHUB PER UTENTE1").
localedef -i it_IT -f UTF-8 --no-archive "$RADICE/usr/lib/locale/it_IT.utf8"
echo "LANG=it_IT.UTF-8" > "$RADICE/etc/locale.conf"
# IL PUNTATORE: le immagini disegnate in Figma (risorse/puntatore/, 32x32 con l'ombra),
# rimpicciolite da strumenti/puntatore.py tutte alla stessa altezza (lo dice li'), diventano il
# tema Xcursor "mshell" con xcursorgen (misura nominale 24; l'attesa animata ha 12 fotogrammi, un
# giro al secondo). I file hanno i nomi standard dei temi Xcursor; gli altri nomi dello stesso
# puntatore sono collegamenti.
# Il tema "default" (quello che chiedono il compositore e i programmi se nessuno dice altro)
# eredita da "mshell": cosi' vale per tutti senza variabili d'ambiente.
P=/tmp/puntatore
rm -rf "$P"
mkdir -p "$P" "$RADICE/usr/share/icons/mshell/cursors" "$RADICE/usr/share/icons/default"
python3 strumenti/puntatore.py "$P"
for nome in default text pointer not-allowed wait; do
    xcursorgen "$P/$nome" "$RADICE/usr/share/icons/mshell/cursors/$nome"
done
cd "$RADICE/usr/share/icons/mshell/cursors"
ln -s default left_ptr
ln -s text xterm
ln -s pointer hand2
ln -s wait watch
ln -s wait progress
cd /w
cat > "$RADICE/usr/share/icons/mshell/index.theme" <<'FINE'
[Icon Theme]
Name=mshell
Comment=Il puntatore di mshell
FINE
cat > "$RADICE/usr/share/icons/default/index.theme" <<'FINE'
[Icon Theme]
Inherits=mshell
FINE
# IL CARATTERE: IBM Plex Sans (decisione della persona), nei pesi che usa il disegno (Regular,
# Medium, SemiBold), con la sua licenza (OFL), nella cartella standard dei caratteri. fontconfig
# (arriva con GTK) li trova da solo; il suo indice (la cache) si fa qui una volta, con fc-cache
# del sistema stesso, invece che a ogni avvio.
echo "3c. carattere IBM Plex Sans"
mkdir -p "$RADICE/usr/share/fonts/truetype/ibm-plex-sans"
cp risorse/caratteri/ibm-plex-sans/* "$RADICE/usr/share/fonts/truetype/ibm-plex-sans/"
chroot "$RADICE" /usr/bin/fc-cache -f
# LE IMPOSTAZIONI PER LE APP (GSettings): i valori di mshell (risorse/impostazioni/) sopra quelli
# di partenza; l'indice delle impostazioni si rifa' qui con lo strumento della libreria GLib del
# sistema (quello che usa Debian quando installa).
echo "3c. impostazioni per le app"
cp risorse/impostazioni/*.gschema.override "$RADICE/usr/share/glib-2.0/schemas/"
chroot "$RADICE" sh -c '/usr/lib/*/glib-2.0/glib-compile-schemas /usr/share/glib-2.0/schemas'

# --- 4. via i pacchetti "da terminale" e "per installare" ---
DA_TERMINALE="grep sed findutils diffutils tar gzip hostname bsdutils ncurses-bin debianutils"
PER_INSTALLARE="dpkg init-system-helpers perl-base sysvinit-utils mawk adduser passwd"
# avanzi: li volevano i pacchetti "per installare" o il kernel di Debian (il suo archivio d'avvio
# lo faremmo con initramfs-tools; noi lo facciamo con dracut degli attrezzi). Nel sistema acceso
# non servono (la persona, 20/11/2026, dall'inventario del sistema).
AVANZI="debconf libdebconfclient0 libsemanage2 libsemanage-common libsepol2 libdb5.3t64 logsave linux-base initramfs-tools initramfs-tools-core initramfs-tools-bin klibc-utils libklibc cpio dracut-install"
D="$RADICE/var/lib/dpkg"
dpkg-query --admindir="$D" -W -f '${Package} ${Installed-Size}\n' | sort > "$B/pacchetti-tutti.txt"
echo "4. tolgo: $DA_TERMINALE $PER_INSTALLARE $AVANZI"
for p in $DA_TERMINALE $PER_INSTALLARE $AVANZI; do
    elenco=$(ls "$D/info/$p.list" "$D/info/$p:"*.list 2>/dev/null | head -n 1)
    [ -n "$elenco" ] || continue
    while read -r f; do
        if [ -L "$RADICE$f" ] || [ -f "$RADICE$f" ]; then rm -f "$RADICE$f"; fi
    done < "$elenco"
done
# quello che resta, con la misura (KB): il "cosa c'e' dentro" del sistema
grep -v -w -E "^($(echo $DA_TERMINALE $PER_INSTALLARE $AVANZI | tr ' ' '|' | sed 's/\./\\./g'))" "$B/pacchetti-tutti.txt" > "$B/pacchetti-sistema.txt"
rm -rf "$D" "$RADICE/var/cache/apt" "$RADICE/var/log/"*.log
# il kernel e' nel file tutto-in-uno: nella radice non serve
cp "$RADICE/boot/vmlinuz-$KVER" "$B/vmlinuz"
rm -rf "$RADICE/boot/"* "$RADICE/vmlinuz" "$RADICE/vmlinuz.old" "$RADICE/initrd.img" "$RADICE/initrd.img.old"

# --- 4b. SISTEMA IN SOLA LETTURA, DATI A PARTE (decisione della persona, strada 1) ---
# La partizione "system" (/) e' montata in sola lettura. Quello che deve cambiare sta nella
# partizione "data", montata in /data e agganciata ai percorsi standard: /data/var su /var
# (registro, stato dei servizi, Flatpak) e /data/home su /home (le persone). /var nasce dal
# /var del sistema appena costruito, che si sposta nell'albero dei dati.
DATI=/tmp/dati
rm -rf "$DATI"
mkdir -p "$DATI"
mv "$RADICE/var" "$DATI/var"
mkdir -p "$DATI/home" "$RADICE/var" "$RADICE/data"
cat > "$RADICE/etc/fstab" <<'FINE'
# fatto da costruisci.sh: il sistema in sola lettura, i dati a parte
PARTLABEL=system  /      ext4  ro                0 1
PARTLABEL=data    /data  ext4  defaults,nodev    0 2
/data/var         /var   none  bind              0 0
/data/home        /home  none  bind              0 0
FINE
# Quello che systemd scriverebbe in /etc al primo avvio si fa qui, perche' /etc non si scrive:
# il numero fisso del computer (machine-id: uno per ogni costruzione) e gli utenti di sistema.
od -An -N16 -tx1 /dev/urandom | tr -d ' \n' > "$RADICE/etc/machine-id"
echo >> "$RADICE/etc/machine-id"
systemd-sysusers --root="$RADICE" >/dev/null
# LA PERSONA: utente1 (UID 1000), con il suo gruppo. Si crea con useradd degli attrezzi, che
# scrive /etc/passwd, /etc/group e /etc/shadow della radice (--root) come farebbe nel sistema;
# la password resta bloccata ("!"): entra solo con l'accesso automatico. La Home si crea a mano
# nell'albero dei dati (/data/home diventa /home), sua e chiusa agli altri (700).
# La shell e' bash (la persona, 27/11/2026: per il terminale). Il gruppo netdev le permette di
# comandare il Wi-Fi (iwctl): iwd risponde solo all'amministratore e a quel gruppo.
groupadd --root "$RADICE" --system netdev
useradd --root "$RADICE" --uid 1000 --user-group --groups netdev --no-create-home --home-dir /home/utente1 --shell /bin/bash utente1
mkdir -m 700 "$DATI/home/utente1"
# FLATHUB PER UTENTE1 (la persona, 24/11/2026: le app si installano per l'utente, nella sua Home,
# senza permessi da amministratore). Si aggiunge qui, con flatpak del sistema stesso, la fonte
# Flathub (risorse/flatpak/flathub.flatpakrepo, con la chiave che controlla le firme) nel deposito
# di utente1 (~/.local/share/flatpak): alla prima accensione si puo' gia' installare.
mkdir -p "$RADICE/tmp/casa"
cp risorse/flatpak/flathub.flatpakrepo "$RADICE/tmp/"
mount --bind /dev "$RADICE/dev"
mount -t proc proc "$RADICE/proc"
chroot "$RADICE" env HOME=/tmp/casa flatpak remote-add --user --if-not-exists flathub /tmp/flathub.flatpakrepo
# LE TRADUZIONI DELLE APP: Flatpak le scarica a parte (le estensioni ".Locale" delle app e delle
# loro basi), solo per le lingue che gli si dicono. Si dice qui, una volta, "italiano": vale
# per ogni installazione, anche fatta da un terminale senza la lingua della sessione.
chroot "$RADICE" env HOME=/tmp/casa flatpak config --user --set languages it
# LE APP GIA' DENTRO (APP_FLATPAK della macchina, di solito vuoto): per le macchine senza internet
# (la chiavetta del portatile, la persona 27/11/2026) le app si scaricano qui da Flathub, con la
# loro base, e finiscono nella Home di utente1 come se le avesse installate lei.
if [ -n "$APP_FLATPAK" ]; then
    echo "4c. app Flatpak: $APP_FLATPAK"
    # /var e' gia' nell'albero dei dati: si aggancia come nel sistema acceso (Flatpak usa /var/tmp)
    mount --bind "$DATI/var" "$RADICE/var"
    chroot "$RADICE" env HOME=/tmp/casa flatpak install --user --noninteractive -y flathub $APP_FLATPAK >/dev/null
    umount "$RADICE/var"
fi
umount "$RADICE/proc" "$RADICE/dev"
mv "$RADICE/tmp/casa/.local" "$DATI/home/utente1/.local"
rm -rf "$RADICE/tmp/casa" "$RADICE/tmp/flathub.flatpakrepo"
chown -R 1000:1000 "$DATI/home/utente1"

# --- 5. il kernel tutto-in-uno ---
echo "5. UKI"
ukify build --linux "$B/vmlinuz" --initrd "$B/initrd.img" \
    --cmdline "root=PARTLABEL=system ro console=tty1 console=$SERIALE $RIGA_KERNEL" \
    --output "$B/$FILE_AVVIO" >/dev/null

# --- 6. il disco ---
MB=1048576
SETTORI_MB=2048
MISURA_AVVIO=64
MISURA_SISTEMA=$(( $(du -sm "$RADICE" | cut -f1) * 13 / 10 + 64 ))
INIZIO_SISTEMA=$((1 + MISURA_AVVIO))
MISURA_DATI=8192                  # i dati: 8 GB, per le app Flatpak (il file del disco e' "sparso")
INIZIO_DATI=$((INIZIO_SISTEMA + MISURA_SISTEMA))
TOTALE=$((INIZIO_DATI + MISURA_DATI + 1))
echo "6. disco: avvio $MISURA_AVVIO MB, system $MISURA_SISTEMA MB, data $MISURA_DATI MB"
rm -f "$B/disco-$MACCHINA.img"
truncate -s ${TOTALE}M "$B/disco-$MACCHINA.img"
sfdisk -q "$B/disco-$MACCHINA.img" <<FINE
label: gpt
start=$SETTORI_MB, size=$((MISURA_AVVIO * SETTORI_MB)), type=U, name="avvio"
start=$((INIZIO_SISTEMA * SETTORI_MB)), size=$((MISURA_SISTEMA * SETTORI_MB)), type=L, name="system"
start=$((INIZIO_DATI * SETTORI_MB)), size=$((MISURA_DATI * SETTORI_MB)), type=L, name="data"
FINE
AVVIO="$B/disco-$MACCHINA.img@@$MB"
mformat -i "$AVVIO" -F -T $((MISURA_AVVIO * SETTORI_MB)) -v AVVIO ::
mmd -i "$AVVIO" ::/EFI ::/EFI/BOOT
mcopy -i "$AVVIO" "$B/$FILE_AVVIO" "::/EFI/BOOT/$FILE_AVVIO"
mkfs.ext4 -q -F -b 4096 -L system -E offset=$((INIZIO_SISTEMA * MB)) -d "$RADICE" \
    "$B/disco-$MACCHINA.img" $((MISURA_SISTEMA * MB / 4096))
mkfs.ext4 -q -F -b 4096 -L data -E offset=$((INIZIO_DATI * MB)) -d "$DATI" \
    "$B/disco-$MACCHINA.img" $((MISURA_DATI * MB / 4096))
echo "pronto: build/disco-$MACCHINA.img ($(wc -l < "$B/pacchetti-sistema.txt") pacchetti nel sistema, $(du -sm "$RADICE" | cut -f1) MB)"
