# mshell: a che punto siamo

Questo documento serve a riprendere il lavoro da un altro computer, anche dopo giorni.
Dice cos'è mshell oggi, come si compila e si prova, cosa è stato fatto, cosa è deciso,
cosa aspetta una risposta e cosa resta da fare.

## 1. In breve

- mshell è un sistema operativo con la sua interfaccia grafica (scrivania, finestre,
  applicazioni), in stile vetro smerigliato.
- Sotto c'è il kernel Linux preso così com'è da Alpine Linux. Sopra, dal primo processo
  in poi, ci sono solo i nostri programmi: niente programmi di una distribuzione, niente shell.
- Gira sul PC a 64 bit (avvio UEFI) e su ARM a 64 bit. Oggi si usa in QEMU, la macchina virtuale.
- Il vecchio kernel fatto da noi (la fase "didattica") è finito e conservato nel ramo `mykernel`.

## 2. Come si compila e si avvia

Serve Docker (la compilazione avviene dentro un container Alpine 3.24) e QEMU.
Il risultato va in `build/<cpu>/` e si può cancellare: viene rifatto.

**Windows, PC** (QEMU in `C:\Program Files\qemu\`):

```powershell
.\run.ps1            # compila e avvia il PC con 2 GB di RAM
```

**Mac o Linux** (QEMU con `brew install qemu`):

```sh
./run.sh             # PC (x86_64), avviato da UEFI
./run.sh arm64       # ARM a 64 bit (sul Mac con chip Apple usa la virtualizzazione vera)
```

**Solo compilare**, senza avviare:

```sh
docker run --rm --platform linux/amd64 -v "$PWD:/w" alpine:3.24 sh /w/build.sh x86_64
docker run --rm --platform linux/amd64 -v "$PWD:/w" alpine:3.24 sh /w/build.sh arm64
```

Da Git Bash su Windows serve `MSYS_NO_PATHCONV=1` e `$(pwd -W)` al posto di `$PWD`.
Con la variabile `NUCLEO=lts` (con Docker: `-e NUCLEO=lts` prima di `alpine:3.24`) si usa
il kernel `linux-lts` di Alpine con il firmware, adatto a un PC vero; senza, si usa
`linux-virt` (6.18), adatto alle macchine virtuali.

## 3. Come si fanno le prove

Le prove automatiche accendono mshell in QEMU senza finestra, lo usano come una persona
(mouse, tastiera, foto dello schermo) e controllano il risultato. Servono `uv` e QEMU.

```sh
uv run test/collaudo.py --cpu x86_64              # tutte le prove sul PC (2 GB)
uv run test/collaudo.py --cpu x86_64 --ram 8192   # lo stesso con 8 GB
uv run test/collaudo.py --solo mouse finestre     # solo alcune (--elenco per vederle)
```

- Sul PC passano tutte, a 2 e a 8 GB.
- ARM è solo compilato: per scelta della persona le prove ARM non si fanno in emulazione
  su un PC x86 (tempi e velocità non sono quelli veri, i risultati non sono affidabili);
  si fanno su un ARM vero, il Mac (vedi la sezione 10).
- **Le prove sul PC Windows sono lente** perché QEMU emula il processore via software. Si
  possono accelerare molto (stimato 5–10 volte) attivando la funzionalità di Windows
  "Piattaforma hypervisor Windows" (serve l'amministratore e un riavvio) e aggiungendo
  `-accel whpx` a QEMU. Da decidere.
- La spiegazione completa delle prove è in `test/LEGGIMI.md`.

## 4. Com'è fatto

**L'accensione, passo per passo**

1. PC: il firmware UEFI trova sul disco la partizione di avvio e lancia un unico file,
   `\EFI\BOOT\BOOTX64.EFI`. Dentro ci sono il kernel, il piccolo archivio di avvio e la
   riga di comando del kernel (questo formato si chiama UKI).
   ARM: QEMU carica direttamente in memoria il kernel e l'archivio (`-kernel`, `-initrd`).
2. Parte Linux, che esegue il nostro programma `avvio` (in `utente/avvio/`): carica i
   moduli del kernel, trova le partizioni per nome, monta `system` in sola lettura come
   `/` e `data` su `/data`, poi passa la mano a `/system/init`.
3. `init` fa partire i servizi (eventi, compositore, launcher, scrivania).

**Il disco**: una tabella GPT con tre partizioni.

| partizione | tipo   | contenuto |
|------------|--------|-----------|
| avvio      | FAT32  | il file UKI |
| system     | ext4, sola lettura | il sistema, montato come `/` |
| data       | ext4, scrivibile   | i dati delle persone, delle app e dei servizi, su `/data` |

**Le cartelle nel sistema** (nomi in inglese minuscolo, scelta della persona):

- `/system/init`, `/system/services/<nome>` (un file per servizio),
  `/system/apps/<app>/{code, manifest, resources}`, `/system/bin`,
  `/system/lib` (vuota, tenuta per una futura libreria condivisa),
  `/system/resources/{fonts, icons, wallpapers}`, `/system/firmware`, `/system/modules`.
- `/data/apps` (app installate), `/data/system` (identità, utenti, permessi, elenco delle
  app installate: solo per root), `/data/services/<nome>`,
  `/data/users/<uid>/{home, bin, appsdata/<app>, preferences}`.
- `/dev`, `/proc`, `/sys`, `/run`, `/tmp` li dà Linux.

**Cosa fa Linux e cosa facciamo noi**

- Lo schermo: DRM/KMS di Linux.
- Tastiera e mouse: i dispositivi evdev di Linux, letti dal nostro servizio `eventi`
  (tastiera italiana, ripetizione dei tasti).
- Le finestre: ogni app disegna in un'area di memoria condivisa (memfd) che passa al
  compositore lungo un socket Unix (SCM_RIGHTS). I socket stanno in `/run/mshell`.
- Attività legge processi e memoria da `/proc`, e lo spazio su disco con `statvfs`.
- Tutti i programmi sono statici, con la libreria C musl, compilati con clang.

**Le cartelle del progetto**

| cartella | cosa c'è |
|----------|----------|
| `utente/libreria/` | la libreria comune: disegno, finestre, toolkit, stile (`stile.h` riprende le variabili del file Figma) |
| `utente/avvio/` | il programma dell'archivio di avvio |
| `utente/programmi/` | init, eventi, compositore, launcher, scrivania |
| `utente/applicazioni/` | Attivita, Contatore, Doom, Esempio, Files, Impostazioni (una cartella ciascuna, con il manifest) |
| `utente/prove/` | programmi solo per le prove (vanno solo nel disco delle prove) |
| `file/` | quello che finisce nella casa della persona (`/data/users/1000/home`) |
| `strumenti/grafica/` | genera font e icone (anche dagli SVG esportati da Figma) |
| `strumenti/anteprima/` | disegna widget e finestre in PNG sul Mac, senza Docker né QEMU (vedi la sezione 6b) |
| `test/` | le prove automatiche e le foto di riferimento |
| `build.sh`, `run.sh`, `run.ps1` | compilare e avviare |

## 5. Cosa è stato fatto (oggi, sul ramo `main`)

**Il passaggio a Linux**
- La nostra parte (da init in su) ora gira sul kernel Linux di Alpine.
- Il PC si avvia da UEFI con un solo file (UKI); ARM da QEMU.
- La libreria dei programmi è stata riscritta sopra Linux (schermo, eventi, finestre, socket).

**Il disco a tre partizioni**
- avvio, system (sola lettura) e data; cartelle in inglese.
- Il launcher cerca le app prima in `/system/apps` e poi in `/data/apps`.

**L'interfaccia**
- Pulsanti che si illuminano al passaggio del mouse (riduci, chiudi, voci della barra delle app).
- Puntatore nero in stile Mac.
- Barra degli strumenti sotto il titolo, con lo stesso vetro, che si spegne insieme al titolo.
- Font e icone nuovi.
- Attività come nel disegno: pagine Processi, Memoria e Disco, con totale e spazio libero
  di `system` e `data`; la riga scelta è in vetro chiaro.

**Le prove**
- Disco di prova con programmi "riempitivi" (in `/data/tests`) per allungare l'elenco dei processi.
- Foto di riferimento rifatte per il puntatore e per Impostazioni.

## 6. Files e cartella Home

L'applicazione Files (per sfogliare i file) e la cartella Home sulla scrivania si stavano
finendo mentre veniva scritto questo documento.

**Fatto** (codice in `utente/applicazioni/Files/`, icona blu "cartella" nell'isola):

- **Cartella Home sulla scrivania** in alto a sinistra: un doppio clic apre Files sulla tua home.
- **Barra laterale**: Scorciatoie (Home, Scrivania, Apps) e Filtri (Immagini, Testi, Programmi;
  un secondo clic toglie il filtro).
- **Barra degli strumenti**: indietro/avanti con la cronologia, percorso cliccabile, vista a
  icone o a elenco, ricerca per nome. Fuori dalla home il percorso parte da "Disco".
- **Viste**: a icone (nomi su due righe con "…") e a elenco (colonne, ordinamento, barra di
  scorrimento). Riga o elemento scelto in vetro chiaro.
- **Stati**: cartella vuota, errore, permesso negato.
- **Menu del tasto destro**: sul file (Apri, Rinomina, Duplica, Informazioni, Elimina) e
  sullo spazio vuoto (Nuova cartella, Ordina per ›, Mostra come icone/elenco), aperti dove
  si clicca.
- **Azioni sui file**: rinomina nel posto, nuova cartella, duplica (solo file), informazioni,
  elimina (file e cartelle vuote). Se un'azione non riesce compare un piccolo avviso in basso.
- **Icone di file e cartelle** prese dal disegno in Figma (56 e 18 px, senza contorni).
- Il launcher ora può aprire un'app su un percorso: aprire una cartella avvia Files lì.
- File d'esempio nella cartella `file/` del progetto (finiscono nella home).
- Prova automatica nuova `files` con 10 immagini di riferimento.

**Prove sul PC**: 9 su 9 a 8 GB. A 2 GB il primo giro è fallito una volta sulla prova
`avvio` (la riga della tastiera USB è arrivata dopo 30 secondi: un problema di tempi
dell'avvio, non di Files); il giro ripetuto è passato 9 su 9. Da tenere d'occhio.

**Non c'è ancora**:
- aprire i file che non sono cartelle (non esiste ancora un visore: "Apri" è grigio);
- eliminare cartelle piene e duplicare cartelle;
- **le scorciatoie da tastiera** (Ctrl+D, F2, Canc, Esc...): il servizio `eventi` oggi non
  manda alle app Ctrl, i tasti funzione, Canc ed Esc. Funziona solo Invio. I menu le mostrano
  già, come nel disegno;
- la libreria accetta percorsi di al massimo 63 lettere (la prova "errore" usa proprio una
  cartella con un percorso più lungo): va alzato a 4096 come su Linux.

**Da rivedere a occhio**: nel menu del tasto destro aperto sopra l'elemento scelto, la
sfocatura sotto il menu lascia una macchia chiara vicino ad "Apri" (si vede in
`test/riferimenti/x86_64/files_menu_file.png`).

## 6b. Doom, tasti fisici e anteprima

**Doom** (`utente/applicazioni/Doom/`): doomgeneric (GPL) con il WAD shareware `doom1.wad`
(4 MB, sha1 `5b2e249b...`). Disegna a 320x200 e viene ingrandito in una finestra 640x480 con
una barra (trascinamento e X). Si apre dalla scrivania (icona rossa con il triangolo).
- Comandi: frecce, Ctrl spara, Spazio usa/apre, Maiusc corre, Alt passi laterali, Invio ed Esc
  per il menu. Salvataggi in `/data/users/1000/appsdata/doom`.
- **Provato a mano su ARM (Mac): funziona.** Su x86 compila ma non è stato provato (in QEMU
  emulato sarà più lento).
- Non c'è l'audio né il mouse. Se parte male l'errore è silenzioso (`I_Error` scrive su uno
  stdout che in mshell non va da nessuna parte).

**Tasti fisici** (premuto/rilasciato): `eventi` manda `EVENTO_TASTO_FISICO` (codice Linux
KEY_*, senza ripetizione automatica) al compositore, che lo recapita solo alla finestra
attiva che l'ha chiesto con `finestra_tasti_fisici()` (messaggio `VOGLIO_TASTI_FISICI`,
notizia `TASTO_FISICO`). Il vecchio `TASTO` (caratteri) non è cambiato. Il toolkit non usa
ancora i tasti fisici: le scorciatoie di Files (Ctrl, F2, Canc, Esc) si possono costruire su
questo, ma la domanda 5 della sezione 8 resta aperta.

**Anteprima su Mac**: `strumenti/anteprima/anteprima.sh <scena> [scala]` disegna in un PNG
(`strumenti/anteprima/out/`) in circa un secondo. Scene: `campo`, `pulsanti`, `impostazioni`,
`files`; una scena nuova è un file C con un `main()` come quello di un'app. Disegna un solo
fotogramma (niente eventi, hover o animazioni) con uno sfondo a gradiente finto: spaziature e
testo sono esatti, il vetro un po' meno.

**Altro**: lo spazio tra l'ultima lettera e il cursore dei campi è `CAMPO_CURSORE_SPAZIO`
(`stile.h`, 1 px), usato anche dal campo di rinomina di Files. `build.sh` lega compiler-rt
nei programmi: su ARM `printf` e `scanf` di musl lo richiedono (long double a 128 bit).

## 7. Decisioni prese

**Sistema**
- Il kernel è quello di Alpine, non più il nostro. Il nostro resta nel ramo `mykernel`.
- Nomi delle cartelle di sistema in inglese minuscolo.
- I comandi della persona stanno in `/data/users/<uid>/bin` e si cercano **dopo**
  `/system/bin`, con un'impostazione della shell simile a PATH. Niente sovrapposizione di
  cartelle (overlay): il sistema resta pulito.
- In futuro una vera libreria condivisa in `/system/lib`, invece di copiare lo stesso codice
  dentro ogni programma.
- Terminale (più avanti): i comandi sono programmi, non app. L'app Terminale riceve un
  permesso esplicito "accedere ai tuoi file", come su macOS. Da dove prendere la shell e i
  comandi (toybox, busybox o fatti da noi) è ancora da decidere.

**Interfaccia** (file Figma "mshell – interfaccia", chiave `X9g7EOJgKBlE4QjmjrTLFt`)
- Font Inter, icone Lucide, aspetto a vetro smerigliato.
- Righe scelte in vetro chiaro.
- Barra degli strumenti con lo stesso vetro della barra del titolo; si spegne con lei.
- Disegno di Files approvato: icone dei file senza contorno in stile Mac (56 px grandi,
  18 px piccole), vista a icone e a elenco, menu del tasto destro, stati (vuota, errore,
  permesso negato).
- La cartella Home sta sulla scrivania, in alto a sinistra.
- La voce "Elimina" dei menu è bianca.

## 8. Da confermare (domande che aspettano la persona)

1. **Il pezzo grigio "Altro (file system)" nella barra del Disco di Attività.** Non c'è nel
   disegno Figma. Copre lo spazio che Linux dice occupato ma che nessuna cartella spiega
   (circa mezzo megabyte). Tenerlo, toglierlo o mostrarlo diversamente?
2. **Nomi in minuscolo**: le cartelle delle app nel sistema (`contatore`, ...) e i nomi dei
   processi in Attività sono in minuscolo. Va bene così?
3. **Servizi come file singoli** in `/system/services` (non una cartella per servizio). Va bene?
4. **La sottocartella "Scrivania"** della casa tiene il nome italiano, mentre le cartelle di
   sistema sono in inglese. Va bene?
5. **Scorciatoie da tastiera in Files**: estendere il servizio `eventi` (e il protocollo)
   perché arrivino alle app Ctrl, F2, Canc ed Esc?
6. **L'avviso in basso in Files** quando un'azione non riesce (es. eliminare una cartella
   piena) non è nel disegno. Va bene, o va disegnato in Figma?
7. **Doppio clic su Home**: oggi apre ogni volta una nuova finestra di Files. Meglio portare
   davanti quella già aperta?

## 9. Da fare (in ordine di importanza)

1. **Avvio su un PC vero** da chiavetta USB o da disco. Il file UKI è pronto; manca la
   firma per Secure Boot.
2. **Caricare i moduli del kernel in base all'hardware trovato** (modalias), invece di un
   elenco fisso.
3. **Firmware**: con `NUCLEO=lts` mettere in `/system/firmware` solo quello che serve.
4. **Il servizio eventi**: cosa succede se si blocca o si chiude (oggi non è gestito).
5. **Libreria condivisa** in `/system/lib`.
6. **Terminale** (vedi le decisioni) e la scelta di shell e comandi.
7. **Visore di immagini JPG/PNG** (si era pensato alla libreria stb_image).
8. **Pulsante a griglia nel launcher** (rimandato).
9. **Isolamento delle app** con namespaces, cgroups e seccomp di Linux (dal progetto di prima).
10. **Internet** (più avanti).
11. **Sistema in sola lettura come immagine erofs + dm-verity, con aggiornamenti A/B** (futuro).
12. **Prove più veloci sul PC Windows** con l'accelerazione hardware (WHPX, vedi la sezione 3).

Non servono più: priorità e tempo reale (li dà Linux); la chiamata "chi sono io" (su Linux
c'è `getuid`).

## 10. Da provare su ARM (sul Mac)

Su ARM finora si è solo compilato. Sul Mac:

```sh
docker run --rm --platform linux/amd64 -v "$PWD:/w" alpine:3.24 sh /w/build.sh arm64
./run.sh arm64
uv run test/collaudo.py --cpu arm64
```

- Le foto di riferimento ARM (`test/riferimenti/arm64/*.png`) sono vecchie: mancano il
  puntatore nuovo, la barra degli strumenti di Impostazioni, Files, la Home e il totale della
  memoria. Vanno rifatte con `--riferimenti`, **dopo aver guardato a occhio ogni schermata**.
- Anche il valore atteso della memoria su ARM (nella prova di Attività) va rifatto.
- Su ARM si è provato a mano solo Doom (funziona); le prove automatiche ARM restano da fare.

## 11. Storia

- Ramo **`mykernel`** (su GitHub), fermo al commit `c7d604b`: la fase del kernel fatto da noi.
  Un kernel x86-64 (avvio UEFI) e ARM64 con USB (xHCI, tastiera e mouse HID), ext4 in sola
  lettura, file system virtuale e il compositore.
- Ramo **`main`**: dal commit `a7df46e` in poi, il kernel di Alpine con la nostra parte sopra.
- Prima ancora c'era un vecchio mshell basato su Linux: si trova nella storia prima del
  commit `2987153` ("Tolto il vecchio mshell su Linux"), per esempio in `2987153~1`.

## 12. Nuova grafica "Flusso" (in corso)

Il nuovo disegno (file Figma "mshell – interfaccia", pagine "Token e dimensioni", "Componenti base",
"Componenti complessi", "Puntatore" dentro "Componenti base", "Userbar – idea Isole") sostituisce il
vetro scuro con Inter. Decisioni della persona: font **IBM Plex Sans** (Inter sparisce a fine
lavoro), scala di grigi, **due temi Chiaro e Scuro a tempo di esecuzione** (oggi non si possono
cambiare da Impostazioni), lavoro direttamente su `main`.

**Regole del disegno** (anche in `strumenti/grafica/token-figma.json`, chiave "regole"):
- Profondità: *incassato* = contiene o riceve (campi, caselle, scelte, binari: ombra interna, mai
  bordo); *sollevato* = si preme o si sposta (pulsanti, pezzo mobile); *sovrapposto* = sta sopra il
  contenuto (menu, barre, finestre, suggerimento); *piatto* = il resto, premuto e disabilitato inclusi.
- Controlli S 32 / M 40 / L 48. Azioni (pulsanti, solo icona, gruppi, ricerca) a pillola; campi dati
  a rettangolo arrotondato (raggio 10/12/14). Titolo e contenuto della finestra sono separati
  (titolo 40 px, divisore da bordo a bordo, finestra raggio 28).

**Fatto**
- Fase 0: font Plex in `grafica_dati.h` (`plex_*`, TTF e licenza OFL in `strumenti/grafica/font/`,
  Inter ancora presente); nuove primitive in `disegno.h` (ombra con diffusione, ombra interna, ombre a
  strati, pillola, cerchio, archi: tutto intero, niente float); token e temi in `tema.h` e nella
  sezione nuova di `stile.h` (29 colori semantici per tema, misure, font, ombre); il tema si sceglie
  all'avvio dal file `/data/users/1000/preferences/tema` ("chiaro" o "scuro", predefinito chiaro).
- Fase 1: pulsante (Primario/Secondario/Ghost, S/M/L, solo icona tondo), campo incassato, ricerca a
  pillola, casella, scelta, gruppo di pulsanti incassato, barra degli strumenti, barra di scorrimento
  con binario che compare sotto il puntatore, titolo e contenuto della finestra separati, compositore
  con ombra a strati e raggio 28. Impostazioni è già nel nuovo disegno (provato in QEMU su ARM).
- Il vecchio stile resta in `stile.h` sotto il banner "VECCHIO STILE (da eliminare a fine
  migrazione)", con i vecchi nomi dei pulsanti (NEUTRO, BLU...) come alias.

**Da fare**
- Migrare Files e Attività: usano ancora `CONTENUTO_*`/`VETRO_*` (testo bianco, illeggibile sul vetro
  chiaro) e il disegno a mano di righe e icone; vanno sui token nuovi e sui tile da 96 px.
- Widget nuovi: interruttore, menu contestuale e a tendina, suggerimento, schede, tabella, griglia
  come widget, puntatore con le sue forme (idle, link, caricamento, paragrafo, disabilitato).
- Userbar "Isole minima" (solo il pulsante App, che apre un vassoio di app verso il basso) al posto
  dell'isola di `scrivania.c`; dove vanno ora ora e notifiche è ancora da decidere.
- Impostazioni: la scelta del tema deve scrivere il file e chiamarsi Chiaro/Scuro. I servizi già
  avviati non si accorgono del cambio finché non ripartono.
- Token del disabilitato di campi, caselle e scelte: oggi è quasi uguale al normale (da rivedere in Figma).
- Il carattere "…" e "–" non esistono nel font (solo code page 437): usare "..." e "-".
- Foto di riferimento delle prove (`test/riferimenti`): da rifare con `--riferimenti` solo dopo aver
  guardato le schermate. Sul Mac `uv` non c'è: le prove non partono finché non si installa.
- Anteprime: `strumenti/anteprima/anteprima.sh <scena>` (campo, pulsanti, impostazioni, files, token,
  primitive, font, scorrimento; `TEMA=scuro`, `SFONDO=bianco`). Non simulano il vetro del compositore:
  per le finestre vale la foto vera da QEMU.

## Per chi continua il lavoro

- Le decisioni sono della persona: si chiede prima di fare, con domanda, contesto ed esempi.
- Il lavoro si divide tra più agenti che lavorano in parallelo.
- Le prove ARM non si fanno in emulazione.
