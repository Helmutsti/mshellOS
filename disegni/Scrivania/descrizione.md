# Scrivania: il disegno (userbar)

Fonte: Figma "mshell – interfaccia" (X9g7EOJgKBlE4QjmjrTLFt).
- Pagina "Scrivania" 81:733. Schermate: "Scrivania – a riposo" 148:769 (userbar 148:773), "Scrivania – vassoio delle app aperto" 148:816 (userbar 148:820).
- Componente Userbar: insieme di varianti 148:768 (sezione 160:3269, pagina "Componenti complessi" 104:2). Varianti "Pulsante App=Normale" 148:594, "=Sopra" 148:652, "=Premuto" 148:710. Le modifiche stanno nelle tre varianti.
- `figma/schermata-chiaro.png`, `figma/schermata-scuro.png`: schermata a riposo 1280 x 800. `figma/userbar-chiaro.png`, `figma/userbar-scuro.png`: solo la userbar, ingrandita 4x (con l'ombra). Scuro = modo Scuro della collezione Semantici messo sulla schermata solo per la foto, poi tolto.

## Userbar (pillola di vetro, 40 x 320, x 8 -> 48, centrata in altezza: y 240 nella schermata)
- Auto-layout verticale, larghezza fissa 40, altezza che si adatta. Padding spazio/4 su tutti i lati, gap spazio/8, angoli raggio/pillola, riempimento superficie/vetro (72%), ombra effetto/ombra/barra. Nessuna linea, nessun ritaglio.
- Dall'alto: Pulsante App (Primario, S, 32), le icone delle app (Secondario, S, 32: Files, Browser, Posta, Impostazioni, Attività), **Foto profilo**, **Ora**.
- Foto profilo: cerchio 32 x 32, angoli raggio/pillola, fondo controllo/sfondo, padding 0. Segnaposto: Simbolo/user (Lucide "user", 16, colore icona Primario) perche non c'e ancora una foto vera. Quando ci sara la foto, diventa un'immagine nello stesso cerchio.
- Ora: testo "09:41" (24 ore, due cifre per le ore), stile testo/piccolo (IBM Plex Sans Regular 12/16), colore testo/primario, spaziatura lettere -0,25 px. Casella 32 x 32 senza padding, testo centrato in orizzontale e in verticale. Scelta: 12 e lo stile piu piccolo che esiste; a 12 il testo e largo 33 px e la pillola ha 32 utili, la spaziatura -0,25 lo porta a ~32.
- Separazione tra app, foto e ora: solo il gap spazio/8.

## Spaziatura (userbar)
- Gli elementi hanno padding 0; lo spazio lo da il padding spazio/4 della pillola.
- Distanza bordo pillola -> primo contenuto (glifo del pulsante App): 4 + 8 (sfondo del pulsante, eccezione del punto 3) = 12 in x e in y in alto.
- In basso: ora (casella 32 con testo centrato) -> bordo pillola: 4 + 8 = 12 in y. Somme: x = 12, y alto = 12, y basso = 12 (alto e basso uguali).
- Foto profilo: cerchio come i pulsanti, a 4 dal bordo; il glifo user a 12.

## Colori e misure (variabili)
- Semantici: superficie/vetro, controllo/sfondo, testo/primario (+ Colore icona: Primario). Misure: spazio/4, spazio/8, raggio/pillola. Stile: testo/piccolo.
- Chiaro e Scuro seguono la collezione Semantici.

## Note
- Il vassoio delle app aperto (148:862) e le sue posizioni non sono cambiate.
