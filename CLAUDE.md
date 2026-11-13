# mshell – istruzioni di lavoro

@COMUNICAZIONE.md

## Il progetto
- Sistema operativo proprio: kernel Linux (pacchetti Alpine 3.24, linux-virt per QEMU) + userland nostro da PID 1 in su (init, compositore, scrivania, launcher, librerie GKit/UIKit, app). musl, programmi statici. Niente lavoro sul kernel: i bisogni si mappano su meccanismi Linux (evdev, DRM/KMS, memfd + SCM_RIGHTS, namespaces).
- Il vecchio kernel didattico (mkernel) è congelato nel ramo `mykernel`.
- Documenti di riferimento, da leggere solo nella parte che serve: `STATO.md` (stato e cose da fare), `APPLICAZIONI.md` (app, pacchetti, permessi: è la fonte di verità), `LIBRERIE.md` (librerie grafiche), `disegni/<App>/descrizione.md` (disegno di ogni app).
- **Solo x86_64.** ARM si lascia alla fine: niente build, prove o controlli ARM finché la persona non lo dice.
- Struttura cartelle decisa (da applicare quando si ristruttura il disco): GPT con avvio (FAT32), system (ext4, montato in sola lettura come `/`), data (`/data`, scrivibile). Nomi su disco in inglese minuscolo. I comandi dell'utente stanno in `/data/users/<uid>/bin` e si cercano DOPO `/system/bin`, mai overlay. Niente dati scrivibili sotto `/system`.

## Lavorare risparmiando
- Aprire solo i file che servono. Prima cercare il punto (grep), poi leggere solo quel pezzo. Mai leggere per intero file grandi (es. il compositore).
- Compilazione silenziosa: mostrare solo errori e ultime righe.
- Raggruppare: compilare, provare e fotografare in un solo comando.
- Foto ritagliate sulla finestra, guardate una volta.

## Delegare ad agenti
- Dividere il lavoro in pezzi indipendenti e affidarli ad agenti in parallelo, anche debug e cicli di prova. Integrazione e controllo finale restano a me.
- Ogni compito di agente: il pezzo più piccolo che produce qualcosa di visibile e salvato con commit. Nelle istruzioni: file esatti da leggere, interfaccia, vincoli, comando di prova, cosa riportare. Modello sonnet per compiti stretti. Non aggiungere pezzi a un agente già partito.
- Se una richiesta tocca più sottosistemi nuovi, dirlo e proporre il primo passo più piccolo.
- Nelle istruzioni agli agenti si ricopiano sempre le regole del flusso che li riguarda (sotto).

## Flusso: disegno in Figma
- File "mshell – interfaccia", chiave `X9g7EOJgKBlE4QjmjrTLFt`. Figma è solo il disegno: i valori si leggono una volta e si ricopiano a mano nel codice. Mai proporre programmi che leggono Figma.
- Pagine per ID, non per nome: Files 81:734, Attività 81:735, Impostazioni 141:732, Store 167:2, Terminale 217:2. Dare sempre all'agente l'indirizzo esatto della schermata.
- Stile: IBM Plex Sans (testo), IBM Plex Mono (terminale), icone Lucide. Variabili per nome **e** collezione (la vecchia collezione "mshell" ha nomi doppi).
- Finestre e app sempre fatte con i componenti della libreria, nella variante più specifica e più nuova.
- Una regola decisa per un componente vale per tutti quelli dello stesso tipo, in Figma e nel codice, senza chiedere.
- **Nessuna linea sotto la barra del titolo**, in nessuna finestra (anche se il componente Figma la mostra).
- Niente ombre tagliate: i frame interni non ritagliano; ritagliano solo finestre ed elenchi che scorrono. I bordi non contano nel layout.
- **Alla fine del disegno** l'agente salva in `disegni/<App>/`: `figma/` con una foto per schermata in chiaro e scuro (fatte una volta, si rifanno solo se il disegno cambia) e `descrizione.md` (struttura della finestra, misure, componenti e varianti, nomi di colori e misure, indirizzi delle schermate).
- Impostazioni è stata ridisegnata (schermata 228:52, vedi `disegni/Impostazioni/`): si lavora sul disegno nuovo, il vecchio frame 139:1708 non si tocca.

### REGOLA DELLA SPAZIATURA (da ricopiare identica in ogni istruzione di disegno)
```
1. Lo spazio interno appartiene a UN SOLO livello: il contenitore che delimita un'area visibile (finestra, pannello, tabella, scheda, barra). Solo lui ha padding.
2. Gli elementi di contenuto (righe di tabella, voci di elenco, celle, etichette, icone, pulsanti dentro una barra) hanno padding ZERO verso il loro contenitore. Lo spazio TRA gli elementi lo dà il gap del contenitore, mai il padding degli elementi.
3. Unica eccezione: un elemento con un suo sfondo visibile (riga selezionata o sotto il puntatore, pulsante) può avere un padding interno, perché lo sfondo non tocchi il testo. In quel caso il contenitore TOGLIE lo stesso valore dal suo padding su quel lato: la somma non cambia.
4. Il numero che conta è la DISTANZA TOTALE tra il bordo visibile del contenitore e il primo pixel di contenuto (testo o icona). Deve essere uguale a un solo valore spazio/*, ed essere uguale in orizzontale e in verticale.
5. Verifica obbligatoria prima di consegnare: per ogni schermata, percorri i livelli dal bordo della finestra fino al testo della prima riga e della prima colonna. Somma i padding di tutti i livelli, separatamente per x e per y. Se le due somme sono diverse, o se più di un livello per lato ha padding diverso da zero (tranne l'eccezione del punto 3), correggi togliendo il padding dal livello PIÙ INTERNO. Nel resoconto scrivi le due somme per ogni schermata.
6. ZONA DEI CONTENUTI UGUALE IN TUTTE LE APP: il contenuto principale di una finestra (elenco di file, tabella dei processi, elenco delle app, impostazioni) sta sempre in un pannello chiaro incassato, come in Files: sfondo superficie/elevata, angoli raggio/pannello, margine spazio/* uguale su tutti i lati rispetto al corpo della finestra. Barra degli strumenti e barra laterale restano fuori dal pannello. Prima di disegnare una schermata, apri "App – Files" e copia la stessa struttura.
7. LO SPAZIO LO DÀ SOLO IL CONTENITORE. Un elemento ha padding solo se è un contenitore che delimita un'area visibile (corpo della finestra, pannello). Tutto il resto ha padding 0: barre laterali, barre degli strumenti, aree, intestazioni, piedi, celle senza sfondo proprio. Mai due livelli annidati che danno spazio sullo stesso lato. Mai frame "margine" che servono solo a spostare qualcosa.
8. STRUTTURA DI OGNI FINESTRA: il corpo ha padding spazio/8 su tutti i lati e gap spazio/8 tra le aree (barra laterale, barra degli strumenti, pannello). Il pannello ha il suo padding. Così barra laterale, barra degli strumenti e pannello partono dalla stessa linea, a 8 dal bordo della finestra.
9. DISTANZE FISSE, uguali in tutte le app: bordo della finestra → primo testo della barra laterale = 16; bordo del pannello → primo testo del contenuto = 16; sfondo della voce selezionata → bordo della finestra = 8 e → pannello = 8. Nessuna linea e nessun bordo tra le aree.
10. VERIFICA SULLA FOTO, non sui numeri dei nodi: prima di consegnare misura queste distanze sulla foto di ogni schermata e scrivile nel resoconto. Se una non torna, la causa è un padding su un livello interno: toglilo lì. Mai compensare spostando o aggiungendo spazio altrove.
```
La regola vale anche per il codice: l'agente del codice la riceve identica.

## Flusso: codice delle app e delle librerie
- Si parte da `disegni/<App>/descrizione.md` e dalle foto in `disegni/<App>/figma/`. Figma (MCP) si apre **solo** su un elemento preciso, quando una misura non si capisce dalla foto.
- I valori grafici si copiano a mano nei file del tema/stile della libreria.
- Prima di consegnare un lavoro grafico: foto della sandbox e foto di Figma della stessa schermata **affiancate**, stessa scala, ritagliate e ingrandite, in chiaro e scuro; elencare ogni differenza, anche di 1 px. Se una regola scritta (STATO.md, commenti) contraddice il disegno, vince Figma e si corregge la regola.
- Dopo ogni cambiamento grafico si aggiornano le foto in `disegni/<App>/codice/`.

## Flusso: prove
- App e librerie si compilano e provano nella sandbox Docker, non in QEMU:
  - `uv run strumenti/sandbox/sandbox.py compila`
  - `uv run strumenti/sandbox/sandbox.py foto <app> [--tema scuro] [--uscita file.png]`
  - `uv run strumenti/sandbox/sandbox.py prova <prove...>`
- QEMU (`build.sh`, `run.sh`/`run.ps1`) solo per problemi di avvio (mount, init), dicendo perché.

## Flusso: git
- Commit solo quando la persona lo chiede, o alla fine di un pezzo approvato. Unione in `main` con `git merge --ff-only`, dopo una build x86_64 completa e l'ok della persona.
