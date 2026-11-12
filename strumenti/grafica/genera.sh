#!/bin/sh
# Rifa' utente/libreria/grafica_dati.h (font, icone, immagini, sfondo). Non serve a ogni
# compilazione: solo quando cambia la grafica. Gira nel container alpine:
#   docker run --rm -v "<cartella del repo>:/w" alpine:3.24 sh /w/strumenti/grafica/genera.sh
set -e
apk add --no-cache python3 py3-pillow rsvg-convert curl unzip >/dev/null
mkdir -p /tmp/inter
cd /tmp/inter
# Inter 4.1, dal sito ufficiale (rsms/inter): servono i tre pesi statici in TTF
curl -sfL -o inter.zip https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip
unzip -oq inter.zip 'extras/ttf/Inter-Regular.ttf' 'extras/ttf/Inter-Medium.ttf' 'extras/ttf/Inter-SemiBold.ttf'
# IBM Plex Sans sta gia' nel repo: strumenti/grafica/font/IBMPlexSans (con OFL.txt)
python3 /w/strumenti/grafica/genera.py /tmp/inter/extras/ttf /w/utente/libreria/grafica_dati.h
