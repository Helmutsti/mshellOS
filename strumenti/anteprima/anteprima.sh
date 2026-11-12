#!/bin/sh
# Uso: strumenti/anteprima/anteprima.sh <scena> [scala] [uscita.png]
#   <scena>: il nome di un file in strumenti/anteprima/scene/ (senza .c). Elenco: senza argomenti.
#   scala:   ingrandimento intero (2 = pixel doppi), per vedere i dettagli. Default 1.
#   uscita:  default strumenti/anteprima/out/<scena>.png
# Variabile SFONDO=nero|bianco per un fondo piatto sotto il vetro.
# Variabile TEMA=chiaro|scuro per il tema dei colori semantici (tema.h); senza, chiaro. Il
# vecchio stile "a vetro" non cambia: lo leggono solo i token nuovi.
set -e
QUI=$(cd "$(dirname "$0")" && pwd)
RADICE=$(cd "$QUI/../.." && pwd)
if [ -z "$1" ]; then
    echo "scene disponibili:"; ls "$QUI/scene" | sed 's/\.c$//; s/^/  /'; exit 1
fi
[ -f "$QUI/scene/$1.c" ] || { echo "scena '$1' non trovata in $QUI/scene" >&2; exit 1; }
mkdir -p "$QUI/out"
USCITA=${3:-$QUI/out/$1.png}
# I materiali del sistema (font, icone, oggetti) sono file su disco: si compilano anche chi li
# legge (GKit, materiali_file.c) e chi sa quali sono (UIKit, materiali.c), e li si fa cercare
# in risorse/ del repository invece che in /system/resources (MSHELL_RISORSE, gkit_disegno.h).
clang -std=gnu11 -O1 -w -I"$RADICE/utente/libreria" -I"$QUI" \
    -DSCENA="\"$QUI/scene/$1.c\"" "$QUI/anteprima.c" \
    "$RADICE/utente/libreria/gkit/materiali_file.c" "$RADICE/utente/libreria/uikit/materiali.c" \
    -o "$QUI/out/.anteprima_$1"
MSHELL_RISORSE="$RADICE/risorse" "$QUI/out/.anteprima_$1" "$USCITA" "${2:-1}"
