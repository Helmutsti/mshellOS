# Picker: lavoro a meta' (da finire)

Ramo di partenza: origin/compositore-scelta. Ramo di lavoro: servizio-picker (pubblicato come servizio-picker-wip).
Niente e' stato compilato ne' provato.

## Fatto
- Punto 1, contratto (completo, non compilato):
  - `utente/libreria/corekit/picker.h`: NOME_PICKER, D_PORTA_PICKER 7, richieste open/save, chiavi name/reason, esiti PICKER_NON_RISPONDE (-1), PICKER_ANNULLATO (-2), PICKER_NON_VALIDO (-3), PICKER_ERRORE (-4), PICKER_ATTESA_MS (10 minuti).
  - `utente/libreria/corekit/interno/picker.c`: `picker_apri(nome, massimo)`, `picker_salva(proposto, nome, massimo)` con `chiedi_entro` lungo.
- Piccola aggiunta a UIKit: campo `secondaria` in `struct etichetta` (uikit.h) e uso in `interno/widget.c` (testo TESTO_SECONDARIO, serve all'etichetta "Nome" di Salva).

## Manca
- 2 Servizio `utente/programmi/picker.c`: da riscrivere al posto del provvisorio (modello: catalog.c: nome prestato da init al descrittore 0, custode al 1, clienti, porte con attach/connect, riprendi_porte). Una richiesta alla volta, `uikit_esegui_scelta(&f, chi.programma)`, poi `finestra_chiudi(&f.finestra)` e risposta con allegato (poi chiudere il proprio fd). Sicurezza: aprire la Home (`/data/users/<getuid()>/home`) e ogni file con `openat2` (syscall 437, struct open_how definita a mano) con `RESOLVE_BENEATH | RESOLVE_NO_MAGICLINKS`; per i file usare O_NONBLOCK, controllare S_ISREG, poi togliere O_NONBLOCK. Salva: O_WRONLY|O_CREAT|O_TRUNC, 0644. Nome valido: non vuoto, senza "/", non "." o "..", senza caratteri di controllo. Elenco cartelle: getdents64 sul fd restituito da openat2, `fstatat(AT_SYMLINK_NOFOLLOW)`, saltare i collegamenti e i tipi diversi da file e cartella, cartelle prima, poi nome senza maiuscole/minuscole. Percorso tenuto relativo alla Home (mai "..").
- 3 Interfaccia: finestra mini (`finestra_tk_mini`, `finestra_tk_dimensione` 480x421, Salva 480x469), colonna con gap 16 (padding 16 lo da' la mini finestra): [Salva: riga "Nome" + campo S, gap 8], barra strumenti (pillola indietro/avanti + percorso "Home"), pannello (padding 8) con tabella (colonne come Files: Nome, Tipo 120, Dimensione 100 a destra; generi cartella/file/immagine/testo/programma), riga pulsanti (spazio elastico, Annulla Secondario S, Apri/Salva Primario S; Apri disabilitato senza file scelto; Salva disabilitato con nome non valido). Esc = annulla. Doppio clic su cartella la apre. Ricostruire i widget a ogni richiesta (finestra_tk_prepara azzera la finestra; in UIKit c'e' solo `avvisi_catalog` statico, va bene). Poi foto sandbox affiancate a Figma (chiaro e scuro), distanze misurate, somme dei padding, foto in `disegni/ApriSalva/codice/`. Nota: la testata mini e' 28 (ALTEZZA_BARRA_MINI), Figma dice 29: 421 = 28+16+32+16+265+16+32+16.
- 4 Launcher (`utente/programmi/launcher.c`): porta 7 verso picker a tutte le app, `picker=7` in MSHELL_PORTE (buffer `elenco[96]` da controllare), array `passati` a 8 elementi, `porta_servizio(NOME_PICKER, &app)`, riga del registro, `_Static_assert` e tabella nel commento (anche `D_PORTA_PICKER == 7`). L'altro agente porta a 9 con tasks all'8: nel merge ci pensa il coordinatore.
- 5 Init (`utente/programmi/init.c`): `risorse_picker[2]` (nome da `ascolta(NOME_PICKER)` e canale del custode), voce del servizio picker con uid UID_APPLICAZIONI e `custodisce = 1`, controllo di `ascolta` nel main.
- 6 Prova: in `utente/prove/applicazioni/Esempio/esempio.c` tasti "o" (picker_apri: `esempio: aperto <nome>, <byte> byte` o `esempio: apertura annullata`), "s" (picker_salva("prova.txt"), scrive "ciao", `esempio: salvato <nome>`), "m" (picker_salva("../fuori.txt") deve dare PICKER_NON_VALIDO: `esempio: salva ../fuori.txt: non valido`). Riscrivere `test/prove/scelta.py` (o nuova `picker.py`) con disco_app.img: nessuna cartella /data/tests/scelta. Togliere il ciclo provvisorio, il disco `disco_scelta.img` (build.sh righe ~766-781 e commento ~70), "scelta" in `strumenti/sandbox/sandbox.py` (DISCHI) e in `avvio_sandbox.c`. Esempio e' 320x220, la scelta 480x421 e' piu' grande: il compositore la centra sopra l'app, controllare dove cadono i clic. Makefile e compila.sh della sandbox gia' compilano picker.
- 7 Documenti: SISTEMA.md (sez. 1 picker; sez. 3 tabella porte: 7 picker), APPLICAZIONI.md (sez. 9, "Aprire un file scelto dalla persona"), COMPONENTI.md (voce picker vera; launcher, init), STATO.md (riga sotto "La finestra Apri/Salva": fatto).

## Compila?
No, non e' mai stato compilato. Mai lanciati docker, sandbox o QEMU.

## Problemi e dubbi
- Domanda aperta: in Salva, se il file esiste gia', si sovrascrive senza chiedere (il disegno non ha una conferma). Da chiedere alla persona.
- Se l'app muore mentre la finestra e' aperta, la finestra resta fino a quando la persona la chiude (si potrebbe guardare il collegamento con `finestra_tk_guarda`): non fatto, non chiesto.
- Se catalog scollega il picker (casella piena tra una finestra e l'altra), UIKit si ricollega da sola alla prossima finestra: il primo disegno puo' avere il tema vecchio.
- Il comando `uv` non c'e': usare il Python indicato nel compito. Il volume sandbox da usare e' `mshell-sandbox-oggetti-picker`.
