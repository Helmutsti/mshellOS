# Crea il disco finito di mshell per il PC (x86_64), pronto per QEMU: build\x86_64\disco.img.
# E' l'uscita del repository (non va su GitHub). Solo il disco: niente dischi delle prove.
# Per installarlo o aggiornarlo sul Desktop: aggiorna.ps1 nella cartella mshellOS-prod.
$ErrorActionPreference = "Stop"
$qui = $PSScriptRoot

# il container degli attrezzi (strumenti/compilazione/Dockerfile), preparato una volta sola
$attrezzi = "mshell-attrezzi-x86_64"
docker image inspect $attrezzi *> $null
if ($LASTEXITCODE -ne 0) {
    docker build -q -t $attrezzi "${qui}\strumenti\compilazione" | Out-Null
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
# gli avvisi del compilatore arrivano sugli errori standard: non devono fermare lo script
$ErrorActionPreference = "Continue"
docker run --rm -e SOLO_DISCO=1 -v "${qui}:/w" $attrezzi sh /w/build.sh x86_64
exit $LASTEXITCODE
