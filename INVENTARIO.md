# Inventario del sistema mshell (19/11/2026)

> **Aggiornamento 20/11/2026:** gli avanzi dell'installazione e gli attrezzi d'avvio (gruppo 3 e le
> voci [togliere] del gruppo 1) ora si tolgono in costruzione: i pacchetti sono 198. La scritta dei
> tasti nella barra e' stata tolta. C'e' anche la versione per il Mac (arm64): il kernel lo sceglie
> la macchina (macchine/<nome>.conf). Il resto dell'elenco e' ancora quello del 19/11.

Tutto quello che c'e' nel sistema acceso, oltre al kernel: i 213 pacchetti di Debian (circa 510 MB),
i programmi nostri e le impostazioni. Per ogni pezzo: a cosa serve e chi lo vuole.

"Chi lo vuole" viene dalle dipendenze dei pacchetti di Debian: un pacchetto entra se uno di quelli
scelti (pacchetti.txt) o uno dei loro lo richiede. **Non si puo' togliere un pacchetto senza
togliere (o rifare) chi lo richiede**, a meno di cancellarne i file a mano dopo l'installazione,
come gia' facciamo per i pacchetti "da terminale" e "per installare" (costruisci.sh, punto 4).

Legenda delle proposte: **[togliere]** non serve al sistema acceso; **[valutare]** si puo' ridurre
con una scelta; **[tenere]** serve.

---

## In breve

- Nel sistema ci sono 213 pacchetti di Debian (circa 510 MB), i programmi di mshell e le
  impostazioni. Tutto e' elencato qui sotto, con a cosa serve, chi lo richiede e quanto pesa.
- Dove si puo' migliorare di piu':
  1. **I driver del kernel e LLVM** (quasi 250 MB insieme): dipende da su quali computer deve
     girare mshell. Con solo schede Intel, per esempio, LLVM non serve; in QEMU senza scheda video
     vera invece si'.
  2. **Le traduzioni di GTK in tutte le lingue e le icone di GNOME** (circa 25 MB): basterebbero
     l'italiano e poche icone.
  3. **Gli avanzi dell'installazione e gli attrezzi d'avvio** (circa 6 MB): si tolgono facilmente e
     senza rischi, come gia' facciamo con gli altri.
  4. **La sicurezza**: sulla linea di servizio c'e' l'accesso automatico come amministratore senza
     password; serve per le prove, ma va tolto prima di usare il sistema davvero.
- X11, la stampa e la rete li richiedono le librerie che usiamo (wlroots, Mesa, GTK): non si
  tolgono come pacchetti, solo cancellando i file a mano, con il rischio di rompere qualcosa.
- Prossimo passo proposto: decidere su quali computer deve girare mshell, e partire dal punto 1.

---

## Riepilogo

| Gruppo | Pacchetti | Peso | Cosa fa |
|---|---|---|---|
| 1. Kernel e avvio | 12 | 108 MB | il kernel con tutti i driver di Debian, e gli attrezzi per l'avvio |
| 2. La base di Debian | 44 | 63 MB | i comandi e le librerie su cui poggia tutto |
| 3. Avanzi dell'installazione | 7 | 3,5 MB | rimasti da pacchetti gia' tolti: non servono |
| 4. systemd, servizi e accesso | 19 | 34 MB | l'avvio dei servizi, i dispositivi, le sessioni, l'accesso automatico |
| 5. Il compositore: schermo, tastiera, mouse | 18 | 13 MB | quello che serve a mshell-compositore oltre alla grafica |
| 6. OpenGL (Mesa) | 18 | 45 MB | il disegno con la scheda video (o col processore, in QEMU) |
| 7. LLVM | 4 | 152 MB | il "compilatore" che Mesa usa per disegnare col processore e per le schede AMD |
| 8. X11 | 28 | 6 MB | il vecchio sistema grafico: noi non lo usiamo |
| 9. GTK e l'interfaccia | 46 | 72 MB | la libreria della barra, dei menu e del cassetto, con testo, immagini, icone |
| 10. Stampa e rete (portate da GTK) | 17 | 12,5 MB | la stampa e i protocolli di rete che GTK si porta dietro |
| **Totale** | **213** | **510 MB** | |

Oltre ai pacchetti: i programmi di mshell, il carattere IBM Plex Sans, il puntatore, lo sfondo e
le impostazioni (in fondo).

---

## 1. Kernel e avvio (108 MB)

| Pacchetto | MB | A cosa serve | Chi lo vuole | Proposta |
|---|---|---|---|---|
| linux-image-6.12…-amd64 | 106 | il kernel di Debian e **tutti** i suoi moduli (driver per migliaia di dispositivi) | scelto (pacchetti.txt) | **[valutare]** il kernel vero e' nel file d'avvio (UKI); qui restano i moduli. Ne usiamo poche decine. Tenere solo quelli per le macchine che vogliamo |
| linux-image-amd64 | 0 | "segnaposto" che punta all'ultima versione del kernel | scelto | [tenere] |
| linux-base | 0,1 | script di Debian per installare kernel | il kernel | **[togliere]** serve solo a installare |
| kmod, libkmod2 | 0,4 | carica i moduli del kernel (i driver) quando servono | kernel, udev, systemd | [tenere] |
| initramfs-tools (+core, +bin) | 0,3 | prepara il "disco d'avvio" (initramfs) | il kernel | **[togliere]** l'avvio lo prepariamo con dracut negli attrezzi, non nel sistema |
| klibc-utils, libklibc | 0,6 | piccoli comandi per l'initramfs di initramfs-tools | initramfs-tools | **[togliere]** come sopra |
| cpio | 1,2 | archivia file (formato dell'initramfs) | initramfs-tools | **[togliere]** come sopra |
| dracut-install | 0,1 | pezzo di dracut | (nessuno nel sistema) | **[togliere]** dracut gira negli attrezzi |

## 2. La base di Debian (63 MB)

| Pacchetto | MB | A cosa serve | Chi lo vuole | Proposta |
|---|---|---|---|---|
| libc6, libc-bin | 15 | la libreria C: ogni programma la usa | tutti | [tenere] |
| coreutils | 18 | i comandi di base (ls, cp, cat…) | obbligatorio | [tenere] (deciso) |
| dash | 0,2 | la shell (/bin/sh) | obbligatorio | [tenere] |
| util-linux, mount, libmount1, libblkid1, libsmartcols1, libuuid1 | 7 | montare i dischi, riconoscere le partizioni | systemd | [tenere] |
| liblastlog2-2, libsqlite3-0 | 1,9 | registro degli ultimi accessi (un piccolo database) | util-linux | **[valutare]** 1,8 MB solo per "chi ha fatto l'ultimo accesso" |
| login, login.defs | 0,5 | apre una sessione sul terminale (la seriale) | scelto | [tenere] finche' c'e' la seriale |
| base-files, base-passwd | 0,8 | le cartelle e gli utenti di sistema di base | obbligatorio | [tenere] |
| procps, libproc2-0, libncursesw6 | 3 | comandi come ps e top | dconf-service | [valutare] utile per noi nelle prove; per il sistema no |
| libtinfo6 | 0,6 | gestione dei terminali | util-linux, procps | [tenere] |
| libssl3t64, openssl-provider-legacy | 8,5 | crittografia (OpenSSL) | systemd, kmod (firme dei moduli) | [tenere]; il "legacy" (vecchi algoritmi) **[valutare]** |
| libgcc-s1, libstdc++6, libatomic1, gcc-14-base | 3,4 | librerie di base dei programmi in C e C++ | molti | [tenere] |
| zlib1g, liblzma5, libzstd1, libbz2-1.0 | 1,5 | compressione | molti | [tenere] |
| libgmp10 | 0,9 | calcoli con numeri grandi | coreutils | [tenere] |
| libcrypt1 | 0,2 | password cifrate | login, PAM | [tenere] |
| libacl1, libattr1, libcap2, libcap-ng0 | 0,3 | permessi dei file e dei programmi | systemd, coreutils | [tenere] |
| libselinux1, libpcre2-8-0 | 1 | SELinux (sicurezza; spenta) e ricerche nel testo | coreutils, systemd, glib | [tenere] (sono richieste anche spente) |
| libaudit1, libaudit-common | 0,2 | registro di sicurezza del kernel | PAM, systemd | [tenere] |
| libapparmor1 | 0,1 | AppArmor (sicurezza) | systemd, dbus | [tenere]; in futuro **[valutare]** per Flatpak |
| libseccomp2 | 0,2 | limita le chiamate al kernel dei servizi | systemd | [tenere] |
| libmd0, libbsd0 | 0,3 | funzioni di base | libedit2 (LLVM) | [tenere] |

## 3. Avanzi dell'installazione (3,5 MB) — tutti **[togliere]**

Rimasti nel sistema perche' li volevano pacchetti che alla fine togliamo (passwd, adduser, dpkg).

| Pacchetto | MB | A cosa serviva |
|---|---|---|
| debconf, libdebconfclient0 | 0,5 | le domande durante l'installazione dei pacchetti |
| libsemanage2, libsemanage-common, libsepol2 | 1,2 | gestione delle regole SELinux (li voleva passwd) |
| libdb5.3t64 | 1,8 | vecchio database usato da un modulo PAM che non usiamo |
| logsave | 0,1 | salva l'uscita di un controllo dei dischi (fsck) che non c'e' |

## 4. systemd, servizi e accesso (34 MB)

| Pacchetto | MB | A cosa serve | Chi lo vuole | Proposta |
|---|---|---|---|---|
| systemd, systemd-sysv, libsystemd-shared, libsystemd0 | 18 | il primo programma: avvia i servizi, registro (journald), sessioni (logind) | scelto | [tenere] (deciso) |
| udev, libudev1 | 10,6 | riconosce i dispositivi e carica i loro driver | systemd | [tenere] |
| dbus (+bin, +daemon, +common, +user-session), libdbus-1-3 | 1,4 | il "centralino" fra servizi e programmi | scelto | [tenere] |
| libpam-runtime, libpam-modules, libpam-modules-bin, libpam0g | 2,3 | PAM: controlla gli accessi | scelto | [tenere] |
| libpam-systemd | 0,7 | registra ogni sessione presso logind | scelto | [tenere] |
| greetd | 1,3 | apre da solo la sessione di utente1 e lancia la scrivania | scelto | [tenere] |

## 5. Il compositore: schermo, tastiera, mouse (13 MB)

| Pacchetto | MB | A cosa serve | Chi lo vuole | Proposta |
|---|---|---|---|---|
| libwlroots-0.18 | 1,2 | la libreria su cui e' fatto mshell-compositore | scelto | [tenere] |
| libwayland-server0, -client0, -egl1 | 0,2 | il protocollo Wayland fra compositore e app | wlroots, GTK | [tenere] |
| libinput10, libinput-bin, libevdev2, libmtdev1t64 | 0,8 | tastiera, mouse, touchpad, touch | wlroots | [tenere] |
| libwacom9, libwacom-common, libgudev-1.0-0 | 1,9 | tavolette grafiche Wacom | libinput | [tenere] (libinput lo richiede) |
| libseat1 | 0,1 | chiede a logind schermo e tastiera | wlroots | [tenere] |
| libxkbcommon0, xkb-data | 7,2 | le disposizioni delle tastiere (italiana) | wlroots, GTK | [valutare] xkb-data ha **tutte** le tastiere del mondo (6,9 MB): basterebbero le nostre |
| libdisplay-info2, libliftoff0 | 0,3 | informazioni sugli schermi, uso dei "piani" della scheda video | wlroots | [tenere] |
| libpixman-1-0 | 0,7 | disegno di base senza scheda video | wlroots, cairo | [tenere] |
| liblcms2-2 | 0,5 | gestione dei colori | wlroots | [tenere] |

## 6. OpenGL (Mesa) (45 MB)

| Pacchetto | MB | A cosa serve | Chi lo vuole | Proposta |
|---|---|---|---|---|
| mesa-libgallium | 41,6 | i driver grafici: Intel, AMD, QEMU, e llvmpipe (disegno col processore) | EGL | [tenere]; **[valutare]** e' unico per tutte le schede |
| libegl1, libegl-mesa0, libgles2, libglvnd0 | 1,4 | OpenGL ES: lo usano il compositore (effetti) e GTK | wlroots, GTK | [tenere] |
| libgbm1, libdrm2, libdrm-common | 0,3 | memoria della scheda video | wlroots, Mesa | [tenere] |
| libdrm-amdgpu1, libdrm-intel1, libpciaccess0 | 0,4 | parti per AMD e Intel | Mesa | [tenere] |
| libvulkan1 | 0,6 | Vulkan (l'altra interfaccia grafica) | GTK, wlroots | [tenere] (richiesto) |
| libelf1t64, libsensors5, libsensors-config, libexpat1, libffi8, libxshmfence1 | 1,8 | pezzi richiesti da Mesa | Mesa | [tenere] |

## 7. LLVM (152 MB) — il pezzo piu' pesante

| Pacchetto | MB | A cosa serve | Chi lo vuole | Proposta |
|---|---|---|---|---|
| libllvm19 | 124 | Mesa lo usa per disegnare col processore (llvmpipe, in QEMU) e per le schede AMD | mesa-libgallium | **[valutare]** sulle schede Intel non serve; in QEMU si'. Togliendolo, niente QEMU senza scheda video vera |
| libz3-4 | 26,5 | un "risolutore" che LLVM si porta dietro | libllvm19 | come sopra |
| libedit2, libxml2 | 2,1 | richiesti da LLVM | libllvm19 | come sopra |

## 8. X11 (6 MB) — **[valutare]**

Il vecchio sistema grafico: mshell e' solo Wayland e non lo usa. Lo richiedono wlroots (per far
girare le app X11 vecchie con Xwayland, che non abbiamo), Mesa e GTK (che sanno anche disegnare su
X11). Non si possono togliere con i pacchetti; si possono cancellare i file a mano, ma e' rischioso
(se una libreria ne carica uno, si rompe). libx11-6, libx11-data, libx11-xcb1, libxau6, libxdmcp6,
libxcb1 e 14 estensioni libxcb-*, libxcursor1, libxdamage1, libxext6, libxfixes3, libxi6,
libxinerama1, libxrandr2, libxrender1.

## 9. GTK e l'interfaccia (72 MB)

| Pacchetto | MB | A cosa serve | Chi lo vuole | Proposta |
|---|---|---|---|---|
| libgtk-4-1, libgtk-4-common | 26,4 | GTK 4: barra, menu, cassetto. Il "common" sono soprattutto **traduzioni** in tutte le lingue | scelto | [tenere]; le traduzioni **[valutare]** (basta l'italiano) |
| libgtk4-layer-shell0 | 0,1 | fa della barra un "livello" sopra le app | scelto | [tenere] |
| libglib2.0-0t64 | 4,3 | la base di GTK | GTK | [tenere] |
| libcairo2, libcairo-gobject2 | 1,6 | disegno 2D | GTK | [tenere] |
| libcairo-script-interpreter2, liblzo2-2 | 0,4 | esegue "script" di cairo (per il debug) | GTK | [tenere] (richiesto) |
| libpango* (3), libharfbuzz0b, libharfbuzz-subset0, libgraphite2-3, libfribidi0 | 3,9 | disegnare il testo: forme delle lettere, scritture da destra a sinistra | GTK | [tenere] |
| libthai0, libthai-data, libdatrie1 | 0,8 | testo in tailandese | pango | [tenere] (richiesto) |
| libfreetype6, libbrotli1, libpng16 | 2,2 | leggere i caratteri | GTK, cairo | [tenere] |
| fontconfig, fontconfig-config, libfontconfig1 | 1,8 | trovare i caratteri installati | GTK | [tenere] |
| fonts-dejavu-core, fonts-dejavu-mono | 3,5 | carattere DejaVu, quello di riserva | fontconfig | [valutare] noi usiamo IBM Plex Sans |
| libgdk-pixbuf-2.0-0, libgdk-pixbuf2.0-common | 3 | caricare immagini | GTK | [tenere] |
| libjpeg62-turbo, libtiff6, libwebp7, libsharpyuv0, libjbig0, liblerc4, libdeflate0 | 3 | formati di immagine (JPEG, TIFF, WebP…) | gdk-pixbuf, GTK | [tenere] |
| libgraphene-1.0-0, libepoxy0 | 1,5 | geometria e OpenGL per GTK | GTK | [tenere] |
| shared-mime-info | 5 | riconoscere i tipi di file | GTK, glib | [tenere] |
| adwaita-icon-theme, hicolor-icon-theme, gtk-update-icon-cache | 14,3 | le icone di GNOME | GTK | **[valutare]** noi usiamo le nostre (Lucide): GTK pero' le cerca per alcuni controlli |
| dconf-gsettings-backend, dconf-service, libdconf1 | 0,3 | dove GTK salva le impostazioni | GTK | [tenere] |
| libcolord2 | 0,6 | profili di colore degli schermi | GTK | [tenere] (richiesto) |
| libcloudproviders0 | 0,1 | cartelle cloud nella finestra "Apri file" | GTK | [tenere] (richiesto) |

## 10. Stampa e rete portate da GTK (12,5 MB) — **[valutare]**

GTK sa stampare: per questo si porta dietro CUPS (la stampa), Avahi (trova le stampanti in rete),
Kerberos (accesso alle reti aziendali) e GnuTLS (connessioni sicure). Noi oggi non stampiamo e non
abbiamo la rete. Sono richiesti da GTK, quindi non si tolgono con i pacchetti.
libcups2t64, libavahi-client3, libavahi-common3, libavahi-common-data, libgnutls30t64,
libnettle8t64, libhogweed6t64, libp11-kit0, libtasn1-6, libidn2-0, libunistring5,
libgssapi-krb5-2, libkrb5-3, libk5crypto3, libkrb5support0, libcom-err2, libkeyutils1.

---

## 11. I programmi nostri

| Cosa | Dove | A cosa serve |
|---|---|---|
| mshell-compositore | /usr/bin | il compositore: finestre, desktop, tasti di Super, animazioni, vetro, ombre (con SceneFX dentro) |
| mshell-barra | /usr/bin | barra di stato, i suoi menu e il cassetto delle app (GTK) |
| mshell-prova + 7 voci del cassetto | /usr/bin, /usr/share/applications | **[di prova]** le app Prova e Uno…Sei |
| stile.css | /usr/share/mshell/barra | l'aspetto della barra, dei menu e del cassetto |
| IBM Plex Sans | /usr/share/fonts | il carattere dell'interfaccia |
| tema del puntatore "mshell" | /usr/share/icons | le forme del puntatore |
| sfondo | /usr/share/mshell | samurai |

## 12. Le impostazioni e i servizi

| Cosa | A cosa serve | Proposta |
|---|---|---|
| accesso automatico come **root** sulla seriale | la shell dell'amministratore per le prove | **[togliere]** prima di usarlo davvero: chi ha la seriale e' amministratore senza password |
| greetd con accesso automatico di utente1 | la scrivania parte da sola | [tenere] finche' non c'e' la schermata di accesso |
| WLR_RENDERER_ALLOW_SOFTWARE, LP_NUM_THREADS | far disegnare il compositore col processore in QEMU | **[valutare]** servono solo in QEMU |
| servizi spenti (ldconfig, aggiornamenti del catalogo, sysext, initctl, creds, getty da tty2 a tty6) | lavori che si fanno una volta alla costruzione | [tenere] |
| sistema in sola lettura, dati a parte (/var, /home sulla partizione data) | il sistema non si sporca | [tenere] (deciso) |
| utente1 senza password | entra solo con l'accesso automatico | [tenere] per ora |
| la scritta dei tasti nella barra | **[di prova]** | **[togliere]** quando non serve |
| registro del compositore in /var/tmp | trovare le cadute | [tenere] |

---

## Le proposte, in ordine di guadagno

1. **I moduli del kernel** (fino a ~90 MB): tenere solo i driver delle macchine che vogliamo.
   Serve decidere su quali computer deve girare mshell.
2. **LLVM** (152 MB): serve solo per disegnare col processore (QEMU) e alle schede AMD. Dipende
   dalle macchine, come sopra.
3. **Traduzioni di GTK e icone di GNOME** (~25 MB): tenere l'italiano e le poche icone che GTK
   cerca davvero.
4. **xkb-data** (~6 MB): tenere solo le tastiere che servono.
5. **Avanzi dell'installazione e attrezzi d'avvio** (~6 MB): togliere come gia' facciamo con i
   pacchetti "per installare". Facile e sicuro.
6. **X11, stampa e rete di GTK** (~19 MB): richiesti dalle librerie; si tolgono solo cancellando
   file a mano, con il rischio che qualcosa si rompa. Da fare per ultimo, se mai.
7. **Sicurezza**: l'accesso come root sulla seriale va tolto prima di usare il sistema davvero.
