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
#   libgtk-4-dev, libgtk4-layer-shell-dev  le intestazioni di GTK 4 e di gtk4-layer-shell: servono
#                               a compilare la barra di stato e i suoi menu (mshell-barra, utente/barra/)
#   librsvg2-dev               le intestazioni di librsvg: il compositore disegna le icone SVG delle app
#   python3-pil                Python con Pillow: porta la foto dello sfondo alla misura dello
#                               schermo, in pixel pronti (strumenti/sfondo.py)
#   x11-apps                    per xcursorgen (in Debian 13 sta in questo pacchetto): fa il tema
#                               del puntatore (Xcursor) dalle immagini di risorse/puntatore/
FROM debian:trixie
RUN apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
        mmdebstrap dracut-core systemd systemd-sysv systemd-ukify systemd-boot-efi \
        fdisk mtools dosfstools e2fsprogs ca-certificates \
        gcc libc6-dev pkg-config libwlroots-0.18-dev libwayland-dev libwayland-bin wayland-protocols \
        libxkbcommon-dev libpixman-1-dev libgtk-4-dev libgtk4-layer-shell-dev librsvg2-dev python3-pil x11-apps >/dev/null && \
    rm -rf /var/lib/apt/lists/*

# SCENEFX (decisione della persona, 19/11/2026): la libreria che porta nel compositore gli angoli
# arrotondati, il vetro sfocato e le ombre. Prende il posto della scena di wlroots e disegna con
# OpenGL ES 2 (il suo "fx_renderer"). Non e' in Debian: si compila qui dai sorgenti, alla versione
# fissa 0.2.1 (la serie 0.2.x e' quella per wlroots 0.18). I sorgenti si scaricano da GitHub con
# la loro impronta (sha256): se il file scaricato non e' quello, la costruzione si ferma.
# La libreria si fa "statica" (libscenefx-0.2.a, in /usr/local): entra tutta dentro
# mshell-compositore, quindi nel sistema non c'e' niente da installare. A lei servono le librerie
# di OpenGL che nel sistema ci sono gia' (libegl1, libgles2, libgbm1, libdrm2, portate da wlroots).
#   meson, ninja-build          il sistema di costruzione di scenefx
#   libegl-dev, libgles-dev, libgbm-dev, libdrm-dev  le intestazioni di OpenGL, GBM e DRM
ADD --checksum=sha256:e50cd8cbeb6564233dced39f21e78d755701fa7fe67f5b354f033f397b69a2b8 \
    https://codeload.github.com/wlrfx/scenefx/tar.gz/refs/tags/0.2.1 /tmp/scenefx-0.2.1.tar.gz
RUN apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
        meson ninja-build libegl-dev libgles-dev libgbm-dev libdrm-dev >/dev/null && \
    rm -rf /var/lib/apt/lists/* && \
    cd /tmp && tar -xzf scenefx-0.2.1.tar.gz && cd scenefx-0.2.1 && \
    meson setup --prefix=/usr/local --libdir=lib --buildtype=release -Ddefault_library=static \
        -Dexamples=false -Drenderers=gles2 build >/dev/null && \
    ninja -C build >/dev/null && ninja -C build install >/dev/null && \
    cd /tmp && rm -rf scenefx-0.2.1 scenefx-0.2.1.tar.gz

# LA LINGUA ITALIANA (la persona, 27/11/2026: "lingua subito"). Le regole dell'italiano (date,
# numeri, ordine alfabetico, lettere) le prepara localedef, che c'e' gia' negli attrezzi; i suoi
# sorgenti (/usr/share/i18n) stanno nel pacchetto locales. Si preparano qui e nel sistema finisce
# solo il risultato (costruisci.sh, 3c), come per il fuso orario: locales nel sistema non entra.
#   locales                    i sorgenti delle lingue per localedef
RUN apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
        locales >/dev/null && \
    rm -rf /var/lib/apt/lists/*
