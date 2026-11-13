# mshell: le librerie (CoreKit, GKit e UIKit)

**Approvato** il 12 novembre 2026. Definisce il confine tra la libreria grafica
che parla con il compositore e quella dei componenti (punto 13 delle cose da fare in STATO.md),
prima di rinominarle.

## In breve

| | **GKit** (la grafica) | **UIKit** (l'interfaccia) |
|---|---|---|
| Cosa fa | disegna e mostra pixel; apre finestre, riceve mouse e tastiera | dà i componenti pronti con l'aspetto del sistema |
| Sa di stile e temi? | **no** | sì |
| Chi la usa | UIKit, il compositore, i programmi che disegnano da soli (Doom) | le applicazioni, la scrivania |
| Come sul Mac / su Linux | Core Graphics + la parte bassa di Quartz / Cairo + libwayland | AppKit / GTK |

Regola del confine: **GKit sa come disegnare, UIKit sa cosa disegnare.** GKit non conosce colori del tema, misure del design, icone del sistema, il font del sistema, componenti. Se una
cosa ha un nome che viene da Figma (Pulsante, Barra laterale, superficie/elevata), è UIKit.

## GKit

Due parti. **La base non dipende da niente (sotto c'è solo CoreKit); gli strumenti di disegno
non dipendono nemmeno dalla base; UIKit dipende da tutte e due.** Un programma prende solo quello
che gli serve: un gioco fatto pixel per pixel usa la finestra e, se vuole, gli strumenti.

Come CoreKit, è divisa in **tre livelli** (il commento in testa a `gkit/gkit.h` li spiega):

| livello | cartella | cosa c'è | chi lo usa |
|---|---|---|---|
| **pubblico** | `gkit/` | `gkit.h` (la finestra, il lato dell'applicazione: aprirla, la tela, mostrarla, le notizie, le zone cambiate), `gkit_disegno.h` (gli strumenti di disegno, i materiali letti da un file) | ogni programma che disegna: UIKit, le app, Doom |
| **del sistema** | `gkit/sistema/` | `protocollo.h` (i messaggi tra la finestra e il compositore, i ruoli speciali e i pannelli della scrivania), `schermo.h` (lo schermo, il DRM: `apri_schermo`, `mappa_schermo`, `schermo_cambiato`), `pixel.h` (i conti per pixel: comporre un colore sopra un altro, la distanza dal bordo di una forma arrotondata, la sfumatura delle ombre) | il compositore, la scrivania, init; `pixel.h` anche il codice di GKit |
| **interno** | `gkit/interno/` | il codice (`finestra.c`, `zone.c`, `pennello.c`, `forme.c`, `testo.c`, `immagini.c`, `immagine_file.c`, `materiali_file.c`, `schermo.c`) e `interno.h`, i passi di ogni disegno che i `.c` si dicono tra loro | solo GKit |

Gli header pubblici hanno solo dichiarazioni, tipi, costanti e i commenti per chi li usa. Le
uniche funzioni scritte dentro gli header sono i conti di ogni pixel (`sistema/pixel.h`,
`interno/interno.h`), `static inline`: si chiamano milioni di volte per immagine, e una chiamata
vera costerebbe più del conto.

**La libreria compilata.** `build.sh` compila i `.c` di `gkit/interno/`, con stb_image (le PNG:
la usa solo GKit), in `build/<cpu>/libgkit.a`. Nel comando di lega l'ordine è: UIKit, il
programma, `libgkit.a`, `libcorekit.a`, la libreria C. Nella sandbox i programmi del sistema
hanno in più lo schermo della sandbox (`strumenti/sandbox/schermo_sandbox.c`) come oggetto
sciolto: le tre funzioni dello schermo si trovano lì, e dall'archivio non si prende `schermo.o`.
L'anteprima sul Mac (`strumenti/anteprima/anteprima.sh`) compila con sé solo gli strumenti di
disegno e le zone (`zone.c`): la finestra e lo schermo lì non ci sono.

### La base (`gkit.h`, e `sistema/` per il lato del compositore)

- **Il protocollo delle finestre** (`sistema/protocollo.h`): cosa si dicono un'applicazione e il
  compositore. Un'app non lo vede: per lei c'è la finestra.
- **Il lato applicazione** (`gkit.h`, codice in `interno/finestra.c`): apre una finestra, dà la
  tela (un pezzo di memoria dove ogni pixel è un numero), la fa mostrare, consegna mouse e
  tastiera. Qui nascono i **tipi di finestra** come li vede il compositore: normale, e **blank**
  (una finestra vera, senza barra del titolo; si sposta e si chiude solo se chi programma le
  aggancia una barra, anche invisibile).
- **Il lato compositore** dello schermo (`sistema/schermo.h`, codice in `interno/schermo.c`, il
  DRM): lo usano solo il compositore e init.

### Gli strumenti di disegno (`gkit_disegno.h`)

**Come** si disegna, senza nessun gusto: rettangoli anche arrotondati, linee,
sfumature, ritagli, testo **con il font che riceve**, immagini e icone **che riceve**. Non sa cosa
sia una cartella né quale sia il font del sistema. Sa caricare un'immagine PNG da un file su
disco (`immagine_carica`, in `gkit/interno/immagine_file.c`, con stb_image): la usa la scrivania per le
icone delle app. E sa leggere da un file un font, un'icona o un'immagine gia' disegnati, nei
formati di `strumenti/grafica/formato.py` (`font_da_file`, `icona_da_file`, `immagine_da_file`,
in `gkit/interno/materiali_file.c`): il file si mette in memoria in sola lettura (mmap), e se manca o
è rovinato lo si scrive nel registro e si dà un sostituto che si disegna senza pericolo. Dove
stanno le risorse del sistema lo dice `percorso_risorsa` (`/system/resources`, o la variabile
`MSHELL_RISORSE` per gli attrezzi fuori da mshell).

## UIKit

Costruita sopra GKit:

- **lo stile e i temi** (oggi `stile.h` e `tema.h`): colori semantici Chiaro/Scuro, misure,
  raggi, con i nomi delle variabili di Figma;
- **i materiali del sistema**: il font del sistema, le icone Lucide, le icone di file e cartelle,
  lo sfondo (`materiali.h`). I dati sono file in `/system/resources`, preparati da
  `strumenti/grafica/genera.py`; l'elenco (nome in C e file) è `materiali_elenco.h`. Ognuno si
  carica la prima volta che serve e si chiama con il suo nome come una variabile (`&plex_14`,
  `oggetto_cartella_grande`), un'icona per numero con `icona()`. È **cosa** si disegna: il
  design di mshell. La scrivania e tutte le app li usano da qui, così una cartella è uguale
  ovunque;
- **la finestra dell'applicazione** come la decide il design: barra del titolo (normale e mini),
  barra degli strumenti, area dei contenuti con la barra laterale, il pannello incassato
  facoltativo; la finestra blank con o senza barra del titolo agganciata (con la barra del
  titolo di UIKit spostare e chiudere arrivano da soli);
- **i componenti** (oggi `toolkit.h`, `elemento_file.h`, `userbar.h`): pulsanti, campi, tabelle,
  menu, stato vuoto, avviso (due e tre scelte), userbar e vassoio;
- **il ciclo degli eventi** dell'applicazione: fuoco, tastiera, ridisegno.

## CoreKit, la libreria comune (sotto tutte e due)

**CoreKit** (`utente/libreria/corekit/`) è la libreria che usano tutti i programmi, anche quelli
senza grafica: file e cartelle, processi, porte (i socket tra programmi), fogli di memoria
condivisa, tempo, e lo standard dei messaggi tra servizi e clienti. Non è grafica: GKit e UIKit
stanno sopra di lei. Come Foundation/CoreFoundation sul Mac, o la GLib su Linux. Le funzioni
hanno i loro nomi italiani di sempre (`apri`, `manda`, `ricevi`, `foglio`...): è cambiato solo
dove stanno i file.

È divisa in **tre livelli**, pensando a un futuro SDK (gli header e la libreria compilata da dare
a chi scrive un'app):

| livello | cartella | cosa c'è | chi lo usa |
|---|---|---|---|
| **pubblico** | `corekit/` | `corekit.h` (l'header principale: file, processi, porte, fogli, tempo), `chiamate.h` (i tipi e le costanti che usa, lo include `corekit.h`), `messaggi.h` (lo standard `chiave=valore`, SISTEMA.md sezione 3: la busta e `chiedi`, il lato di chi chiede), `apertura.h` (chiedere al launcher di aprire un'altra app o un file) | ogni programma, app e comando |
| **del sistema** | `corekit/sistema/` | `launcher.h` (il protocollo del launcher: anche installare e togliere le app, aprire la shell come sistema), `pacchetto.h` (i pacchetti `.app`), `descrizione.h` (il manifesto di un'app), `privilegi.h` (quello che riesce solo al sistema: `avvia_come...`, `cambia_proprietario`), `servizi.h` (il lato del servizio nei messaggi: `chi_chiede`, `ha_permesso`) | solo i programmi del sistema e le app del sistema: come i "framework privati" del Mac, sono header ma possono cambiare senza avviso |
| **interno** | `corekit/interno/` | il codice (`corekit.c`, `file.c`, `comunicazione.c`, `messaggi.c`, `apertura.c`, `descrizione.c`, `pacchetto.c`) e `interno.h`, che si dicono tra loro | solo CoreKit |

Un programma scrive `#include "corekit/corekit.h"` (e `"corekit/messaggi.h"`,
`"corekit/apertura.h"`... se gli servono). Oggi Files, Attività e la scrivania usano solo
`apertura.h`; lo Store (installare, aggiornare, togliere le app) e il Terminale (aprire la shell
come sistema) usano `sistema/launcher.h`. `testo437.h` (da UTF-8 ai caratteri dei font del
sistema) riguarda i font del sistema: sta in UIKit, `uikit/testo437.h`. Gli header pubblici hanno solo dichiarazioni,
tipi, costanti e i commenti per chi li usa; l'unico codice dentro sono due funzioni di una riga
(`lunghezza`, `scrivi_testo`).

**La libreria compilata.** `build.sh` compila i `.c` di `corekit/interno/`, con miniz e tomlc99
(le librerie di altri che usa solo CoreKit), in una libreria statica, `build/<cpu>/libcorekit.a`,
e ogni programma si lega a lei: dall'archivio entra solo quello che il programma usa. La sandbox
fa lo stesso (`strumenti/sandbox/Makefile`). GKit segue lo stesso schema (`libgkit.a`, sopra);
UIKit per ora resta di oggetti sciolti e si riordinerà allo stesso modo.

## Un punto che oggi attraversa il confine

Il **compositore** disegna la barra del titolo di un'app che non risponde (lei non può più
ridisegnarsi), e per farlo usa lo stile. Proposta: il compositore usa **solo GKit**, e UIKit
gli fornisce una sola funzione, "disegna la barra del titolo congelata", che conosce anche le
barre mini. Così la dipendenza è una, scritta e voluta, e risolve anche il problema delle
finestre mini segnalato dalla libreria.

## Come diventa il codice (dopo l'approvazione)

- Due cartelle: `utente/libreria/gkit/` (`gkit.h` la base, `gkit_disegno.h` gli strumenti) e
  `utente/libreria/uikit/` (`uikit.h`, che include tutto quello che serve a un'app).
- `grafica_dati.h` si divide: i dati di font e icone vanno a UIKit, il codice che li disegna resta in GKit.
  (Poi i dati sono usciti dai programmi: sono file in `/system/resources`, punto 16.)
- Le funzioni tengono i loro nomi italiani; cambiano solo quelle che contengono "toolkit" (per
  esempio `toolkit_esegui` diventa `uikit_esegui`).
- Un agente sposta i file, aggiorna build.sh, le app, il compositore e le prove, compila PC e
  ARM e fa girare il collaudo.
