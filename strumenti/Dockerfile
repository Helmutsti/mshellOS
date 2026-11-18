# Gli ATTREZZI per costruire il sistema: un Debian 13 (lo stesso del sistema) con gli strumenti
# che servono SOLO a costruire, e che nel sistema non finiscono:
#   mmdebstrap                  crea un sistema Debian da un elenco di pacchetti
#   dracut, systemd(-sysv)      preparano l'archivio d'avvio (initramfs) con systemd dentro
#   systemd-ukify, systemd-boot-efi  uniscono kernel, archivio d'avvio e impostazioni nel file
#                               unico (UKI) che il firmware fa partire
#   fdisk, mtools, dosfstools, e2fsprogs  il disco: la tabella delle partizioni, la FAT32
#                               dell'avvio e l'ext4 del sistema
#   gcc, libc6-dev, pkg-config  il compilatore C, per i programmi di mshell (utente/)
#   libwlroots-0.18-dev, libwayland-dev, libwayland-bin (wayland-scanner), wayland-protocols,
#   libxkbcommon-dev, libpixman-1-dev  quello che serve a compilare il compositore (wlroots) e i
#                               programmi Wayland: le intestazioni e le descrizioni dei protocolli
#   python3-pil                 Python con Pillow: porta la foto dello sfondo alla misura dello
#                               schermo, in pixel pronti (strumenti/sfondo.py)
#   x11-apps                    per xcursorgen (in Debian 13 sta in questo pacchetto): fa il tema
#                               del puntatore (Xcursor) dalle immagini di risorse/puntatore/
FROM debian:trixie
RUN apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
        mmdebstrap dracut-core systemd systemd-sysv systemd-ukify systemd-boot-efi \
        fdisk mtools dosfstools e2fsprogs ca-certificates \
        gcc libc6-dev pkg-config libwlroots-0.18-dev libwayland-dev libwayland-bin wayland-protocols \
        libxkbcommon-dev libpixman-1-dev python3-pil x11-apps >/dev/null && \
    rm -rf /var/lib/apt/lists/*
