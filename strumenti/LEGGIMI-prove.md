# Le prove di mshell: tre velocita'

Il metodo generale (perche', come rifarlo altrove) e' in `flusso-sviluppo-veloce.md` sul Desktop
della persona; qui c'e' come si usa in mshell. Tempi misurati il 19/11/2026 su questo PC.

| Prova | Comando | Tempo | Chi |
|---|---|---|---|
| **Prova veloce** | `uv run --with pillow python -I strumenti/prova-veloce.py -c "<copione>"` | ~20 s (la prima volta ~1 min: accende QEMU) | tutti, sempre |
| **Aggiornamento del disco** | `docker run --rm --privileged -v "<cartella>:/w" mshell-debian-attrezzi sh /w/strumenti/aggiorna-disco.sh` | ~3 s | quando la prova veloce segnala un file nuovo |
| **Costruzione completa** | `avvia.ps1` (o `costruisci.sh qemu-pc` nel container) | ~6 min | solo l'agente principale: un controllo automatico la blocca agli agenti (`.claude/settings.json`) |

## I pezzi
- `strumenti/compila.sh <radice>`: compila i programmi di mshell e mette i loro file nella radice. Lo usano tutte e tre le prove.
- `strumenti/aggiorna-disco.sh [disco]`: compila e copia i file dentro la partizione "system" del disco gia' costruito.
- `strumenti/prova-veloce.py`: compila; accende (se serve) il QEMU di prova nascosto, su una copia del disco con `-snapshot`; dalla seriale copia i programmi nuovi in `/run/prova` e li sovrappone a quelli del sistema (`mount --bind`); riavvia la scrivania (greetd); esegue il copione; foto in `build/prova-<posto>/foto/`.
- `.claude/agents/sviluppo-mshell.md`: il tipo di agente con le regole fisse.
- `.claude/controlli/blocca-costruzione.sh`: il controllo automatico.

## Il copione
Una istruzione per riga o separate da `;`; `#` per i commenti.

| Istruzione | Cosa fa |
|---|---|
| `tasti meta_l-tab` | preme e lascia una combinazione (nomi di QEMU: `meta_l` = Start, `spc`, `ret`, `esc`, `up`, `left`...) |
| `tieni meta_l` / `lascia meta_l` | tiene premuto un tasto e lo lascia (es. per la cornice del focus) |
| `scrivi prova` | scrive lettere, cifre e spazi |
| `muovi 640 400` | sposta il puntatore sul pixel x y (schermo 1280x800), senza clic |
| `aspetta 300` | aspetta i millisecondi |
| `clic 1200 16` | clic con il tasto sinistro sul pixel x, y dello schermo |
| `foto nome` | foto dello schermo in `build/prova-<posto>/foto/nome.png` |
| `app` | apre l'app di prova nel desktop attuale |
| `seriale <comando>` | comando nella shell dell'amministratore, con l'uscita |

Opzioni: `--posto N` (un QEMU di prova per agente: porte 4450+10N..4452+10N), `--spegni`.

Esempio: `-c "tieni meta_l; tasti right; aspetta 300; foto focus; lascia meta_l"`.

## Limiti
- I file che nel sistema non ci sono ancora (un programma nuovo) non si possono sovrapporre: serve l'aggiornamento del disco, poi `--spegni` e di nuovo la prova veloce.
- Il QEMU di prova usa la copia del disco fatta all'ultima costruzione o aggiornamento: se cambia il disco, la copia si rifa' da sola alla prossima accensione (`--spegni` prima).
