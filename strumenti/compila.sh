#!/bin/sh
# compila.sh: COMPILA I PROGRAMMI DI MSHELL (utente/) e li mette, con i loro file, in una radice
# (cartella che fa da "/" del sistema). Gira nel container degli attrezzi, dalla cartella /w.
#   sh strumenti/compila.sh <radice>
# Lo usano tutte e tre le prove (strumenti/LEGGIMI-prove.md): la costruzione completa
# (costruisci.sh, con la radice del sistema), l'aggiornamento del disco (aggiorna-disco.sh) e la
# prova veloce (prova-veloce.py): cosi' le istruzioni di compilazione esistono una volta sola.
set -e
cd /w
RADICE=${1:?manca la radice}
mkdir -p "$RADICE/usr/bin"

# Si compilano con le librerie degli attrezzi, che sono le stesse del sistema (Debian 13):
#   mshell-compositore    il compositore Wayland (tinywl di wlroots, utente/compositore/)
#   mshell-prova          l'app di prova: una finestra scura (utente/prove/app-scura.c); nel
#                         cassetto delle app come "Prova" (il suo .desktop in /usr/share/applications)
#   mshell-barra          la barra di stato, i suoi menu e il cassetto delle app (utente/barra/,
#                         GTK 4); il suo foglio di stile va in /usr/share/mshell/barra/
#   token.css             i valori del disegno (risorse/), in /usr/share/mshell/: li leggono la
#                         barra e il compositore quando partono
# I protocolli dei livelli e dell'elenco delle app non stanno nei pacchetti di Debian: le loro
# descrizioni sono copiate dai sorgenti di wlroots 0.18.2 in utente/compositore/protocolli/.
echo "3b. compositore, app di prova e barra"
C=/tmp/compila
rm -rf "$C"
mkdir -p "$C"
PROTOCOLLI=$(pkg-config --variable=pkgdatadir wayland-protocols)
MIEI=utente/compositore/protocolli
wayland-scanner server-header "$PROTOCOLLI/stable/xdg-shell/xdg-shell.xml" "$C/xdg-shell-protocol.h"
wayland-scanner server-header "$MIEI/wlr-layer-shell-unstable-v1.xml" "$C/wlr-layer-shell-unstable-v1-protocol.h"
wayland-scanner client-header "$MIEI/wlr-foreign-toplevel-management-unstable-v1.xml" \
    "$C/wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"
wayland-scanner private-code "$MIEI/wlr-foreign-toplevel-management-unstable-v1.xml" \
    "$C/wlr-foreign-toplevel-management-unstable-v1-protocol.c"
wayland-scanner client-header "$PROTOCOLLI/stable/xdg-shell/xdg-shell.xml" "$C/xdg-shell-client-protocol.h"
wayland-scanner private-code "$PROTOCOLLI/stable/xdg-shell/xdg-shell.xml" "$C/xdg-shell-protocol.c"
# Il compositore si porta dentro SceneFX (la libreria statica compilata negli attrezzi,
# strumenti/Dockerfile) e usa le librerie di OpenGL del sistema (EGL, GLES 2, GBM, DRM).
# --export-dynamic: le funzioni della scena di SceneFX si vedono anche da wlroots, che le chiama
# per le superfici delle app e dei livelli (spiegato in utente/compositore/effetti.c).
# pangocairo: il testo e le icone della scrivania (scrivania.c); nel sistema c'e' gia', lo porta GTK.
SCENEFX=$(pkg-config --variable=libdir scenefx-0.2)/libscenefx-0.2.a
gcc -O2 -Werror -DWLR_USE_UNSTABLE -I"$C" \
    $(pkg-config --cflags scenefx-0.2 wlroots-0.18 wayland-server xkbcommon pangocairo) \
    -o "$RADICE/usr/bin/mshell-compositore" utente/compositore/tinywl.c "$SCENEFX" \
    -Wl,--export-dynamic \
    $(pkg-config --libs wlroots-0.18 wayland-server xkbcommon egl glesv2 gbm libdrm pixman-1 pangocairo) -lm
gcc -O2 -Wall -Werror -I"$C" -o "$RADICE/usr/bin/mshell-prova" \
    utente/prove/app-scura.c "$C/xdg-shell-protocol.c" $(pkg-config --libs wayland-client)
mkdir -p "$RADICE/usr/share/applications"
cp utente/prove/mshell-prova*.desktop "$RADICE/usr/share/applications/"  # Prova e Uno..Sei
# gtk4-layer-shell va prima di wayland-client (lo chiede la sua documentazione)
# (gio-unix: i file .desktop delle app, per il cassetto)
gcc -O2 -Wall -Werror -I"$C" $(pkg-config --cflags gtk4-layer-shell-0 gtk4 gio-unix-2.0 wayland-client) \
    -o "$RADICE/usr/bin/mshell-barra" utente/barra/*.c \
    "$C/wlr-foreign-toplevel-management-unstable-v1-protocol.c" \
    $(pkg-config --libs gtk4-layer-shell-0 gtk4 gio-unix-2.0 wayland-client)
mkdir -p "$RADICE/usr/share/mshell/barra"
cp utente/barra/stile.css "$RADICE/usr/share/mshell/barra/stile.css"
# i valori del disegno (colori, misure, ombre): un solo elenco, che leggono la barra e il compositore
cp risorse/token.css "$RADICE/usr/share/mshell/token.css"

