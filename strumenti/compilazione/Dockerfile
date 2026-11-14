# Il container degli attrezzi di build.sh: Alpine 3.24 con gli attrezzi gia' installati,
# cosi' build.sh non li reinstalla a ogni giro. Nativo: si fa per il processore del computer
# (mshell-attrezzi-aarch64 sul Mac con chip Apple, mshell-attrezzi-x86_64 sul PC).
# Lo prepara strumenti/compilazione/attrezzi.sh (run.sh, run.ps1 e collaudo.py lo chiamano).
# build.sh funziona anche in un alpine:3.24 nudo: installa da se' quello che manca.
FROM alpine:3.24
RUN apk add --no-cache clang lld binutils make e2fsprogs sfdisk mtools zip && \
    if [ "$(uname -m)" = x86_64 ]; then apk add --no-cache efi-mkuki systemd-efistub; fi
