# mshell: le librerie della grafica (GKit e UIKit)

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

Due livelli. **La base non dipende da niente; gli strumenti di disegno dipendono solo dalla base;
UIKit dipende da tutte e due.** Un programma prende solo quello che gli serve: un gioco fatto
pixel per pixel usa solo la base.

### La base (`gkit.h`)

- **Il protocollo delle finestre** (oggi `protocollo.h`): cosa si dicono un'applicazione e il
  compositore, condiviso tra i due lati.
- **Il lato applicazione** (oggi `finestra.h`): apre una finestra, dà la tela (un pezzo di memoria
  dove ogni pixel è un numero), la fa mostrare, consegna mouse e tastiera. Qui nascono i **tipi di
  finestra** come li vede il compositore: normale, e **blank** (una finestra vera, senza barra del
  titolo; si sposta e si chiude solo se chi programma le aggancia una barra, anche invisibile).
- **Il lato compositore** dello schermo (oggi `schermo.c`, il DRM): lo usa solo il compositore.

### Gli strumenti di disegno (`gkit_disegno.h`)

Oggi `disegno.h`. **Come** si disegna, senza nessun gusto: rettangoli anche arrotondati, linee,
sfumature, ritagli, testo **con il font che riceve**, immagini e icone **che riceve**. Non sa cosa
sia una cartella né quale sia il font del sistema. Sa già caricare un'immagine PNG da un file su
disco (`immagine_carica`, in `gkit/immagine_file.c`, con stb_image): la usa la scrivania per le
icone delle app. Più avanti imparerà anche a caricare un font (punto 16 delle cose da fare).

## UIKit

Costruita sopra GKit:

- **lo stile e i temi** (oggi `stile.h` e `tema.h`): colori semantici Chiaro/Scuro, misure,
  raggi, con i nomi delle variabili di Figma;
- **i materiali del sistema**: il font del sistema, le icone Lucide, le icone di file e cartelle
  (oggi tutti insieme in `grafica_dati.h`, generato da `strumenti/grafica/genera.py`). È **cosa**
  si disegna: il design di mshell. La scrivania e tutte le app li usano da qui, così una cartella
  è uguale ovunque;
- **la finestra dell'applicazione** come la decide il design: barra del titolo (normale e mini),
  barra degli strumenti, area dei contenuti con la barra laterale, il pannello incassato
  facoltativo; la finestra blank con o senza barra del titolo agganciata (con la barra del
  titolo di UIKit spostare e chiudere arrivano da soli);
- **i componenti** (oggi `toolkit.h`, `elemento_file.h`, `userbar.h`): pulsanti, campi, tabelle,
  menu, stato vuoto, avviso (due e tre scelte), userbar e vassoio;
- **il ciclo degli eventi** dell'applicazione: fuoco, tastiera, ridisegno.

## Fuori da tutte e due

La **libreria dei programmi** (oggi `libreria.h`, `file.c`, `comunicazione.c`, `libreria.c`,
`chiamate.h`) e le parti delle app (`descrizione.h`, `pacchetto.h`, `launcher.h`) non sono
grafica: restano dove sono, con il loro nome.

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
- Le funzioni tengono i loro nomi italiani; cambiano solo quelle che contengono "toolkit" (per
  esempio `toolkit_esegui` diventa `uikit_esegui`).
- Un agente sposta i file, aggiorna build.sh, le app, il compositore e le prove, compila PC e
  ARM e fa girare il collaudo.
