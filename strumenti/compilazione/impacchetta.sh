#!/bin/sh
# impacchetta.sh CARTELLA ID CPU CODE USCITA: il pacchetto di un'applicazione. Lo chiama
# programmi.mk, solo quando il programma o un file della cartella sono cambiati.
#
# Il pacchetto (APPLICAZIONI.md, sezione 3) e' una cartella, USCITA/<id>/, con dentro:
#   manifest.toml  il manifesto, con "@PROCESSORE@" sostituito dal processore (x86_64 o
#                  arm64): il sorgente e' uno, i pacchetti uno per processore, e cosi' il
#                  manifesto non puo' sbagliare processore
#   code           il programma
#   resources/     le icone (la cartella resources/ dei sorgenti) e tutti gli altri file
#                  della cartella che non sono .c, .h o il manifesto (es. doom1.wad)
# e lo stesso chiuso in uno ZIP, USCITA/<id>.app, con le date fisse (sempre lo stesso file).
# Permessi: code 755, il resto 644, le cartelle 755 (proprietari: li da' build.sh).
set -e
cartella=$1
id=$2
cpu=$3
code=$4
uscita=$5

manifesto="${cartella}manifest.toml"
[ -f "$manifesto" ] || { echo "impacchetta.sh: manca $manifesto"; exit 1; }
grep -q "^id = \"$id\"" "$manifesto" || { echo "impacchetta.sh: in $manifesto l'id non e' \"$id\""; exit 1; }
grep -q '^processore = "@PROCESSORE@"' "$manifesto" ||
    { echo "impacchetta.sh: in $manifesto manca processore = \"@PROCESSORE@\""; exit 1; }

pacchetto="$uscita/$id"
rm -rf "$pacchetto"
mkdir -p "$pacchetto/resources"
sed "s/@PROCESSORE@/$cpu/" "$manifesto" > "$pacchetto/manifest.toml"
cp "$code" "$pacchetto/code"
for r in "$cartella"*; do
    case "$r" in *.c|*.h|*/manifest.toml) continue ;; esac
    [ -f "$r" ] && cp "$r" "$pacchetto/resources/"
done
[ -d "${cartella}resources" ] && cp -R "${cartella}resources/." "$pacchetto/resources/"
find "$pacchetto" -type d -exec chmod 755 {} +
find "$pacchetto" -type f -exec chmod 644 {} +
chmod 755 "$pacchetto/code"
find "$pacchetto" -exec touch -d "2026-01-01 00:00:00" {} +

zip_assoluto="$(cd "$uscita" && pwd)/$id.app"
rm -f "$zip_assoluto"
(cd "$pacchetto" && find manifest.toml code resources | sort | zip -X -q -D "$zip_assoluto" -@)
printf '  %-40s %7s byte  pacchetto\n' "$uscita/$id.app" "$(wc -c < "$zip_assoluto")"
