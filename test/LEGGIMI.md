# Le prove automatiche di mshell

`test/collaudo.py` accende mshell (il kernel Linux con i nostri programmi) in QEMU **senza finestra**, lo usa come farebbe una
persona (mouse, tastiera, fotografie dello schermo) e controlla che faccia quello che deve.
Ogni prova gira sia sul PC (`x86_64`, avviato dal firmware UEFI) sia su ARM64, e ogni volta
su un sistema appena acceso. Il disco (uno solo, con le partizioni avvio, system e data)
viene copiato prima di ogni prova: `build/` non si tocca.

## Cosa si prova

| prova          | cosa controlla |
|----------------|----------------|
| `avvio`        | la seriale dell'avvio: Linux partito (da UEFI sul PC), la RAM, i moduli caricati, le partizioni montate (system come `/` in sola lettura, data in `/data`), tastiera e tavoletta USB prese dal servizio eventi; la scrivania compare |
| `applicazioni` | ogni applicazione si apre dall'isola (finestra al suo posto, puntino sotto l'icona); il pulsante del menu non rompe niente |
| `mouse`        | il puntatore gira in tondo per 8 s (e 4 s sopra una finestra): segue, non si blocca, arriva preciso |
| `tastiera`     | Esempio cambia colore a ogni tasto; un tasto tenuto giù si ripete, e lasciato si ferma |
| `finestre`     | portare davanti, trascinare per la barra, ridurre e riaprire dall'isola, chiudere |
| `attivita`     | la pagina Memoria di Attività dice 1972 MB (PC con 2 GB; 7946 con 8 GB: il MemTotal di Linux); "Termina processo" chiude Contatore |
| `scorrimento`  | con 4 applicazioni aperte e 6 processi "riempitivi" (vedi sotto) la tabella dei processi di Attività trabocca: la rotella la scorre, la barra di scorrimento compare e dopo ~1,3 s sparisce, si trascina; un clic dopo lo scorrimento sceglie la riga giusta |
| `immagini`     | alcune scene fisse confrontate pixel per pixel con le foto di riferimento (l'ora è coperta) |

Dopo ogni prova si controlla anche che nella seriale non ci siano `Kernel panic`, `PANICO`,
`ERRORE` o `non riesco`.

## Il disco delle prove

Ogni prova accende il sistema con il disco normale (`disco.img`, lo stesso di `run.sh`).
`build.sh` prepara anche un secondo disco, `disco_prove.img`: uguale, ma nella partizione
data c'è in più la cartella `/data/tests`, con i programmi di `utente/prove/`. Init,
all'accensione, fa partire tutto quello che trova in quella cartella (nel disco normale
non c'è, e non succede niente).

Per ora dentro ci sono 6 copie di `riempitivo` (`riempitivo-1` ... `riempitivo-6`): un
programma che dorme e basta, senza finestra, e serve solo ad allungare l'elenco dei
processi. Lo usa `scorrimento`, perché la tabella di Attività abbia più righe di quante ne
stanno (ogni applicazione si apre una volta sola, quindi da sola la prova non ne può
aggiungere). Una prova sceglie il disco delle prove scrivendo nel suo file:

```python
DISCO = "disco_prove.img"
```

Le altre prove non vedono i riempitivi: i loro conti (righe, processi, foto di
riferimento) non cambiano.

## Come si lancia

Serve [uv](https://docs.astral.sh/uv/) (installa da solo Python e Pillow) e QEMU, con il
firmware UEFI per il PC (`edk2-x86_64-code.fd`, che viene con QEMU). Il firmware si cerca
vicino a QEMU (`C:\Program Files\qemu\share\` su Windows, `<prefisso>/share/qemu/` con
Homebrew e su Linux), poi in `$(brew --prefix qemu)/share/qemu/` e `/usr/share/qemu/`, come
`run.sh`; se sta altrove, si dice con la variabile `OVMF` (il percorso del file).

**Windows** (QEMU in `C:\Program Files\qemu\`, o nel PATH):

```powershell
uv run test\collaudo.py
```

**Mac / Linux** (QEMU nel PATH, per esempio `brew install qemu`):

```sh
uv run test/collaudo.py
```

Le opzioni:

```sh
uv run test/collaudo.py --cpu x86_64           # solo il PC ("x86" vale lo stesso; oppure arm64; tutte se non si dice)
uv run test/collaudo.py --ram 8192             # il PC con 8 GB di RAM (2048 MB se non si dice; ARM64 ha sempre 512)
uv run test/collaudo.py --solo mouse finestre  # solo alcune prove (--elenco per vederle)
uv run test/collaudo.py --compila              # prima compila con Docker, come run.sh
uv run test/collaudo.py --build altra/cartella # usa un'altra cartella compilata (con dentro x86_64/ e arm64/)
uv run test/collaudo.py --porte 4475           # QMP su 4475, monitor HMP su 4476 (4465 se non si dice)
uv run test/collaudo.py --tieni                # tiene le foto anche delle prove riuscite
```

Alla fine c'è un riassunto; se una prova fallisce il programma esce con codice 1, e in
`test/risultati/<cpu>/<prova>/` restano le foto, la seriale e (per le immagini) la foto
delle differenze, in rosso. `test/risultati/` non va nel repo.

Se QEMU è già aperto (con `run.sh`), le prove funzionano lo stesso: usano una copia del
disco e porte loro. Se le porte sono occupate, se ne scelgono altre con `--porte`.

L'orologio del PC virtuale è fisso (`-rtc base=2026-01-01T10:00:00,clock=vm`): l'isola
segna sempre "gio 1 gen 10:00" o poco più, prova dopo prova. Sul PC l'accensione dura un po'
di più: prima parte il firmware UEFI, poi Linux.

## Aggiungere una prova

Un file nuovo in `test/prove/`, per esempio `test/prove/orologio.py`:

```python
"""orologio: ... (cosa prova e come, per chi legge)"""
import scrivania as s

NOME = "orologio"
ORDINE = 80                  # la posizione nella fila
DESCRIZIONE = "una riga per --elenco"

def esegui(p):
    m = p.m                  # la macchina, gia' accesa, con la scrivania pronta
    s.apri(m, "Contatore")   # clic sull'isola, aspetta che si apra
    foto = m.foto("aperta")  # la foto resta in risultati/ se la prova fallisce
    p.controlla(..., "cosa non va")
```

Gli strumenti stanno in `test/qemu.py` (accendere, foto, `clic`, `trascina`, `rotella`, `cerchio`,
`premi`, `tasto_giu`/`tasto_su`, `hmp`, `aspetta_che`, `aspetta_schermo_fermo`) e in
`test/scrivania.py` (dove sono l'isola e le finestre: se cambia il disegno, si cambia lì).
Meglio **aspettare una condizione** (`aspetta_che`) che dormire un tempo fisso.

## Le foto di riferimento

`immagini` e `attivita` confrontano lo schermo con le foto in `test/riferimenti/<cpu>/`
(`x86_64/` e `arm64/`). Per la memoria c'è una foto per ogni RAM provata:
`memoria_totale_2048.png` e `memoria_totale_8192.png` sul PC, `memoria_totale_512.png` su ARM64.
Quando il disegno cambia apposta (nuovo stile, nuove applicazioni), si rifanno:

```sh
uv run test/collaudo.py --riferimenti            # tutte e due le CPU
uv run test/collaudo.py --riferimenti --cpu x86_64  # solo il PC
uv run test/collaudo.py --riferimenti --cpu x86_64 --ram 8192 --solo attivita  # la memoria a 8 GB
```

Prima di salvarle nel repo, guardarle: devono mostrare la cosa giusta (per
`memoria_totale_<MB>.png`, le cifre 1972 sul PC a 2 GB e 7946 a 8 GB; su ARM64 il MemTotal
di Linux con 512 MB).
