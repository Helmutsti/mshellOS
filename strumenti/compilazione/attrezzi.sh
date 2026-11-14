#!/bin/sh
# attrezzi.sh: prepara (una volta sola) il container degli attrezzi per il processore del
# computer e scrive il suo nome. Uso: immagine=$(sh strumenti/compilazione/attrezzi.sh)
set -e
qui="$(cd "$(dirname "$0")" && pwd)"
immagine="mshell-attrezzi-$(uname -m | sed 's/^arm64$/aarch64/')"
if ! docker image inspect "$immagine" >/dev/null 2>&1; then
    echo "preparo gli attrezzi (una volta sola) ..." >&2
    docker build -q -t "$immagine" "$qui" >/dev/null
fi
echo "$immagine"
