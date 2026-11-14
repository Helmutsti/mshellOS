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
la usa solo GKit), in `build/<cpu>/libgkit.a`. Nel comando di lega l'ordine è: il programma,
`libuikit.a`, `libgkit.a`, `libcorekit.a`, la libreria C. Nella sandbox i programmi del sistema
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
- **il menu a tendina** (`tendina.h`): il campo con la scelta attuale e la freccia; apre sotto di sé l'elenco (il menu di `menu.h`, con la spunta); clic o frecce+Invio, Esc chiude;
- **il ciclo degli eventi** dell'applicazione: fuoco, tastiera, ridisegno.

Come CoreKit e GKit, è divisa in **tre livelli** (il commento in testa a `uikit/uikit.h` li
spiega). **Il riordino è finito**: le basi, il nucleo (`uikit.h`), i componenti complessi e quelli piccoli hanno solo dichiarazioni negli header, e il codice nei `.c` di `uikit/interno/`, compilato in `libuikit.a`.

| file | livello | perché |
|---|---|---|
| `uikit/uikit.h` | **pubblico** | il nucleo: i tipi dei widget e della finestra, le costanti e le dichiarazioni delle funzioni per le app (`colonna_prepara`, `pulsante_prepara`, `finestra_tk_prepara`, `uikit_esegui`...), più quelle per chi scrive un widget suo (`widget_misura`, `widget_disponi`, `widget_disegna`, `widget_contiene`). Include tutto quello che serve a un'app, anche i componenti |
| `uikit/stile.h` | **pubblico** | colori semantici, misure, font e ombre del design: li usano anche le app (per esempio `TEMA_SCURO_IN_USO` in Impostazioni, `OMBRA_TAB` in Files) |
| `uikit/tema.h` | **pubblico** | i due temi, quello in uso, `tema_salva` |
| `uikit/materiali.h`, `uikit/materiali_elenco.h` | **pubblico** | i font, le icone e gli oggetti del sistema per nome (`&plex_14`, `icona()`); l'elenco lo genera `strumenti/grafica/genera.py` |
| `uikit/tasti.h` | **pubblico** | i codici dei tasti e gli ascoltatori delle scorciatoie (Files, Terminale) |
| `uikit/testo437.h` | **pubblico** | da UTF-8 ai caratteri dei font del sistema (Store, scrivania): serve a chiunque scriva con i font del sistema testi che arrivano da fuori |
| `uikit/sistema/uikit_compositore.h` | **del sistema** | la barra congelata, le ombre delle finestre e il tema per il compositore: lo usa solo lui |
| `uikit.h` (scorrimento orizzontale) | **pubblico** | `barra_scorrimento_prepara_orizzontale`, `_posto_orizzontale`, `_rotella_xy` (rotella orizzontale, o verticale con Maiusc): come la verticale, ruotata (`uikit.h`) |
| `gkit_disegno.h`, `uikit.h` (spaziatura) | **pubblico** | GKit: `testo_spaziato`, `larghezza_testo_spaziato` (centesimi di pixel); UIKit: `spaziatura_del_font`, `larghezza_testo_stile`, `testo_centrato` la applica (titolo -24, etichetta +24, `stile.h`) |
| `uikit/interno/` | **interno** | il codice delle basi: `tema.c` (i valori dei due temi, il tema in uso, le preferenze), `stile.c` (le tabelle delle ombre), `materiali.c`, `testo437.c`, `uikit_compositore.c` |
| `uikit/interno/` | **interno** | il codice del nucleo: `widget.c` (l'albero, riga e colonna, spazi, etichetta), `pulsanti.c` (pulsante, casella, scelta, interruttore), `campo.c`, `stato_vuoto.c`, `avviso.c`, `finestra_tk.c` (la finestra come widget: misure, zone da ridisegnare, testata, opzioni, chiamate "ogni tanto", fuoco), `barra_strumenti.c` (con il pulsante della barra, la pillola — il gruppo di azioni — e il percorso), `suggerimento.c` (il suggerimento: `widget_suggerimento`, compare dopo 500 ms di puntatore fermo, sparisce se esce o si clicca), `scorrimento.c`, `ciclo.c` (il ciclo degli eventi: l'unico che parla con il compositore) |
| `uikit/menu.h`, `tabella.h`, `griglia.h`, `barra_laterale.h`, `schede.h`, `oggetti.h`, `elemento_file.h`, `pagina.h`, `paragrafo.h`, `pannello.h` | **pubblico** | i componenti complessi: tipi, costanti e dichiarazioni delle funzioni che usano le app (Files, Attività, Impostazioni, Store, Terminale); li include `uikit.h` |
| `uikit/sistema/userbar.h` | **del sistema** | la userbar e il vassoio delle app: li disegna solo la scrivania |
| `uikit/interno/` | **interno** | il codice dei componenti complessi, un `.c` per header: `menu.c`, `tendina.c`, `tabella.c`, `griglia.c`, `barra_laterale.c`, `schede.c`, `oggetti.c`, `elemento_file.c`, `pagina.c`, `paragrafo.c`, `pannello.c`, `userbar.c` |
| `uikit/grafico.h`, `etichetta_stato.h`, `messagebox.h`, `icona_app.h`, `riga_permesso.h`, `riga_app.h`, `informazioni.h` | **pubblico** | i componenti piccoli: barra di utilizzo e legenda con `testo_byte` (Attività), etichetta di stato, messaggio in linea, icona dell'app, riga di permesso, riga dell'app (Store), informazioni nella mini finestra (Files). Oggi li usano app del sistema, ma sono pezzi generici: ricevono solo testi, simboli e numeri, non sanno niente di pacchetti o permessi veri, e un'app di terzi può usarli |
| `uikit/interno/` | **interno** | il codice dei componenti piccoli: un `.c` per header (`grafico.c`, `etichetta_stato.c`, `messagebox.c`, `icona_app.c`, `riga_permesso.c`, `riga_app.c`, `informazioni.c`), più `informazioni_mostra.c`, a parte perché apre la mini finestra con il ciclo degli eventi (l'anteprima lo lascia fuori come `ciclo.c`) |
| `uikit/interno/componenti_piccoli.h` | **interno** | quello che un componente piccolo usa di un altro: l'etichetta di stato disegnata nella riga di permesso, l'icona dell'app nella riga dell'app (`etichetta_stato_disegna_in`, `icona_app_disegna_in`...). Un'app usa i widget |
| `uikit/interno/nucleo.h` | **interno** | le funzioni e i dati del nucleo che servono solo dentro UIKit: ai `.c` del nucleo tra loro, ai `.c` dei componenti (anche quello che si dicono tra loro: il testo che va a capo o si taglia, `testo_a_capo`, `paragrafo_a_capo`, `testo_tagliato`; il campo del nome nuovo; l'intestazione di un gruppo) e all'anteprima (`finestra_tk_bordo`, il contenuto fino ai bordi). Qui anche il puntatore e il fuoco come li muove il ciclo degli eventi (`uikit_sotto`, `uikit_fuoco`): un'app non li tocca, UIKit segue il puntatore e il fuoco da sola (un widget nascosto perde il puntatore, uno tolto dalla finestra anche il fuoco: `widget_esce`) e per dare la tastiera a un widget c'è `finestra_tk_dai_fuoco` in `uikit.h`. Un'app non lo include |

Gli header riordinati hanno solo dichiarazioni, tipi, costanti, macro e i commenti per chi li
usa. I valori del design che prima erano tabelle `static const` negli header (i due temi, le
ombre a strati, le misure dei pulsanti e dei campi) ora stanno una volta sola in `interno/`; le
macro (`SUPERFICIE_ELEVATA`, `OMBRA_TAB`, `OMBRA_N`...) si usano come prima. Anche i tipi dei widget
(`tipo_finestra_tk`, `tipo_contenitore`...) sono uno solo per programma: prima ogni file che
includeva `uikit.h` ne aveva una copia sua.

**La libreria compilata.** `build.sh` compila i `.c` di `uikit/interno/` in
`build/<cpu>/libuikit.a`, che nel comando di lega va subito dopo gli oggetti del programma e prima
di `libgkit.a`. Il tema si legge dalle preferenze prima di `main` (un constructor in
`interno/tema.c`, nello stesso file del tema in uso): lo prende ogni programma che disegna con i
colori del tema; quelli che non li usano (init, i servizi senza grafica) non lo prendono.
L'anteprima (`strumenti/anteprima/anteprima.sh`) compila con sé tutto `uikit/interno/` tranne
`ciclo.c` (e `informazioni_mostra.c`, che lo usa): al posto di `uikit_esegui` c'è la sua (in `anteprima.c`), che si tiene la finestra e
torna; poi l'anteprima la impagina e la disegna una volta su un PNG.

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
| **del sistema** | `corekit/sistema/` | `launcher.h` (il contratto del launcher, con lo standard dei messaggi, e le funzioni per chiedergli le cose: anche installare e togliere le app, aprire la shell come sistema), `pacchetto.h` (i pacchetti `.app`), `descrizione.h` (il manifesto di un'app), `privilegi.h` (quello che riesce solo al sistema: `avvia_come...`, `cambia_proprietario`), `servizi.h` (il lato del servizio nei messaggi: `chi_chiede`, `ha_permesso`) | solo i programmi del sistema e le app del sistema: come i "framework privati" del Mac, sono header ma possono cambiare senza avviso |
| **interno** | `corekit/interno/` | il codice (`corekit.c`, `file.c`, `comunicazione.c`, `messaggi.c`, `apertura.c`, `launcher.c` (le richieste al launcher), `descrizione.c`, `pacchetto.c`) e `interno.h`, che si dicono tra loro | solo CoreKit |

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
fa lo stesso (`strumenti/sandbox/Makefile`). GKit e UIKit seguono lo stesso schema
(`libgkit.a` e `libuikit.a`, sopra).

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
