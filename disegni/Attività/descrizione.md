# Attività: il disegno

Fonte: Figma "mshell – interfaccia" (X9g7EOJgKBlE4QjmjrTLFt), pagina "App – Attività" (81:735).
Schermate (cornice con padding spazio/32): Processi 133:921, Processi riga selezionata 133:1090, Processi avviso di riavvio 133:1257 (con velo 138:908 e avviso 133:1424), Memoria 133:1539, Disco 133:1609. Icona dell'app: sezione 166:477.

- `figma/`: foto ritagliate sulla finestra, in chiaro e scuro (`-chiaro.png`, `-scuro.png`): `processi` (133:921), `processi-riga-selezionata` (133:1090), `processi-avviso` (133:1257), `memoria` (133:1539), `disco` (133:1609). Scuro = Semantici sul modo "Scuro" sulla cornice, poi tolto: nessuna cornice ha un modo esplicito (sono in Chiaro).

## Struttura della finestra (768 x 526, uguale per tutte le schede: nel disegno Memoria e Disco sono più basse solo perché hanno meno contenuto)
- Titolo finestra (libreria), linea sotto il titolo nascosta.
- Corpo "Contenuto" (verticale): riempimento superficie/vetro, angoli in basso raggio/finestra, padding spazio/8, gap spazio/8.
  - Barra degli strumenti: padding 0, nessun riempimento. Schede (Dimensione=S) e, con una riga selezionata, "Azioni" (Riavvia, Termina processo).
  - Pannello contenuto: superficie/elevata, raggio/pannello.
    - Processi: padding spazio/8; Tabella processi padding 0 (intestazione 32, righe 40, celle con padding orizzontale 8: le righe hanno sfondo proprio); Piede ("8 processi · 34,6 MB in uso") alto 32, padding 0,8,0,8, testo centrato.
    - Memoria e Disco: padding spazio/16; Riepilogo, barra di utilizzo e legenda con padding 0.
- Il vecchio frame "Contenuto" (a tutta larghezza, superficie/elevata) e ora il "Pannello contenuto" incassato.

## Somme della spaziatura (punto 5)
- Bordo del pannello -> primo testo: Processi x = 8 (pannello) + 8 (riga/intestazione) = 16, y = 8 + 8 = 16; Memoria e Disco x = 16, y = 16. Misurato sulle foto: 16 in x, primo pixel delle lettere a 19-20 in y (scatola del testo a 16).
- Dal bordo della finestra:
- Processi: x = 8 (corpo) + 8 (pannello) + 8 (cella) = 24; y = 8 + 8 + 8 (intestazione 32, testo 16) = 24.
- Memoria e Disco: x = 8 + 16 = 24; y = 8 + 16 = 24.
- Barra degli strumenti: a 8 dal bordo sinistro e dal titolo.

## Voce della barra laterale (componente 133:1721, tutte le app)
- Stato=Selezionata (133:1716): sfondo Semantici/controllo/selezione-vetro (Chiaro #000000 8%, Scuro #FFFFFF 12%).
- Stato=Sopra (133:1711): sfondo Semantici/controllo/sopra-vetro (Chiaro #000000 4%, Scuro #FFFFFF 6%).
- Padding orizzontale 8, verticale 0 (sfondo proprio, punto 3).
