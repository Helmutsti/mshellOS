# Apri / Salva (finestra di sistema in sovraimpressione): il disegno

Fonte: Figma "mshell – interfaccia" (X9g7EOJgKBlE4QjmjrTLFt), pagina "Sistema – Apri e Salva" (289:674).
Finestra mostrata da un servizio del sistema, non dall'app: compare SOPRA la finestra dell'app che la chiede. La persona sceglie un file (Apri) o cartella e nome (Salva), l'app riceve solo quel file.

Usa il layout "finestra minima" (mini finestra) della libreria: Titolo finestra Dimensione=Mini (pagina Componenti complessi 104:2, esempio "Finestra mini – Informazioni" 152:5268). Non e una copia di Files.

## Schermate (cornice 1024 x 664, padding spazio/32; copia di Files 135:2 usata come finestra dietro)
- Apri, nessun file scelto: 293:2361 (finestra di scelta 293:2410, 480 x 421). Apri disabilitato.
- Apri, un file scelto: 293:2590 (finestra 293:2639, 480 x 421). Riga appunti.txt selezionata, Apri attivo.
- Salva, con nome: 293:2836 (finestra 293:2885, 480 x 469). Campo Nome "Senza titolo.txt", Salva attivo.
- In ogni cornice: "Finestra Files (dietro)" (960 x 600, titolo Stato=Inattiva, sidebar con "Desktop" al posto di "Scrivania" solo in questa copia), sopra "Velo della finestra" (istanza del componente 138:455, 960 x 600 a x 32 y 32) e sopra la finestra di scelta (assoluta, x 272, y centrata: 122 in Apri, 98 in Salva).

## Foto (`figma/`)
- Ritagliate sulla finestra di scelta (480 x 421 o 480 x 469), chiaro e scuro: `apri`, `apri-scelto`, `salva`, ognuna `-chiaro.png` / `-scuro.png`.
- Con la sovraimpressione intera (cornice 1024 x 664 con margine di ombra): `apri-sopra`, `apri-scelto-sopra`, `salva-sopra`, ognuna `-chiaro.png` / `-scuro.png`.
- Scuro = collezione Semantici sul modo "Scuro" sulla cornice, poi tolto (come Files).

## Struttura della finestra di scelta
Frame verticale (libreria "mini finestra", effetti di finestra della libreria), larghezza 480, gap 0, senza fondo proprio:
- Titolo finestra: Stato=Attiva, Comandi=No, Dimensione=Mini (altezza 28, solo pulsante chiudi, nessun riduci a icona). Testo "Apri" o "Salva". Linea sotto il titolo nascosta.
- Contenuto (verticale): padding spazio/16 su tutti i lati (come la mini finestra della libreria), gap spazio/16 (uguale al padding, come nelle finestre normali dove sono 8 e 8; il gap e dato dal frame Contenuto stesso), non ritaglia. Dall'alto:
  1. Solo Salva: "Barra del nome" (orizzontale, padding 0, gap spazio/8, altezza 32): etichetta "Nome" + Campo (93:930, Dimensione=S, Stato=Attivo, icona nascosta, riempie la larghezza).
  2. Barra degli strumenti (altezza 32, padding 0, gap spazio/8): Indietro e avanti (Dimensione=S, Contenuto=Icone) + Percorso ("Home", testo come l'ultimo elemento del percorso di Files).
  3. Pannello contenuto: superficie/elevata, raggio/pannello, padding spazio/8; dentro la Tabella (componente, colonne Nome, Tipo, Dimensione) con 5 righe (Desktop, Documenti, Immagini cartelle; appunti.txt, lista-spesa.txt file).
  4. Barra dei pulsanti (orizzontale, padding 0, gap spazio/8, a destra): Pulsante Secondario S Normale "Annulla" e Pulsante S "Apri"/"Salva" (icona spenta): Primario Disabilitato (schermata 1) o Primario Normale (schermate 2 e 3).
- Riga selezionata (schermata 2): Riga di tabella Stato=Selezionata e cella Nome Stato=Selezionato.
- Altezza: 28 (titolo) + 16 (padding) + 32 (barra degli strumenti) + 16 (gap) + 265 (pannello: 8 + tabella 249 + 8) + 16 (gap) + 32 (pulsanti) + 16 (padding) = 421 (arrotondato). Salva aggiunge barra del nome 32 + 16 = 469.

## Decisioni della persona (16 novembre 2026)
- Finestra minima in sovraimpressione sopra l'app che la chiede, non dentro Files.
- Niente barra laterale; colonne Nome / Tipo / Dimensione; si apre sulla Home; la cartella della scrivania si chiama "Desktop".
- 32 dal bordo della finestra al primo testo dell'elenco (16 della finestra minima + 16 del pannello).
- Spazio uguale in verticale: gap tra le aree = padding = 16, dato dal frame "Contenuto". Pannello -> pulsanti = pulsanti -> fondo della finestra.

## Distanze misurate sulla foto (salva-chiaro.png, 480 x 469)
- Titolo (28) -> campo Nome (44) 16; campo Nome (44-76) -> barra degli strumenti (92) 16; barra degli strumenti (92-124) -> pannello (140) 16; pannello (140-405) -> pulsanti (421-453) 16; pulsanti -> fondo della finestra (469) 16. Apri: stesse distanze senza la barra del nome.

## Somme della spaziatura (punto 5)
- Primo testo della barra degli strumenti ("Home"), orizzontale: 16 (contenuto) + percorso senza padding; il pulsante Indietro parte a x = 16 dal bordo della finestra.
- Pannello -> primo testo della tabella: x = 8 (pannello) + 8 (intestazione come le righe) = 16; y = 8 + 8 (intestazione 32, testo 16) = 16. Dal bordo della finestra: x 16 + 16 = 32.
- Bordo della finestra -> pannello, -> pulsanti, -> barra degli strumenti: 16 su x e y (solo il livello "Contenuto" ha padding).
- Pulsante principale -> bordo destro e fondo della finestra: 16 e 16.
- Misurato sulle foto (480 x 421): pannello a x 16 e fino a 464 (16 dal bordo destro), testo "Nome" della tabella a x 32 (16 dal pannello), sfondo della riga selezionata a x 24 (8 dal pannello), pulsanti finiscono a 16 dal bordo destro e dal fondo; titolo alto 28 (il codice Figma del titolo mini dice 28: 28 + 441 = 469).

## Componenti usati
Titolo finestra (Mini), Indietro e avanti, Tabella 105:446 (Riga di tabella), Pulsante 92:1001, Campo 93:970, Velo della finestra 138:455 (solo nella finestra dietro). Variabili: Semantici (colori), Misure (spazio/8, spazio/16, raggio/pannello).

## Salva, il file esiste gia: richiesta "Sostituire il file?"
Decisioni della persona (17 novembre 2026): in "Salva", se il file con quel nome esiste gia, si chiede con una PICCOLA FINESTRA SOPRA la finestra "Salva", con due pulsanti: "Annulla" e "Sostituisci". La finestra senza titolo ("blank") e la stessa forma degli avvisi: si usa il componente Avviso, Scelte=Due, con le sue misure, senza cambiarlo. Nessun'altra variante. Regola: varianti e stili dei componenti non si cambiano mai rispetto alla libreria senza chiedere (il pulsante Sostituisci resta Pericolo, come nel componente).

- Schermata: 298:992 "Salva – sostituire il file?" (pagina 289:674, x 3300, 1024 x 664, padding spazio/32). Copia di "Salva – con nome" 293:2836, con il campo Nome "appunti.txt" (il file che esiste gia).
- Ordine dei livelli: Finestra Files (dietro) 298:993, Velo della finestra 298:1016 (960 x 600), Finestra Salva 298:1017 (480 x 469, x 272 y 98), Velo della finestra Salva 298:1300 (istanza del componente Velo 138:455, assoluta, 480 x 469 sulla finestra Salva), Avviso 302:1267.
- Avviso 302:1267: istanza del componente Avviso 185:3085, Scelte=Due (132:1063), 400 x 160, assoluta a x 312 y 253 nella cornice (centrata sulla finestra Salva, 272+40 e 98+155). Misure del componente, non toccate: padding spazio/24, gap spazio/24, fondo pieno superficie/elevata, effetti di finestra della libreria.
  - Testi (gap spazio/8): "Titolo" "Sostituire il file?" e "Messaggio" "Il file appunti.txt esiste già. Vuoi sostituirlo?" (testo provvisorio, la persona dara quello definitivo). Il Messaggio e colorato testo/primario (Semantici), come nel componente.
  - Il componente Avviso non ha una proprieta per nascondere il titolo: resta, con il testo "Sostituire il file?". Decisione della persona: il titolo del componente si tiene.
  - Azioni (a sinistra del bordo destro, gap spazio/8): Pulsante Secondario M Stato=Normale "Annulla" + Pulsante M Pericolo Normale "Sostituisci" (come nel componente della libreria: cambiato solo il testo).
  - Altezza: 24 + 48 (testi: 20 + 8 + 20) + 24 + 40 + 24 = 160.
- Componenti: Avviso 185:3085, Pulsante 92:1001, Velo della finestra 138:455, Campo 93:970 (Salva). Variabili: Semantici (colori), Misure.
- Nella finestra Salva sotto "Annulla" e in Stato=Sopra (come nell'originale 293:3060): lasciato com'e.

### Foto (`figma/`)
- `sostituisci-chiaro.png` / `sostituisci-scuro.png`: ritagliate sull'Avviso (400 x 160).
- `sostituisci-sopra-chiaro.png` / `sostituisci-sopra-scuro.png`: cornice intera con margine di ombra. Scuro = collezione Semantici sul modo "Scuro" sulla cornice, poi tolto.

### Distanze misurate sulla foto (sostituisci-chiaro.png, 400 x 160)
- Bordo sinistro -> testo (titolo e messaggio): 24. Bordo destro -> pulsante Sostituisci: 24. Alto -> titolo: 24. Pulsanti -> fondo: 24. Titolo -> messaggio 8; messaggio -> pulsanti 24.
- Somme della spaziatura: x = 24, y = 24 (solo il livello dell'Avviso ha padding; Testi e Azioni hanno padding 0, il padding dei pulsanti e il loro sfondo, eccezione del punto 3). 24 e il margine proprio del componente Avviso (diverso dal 16 della finestra Salva).
