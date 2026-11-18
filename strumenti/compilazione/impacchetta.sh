#!/bin/sh
# impacchetta.sh CARTELLA ID CPU PROGRAMMA USCITA: il pacchetto di un'applicazione. Lo chiama
# programmi.mk, solo quando il programma o un file della cartella sono cambiati.
#
# Il pacchetto (APPLICAZIONI.md, sezione 3) e' una cartella, USCITA/<id>/, con dentro:
#   manifest.toml  il manifesto, con "@PROCESSORE@" sostituito dal processore (x86_64 o
#                  arm64): il sorgente e' uno, i pacchetti uno per processore, e cosi' il
#                  manifesto non puo' sbagliare processore
#   code           il programma (le nostre app: PROGRAMMA e' un file)
#   linux/         OPPURE, per le app Linux (utente/applicazioni_linux/, manifesto con
#                  linux = true): l'albero fatto da app_linux.sh (PROGRAMMA e' quella
#                  cartella), con il caricatore di musl, le librerie, il programma e i dati
#   resources/     le icone (la cartella resources/ dei sorgenti) e tutti gli altri file
#                  della cartella che non sono .c, .h, il manifesto o alpine.txt (es. doom1.wad)
# e lo stesso chiuso in uno ZIP, USCITA/<id>.app, con le date fisse (sempre lo stesso file).
# Permessi: code 755, il resto 644, le cartelle 755; dentro linux/ restano quelli che da'
# app_linux.sh (755 il caricatore e i programmi, 644 il resto). Proprietari: li da' build.sh.
set -e
cartella=$1
id=$2
cpu=$3
programma=$4
uscita=$5

manifesto="${cartella}manifest.toml"
[ -f "$manifesto" ] || { echo "impacchetta.sh: manca $manifesto"; exit 1; }
grep -q "^id = \"$id\"" "$manifesto" || { echo "impacchetta.sh: in $manifesto l'id non e' \"$id\""; exit 1; }
grep -q '^processore = "@PROCESSORE@"' "$manifesto" ||
    { echo "impacchetta.sh: in $manifesto manca processore = \"@PROCESSORE@\""; exit 1; }
if [ -d "$programma" ] && ! grep -q '^linux = true' "$manifesto"; then
    echo "impacchetta.sh: $manifesto e' di un'app Linux ma non dice linux = true"; exit 1
fi

pacchetto="$uscita/$id"
rm -rf "$pacchetto"
mkdir -p "$pacchetto/resources"
sed "s/@PROCESSORE@/$cpu/" "$manifesto" > "$pacchetto/manifest.toml"
for r in "$cartella"*; do
    case "$r" in *.c|*.h|*/manifest.toml|*/alpine.txt) continue ;; esac
    [ -f "$r" ] && cp "$r" "$pacchetto/resources/"
done
[ -d "${cartella}resources" ] && cp -R "${cartella}resources/." "$pacchetto/resources/"
find "$pacchetto" -type d -exec chmod 755 {} +
find "$pacchetto" -type f -exec chmod 644 {} +
if [ -d "$programma" ]; then
    contenuto=linux
    cp -R "$programma" "$pacchetto/linux"     # cp -R tiene i permessi dati da app_linux.sh
else
    contenuto=code
    cp "$programma" "$pacchetto/code"
    chmod 755 "$pacchetto/code"
fi
find "$pacchetto" -exec touch -d "2026-01-01 00:00:00" {} +

zip_assoluto="$(cd "$uscita" && pwd)/$id.app"
rm -f "$zip_assoluto"
(cd "$pacchetto" && find manifest.toml $contenuto resources | sort | zip -X -q -D "$zip_assoluto" -@)
printf '  %-40s %7s byte  pacchetto\n' "$uscita/$id.app" "$(wc -c < "$zip_assoluto")"
