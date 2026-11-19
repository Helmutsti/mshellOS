#!/bin/sh
# Controllo automatico di Claude Code (hook PreToolUse su Bash e PowerShell): la costruzione
# completa del sistema (costruisci.sh, 5-7 minuti) e' riservata all'agente principale. Se il
# comando viene da un agente (nei dati c'e' "agent_type") e la lancia, lo ferma (uscita 2: il
# motivo, su stderr, lo legge l'agente). Guida: strumenti/LEGGIMI-prove.md.
dati=$(cat)
case "$dati" in
  *'"agent_type"'*)
    case "$dati" in
      *costruisci.sh*)
        echo "La costruzione completa del sistema e' riservata all'agente principale. Usa la prova veloce (strumenti/prova-veloce.py) o l'aggiornamento del disco (strumenti/aggiorna-disco.sh)." >&2
        exit 2 ;;
    esac ;;
esac
exit 0
