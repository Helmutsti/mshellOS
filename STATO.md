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
./run.sh             # compila e avvia, sul processore del computer (arm64 sul Mac con chip Apple)
```

**Solo compilare**, senza avviare (sempre sul processore del computer, Docker nativo):

```sh
docker run --rm -v "$PWD:/w" $(sh strumenti/compilazione/attrezzi.sh) sh /w/build.sh
```

Prima si compila, poi si monta: i programmi pronti restano in `build/<cpu>/oggetti` e make
(`strumenti/compilazione/programmi.mk`, lo stesso della sandbox) rifa' solo quello che e'
cambiato; archivio di avvio, kernel e UKI si rifanno solo se cambia quello da cui vengono; il
disco si monta ogni volta dai pezzi pronti. Sul Mac (14/11/2026): da zero 13 s, dopo una
modifica circa 5 s (prima, con x86 emulato, oltre 5 minuti). Il container degli attrezzi
(`mshell-attrezzi-<arch>`) si prepara una volta sola.

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

**La shell di prova** (solo PC, provvisoria: la persona l'ha voluta nella versione normale "e
poi la correggiamo"). È una shell stupida, `utente/programmi/shell.c`, con solo `pwd`, `cd`, `ls`
(anche `-l`), `help` ed `exit`; gira come sistema (uid 0) sulla seriale. Il programma è in
`/system/bin/shell`; init la fa partire sulla seriale (`/dev/ttyS0`) e la fa ripartire quando
finisce con `exit`. La shell abbassa da sé il livello dei messaggi del kernel (equivalente di
`dmesg -n 1`), così non si mescolano con quello che si scrive. La riga la corregge il kernel
(la seriale è un terminale normale: eco e backspace); Ctrl-D come `exit`.
Ogni altra parola fa partire un **programma**: con `/` è un percorso (assoluto o relativo alla
cartella), senza si cerca in `/system/bin` e poi in `/data/users/<uid>/bin`. Le parole si
dividono agli spazi, `"tra virgolette"` è una parola sola (niente variabili, tubi, redirezioni).
Il programma gira nella cartella della shell, con PWD e HOME=/home, in primo piano: ha lui il
terminale (Ctrl-C arriva a lui), poi la shell se lo riprende e dice l'esito se non è 0. Ctrl-Z
non mette da parte niente (il programma riparte subito). La prova usa `utente/prove/argomenti.c`,
nel disco `disco_shell.img`.

**I comandi** sono fatti da noi, uno per file in `utente/comandi/<nome>.c`: `build.sh` (e la
sandbox) li mette in `/system/bin/<nome>`, legati solo con CoreKit. Il primo è **`webask`**, un
"curl" essenziale: `webask [-i] http://nome[:porta]/percorso` scrive il contenuto della pagina
(`-i` anche stato e intestazioni). Solo HTTP, niente HTTPS (decisione della persona); i
reindirizzamenti non si seguono, si dice dove porterebbero. Codici di uscita in `webask -h` e
nel commento in testa al file. Nessun altro comando di base per ora (decisione della persona).

Per usarla: nella finestra di QEMU apri la seriale, **View (Visualizza) -> serial0** (compare il
registro dell'avvio e, in fondo, il prompt `/ # `; se non si vede, Invio). Per tornare allo
schermo: View -> virtio-vga. Con `-serial stdio` (come l'ARM di `run.sh`) la seriale è il
terminale da cui parte QEMU. `uv run test/shell.py` la prova da sola (accende
`build/x86_64/disco.img` e scrive i comandi sulla seriale). Nella sandbox Docker non c'è kernel
né seriale: per una shell si entra nel container con `docker exec -it <nome> sh`.

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
  `/system/resources/{fonts, icons, wallpapers}` (le risorse del sistema: font, icone e sfondo
  gia' disegnati, da `risorse/` del repository), `/system/firmware`, `/system/modules`.
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

**Come si disegna: solo quello che cambia** (le "zone rovinate", come in Wayland, GTK e macOS)

- **Nell'app (UIKit).** Un widget che cambia lo dice con `widget_cambiato` (tutto il suo spazio,
  più i 24 pixel che può sbordare) o `widget_cambiato_zona` (solo un pezzo: la riga di una
  tabella, il tile di una griglia, la voce di un menu o della barra laterale). La finestra
  raccoglie le zone (al massimo 8; quelle che si toccano diventano una) e al ridisegno rifà
  **solo loro**: per ogni zona ritaglia il pennello lì e disegna l'albero dal fondo, saltando i
  widget che non la toccano. Il risultato è uguale pixel per pixel a un ridisegno intero.
  `uikit_ridisponi` (impaginazione cambiata) e l'avviso (il velo) rifanno tutto. Un widget
  nascosto, o in una pagina nascosta, non chiede niente.
- **Le due immagini.** L'immagine libera è quella di due fotogrammi fa: GKit tiene per ognuna le
  zone che le mancano (`finestra_arretrate`) e UIKit le rifà insieme a quelle nuove.
- **Il protocollo.** `MOSTRA` porta le zone cambiate (`finestra_mostra_zone`, in `gkit.h`; 0 zone =
  tutta la finestra, come fa Doom con `finestra_mostra`). Il compositore somma le zone dei
  `MOSTRA` arrivati tra una composizione e l'altra e ricompone solo quei rettangoli dello
  schermo. Il vetro smerigliato di ogni finestra (la sfocatura già ingrandita) è tenuto da parte
  e si rifà solo quando la finestra si sposta o cambia quello che c'è dietro.
- **Le notizie a gruppi.** Il ciclo degli eventi (`uikit_esegui`) serve tutte le notizie arrivate
  prima di ridisegnare, una volta sola; dei movimenti del puntatore di fila conta l'ultimo. Se
  le due immagini sono tutte e due del compositore non lo aspetta: continua a servire le notizie
  finché una torna libera.
- **Gli strumenti di disegno** (`gkit_disegno.h`) lavorano solo dentro il ritaglio, e le forme
  arrotondate calcolano la distanza dal bordo solo vicino agli angoli e ai lati: il centro di
  ogni riga si riempie diritto. Provato su più di un milione di casi a caso: stessi byte di prima.
- **Attività** ridisegna ogni secondo solo le righe il cui testo è cambiato, e la barra e la
  legenda solo se cambiano i numeri scritti o i pixel della barra; le schede nascoste niente.
- Tempi nella sandbox, prima → dopo (un cambio sotto il puntatore, app + compositore): icona di
  Files 7,2 + 3,6 ms → 0,4 + 0,4 ms; riga di Files 11,2 + 3,5 → 0,4 + 0,6; riga di Attività
  12,8 + 3,7 → 0,4 + 0,8; aprire un menu 12,5 + 3,5 → 1,6 + 0,8; Attività ferma: un ridisegno
  intero al secondo → niente.
- **Trascinare una finestra** (ramo `difetto-trascina`). Prima a ogni passo il compositore rifaceva
  l'ombra a strati e il vetro smerigliato di tutta la finestra nel posto nuovo. Ora:
  - **il vetro dello sfondo da solo** (sfondo e colore di quando non c'è) è calcolato una volta
    per tutto lo schermo e rifatto solo quando lo sfondo cambia. Ogni pixel del vetro dipende
    solo da quello che ha intorno (44 pixel): dove lì intorno c'è solo lo sfondo, il vetro della
    finestra è quello, copiato. Si rifanno davvero solo i pezzi vicini ad altro: una cornice lungo
    i bordi (lì c'è l'ombra della finestra stessa) e intorno alle finestre che stanno sotto. Se i
    pezzi sono quasi tutta la finestra (sopra un'altra finestra) si rifà tutto, come prima;
  - ogni zona da rifare comincia come una copia dello sfondo da solo, già composto;
  - l'ombra: i colori per i 256 valori della tabella si preparano una volta, e sotto la finestra
    (dove l'ombra non c'è) non si guarda;
  - i conti della sfocatura sono più svelti (le somme dei colori insieme, la divisione già fatta,
    l'ingrandimento in due tempi) e le immagini delle finestre si mescolano due byte alla volta
    (`sopra_a_riga` e `sopra_a_vero` in `gkit/sistema/pixel.h`).
  I movimenti del mouse arrivati durante una composizione erano già messi insieme (si compone una
  volta con l'ultima posizione), e si rifà solo il posto di prima e quello di adesso.
  Tempi nella sandbox, a passo: Files sulla scrivania 6,9 → 2,5 ms; Impostazioni in parte sopra
  Files 4,8 → 2,1 ms. Il risultato è uguale pixel per pixel: 139 foto prese durante e dopo i
  trascinamenti (tema chiaro e scuro) sono identiche byte per byte a quelle di prima, e durante la
  prova ogni vetro rifatto è stato confrontato con quello del calcolo vecchio (525 vetri, nessuna
  differenza).

**Il puntatore e le finestre dietro.** Una finestra che non ha il fuoco non sente il puntatore
che passa: riceve solo il clic (che la porta davanti e le dà il fuoco) e la rotella (che la fa
scorrere senza portarla davanti). Così nelle finestre dietro non si accende niente. Quando il
puntatore esce dalla finestra col fuoco, o lei perde il fuoco, riceve `USCITO` e spegne quello
che aveva acceso (anche la voce di un menu aperto). I pannelli della scrivania (userbar,
vassoio) il puntatore lo sentono sempre.

**Le cartelle del progetto**

| cartella | cosa c'è |
|----------|----------|
| `utente/libreria/corekit/` | CoreKit, la libreria comune (vedi `LIBRERIE.md`): pubblico in `corekit/` (`corekit.h`: file, processi, socket...; `messaggi.h`), del sistema in `corekit/sistema/` (launcher, pacchetti, manifesti), il codice in `corekit/interno/`; si compila in `libcorekit.a` |
| `utente/libreria/gkit/` | GKit, la grafica (vedi `LIBRERIE.md`): pubblico in `gkit/` (`gkit.h`: la finestra dell'app; `gkit_disegno.h`: gli strumenti di disegno), del sistema in `gkit/sistema/` (protocollo delle finestre, schermo, conti per pixel), il codice in `gkit/interno/`; si compila in `libgkit.a` |
| `utente/libreria/uikit/` | UIKit, l'interfaccia: `uikit.h` (finestra, componenti, ciclo degli eventi: include tutto quello che serve a un'app), stile e temi (`stile.h` riprende le variabili del file Figma), materiali del sistema (`materiali.h`: font, icone e oggetti, caricati dai file in `/system/resources` la prima volta che servono; l'elenco in `materiali_elenco.h`), userbar, quello che serve al compositore (`sistema/uikit_compositore.h`); il codice già fuori dagli header in `interno/`, compilato in `libuikit.a` (riordino in corso, vedi LIBRERIE.md) |
| `utente/avvio/` | il programma dell'archivio di avvio |
| `utente/programmi/` | init, eventi, compositore, launcher, scrivania |
| `utente/applicazioni/` | Attivita, Doom, Files, Impostazioni, Store (una cartella ciascuna, con `manifest.toml` e `resources/`) |
| `utente/prove/applicazioni/` | Esempio e Contatore: app solo per le prove, non del sistema (vanno solo in `disco_app.img`, `disco_prove.img` e `disco_pacchetti.img`, e nella sandbox) |
| `utente/libreria/esterni/` | librerie di altri: miniz (zip), tomlc99 (TOML) e stb_image (PNG), con le loro licenze |
| `utente/prove/` | programmi solo per le prove (vanno solo nel disco delle prove) |
| `file/` | quello che finisce nella casa della persona (`/data/users/1000/home`) |
| `risorse/` | le risorse del sistema, gia' pronte, che vanno in `/system/resources`: `fonts/*.font`, `icons/{lucide,objects,files,controls}/`, `wallpapers/default.image` (le fa `strumenti/grafica/genera.py`; i formati in `strumenti/grafica/formato.py`) |
| `strumenti/grafica/` | genera font, icone e sfondo (anche dagli SVG esportati da Figma) come file in `risorse/` |
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
  centrata in altezza: il pulsante App e sotto le app del sistema (Attività, Doom,
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
  `PANNELLO_SOPRA` in `gkit/sistema/protocollo.h`); le finestre si aprono a scala da x 140 (le prime da x 60 se la
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
- **Azioni sui file**: rinomina nel posto, nuova cartella, duplica (file e cartelle, anche piene: la
  cartella si copia con tutto il contenuto), elimina (file, cartelle vuote e cartelle piene: per le
  piene prima compare l'avviso di UIKit "Eliminare \"nome\"?" che dice quanti elementi ci sono
  dentro, e solo con "Elimina" si toglie tutto; i collegamenti simbolici non si seguono; se
  qualcosa non si riesce a togliere resta dov'è e compare il piccolo avviso in basso). Se un'azione
  non riesce compare un piccolo avviso in basso.
- **Icona dell'app** dal disegno (SVG 96 e PNG 32 e 18, chiara e scura) nel `manifest.toml`.
- Prova `files` aggiornata (anche le scorciatoie, la mini finestra, la cartella vuota). Le foto
  di riferimento `test/riferimenti/*/files_*.png` sono quelle di prima: vanno rifatte dopo
  averle guardate. Lo stato "permesso negato" non si prova più: senza Apps, dalla casa non si
  arriva alle cartelle del sistema.

**Non c'è ancora**:
- aprire i file che non sono cartelle (non esiste ancora un visore: "Apri" è grigio);
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
- Ctrl e Spazio diventano i tasti "virtuali" di doomgeneric `KEY_FIRE` e `KEY_USE` (i comandi
  predefiniti di `m_controls.c`), non `KEY_RCTRL` e `' '` del Doom originale: fino alla
  correzione Ctrl non sparava e Spazio non apriva le porte (sbaglio presente da quando c'è
  Doom). Maiusc dà `KEY_RSHIFT` (corre), Alt e Alt Gr `KEY_RALT` (passi laterali).
- Provato su x86 nella sandbox e in QEMU con la prova `tasti_speciali` (Invio per cominciare,
  Ctrl tenuto giù spara di continuo, freccia tenuta giù cammina, Esc apre e chiude il menu).
- Non c'è l'audio né il mouse. Se parte male l'errore è silenzioso (`I_Error` scrive su uno
  stdout che in mshell non va da nessuna parte).

**Tasti fisici** (premuto/rilasciato): `eventi` manda `EVENTO_TASTO_FISICO` (codice Linux
KEY_*, senza ripetizione automatica) al compositore, che lo recapita solo alla finestra
attiva che l'ha chiesto con `finestra_tasti_fisici()` (messaggio `VOGLIO_TASTI_FISICI`,
notizia `TASTO_FISICO`). Il vecchio `TASTO` (caratteri) non è cambiato. UIKit chiede i tasti
fisici per ogni sua finestra e li passa agli "ascoltatori" delle scorciatoie (`uikit/tasti.h`,
`finestra_tk_ascolta_tasti`); mentre Ctrl o Alt sono giù i caratteri non arrivano ai campi.
Esc non è un carattere (arriva solo come tasto fisico): su un avviso (bloccante, o con il
fuoco) UIKit lo usa come Annulla, prima degli ascoltatori. La prova `tasti_speciali`
(`test/prove/`) controlla Doom, le scorciatoie di Files (Ctrl+Maiusc+N, Ctrl+D, Ctrl+I, Canc,
Esc), Esc sull'avviso e il campo della ricerca (Maiusc, Alt Gr, niente caratteri con Ctrl/Alt).

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
  permesso esplicito "accedere ai tuoi file", come su macOS. I comandi li facciamo noi
  (`utente/comandi/`, il primo è `webask`).
- Terminale in finestra (app Terminale, si apre dal vassoio delle app): la shell dentro gira come
  **sistema** (uid 0), non come la persona, e **chiunque può chiederla** al launcher
  (`open_shell`, che le passa il terminale finto). Decisione della persona, provvisoria:
  **terminale come sistema aperto a chiunque: da proteggere** ("poi lo proteggiamo"). I comandi
  restano quelli della shell (nessuno per modificare); `/system` resta in sola lettura.

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
- Le finestre senza fuoco ricevono solo il clic e la rotella, non il puntatore che passa:
  niente illuminato nelle finestre dietro (vedi la sezione 4).

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

## 9. Da fare

Elenco unico delle cose ancora da fare (aggiornato il 12/11/2026). Le domande che aspettano la
persona sono nella sezione 8.

**Da proteggere**: il terminale in finestra apre una shell come sistema (uid 0) a chiunque la
chieda al launcher (`open_shell`, `utente/programmi/launcher.c`): decisione provvisoria della
persona, "poi lo proteggiamo" (chi può chiederla, e con quale permesso).

**I permessi delle porte rispondono sempre sì (provvisorio): da accendere.** La funzione dei
permessi dei servizi (`ha_permesso`, `corekit/sistema/servizi.h` e `corekit/interno/messaggi.c`) oggi non controlla niente e
scrive ogni domanda nel registro con "provvisorio"; va resa vera (sistema / app con i permessi
concessi / comando della persona).

**Il launcher parla con lo standard dei messaggi** (13/11/2026): richieste `request=open_app`,
`install_package`... in testo `chiave=valore`, esiti `result=` con `reason=` per i perché del
launcher; il resoconto dei pacchetti e l'elenco delle app nel foglio allegato. Il contratto è in
testa a `corekit/sistema/launcher.h`, le funzioni per chi chiede in
`corekit/interno/launcher.c`. Chiede `ha_permesso` per `apps.manage`, `programs.open`,
`system.shell` (provvisori). Resta col formato suo il compositore (rimandato: punto 26).

**Fatti per ultimi** (12/11/2026): i tasti speciali (l'errore era nella tabella dei tasti di
Doom; Esc ora chiude gli avvisi) e le risorse su disco (`/system/resources`, sezione 4). Restano
da scegliere un disegnatore di SVG e di TrueType, se un giorno serviranno.

**App e interfaccia**
1. **Impostazioni** da rifare da zero sul disegno Figma: scelta del tema Chiaro/Scuro (scrive il
   file delle preferenze; i servizi già accesi devono accorgersene), pagina dei permessi.
2. **Visore di immagini JPG/PNG** (rimandato): serve il disegno in Figma e la regola di come Files
   sceglie l'app che apre un file (tipi di file nel manifesto, app predefinita...). stb_image c'è
   già: basta togliere `STBI_ONLY_PNG` in `esterni/stb/stb_image.c`.
3. **Store**: le icone delle app (oggi un quadrato) leggendole dai pacchetti; "Installa da file",
   la ricerca degli aggiornamenti.
4. **La scrivania legge le app solo all'avvio**: un'app installata o tolta compare o sparisce solo
   quando riparte.
5. **La conferma di eliminazione in Files** non è disegnata in Figma (testo scelto dall'agente).
6. **Pezzi della libreria che mancano**: interruttore, menu a tendina, suggerimento al passaggio
   del puntatore, gruppo di azioni, forme del puntatore (collegamento, testo, non disponibile,
   attesa). Fatti: barra di scorrimento orizzontale (`barra_scorrimento_prepara_orizzontale`,
   scena `scorrimento`; nessuna app la usa ancora) e testo con spaziatura delle lettere
   (`testo_spaziato`; titolo -1%, etichetta +2%).
7. **Togliere la vecchia grafica** (vetro scuro, Inter) da codice e Figma.
8. **Controllo sulla lunghezza** del percorso del programma di un'app (`MAX_PERCORSO_PROGRAMMA`
   contro `MAX_ID`): un controllo in compilazione, se la persona lo vuole.

**Prove**
9. Rifare le immagini di confronto vecchie (Files, Attività, scrivania) dopo averle guardate.
10. La prova `scorrimento` fallisce ("la barra non compare"), anche prima degli ultimi lavori.
11. La prova `attivita` nella sandbox non può controllare i 2 GB (vede la memoria del PC).

**Regole delle app** (APPLICAZIONI.md)
12. Permessi chiesti al primo avvio e rivedibili in Impostazioni; chiavi (un canale per servizio).
13. **Isolamento delle app** con namespaces, cgroups e seccomp di Linux.
14. Firme e registro con scadenze; versione minima del sistema; tipi di file che un'app apre;
    ruoli; moduli aggiuntivi.

**Sistema**
15. **Avvio su un PC vero** da chiavetta USB o da disco. Il file UKI è pronto; manca la firma per
    Secure Boot.
16. **Caricare i moduli del kernel in base all'hardware trovato** (modalias).
17. **Firmware**: con `NUCLEO=lts` mettere in `/system/firmware` solo quello che serve.
18. **Il servizio eventi**: cosa succede se si blocca o si chiude.
19. **Libreria condivisa** in `/system/lib`.
20. **Terminale** e la scelta di shell e comandi.
21. **Internet**: c'è il servizio `network` (accende `eth0`, indirizzo con DHCP o fisso da
    `/system/config/network/network.conf`, DNS in `/run/network/resolv.conf`; prova con
    `disco_rete.img` in QEMU). Prima di accendere la scheda mette il firewall (nftables, tabella
    `inet mshell`, parlando al kernel via netlink con codice nostro), con le regole predefinite di
    `/system/config/network/firewall.conf`: ingresso bloccato tranne risposte, ping e ICMPv6
    necessario; inoltro bloccato; uscita libera. Prova con `disco_firewall.img` e
    `disco_firewall_aperto.img` in QEMU (inoltro dal PC alla porta 8080). Mancano la
    configurazione della persona in `/data` e la porta di controllo.
22. **Sistema in sola lettura** come immagine erofs + dm-verity, con aggiornamenti A/B.
23. **Prove più veloci su Windows**: accelerazione WHPX; con `-smp 4` QEMU su Windows si blocca
    con la finestra (le prove senza finestra usano già 4 processori).
24. **Integrazione a pannelli**; **ragionare sulla barra del titolo**; pulsante a griglia nel
    launcher (rimandato).
25. **Si lavora sul processore del computer** (decisione della persona, 14/11/2026): sul Mac
    tutto in arm64, sul PC Windows tutto in x86_64, niente emulazione. Da adattare
    compilazione, sandbox e prove; le prove ARM sul Mac (sezione 10) diventano quelle normali.

**Rimandati per il futuro** (decisione della persona, 13/11/2026)
26. **Il compositore sullo standard dei messaggi.** Per ora tiene il suo formato a dati fissi
    (`gkit/sistema/protocollo.h`): passano decine di messaggi al secondo (mouse, tasti, zone da
    ridisegnare) e trasformarli in testo costerebbe a ogni messaggio; come Wayland su Linux e il
    server delle finestre del Mac, separati da D-Bus/XPC. Da riprendere almeno per le richieste
    rare (cambiare il tema, aprire un pannello), se un comando dovrà farle.
27. **Banco di prova per le app**: usare un'app dal vivo mentre la si scrive, senza avviare il
    sistema (l'app in una finestra sul PC, la sandbox usabile dal browser, o l'anteprima che si
    ridisegna da sola). Oggi ci sono la sandbox (foto) e l'anteprima (tre scene rotte).

**Da riprendere** (lasciati qui il 13/11/2026)
28. ~~**Nomi dei permessi in inglese**~~ FATTO il 14/11/2026 (deciso dalla persona): `app.gestire` → `apps.manage`,
    `file.leggere` → `files.read`, `file.scrivere` → `files.write`, `notifiche` →
    `notifications`, `rete` → `network`, `processi` → `processes`, `impostazioni.sistema` →
    `settings.system`, `spegnimento` → `system.power`, `audio` resta. Ovunque: manifesti,
    manifest-guida.toml, pacchetto.c/descrizione.c/launcher.c, test/pacchetti, APPLICAZIONI.md
    (una sola tabella: oggi il launcher controlla sia `apps.manage` sia `app.gestire`). I testi
    per la persona restano in italiano; le chiavi del manifesto (`permessi`, `programma`...) non
    sono state decise. Un lavoro iniziato è rimasto a metà sul ramo dell'agente, non unito.
29. ~~**Il componente UIKit "messaggio"**~~ FATTO il 14/11/2026: si chiama `messagebox` (deciso dalla persona): `uikit/alert.h`,
    `interno/alert.c`, `struct alert`, `alert_*`. Oggi ha lo stesso nome di `struct messaggio`
    dello standard dei messaggi (`corekit/messaggi.h`), e un'app grafica non può includerli
    insieme. Serve prima della pagina della rete nelle Impostazioni.
30. **Comandi di base** (elencare, copiare, leggere file): toybox, busybox o nostri; ancora
    niente, per decisione della persona. Insieme: `PATH`, `TERM`, e `/home` per i programmi che
    non usano CoreKit. Proposta: spostare la shell in `utente/comandi/`.
31. **La shell**: Ctrl-Z ferma un programma ma la shell non sa tenerlo da parte (lo fa ripartire);
    Ctrl-C provato solo sulla seriale, non nel Terminale; la prova `test/shell.py` aspetta
    "cartella non trovata" ma la shell scrive "cartella inesistente" (già prima).
32. **HTTPS** per `webask`: ancora niente (BearSSL o mbedTLS quando servirà).
33. **Il servizio di rete**: porta di controllo (per la pagina della rete e un comando
    `network`), configurazione della persona in `/data`, API del firewall per le app (due
    permessi: aggiungere regole / controllo pieno, sempre attraverso il servizio). Non provati:
    rinnovo del DHCP, `mode=static`, firewall con la tabella già presente, IPv6.
34. **Store e Terminale** usano la parte "del sistema" del launcher (installare, shell come
    sistema): da decidere se restano app di sistema per sempre.
35. **Comandi distribuiti dalla persona**: come si installano e con quali poteri (SISTEMA.md,
    domande aperte).
36. **L'apertura della shell dal Terminale** con il launcher nuovo non è provata mentre gira.

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
  (titolo 40 px, finestra raggio 28). Sotto il titolo nessuna linea e nessun pixel in più, in
  nessuna finestra: il corpo comincia subito sotto (deciso dalla persona, come nel disegno).
- Le icone prendono il colore dalla raccolta "Colore icona" del disegno (`COLORE_ICONA(...)`,
  `tema.h`): Primario, Inverso, Attenuato, Secondario, Pericolo, Successo, Avviso, Informazione,
  col modo che usa il componente in Figma.
- La barra di scorrimento non va mai sopra il contenuto: sta nel padding a destra (14 px). Se il
  contenitore ha un margine di almeno 14 (una pagina), la barra ci sta dentro e il contenuto non
  cambia; se no (Files: tabella e griglia nel pannello) il contenuto, quando scorre, lascia libero
  a destra quanto manca per arrivare a 14 (`barra_scorrimento_riserva`, `uikit.h`).

**Fatto**
- Fase 0: font Plex in `uikit/materiali_dati.h` (oggi file in `risorse/fonts/`) (`plex_*`, TTF e licenza OFL in `strumenti/grafica/font/`,
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
- `elemento_file.h` (era l'icona
  Home della scrivania, oggi tolta) usa ancora i nomi e le icone vecchie.
- Prove: `test/scrivania.py` ha le posizioni della userbar e del vassoio (ramo `scrivania-figma`);
  nella sandbox passano applicazioni, finestre, mouse e tastiera. `scorrimento.py` (la barra di
  scorrimento non compare) falliva nella sandbox anche prima della userbar, uguale; `store.py`
  (dopo "Svuota dati") falliva per una corsa dello Store, ora risolta (sotto); `files.py` e `immagini.py` falliscono sulle foto di riferimento,
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
- **Esempio e Contatore non sono app del sistema**: i sorgenti stanno in
  `utente/prove/applicazioni/` e `disco.img` (quello di `run.sh`) non li ha. Li ha solo
  `disco_app.img` (e i dischi delle prove che ne partono): Esempio in `/system/apps`, il
  pacchetto `contatore.app` sulla Scrivania della persona (`/data/users/1000/home/Scrivania/`),
  per provare l'installazione. Le prove che li usano scrivono `DISCO = "disco_app.img"`.
  Anche la sandbox li ha (è un ambiente di prova).
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
  Store li traduce (`uikit/testo437.h`, ora nella libreria perché serve anche alla scrivania).
- Il launcher sa anche togliere un'app (`RIMUOVI_PACCHETTO`, con o senza i dati delle persone)
  e svuotarne i dati (`SVUOTA_DATI`, anche per quelle del sistema; la cartella resta, della
  persona, 700). Se l'app è accesa risponde "Chiudi <nome> prima di …" e non la ferma. I dati
  di una persona si cancellano con un processo che ha la sua identità, non come sistema.

**Prove**
- `test/pacchetti/prova.sh`: 78 controlli (con togliere, svuotare, gli id con "/" e "..", i
  collegamenti che puntano fuori, un'app accesa).
- `test/prove/store.py`: in QEMU, tutto il giro dello Store con contatore.app.
- "Apri <nome>" chiude prima la finestra dello Store e solo dopo chiede al launcher di aprire
  l'app (`store.c`, in `richiesta()`): il compositore dà il posto a cascata dal primo posto
  libero, e con la richiesta fatta a finestra ancora aperta l'app nuova (veloce nella sandbox)
  compariva prima che quella dello Store fosse tolta e finiva nel posto sbagliato; le prove
  seguenti sbagliavano i clic ("Chiudi Contatore prima…" su "Svuota dati"). `store.py` ora
  passa nella sandbox, ripetuto.

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
