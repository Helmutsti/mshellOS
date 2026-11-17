# Blocco note e area di testo: il disegno

Fonte: Figma "mshell – interfaccia" (X9g7EOJgKBlE4QjmjrTLFt). Pagina "App – Blocco note" (314:2269). Componente "Area di testo" nella pagina Componenti base (80:740), sezione "Area di testo" 314:2078.

## Componente Area di testo (set 314:2077)
- Foto: `figma/area-di-testo-chiaro.png` (set, 1992 x 504) e `figma/area-di-testo-scuro.png` (cornice scura 314:2080, istanze con Semantici=Scuro).
- Varianti (proprieta): Contenuto = Vuota | Testo | Selezione | Lungo; Fuoco = Sì | No. 8 varianti, ognuna 480 x 240 (le istanze si allargano: riempiono la larghezza e l'altezza).
  - Vuota: segnaposto "Scrivi qui…" (testo/secondario); con fuoco il cursore sta all'inizio.
  - Testo: 5 righe; con fuoco il cursore e a fine ultima riga.
  - Selezione: selezione su due righe ("latte" e "- uo"); con fuoco cursore a fine selezione.
  - Lungo: 14 righe, piu lunghe dell'area (ritaglio), con la Barra di scorrimento della libreria (Orientamento=Verticale, Stato=Scorrimento, 14 px, assoluta, a destra, a filo del bordo come nel Terminale).
  - Fuoco Sì: cursore a barra e selezione blu tenue. Fuoco No: nessun cursore, selezione grigia (testo non cambia colore).
- Struttura (come il pannello contenuto di Files e del Terminale): il componente E il pannello. Frame verticale, fondo superficie/elevata, raggio/pannello, padding spazio/16 su tutti i lati, gap spazio/0, ritaglio acceso. Dentro: "Testo dell'area" (verticale, padding 0, gap 0, riempie la larghezza) con una "Riga N" per riga (orizzontale, padding 0, altezza 20); la selezione e un frame "Selezione" con fondo, dentro il testo; il "Cursore" e un rettangolo 2 x 16 assoluto nella riga (y 2).
- Testo: IBM Plex Mono Regular 13/20 (larghezza carattere 7,8). Colore testo/primario; selezione con fuoco informazione/testo-tenue (come il Terminale); segnaposto testo/secondario.
- Variabili nuove (collezione Semantici, alias, stessi valori in Chiaro e Scuro): testo/cursore (= testo/primario), testo/selezione (= informazione/tenue, come il Terminale), testo/selezione-inattiva (= controllo/sfondo-premuto, grigio). Ambito FRAME_FILL e SHAPE_FILL.
- Larghezza della tabulazione e Maiusc+Tab: non decisi, non disegnati.

## Finestra Blocco note (760 x 588)
- Titolo finestra (libreria): Stato=Attiva (Inattiva sotto l'Apri), Comandi=No, Dimensione=Normale. Nessuna linea sotto il titolo.
- Corpo "Contenuto": padding spazio/8, gap spazio/8, verticale. Nessuna barra laterale.
  1. Barra degli strumenti (orizzontale, padding 0, gap spazio/8, altezza 32): quattro Pulsante icona Tipo=Secondario, Dimensione=S, Stato=Normale: Nuovo (Simbolo/plus), Apri (Simbolo/cartella), Salva (Simbolo/save), Salva con nome (Simbolo/file-pen, 332:3899). Nient'altro. Il suggerimento (tooltip) di Salva con nome e "Salva con nome" (si vede solo nel codice, come per gli altri pulsanti).
  2. Area di testo (istanza, riempie la larghezza e l'altezza: 744 x 492).
- Tema: cornici con Semantici su Chiaro o Scuro in modo esplicito (come il Terminale). Cornice 824 x 652 (padding 32 attorno alla finestra).

## Schermate (cornice Chiaro in alto y 0, Scuro sotto y 800)
| Schermata | Chiaro | Scuro | Titolo | Area di testo |
|---|---|---|---|---|
| a. Documento nuovo vuoto | 314:2270 (finestra 314:2271) | 314:3804 (314:3805) | Senza titolo – Blocco note | Vuota, Fuoco Sì |
| b. Testo, cursore e selezione | 314:2305 (314:2306) | 314:3813 (314:3814) | lista-spesa.txt – Blocco note | Selezione, Fuoco Sì |
| c. Modifiche non salvate | 314:2340 (314:2341) | 314:3822 (314:3823) | lista-spesa.txt* – Blocco note | Testo, Fuoco Sì |
| d. Apri sopra il documento | 314:3711 | 314:3831 | lista-spesa.txt (finestra dietro Inattiva) | Selezione, Fuoco No |

- c: le modifiche non salvate si vedono con un asterisco "*" subito dopo il nome nel titolo (decisione della persona). Il titolo e la proprieta Titolo dell'istanza Titolo finestra.
- e. Salvare le modifiche? (premendo Nuovo o Apri con modifiche non salvate): cornice Chiaro 332:3971, Scuro 332:4014 (x 3600, y 0 / 800). Copia della schermata c con, in ordine: Velo della finestra (istanza 138:455, assoluto, 760 x 588 a 32,32) sopra la finestra Blocco note (Attiva) e Avviso (istanza del set Avviso 185:3085, Scelte=Tre, 480 x 160, assoluto, centrato nella finestra a x 172 y 246). Testi: titolo "Salvare le modifiche?", messaggio "Le modifiche a lista-spesa.txt andranno perse se non le salvi." (proposta, da confermare). Pulsanti da sinistra, Pulsante M: Non salvare (Secondario), Annulla (Secondario), Salva (Primario). Nelle istanze i nodi si chiamano ancora Annulla / Alternativa / Conferma (non si possono riordinare dentro un'istanza): contano le etichette.
- Tutte le schermate (a-e) hanno il quarto pulsante nella barra degli strumenti, anche la finestra dietro in d.
- d: copia della schermata Apri 293:2361 (pagina 289:674) con la finestra Blocco note al posto di Files; Velo della finestra 760 x 588 a x 32 y 32; finestra Apri 480 x 421 centrata (x 172, y 115,5). Per il resto invariata.

## Foto (`figma/`)
- a, b, c: `nuovo`, `testo-selezione`, `non-salvato`, ognuna `-chiaro.png` / `-scuro.png`, ritagliate sulla finestra (con il margine d'ombra).
- e: `salvare-modifiche-chiaro.png` / `salvare-modifiche-scuro.png`: cornice intera.
- d: `apri-sopra-chiaro.png` / `apri-sopra-scuro.png`: cornice intera.
- Componente: `area-di-testo-chiaro.png`, `area-di-testo-scuro.png`.

## Spaziatura (regola punti 1-10)
- Livelli con padding: solo "Contenuto" (8) e il pannello Area di testo (16). Barra degli strumenti, righe, testo: padding 0 (i pulsanti hanno il loro sfondo).
- Somma bordo finestra -> primo testo: x = 8 + 16 = 24; y (dal bordo del corpo, sotto il titolo e la barra) = 8 + 32 + 8 + 16 dal bordo del corpo; dal bordo del pannello al primo testo 16 in x e in y. Dove ci sono due livelli (corpo + pannello) lo sono per costruzione: 8 + 16 = 24 in x; in y il testo sta 16 sotto il bordo del pannello, che sta a 8 + 32 + 8 sotto il titolo.
- Misurato sulla foto (nuovo-chiaro.png, margine d'ombra 24): bordo finestra -> pannello 8; bordo pannello -> primo testo 16 (x) e 16 (y); barra degli strumenti: primo pulsante a 8 dal bordo della finestra e 8 sotto il titolo; barra -> pannello 8; pannello -> fondo e -> bordo destro della finestra 8.
- Nessuna linea e nessun bordo tra le aree.

## Componenti usati
Titolo finestra, Pulsante icona 92:1170, Avviso 185:3085 (Scelte=Tre), Simbolo/file-pen 332:3899 (nuovo, pagina Componenti base, sezione Icone – Simbolo, dopo Simbolo/pacchetto; Lucide "file-pen", tre tratti come gli altri simboli, solo nel set Chiaro), Barra di scorrimento 113:3035, Velo della finestra 138:455, finestra Apri (copia di 293:2410). Variabili: Semantici (superficie/elevata, superficie/scena, testo/primario, testo/secondario, informazione/testo-tenue, testo/cursore, testo/selezione, testo/selezione-inattiva), Misure (spazio/0, spazio/8, spazio/16, raggio/pannello).

## Icona dell'app
Il Blocco note non ha ancora una pagina di disegno: l'icona sta in "Icone delle app senza pagina" (sezione 166:799, pagina Scrivania), accanto a Doom, Contatore ed Esempio. Cornice "Blocco note" **317:8097** (stessa struttura di Contatore: titolo, riquadro Tema Chiaro, riquadro Tema Scuro).

Simbolo: Lucide "file-text" (stessi cinque tratti dell'SVG del codice), bianco, tratto con estremi e angoli arrotondati.

| Misura | Componente (chiaro / scuro) | Raggio | Simbolo | Tratto |
|---|---|---|---|---|
| 96 x 96 (disegno principale, SVG) | 317:8102 / 317:8133 | 22 | 52 x 52 a (22, 22) | 4,5 |
| 32 x 32 (PNG facoltativa) | 317:8112 / 317:8143 | 7,5 | 20 x 20 a (6, 6) | 2 |
| 18 x 18 (PNG facoltativa) | 317:8122 / 317:8153 | 4,5 | 12 x 12 a (3, 3) | 1,5 |

Nomi: "Icona Blocco note – Chiaro|Scuro – 96|32|18". Strati: Fondo (rettangolo), Luce (solo 96 e 32), Simbolo.
- Fondo: variabile **sistema/arancio/saturo** (collezione Semantici; chiaro = base/arancio/saturo-chiaro #FF9500, scuro = base/arancio/saturo-scuro #FF9F0A). Il riquadro Tema Scuro ha il modo Scuro della collezione Semantici, quindi i due temi cambiano da soli.
- Luce: sfumatura lineare dall'alto, bianco 24% -> 0%, stesso raggio del fondo (come le altre icone).
- Simbolo: bianco fisso (come le altre icone).

Foto: `figma/icona-chiaro.png`, `figma/icona-scuro.png` (96 ingrandita 4x), `figma/icone-app.png` (tutte le icone delle app, chiaro sopra e scuro sotto).
