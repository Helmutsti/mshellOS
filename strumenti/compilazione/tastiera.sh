#!/bin/sh
# tastiera.sh USCITA: compila la KEYMAP della tastiera italiana per i programmi Wayland.
# La chiamano build.sh (il disco) e strumenti/sandbox/compila.sh (la sandbox), nel container degli
# attrezzi, che ha xkbcomp e xkeyboard-config (i pacchetti di Alpine). Scrive USCITA (un file, di
# solito "it.xkb"), che va in /system/resources/keymaps/it.xkb: il compositore la legge all'avvio e
# la manda a ogni programma Wayland che chiede la tastiera (wl_keyboard.keymap).
#
# PERCHE' COSI'. Un programma Wayland riceve dal compositore i codici dei tasti (quelli di
# Linux) e si traduce da se' i caratteri con la keymap, un testo nel formato XKB: "il tasto 26
# da' e' e, con Maiusc, e' acuta, con AltGr la parentesi quadra". Il testo non lo scriviamo a
# mano (migliaia di righe, e sbagliare un tasto e' facile): lo compila xkbcomp dai file ufficiali
# di xkeyboard-config (layout "it", lo stesso di Linux sui PC italiani), a ogni compilazione del
# sistema. Dentro il compositore non c'e' nessuna libreria XKB: gli arriva un file, e lo gira.
#
#   xkb_keycodes  evdev+aliases(qwerty)   i numeri dei tasti: quelli di Linux piu' 8 (la regola di XKB)
#   xkb_types     complete                come si sceglie il "livello" di un tasto (Maiusc, AltGr, Bloc Num...)
#   xkb_compat    complete                cosa fanno i modificatori (Bloc Maiusc, Bloc Num...)
#   xkb_symbols   pc+it+inet(evdev)+kpdl(comma)
#                                         il layout: i tasti del PC 105 e le lettere dell'italiano, i tasti
#                                         multimediali, e la virgola sul punto del tastierino (come eventi)
#
# La tastiera italiana che traduce il servizio eventi (utente/programmi/eventi.c) per le nostre
# app e questa devono dare gli stessi caratteri: la prova wayland_ingresso confronta i due.
set -e
USCITA=${1:?}
: "${XKB_BASE:=/usr/share/X11/xkb}"
mkdir -p "$(dirname "$USCITA")"
LAVORO=$(mktemp -d)
trap 'rm -rf "$LAVORO"' EXIT
cat > "$LAVORO/it.txt" <<'FINE'
xkb_keymap {
    xkb_keycodes  { include "evdev+aliases(qwerty)" };
    xkb_types     { include "complete" };
    xkb_compat    { include "complete" };
    xkb_symbols   { include "pc+it+inet(evdev)+kpdl(comma)" };
};
FINE
xkbcomp -w 0 -xkb -I"$XKB_BASE" "$LAVORO/it.txt" "$LAVORO/it.xkb" 2>"$LAVORO/errori" || {
    cat "$LAVORO/errori" >&2
    echo "tastiera.sh: xkbcomp non ha compilato la keymap" >&2
    exit 1
}
[ -s "$LAVORO/it.xkb" ] || { echo "tastiera.sh: la keymap e' vuota" >&2; exit 1; }
cp "$LAVORO/it.xkb" "$USCITA"
