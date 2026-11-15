# Scrivania: il disegno (userbar)

Fonte: Figma "mshell – interfaccia" (X9g7EOJgKBlE4QjmjrTLFt).
- Pagina "Scrivania" 81:733. Schermate: "Scrivania – a riposo" 148:769 (userbar 148:773), "Scrivania – vassoio delle app aperto" 148:816 (userbar 148:820).
- Componente Userbar: insieme di varianti 148:768 (sezione 160:3269, pagina "Componenti complessi" 104:2). Varianti "Pulsante App=Normale" 148:594, "=Sopra" 148:652, "=Premuto" 148:710. Le modifiche stanno nelle tre varianti.
- `figma/schermata-chiaro.png`, `figma/schermata-scuro.png`: schermata a riposo 1280 x 800. `figma/schermata-vassoio-chiaro.png`: schermata 148:816 (vassoio aperto, chiaro). `figma/userbar-chiaro.png`, `figma/userbar-scuro.png`: solo la userbar a scala 4 (224 x 1392, con l'ombra), esportate da Figma con una copia temporanea ingrandita 4x (poi cancellata). Scuro = modo Scuro della collezione Semantici messo sulla schermata solo per la foto, poi tolto.

## Userbar (pillola di vetro, 40 x 332, x 8 -> 48, centrata in altezza: y 234 nella schermata)
- Auto-layout verticale, larghezza fissa 40, altezza che si adatta. Padding spazio/4 su alto, destra e sinistra; **in basso spazio/8** (vedi spaziatura). **Gap spazio/16 tra i due gruppi.** Angoli raggio/pillola, riempimento superficie/vetro (72%), ombra effetto/ombra/barra. Nessuna linea, nessun ritaglio.
- Due gruppi (auto-layout verticale, padding spazio/0, gap spazio/8, senza sfondo, senza ritaglio):
  - "Gruppo app" (232 di altezza): Pulsante App (Primario, S, 32) + app (Secondario, S, 32: Files, Browser, Posta, Impostazioni, Attivita).
  - "Gruppo profilo" (72): Foto profilo + Ora.
- Gap tra i gruppi = spazio/16: e il doppio del gap interno (8), quindi lo stacco si vede senza linea; 24 sarebbe troppo grande per una pillola di 332 su 800 e la fa crescere oltre il necessario.
- Foto profilo: cerchio 32 x 32, angoli raggio/pillola, fondo controllo/sfondo, padding 0. Segnaposto: Simbolo/user (Lucide "user", 16, colore icona Primario) perche non c'e ancora una foto vera.
- Ora: testo su due righe, "09" sopra e "41" sotto (ore e minuti, 24 ore), stile testo/piccolo (IBM Plex Sans Regular 12/16), colore testo/primario, centrato, spaziatura lettere 0. Casella larga 32 (riempie il gruppo), alta 32, padding 0.
- Separazione tra app e profilo: solo il gap spazio/16 della pillola.

## Spaziatura (userbar)
- Gli elementi e i gruppi hanno padding 0; lo spazio lo da solo la pillola.
- Alto: bordo pillola -> glifo del pulsante App = 4 + 8 (sfondo del pulsante, eccezione del punto 3) = 12 in y; lati: 4 + 8 = 12 in x. Somme alto: x = 12, y = 12.
- Basso: la riga "41" ha 16 di interlinea con le cifre piu basse di circa 4 dal fondo della riga, quindi la pillola ha padding in basso spazio/8: 8 + circa 4 = 12 dall'ultima cifra al bordo. Somme basso: y = 12 (misurato sulla foto 4x: alto 13, basso 11), x del testo = 13.
- Il padding in basso e l'unico livello con spazio sul lato basso (il testo non ha sfondo, quindi non c'e l'eccezione del punto 3): e un compromesso, vedi domande nel resoconto.

## Colori e misure (variabili)
- Semantici: superficie/vetro, controllo/sfondo, testo/primario (+ Colore icona: Primario). Misure: spazio/4, spazio/8, raggio/pillola. Stile: testo/piccolo.
- Chiaro e Scuro seguono la collezione Semantici.

## Note
- Vassoio delle app aperto (148:862): centrato in altezza sulla userbar (centro del vassoio = centro della pillola = y 400; vassoio a y 312).
