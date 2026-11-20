# mshell (Debian) – istruzioni di lavoro

@COMUNICAZIONE.md

## Il progetto (deciso il 18/11/2026)
- Base: **Debian stabile** (13, trixie), costruita da zero da un elenco di pacchetti. **Ogni pacchetto passa dal giudizio della persona** prima di entrare, con una riga che dice cos'è e perché c'è. Niente entra se non è nell'elenco.
- PID 1: **systemd**. Avvio: **kernel tutto-in-uno (UKI)** avviato direttamente dal firmware UEFI, senza caricatore.
- App: **solo Flatpak** (Flathub), unico modo di installare app. La base resta nuda.
- I pacchetti obbligatori di Debian: restano solo quelli che servono al sistema acceso (libc, base-files, base-passwd, util-linux, coreutils, dash). Quelli "da terminale" (bash, grep, sed, findutils, diffutils, tar, gzip, hostname, bsdutils, ncurses-base, debianutils) NON ci sono: si mettono solo se strettamente necessari, con l'ok della persona. Quelli per installare (dpkg, init-system-helpers, perl-base, sysvinit-utils, mawk) si usano durante la costruzione e poi si tolgono dal sistema finale.
- Il vecchio mshell fatto da zero è nel ramo e nell'etichetta `archivio-mshell-da-zero`: disegni Figma, librerie e app si riprendono da lì quando servono. Il ramo `main` è questo sistema Debian.
- Primo passo: niente `login`, accesso automatico all'amministratore sul terminale (finché non c'è la schermata di accesso grafica).
- Si lavora sul processore del computer (PC x86_64, Mac arm64); Docker e QEMU nativi.

## Lavorare
- Aprire solo i file che servono; compilazione silenziosa; commit solo quando la persona lo chiede.
- Agenti in parallelo per pezzi indipendenti; integrazione e controllo finale restano all'agente principale. Gli agenti partono da un ramo caricato online (o dal ramo locale indicato esplicitamente).
- Le misure di velocità in QEMU si fanno a PC fermo.
- In chat sempre in italiano.
