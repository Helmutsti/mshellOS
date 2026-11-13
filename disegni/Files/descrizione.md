# Files: il disegno

Fonte: Figma "mshell – interfaccia" (X9g7EOJgKBlE4QjmjrTLFt), pagina "App – Files" (81:734).
Schermate (cornice 1024 x 664, padding spazio/32, finestra 960 x 600): vista a elenco 135:2, vista a icone con menu del file 135:404, cartella vuota 135:796, errore di lettura 139:1495, permesso negato 139:1602. Icona dell'app: sezione 165:4100.

- `figma/`: foto ritagliate sulla finestra (960 x 600), in chiaro e scuro: `elenco` (135:2), `icone-menu` (135:404), `cartella-vuota` (135:796), `errore-di-lettura` (139:1495), `permesso-negato` (139:1602), ognuna `-chiaro.png` e `-scuro.png`. Scuro = collezione Semantici sul modo "Scuro" sulla cornice, poi tolto: cornici e finestre non hanno nessun modo esplicito (sono in Chiaro).
- `codice/`: foto dell'app vera (sandbox).

## Struttura della finestra (principio: lo spazio lo da solo il contenitore)
- Titolo finestra (libreria), Stato=Attiva, Comandi=No. La linea sotto il titolo e nascosta (regola di tutte le finestre).
- Corpo "Contenuto" (orizzontale): padding spazio/8 su tutti i lati, gap spazio/8. Riempie il resto della finestra.
  - Barra laterale: 184 di larghezza, padding 0, gap spazio/4, nessun bordo. Sta sotto il titolo, a tutta altezza.
  - Area principale (verticale): padding 0, gap spazio/8. Contiene la barra degli strumenti e il pannello.
    - Barra degli strumenti: padding 0, gap spazio/8, nessun riempimento, altezza 32 (Indietro e avanti, Vista a icone o elenco, Percorso, Ricerca).
    - Pannello contenuto: superficie/elevata, raggio/pannello, padding spazio/8 su tutti i lati; contiene il componente "Tabella" (padding 0; intestazione 32 con padding orizzontale 8 come le righe; celle "Intestazione di colonna" padding 0; righe 40 con padding orizzontale 8 perche hanno sfondo proprio). Vista a icone: "Griglia" padding 0; l'elemento ha sfondo proprio (fondo icona 64 con l'oggetto a 8 dentro, fondo etichetta con padding 8).
    - "Menu del file" (135:404): posizionato in assoluto nell'area, x 528, y 64 (stesso punto di prima, nella finestra).
- Il frame "Margine riquadro" e l'elemento "Allineamento barra laterale" non esistono piu.

## Barra laterale
- Intestazione di menu (padding 8,8,4,8 del componente: allinea il testo alle voci) e Voce della barra laterale (padding orizzontale 8, verticale 0, sfondo proprio).

## Somme della spaziatura (punto 5)
- Barra laterale, prima intestazione: x = 8 (corpo) + 8 (intestazione) = 16; y = 8 (corpo) + 8 (intestazione) = 16.
- Pannello, intestazione della tabella: x = 8 (pannello) + 8 (intestazione come le righe) = 16; y = 8 (pannello) + 8 (intestazione 32, testo 16) = 16. Dal bordo della finestra 24 e 24 (corpo 8 + 16). Vista a icone: x = 8 + 8 (etichetta) = 16, y = 8 + 8 (oggetto nel fondo icona) = 16. Il pannello sta a 8 dal bordo destro e inferiore della finestra, a 8 dalla barra laterale e a 8 dalla barra degli strumenti.
- Misurato sulle foto: bordo del pannello -> primo testo 16 in x; in y il primo pixel delle lettere cade a 19-20 (scatola del testo a 16 piu lo spazio sopra le lettere).
- Barra degli strumenti: i pulsanti partono dal bordo sinistro del pannello (allineati), a 8 dal titolo.

## Indirizzi
- 135:2 (finestra 135:3, titolo 135:4, barra strumenti 135:77, corpo 135:24, barra laterale 135:25, area 135:76, pannello 154:2719, tabella 135:139); 135:404 (finestra 135:405, area 135:471, menu 135:727); 135:796 (finestra 135:797); 139:1495 (finestra 139:1496); 139:1602 (finestra 139:1603).

## Voce della barra laterale (componente 133:1721, tutte le app)
- Stato=Selezionata (133:1716): sfondo Semantici/controllo/selezione-vetro (Chiaro #000000 8%, Scuro #FFFFFF 12%).
- Stato=Sopra (133:1711): sfondo Semantici/controllo/sopra-vetro (Chiaro #000000 4%, Scuro #FFFFFF 6%).
- Padding orizzontale 8, verticale 0 (sfondo proprio, punto 3).
