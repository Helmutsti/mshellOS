# Gli ATTREZZI per costruire il sistema: un Debian 13 (lo stesso del sistema) con gli strumenti
# che servono SOLO a costruire, e che nel sistema non finiscono:
#   mmdebstrap                  crea un sistema Debian da un elenco di pacchetti
#   dracut, systemd(-sysv)      preparano l'archivio d'avvio (initramfs) con systemd dentro
#   systemd-ukify, systemd-boot-efi  uniscono kernel, archivio d'avvio e impostazioni nel file
#                               unico (UKI) che il firmware fa partire
#   fdisk, mtools, dosfstools, e2fsprogs  il disco: la tabella delle partizioni, la FAT32
#                               dell'avvio e l'ext4 del sistema
FROM debian:trixie
RUN apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
        mmdebstrap dracut-core systemd systemd-sysv systemd-ukify systemd-boot-efi \
        fdisk mtools dosfstools e2fsprogs ca-certificates >/dev/null && \
    rm -rf /var/lib/apt/lists/*
