# Store: il disegno

Fonte: Figma "mshell – interfaccia" (X9g7EOJgKBlE4QjmjrTLFt), pagina "App – Store" (167:2).
Schermate: app installate 168:29, nessuna app tua 168:201, dettaglio dell'app 168:306, richiesta di installazione 169:274, app installata 169:419, richiesta di aggiornamento 169:458, installazione bloccata (processore 169:551, sistema vecchio 169:597, nome gia usato 169:643), pacchetto illeggibile 169:689, app installate tema scuro 169:730, dettaglio app del sistema 178:590, togli app conferma 178:701, svuota dati conferma 178:781, app tolta con i dati 178:829, app tolta dati tenuti 178:884, dati svuotati 178:939. Componenti locali 167:3.

- `figma/`: foto ritagliate sulla finestra, in chiaro e scuro (`-chiaro.png`, `-scuro.png`): `app-installate` (168:29), `nessuna-app-tua` (168:201), `dettaglio-app` (168:306), `richiesta-installazione` (169:274), `app-installata` (169:419), `richiesta-aggiornamento` (169:458), `bloccata-processore` (169:551), `bloccata-sistema-vecchio` (169:597), `bloccata-nome-gia-usato` (169:643), `pacchetto-illeggibile` (169:689), `dettaglio-app-sistema` (178:590), `togli-app-conferma` (178:701), `svuota-dati-conferma` (178:781), `app-tolta-con-dati` (178:829), `app-tolta-dati-tenuti` (178:884), `dati-svuotati` (178:939). In piu `app-installate-tema-scuro.png` (169:730, la schermata disegnata apposta in scuro: e l'unica cornice con il modo esplicito "Scuro"). Scuro delle altre = Semantici sul modo "Scuro" sulla cornice, poi tolto: sono in Chiaro senza modo esplicito.
- Le istanze delle icone delle app ("Icona ... – Chiaro – 32/96", "Icona dell'app") non hanno modo esplicito: seguono il modo della schermata. Le icone sono quasi tutte a colori fissi (solo uno strato legato a variabile; Doom tutto fisso), quindi in scuro cambiano poco.

## Struttura della finestra
- Titolo finestra (libreria), linea sotto il titolo nascosta.
- Corpo "Contenuto" (verticale): riempimento superficie/vetro, angoli in basso raggio/finestra, padding spazio/8, gap spazio/8.
  - Barra degli strumenti (dettaglio e conferme): padding 0, nessun riempimento.
  - Pannello contenuto: superficie/elevata, raggio/pannello, ritaglio acceso.
    - Schermate a elenco (app installate, nessuna app tua, app tolta...): padding alto/basso spazio/16, sinistra/destra spazio/8 (le righe "Riga dell'app" hanno sfondo proprio con padding 8: punto 3). Elenco con padding 0.
    - Altre schermate: padding spazio/16 su tutti i lati; Elenco e "Stato vuoto" (istanza, padding portato a 0) senza padding.
  - Azioni (dove ci sono i pulsanti): padding 0, a destra, sotto il pannello, a 8 dai bordi.
- Il pannello prima occupava tutto il corpo (senza margine): ora e incassato come in Files.

## Somme della spaziatura (punto 5)
- Bordo del pannello -> primo testo: schermate a elenco x = 8 + 8 = 16, y = 16; altre x = 16, y = 16. Misurato sulle foto: 16 in x, primo pixel delle lettere a 19-22 in y (scatola del testo a 16). In "app tolta" il primo elemento e il messaggio con sfondo proprio: sfondo a 8 in x e 16 in y, testo dentro a 12.
- Dal bordo della finestra:
- Schermate a elenco: x = 8 (corpo) + 8 (pannello) + 8 (riga o intestazione) = 24; y = 8 + 16 = 24.
- Altre schermate: x = 8 + 16 = 24; y = 8 + 16 = 24.

## Voce della barra laterale (componente 133:1721, tutte le app)
- Stato=Selezionata (133:1716): sfondo Semantici/controllo/selezione-vetro (Chiaro #000000 8%, Scuro #FFFFFF 12%).
- Stato=Sopra (133:1711): sfondo Semantici/controllo/sopra-vetro (Chiaro #000000 4%, Scuro #FFFFFF 6%).
- Padding orizzontale 8, verticale 0 (sfondo proprio, punto 3).
