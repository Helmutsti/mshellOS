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

**La sandbox: prove in pochi secondi, senza QEMU** (solo PC, `strumenti/sandbox/`). I programmi
veri di mshell girano in un container Docker, compilati come in `build.sh`; cambiano solo lo
schermo (un file in memoria al posto del DRM) e il servizio eventi (mouse e tastiera arrivano
da una presa TCP, tradotti dallo stesso codice di `eventi.c`). Serve solo Docker.

```sh
uv run strumenti/sandbox/sandbox.py foto files                  # apre Files e salva un PNG (~2 s)
uv run strumenti/sandbox/sandbox.py foto attivita --tema scuro  # un'app, con il tema scuro
uv run strumenti/sandbox/sandbox.py prova applicazioni tastiera # prove di test/prove, così come sono
uv run strumenti/sandbox/sandbox.py compila                     # ricompila solo ciò che è cambiato
```

- Se un sorgente è cambiato, `foto` e `prova` ricompilano da sole (pochi secondi; la prima
  volta prepara gli attrezzi con i pacchetti di `build/scarico`).
- Per i programmi in Python c'è la classe `Sandbox`, con gli stessi comandi di `test/qemu.py`
  (clic, tasti, rotella, foto, aspetta che lo schermo sia fermo) più `apri("files")`.
- Le foto sono uguali a quelle di QEMU pixel per pixel (provato: scrivania, Files, Attività),
  tranne l'ora e i dati che cambiano da soli (numeri dei processi, memoria, disco).
- Limiti: l'orologio è quello vero (le date dei file della casa si spostano a "ieri, 18:02");
  la memoria e il disco sono quelli del computer (la prova `attivita` sul totale della
  memoria non vale); nella radice ci sono in più `etc`, `.dockerenv` e `.sandbox`; la prova
  `avvio` (kernel, moduli, USB) non ha senso qui.

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
  `/system/apps/<app>/{code, manifest.toml, resources}`, `/system/bin`,
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
| `utente/libreria/` | la libreria dei programmi (`libreria.h`: file, processi, socket...) |
| `utente/libreria/gkit/` | GKit, la grafica (vedi `LIBRERIE.md`): protocollo delle finestre, finestra dell'app e schermo (`gkit.h`), strumenti di disegno (`gkit_disegno.h`) |
| `utente/libreria/uikit/` | UIKit, l'interfaccia: `uikit.h` (finestra, componenti, ciclo degli eventi: include tutto quello che serve a un'app), stile e temi (`stile.h` riprende le variabili del file Figma), materiali del sistema (`materiali.h`, font e icone), userbar, quello che serve al compositore (`uikit_compositore.h`) |
| `utente/avvio/` | il programma dell'archivio di avvio |
| `utente/programmi/` | init, eventi, compositore, launcher, scrivania |
| `utente/applicazioni/` | Attivita, Contatore, Doom, Esempio, Files, Impostazioni (una cartella ciascuna, con `manifest.toml` e `resources/`) |
| `utente/libreria/esterni/` | librerie di altri: miniz (zip), tomlc99 (TOML) e stb_image (PNG), con le loro licenze |
| `utente/prove/` | programmi solo per le prove (vanno solo nel disco delle prove) |
| `file/` | quello che finisce nella casa della persona (`/data/users/1000/home`) |
| `strumenti/grafica/` | genera font e icone (anche dagli SVG esportati da Figma) |
| `strumenti/anteprima/` | disegna widget e finestre in PNG sul Mac, senza Docker né QEMU (vedi la sezione 6b) |
| `strumenti/sandbox/` | mshell vero in Docker, senza QEMU, per foto e prove veloci (vedi la sezione 3) |
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
- **La scrivania come nella pagina "Scrivania" di Figma** (ramo `scrivania-figma`): al posto
  della vecchia isola in alto c'è la **userbar**, una pillola di vetro sul bordo sinistro,
  centrata in altezza: il pulsante App e sotto le app del sistema (Attività, Doom, Esempio,
  Files, Impostazioni, Store). Il pulsante App apre il **vassoio delle app** a destra: tutte le
  app installate in una griglia da 3 (anche quelle della persona, per esempio Contatore). Un clic
  su un'app la apre o, se è aperta, la riporta davanti; un clic nel vassoio lo chiude, e si
  chiude anche premendo sullo sfondo o quando un'altra finestra prende il fuoco.
  Le **icone sono quelle vere** delle app: la PNG piccola del pacchetto (18, la più vicina ai
  16 della userbar e ai 20 del vassoio), del tema in uso, se no dell'altro tema; letta con
  stb_image (`esterni/stb`, pubblico dominio o MIT) da `immagine_carica` di GKit. Esempio,
  Impostazioni e Contatore hanno solo l'SVG: per loro il simbolo "pacchetto".
  La **cartella Home sulla scrivania non c'è più** (Files si apre dalla userbar).
  **L'ora e la data non ci sono più**: il disegno non le mostra (vedi "Da confermare").
  Scelte nostre dove il disegno non dice niente (da confermare): il segno delle app aperte è una
  lineetta scura larga 2 nel margine a sinistra del pulsante (alta 4 se aperta, 14 se in primo
  piano); nel vassoio le app sono in ordine (prima quelle del sistema, poi le altre, per nome);
  mentre il vassoio è aperto il pulsante App resta premuto. Il compositore dà a ogni pannello
  l'ombra del suo tipo (la userbar "barra", il vassoio "sovrapposto", `PANNELLO_BARRA` e
  `PANNELLO_SOPRA` in `gkit.h`); le finestre si aprono a scala da x 140 (le prime da x 60 se la
  scrivania ripartisse): mai sotto la userbar (x 8..48). Non c'è una "finestra ingrandita".

**Le prove**
- Disco di prova con programmi "riempitivi" (in `/data/tests`) per allungare l'elenco dei processi.
- Foto di riferimento rifatte per il puntatore e per Impostazioni.

## 6. Files

**Fatto** (codice in `utente/applicazioni/Files/`, ramo `files-figma`): Files è come nel disegno
di Figma (pagina "App - Files"), fatto solo di componenti di UIKit.

- La cartella Home sulla scrivania **non c'è più** (tolta con la userbar: "non serve più").
- **Finestra**: titolo, barra degli strumenti da 48 (padding 8), sotto la barra laterale (larga
  200, sul vetro, con la linea a destra) e il pannello chiaro (8 di margine ai lati e sotto). La
  finestra resta 800 x 480 (il disegno è 960 x 600): così sta nella scala delle finestre delle
  prove.
- **Barra degli strumenti**: i controlli cominciano dove finisce la barra laterale: indietro e
  avanti (frecce chevron), icone o elenco, il percorso "Home / Documenti" (fuori dalla casa parte
  da "Disco"), la ricerca larga 200.
- **Barra laterale**: Scorciatoie (Home, Scrivania), Filtri (Immagini, Testi, Programmi; un
  secondo clic toglie il filtro), il titolo "Tags" (senza voci, come nel disegno). Apps non
  c'è più (non è nel disegno).
- **Viste**: la griglia di icone e la tabella (Nome, Tipo, Dimensione; un clic
  sull'intestazione ordina, la freccia dice il verso). La colonna Modificato non c'è più (non è
  nel disegno); l'ordine per data resta nel menu "Ordina per".
- **Icone dei file**: gli oggetti del disegno (48 e 24 px, colori del tema), con la variante
  più specifica: cartelle Home, Scrivania, Documenti, Immagini, Download, applicazioni; file
  TXT, PDF, PNG, ZIP, BIN.
- **Stati**: cartella vuota (senza icona, come nel disegno), errore, permesso negato.
- **Menu del tasto destro**: sul file (Apri, Rinomina, Duplica, Informazioni, Elimina, voci S
  senza simboli, come nel disegno) e sullo spazio vuoto (Nuova cartella, Ordina per ›, Mostra
  come icone/elenco, fatto allo stesso modo). Esc lo chiude.
- **Informazioni**: in una mini finestra a parte (Tipo, Dimensione, Posizione); Esc o la X la
  chiudono.
- **Scorciatoie**: Invio (apre, conferma il nome), F2, Ctrl+D, Ctrl+I, Canc, Ctrl+Maiusc+N,
  Esc (chiude il menu, annulla il nome nuovo, toglie la scelta).
- **Azioni sui file**: rinomina nel posto, nuova cartella, duplica (solo file), elimina (file e
  cartelle vuote). Se un'azione non riesce compare un piccolo avviso in basso.
- **Icona dell'app** dal disegno (SVG 96 e PNG 32 e 18, chiara e scura) nel `manifest.toml`.
- Prova `files` aggiornata (anche le scorciatoie, la mini finestra, la cartella vuota). Le foto
  di riferimento `test/riferimenti/*/files_*.png` sono quelle di prima: vanno rifatte dopo
  averle guardate. Lo stato "permesso negato" non si prova più: senza Apps, dalla casa non si
  arriva alle cartelle del sistema.

**Non c'è ancora**:
- aprire i file che non sono cartelle (non esiste ancora un visore: "Apri" è grigio);
- eliminare cartelle piene e duplicare cartelle;
- (fatto) i percorsi arrivano a 4096 lettere come su Linux (`MAX_PERCORSO`; i nomi a 255,
  `MAX_NOME_FILE`): la cartella dal nome lungo di Documenti ora si apre. I messaggi del
  launcher hanno posto per un percorso intero (`MAX_MESSAGGIO` 5120); il percorso del
  *programma* di un'app resta corto (`MAX_PERCORSO_PROGRAMMA` 64) perché viaggia nelle notizie
  del compositore;

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
notizia `TASTO_FISICO`). Il vecchio `TASTO` (caratteri) non è cambiato. UIKit chiede i tasti
fisici per ogni sua finestra e li passa agli "ascoltatori" delle scorciatoie (`uikit/tasti.h`,
`finestra_tk_ascolta_tasti`); mentre Ctrl o Alt sono giù i caratteri non arrivano ai campi.

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
- Sulla scrivania non c'è la cartella Home (prima c'era, in alto a sinistra).
- La scrivania è quella della pagina "Scrivania" di Figma: userbar a sinistra e vassoio delle
  app (la pagina "Userbar – idea Isole" è superata).
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
5. ~~**Scorciatoie da tastiera in Files**~~: fatte con i tasti fisici che `eventi` già manda
   (vedi la sezione 6b), senza cambiare il servizio.
6. **L'avviso in basso in Files** quando un'azione non riesce (es. eliminare una cartella
   piena) non è nel disegno. Va bene, o va disegnato in Figma?
7. ~~**Doppio clic su Home**~~: la cartella Home sulla scrivania non c'è più.
8. **Dove vanno l'ora, la data e le notifiche.** Stavano nella vecchia isola; il disegno della
   scrivania (userbar a sinistra) non le mostra, e per ora non si vedono da nessuna parte.
9. **Le scelte della userbar che il disegno non dice** (sezione 5): il segno delle app aperte,
   l'ordine nel vassoio, il pulsante App premuto mentre il vassoio è aperto, quali app sono
   fissate (oggi quelle del sistema).

## 9. Da fare (in ordine di importanza)

1. **Avvio su un PC vero** da chiavetta USB o da disco. Il file UKI è pronto; manca la
   firma per Secure Boot.
2. **Caricare i moduli del kernel in base all'hardware trovato** (modalias), invece di un
   elenco fisso.
3. **Firmware**: con `NUCLEO=lts` mettere in `/system/firmware` solo quello che serve.
4. **Il servizio eventi**: cosa succede se si blocca o si chiude (oggi non è gestito).
5. **Libreria condivisa** in `/system/lib`.
6. **Terminale** (vedi le decisioni) e la scelta di shell e comandi.
7. **Visore di immagini JPG/PNG**: stb_image c'è già (solo PNG, per le icone delle app); per il
   visore basta togliere `STBI_ONLY_PNG` in `esterni/stb/stb_image.c`.
8. **Pulsante a griglia nel launcher** (rimandato).
9. **Isolamento delle app** con namespaces, cgroups e seccomp di Linux (dal progetto di prima).
10. **Internet** (più avanti).
11. **Sistema in sola lettura come immagine erofs + dm-verity, con aggiornamenti A/B** (futuro).
12. **Prove più veloci sul PC Windows** con l'accelerazione hardware (WHPX, vedi la sezione 3).
13. ~~**Definire meglio il confine tra libreria grafica che parla con il composer e il toolkit.**~~
    Fatto: GKit e UIKit, vedi `LIBRERIE.md`.
14. **Integrazione a pannelli.**
15. **Ragionare sulla title bar.**
16. **Spostare tutte le risorse su disco** (font, icone, immagini), invece di compilarle dentro i
    programmi. Fino ad allora le icone SVG dei pacchetti non si disegnano (lo Store mostra un
    segnaposto) e la scelta della libreria per gli SVG è rimandata.

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
  (titolo 40 px, finestra raggio 28). Il divisore da bordo a bordo sotto il titolo si vede solo
  se sotto c'è il contenuto; con la barra degli strumenti sotto, titolo e barra sono un vetro solo
  (deciso dalla persona: nel disegno la linea c'è, ma sul vetro di Figma non si vede).
- Le icone prendono il colore dalla raccolta "Colore icona" del disegno (`COLORE_ICONA(...)`,
  `tema.h`): Primario, Inverso, Attenuato, Secondario, Pericolo, Successo, Avviso, Informazione,
  col modo che usa il componente in Figma.
- La barra di scorrimento non va mai sopra il contenuto: sta nel padding a destra (14 px). Se il
  contenitore ha un margine di almeno 14 (una pagina), la barra ci sta dentro e il contenuto non
  cambia; se no (Files: tabella e griglia nel pannello) il contenuto, quando scorre, lascia libero
  a destra quanto manca per arrivare a 14 (`barra_scorrimento_riserva`, `uikit.h`).

**Fatto**
- Fase 0: font Plex in `uikit/materiali_dati.h` (`plex_*`, TTF e licenza OFL in `strumenti/grafica/font/`,
  Inter ancora presente); nuove primitive in `gkit/gkit_disegno.h` (ombra con diffusione, ombra interna, ombre a
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
- Files e Attività sono sul disegno nuovo (fatto), ma restano da confermare le scelte dove Figma non
  dice niente. Files: campo di rinomina (32 px nell'elenco, 24 sul tile), avviso a pillola coi
  colori del suggerimento, menu dello spazio vuoto fatto come quello del file, tile "sopra" col
  riquadro CONTROLLO_SFONDO, 4 px tra menu e sottomenu.
- In UIKit, ognuno nel suo file (ramo `files-figma`): menu con voce, intestazione, separatore e
  sottomenu (`menu.h`), tabella con intestazioni di colonna e freccia dell'ordine e righe
  (`tabella.h`), griglia di icone con i tile (`griglia.h`), barra laterale e sue voci
  (`barra_laterale.h`), oggetti di file e cartelle (`oggetti.h`), pannello chiaro del contenuto
  (`pannello.h`), informazioni nella mini finestra (`informazioni.h`, con l'"altra finestra"
  dell'app in `uikit.h`), scorciatoie (`tasti.h`); il percorso ha la "/" del disegno.
- **Attività e Store sul disegno Figma** (ramo `attivita-store-figma`), fatti con componenti di
  UIKit; nuovi, ognuno nel suo file in `uikit/`: `schede.h` (schede con l'indicatore di 2 px),
  `grafico.h` (barra di utilizzo e legenda), `pagina.h` (l'area dei contenuti chiara, con il suo
  margine, che può scorrere), `paragrafo.h` (testo che va a capo, con l'interlinea del disegno),
  `messaggio.h` (messaggio in linea: avviso, errore, informazione), `etichetta_stato.h`
  (Concesso/Non concesso/Nuovo), `riga_permesso.h`, `riga_app.h`, `icona_app.h` (segnaposto
  dell'icona di un'app). La tabella dei processi è quella di `tabella.h` (riga "Processo",
  ordinamento per colonna). Icone Lucide nuove per lo Store in `strumenti/grafica`. Attività e
  Store hanno le loro icone vere (SVG e PNG 32/18). Le scelte dove Figma non dice niente sono da
  confermare. Il pannello incassato di Files (`pannello.h`) resta l'eccezione approvata per Files;
  le altre app usano la pagina (`pagina.h`), con 24 dal bordo dell'area al primo testo, in x e in y.
- Mancano ancora nella libreria: testo con spaziatura delle lettere. `elemento_file.h` (era l'icona
  Home della scrivania, oggi tolta) usa ancora i nomi e le icone vecchie.
- Prove: `test/scrivania.py` ha le posizioni della userbar e del vassoio (ramo `scrivania-figma`);
  nella sandbox passano applicazioni, finestre, mouse e tastiera. `scorrimento.py` (la barra di
  scorrimento non compare) e `store.py` (dopo "Svuota dati") falliscono nella sandbox anche
  prima della userbar, uguali; `files.py` e `immagini.py` falliscono sulle foto di riferimento,
  che con la scrivania nuova vanno rifatte dopo averle guardate. `attivita.py` e `store.py` sono
  aggiornati al disegno nuovo; il riferimento `memoria_totale_*.png` di Attività è da rifare (ora il
  totale si scrive in GB, "1,9 GB", come nel disegno), dopo averlo guardato.
- Widget nuovi: interruttore, menu contestuale e a tendina, suggerimento, schede, tabella, griglia
  come widget, puntatore con le sue forme (idle, link, caricamento, paragrafo, disabilitato).
- ~~Userbar al posto dell'isola di `scrivania.c`~~: fatta, con il disegno della pagina "Scrivania"
  (sezione 5); dove vanno ora e notifiche è ancora da decidere.
- Impostazioni: la scelta del tema deve scrivere il file e chiamarsi Chiaro/Scuro. I servizi già
  avviati non si accorgono del cambio finché non ripartono.
- Token del disabilitato di campi, caselle e scelte: oggi è quasi uguale al normale (da rivedere in Figma).
- Il carattere "…" e "–" non esistono nel font (solo code page 437): usare "..." e "-".
- Foto di riferimento delle prove (`test/riferimenti`): da rifare con `--riferimenti` solo dopo aver
  guardato le schermate. Sul Mac `uv` non c'è: le prove non partono finché non si installa.
- Anteprime: `strumenti/anteprima/anteprima.sh <scena>` (campo, pulsanti, impostazioni, files, token,
  primitive, font, scorrimento; `TEMA=scuro`, `SFONDO=bianco`). Non simulano il vetro del compositore:
  per le finestre vale la foto vera da QEMU.

## 13. Le app come pacchetti .app (il cuore, fatto)

Le regole sono in `APPLICAZIONI.md`, il formato del manifesto in `manifest-guida.toml`.

**Fatto**
- Ogni app ha un `manifest.toml` (al posto del vecchio `manifest`) e due icone provvisorie in
  `resources/` (un quadrato colorato con un'icona Lucide, tema chiaro e scuro; quelle vere
  arriveranno da Figma). Permessi come nella tabella "Le app di oggi". La versione del sistema
  è 1 (`descrizione.h`).
- Il processore nel manifesto lo scrive `build.sh`: nel sorgente c'è `processore = "@PROCESSORE@"`,
  e per ogni processore lo sostituisce con `x86_64` o `arm64`.
- `build.sh` fa un pacchetto per ogni app: `build/<cpu>/pacchetti/<id>.app` (uno zip con
  manifesto, programma e `resources/`). Le app del sistema stanno anche scompattate in
  `/system/apps/<id>`, con lo stesso contenuto.
- **Contatore non è più un'app del sistema.** Il suo pacchetto `contatore.app` sta sulla
  Scrivania della persona (`/data/users/1000/home/Scrivania/`), per provare l'installazione.
  Nella userbar quindi non c'è; dopo l'installazione è nel vassoio delle app.
- Librerie di altri, in `utente/libreria/esterni/`: miniz 3.0.2 (zip) e tomlc99 (TOML), con
  licenza MIT, e stb_image 2.30 (PNG, pubblico dominio o MIT).
- Una libreria nuova per i pacchetti (`pacchetto.c`): legge un `.app` senza scompattarlo, lo
  controlla (errori che fermano l'installazione e avvisi, ognuno con una frase per la persona),
  lo installa in `/data/apps/<id>` (anche come aggiornamento, tutto in un colpo, senza toccare i
  dati della persona) e dà l'elenco delle app installate. Rifiuta i percorsi che escono dalla
  cartella dell'app e i collegamenti.
- Installa il launcher, perché solo il sistema può scrivere in `/data/apps`. Le richieste per il
  futuro Store (descrivi, installa, aggiorna, elenco, apri un file) sono scritte in `launcher.h`.
  Può installare solo un'app del sistema con il permesso `app.gestire` (lo Store).
- Aprire un `.app` (doppio clic in Files) non avvia mai l'app: si apre lo Store su quel file, e se
  lo Store non c'è compare l'avviso "Le app vanno installate prima di aprirle".

**Prove**
- `test/pacchetti/prova.sh` (in Docker, senza QEMU): i controlli sulla libreria dei pacchetti.
- `test/prove/pacchetti.py`: in QEMU, con un disco suo (`disco_pacchetti.img`), installa
  contatore.app chiedendolo al launcher, ne svuota i dati e la apre.
- Le prove che usavano Contatore ora usano Esempio, Impostazioni o Files. Le foto di
  riferimento di `immagini` (e alcune di `files`) vanno rifatte dopo averle guardate.

**Lo Store (le funzioni di base, fatte)** — `utente/applicazioni/Store/`, un'app del sistema
con il permesso `app.gestire`. L'aspetto è quello della pagina "App – Store" di Figma, fatto con
i componenti di UIKit.
- Aperto dalla userbar: l'elenco delle app installate, in due sezioni ("Le tue app", "App del
  sistema" con la sua spiegazione); ogni riga con icona, nome, autore e versione e la freccia.
  Senza app della persona: "Non hai ancora installato app". Un clic su una riga apre il dettaglio
  al posto dell'elenco ("App installate" nella barra degli strumenti per tornare): icona, nome,
  autore e versione, descrizione, "Svuota dati" e "Togli app", i permessi con l'etichetta
  "Concesso" (se la persona li ha concessi non è ancora scritto da nessuna parte: si mostra lo
  stato di partenza del disegno) e "Gestisci in Impostazioni" (apre Impostazioni, che non hanno
  ancora la pagina dei permessi), "Cosa fa di diverso" e i ruoli.
- "Svuota dati" e "Togli app" chiedono conferma con l'avviso di UIKit (tre scelte per togliere).
  Dopo, un messaggio in linea: "Dati svuotati" nel dettaglio, ""<nome>" è stata tolta" in cima
  all'elenco (con o senza i dati), o l'errore del launcher (per esempio l'app è aperta).
- Aprendo un `.app` (doppio clic in Files): la richiesta di installazione (titolo della finestra
  "Installa \"<nome>\""), con gli avvisi (l'icona mancante per un tema con le parole del disegno,
  gli altri con la frase del launcher), i permessi che chiederà, "Cosa fa di diverso", i ruoli,
  "Annulla" e "Installa". Per una versione più nuova: "Aggiorna", con "Cosa cambia" (versione,
  permessi nuovi con l'etichetta "Nuovo", differenze nuove). Se non si può installare: un
  messaggio d'errore per motivo (processore, sistema troppo vecchio, nome di un'app del sistema,
  o generico) e "Chiudi". Pacchetto illeggibile: "Impossibile leggere il pacchetto". Dopo
  l'installazione: "<nome> è installata", con "Chiudi" e "Apri <nome>".
- I testi del launcher e dei manifesti sono in UTF-8, il font ha solo la code page 437: lo
  Store li traduce (`testo437.h`, ora nella libreria perché serve anche alla scrivania).
- Il launcher sa anche togliere un'app (`RIMUOVI_PACCHETTO`, con o senza i dati delle persone)
  e svuotarne i dati (`SVUOTA_DATI`, anche per quelle del sistema; la cartella resta, della
  persona, 700). Se l'app è accesa risponde "Chiudi <nome> prima di …" e non la ferma. I dati
  di una persona si cancellano con un processo che ha la sua identità, non come sistema.

**Prove**
- `test/pacchetti/prova.sh`: 78 controlli (con togliere, svuotare, gli id con "/" e "..", i
  collegamenti che puntano fuori, un'app accesa).
- `test/prove/store.py`: in QEMU, tutto il giro dello Store con contatore.app.

**Non c'è ancora**
- La scrivania legge le app solo all'avvio: un'app appena installata (o tolta) compare (o
  sparisce) quando riparte.
- La scrivania mostra le PNG vere delle app; le app con il solo SVG hanno il simbolo
  "pacchetto" (disegnare gli SVG è un passo successivo). Lo Store mostra ancora un segnaposto
  (quadrato con il simbolo "package"): la riga dell'elenco (32) e il dettaglio (64) sono
  componenti diversi (`riga_app.h`, `icona_app.h`) e per un pacchetto da installare l'icona sta
  dentro lo zip; potrebbero usare `immagine_carica` anche loro, ma è un lavoro a parte.
- Nello Store: "Installa da file…", la ricerca automatica degli aggiornamenti (aspettano la
  decisione della persona).
- I permessi chiesti al primo avvio, l'isolamento, le firme.

## Per chi continua il lavoro

- Le decisioni sono della persona: si chiede prima di fare, con domanda, contesto ed esempi.
- Il lavoro si divide tra più agenti che lavorano in parallelo.
- Le prove ARM non si fanno in emulazione.
