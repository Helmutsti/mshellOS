# Puntatore: il disegno

Fonte: Figma "mshell – interfaccia" (X9g7EOJgKBlE4QjmjrTLFt), pagina "Puntatore".
- Componente Puntatore (set di varianti): 95:788, foto `figma/puntatore_chiaro.png`.
- Prova nel tema scuro: frame 160:3786 (sfondo superficie/scena scuro), foto `figma/puntatore_scuro.png`.

**Regola (deciso dalla persona): il puntatore è uguale nei due temi.** Forma scura, contorno bianco, stessa ombra. Cambia solo lo sfondo su cui sta.

## Le cinque forme (variante Tipo=...)
Idle (Normale, freccia) 95:756; Link (Collegamento, mano) 95:761; Caricamento (Attesa, cerchio) 95:776; Paragrafo (Testo, I-beam) 95:783; Disabilitato (Non disponibile, freccia con divieto) 97:882.
Ogni forma ha due parti: "contorno" (sotto) e "forma" (sopra).

## Colori (collezione Semantici, modalità Chiaro e Scuro con gli stessi valori)
- `cursore/riempimento` = forma: #37373B (primitiva 87:747). Uguale in Chiaro e Scuro.
- `cursore/contorno` = contorno: #FFFFFF (primitiva 87:734). Uguale in Chiaro e Scuro.
- Nel Caricamento l'anello di fondo usa il riempimento al 22% di opacità.

## Ombra: stile `ombra/puntatore` (su ogni variante)
- Prima: y=1, sfocatura 1 (Figma; 0,5 di sigma), spread 0, nero, colore `effetto/ombra/puntatore/1-colore` = opacità 0,20.
- Seconda: y=3, sfocatura 6 (Figma; 3 di sigma), spread -1, nero, colore `effetto/ombra/puntatore/2-colore` = opacità 0,26.
- Stessi valori in Chiaro e Scuro (prima il Scuro aveva 0,55).

## Note
- Le istanze del puntatore seguono le variabili: nessuna modifica a mano sulle istanze.
- Le variabili `scorrimento/cursore*` sono della barra di scorrimento, non del puntatore.
