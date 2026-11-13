# Impostazioni: il disegno

Fonte: Figma "mshell – interfaccia" (X9g7EOJgKBlE4QjmjrTLFt), pagina "App – Impostazioni" (141:732).
Schermata "App – Impostazioni" 228:52, finestra "Finestra Impostazioni" 228:53 (sezione Aspetto aperta).
Il vecchio disegno (frame "Impostazioni" 139:1708) resta sulla pagina, non toccato. Icona dell'app: sezione 166:566 (invariata).

- `figma/aspetto-chiaro.png`, `figma/aspetto-scuro.png`: foto ritagliate sulla finestra (800 x 520). Si rifanno solo se il disegno cambia.
- Lo scuro si ottiene mettendo la collezione Semantici sul modo "Scuro" sul frame 228:52; finita la foto il frame torna al modo esplicito "Chiaro". La finestra 228:53 non ha un modo suo. Solo per la foto scura la scelta del tema e portata su "Scuro"; nel disegno resta "Chiaro".
- `codice/`: foto dell'app vera (sandbox), da aggiornare a ogni cambiamento grafico.

## Finestra (800 x 520)
- Cornice esterna "App – Impostazioni": padding spazio/32 su tutti i lati, nessun riempimento. Ombra della finestra copiata da Files (Semantici/effetto/ombra/finestra/*).
- Titolo finestra (libreria): Stato=Attiva, Comandi=No, Dimensione=Normale, testo "Impostazioni". Linea sotto il titolo nascosta (regola di tutte le finestre).
- Corpo "Contenuto" (228:67): riempimento Semantici/superficie/vetro, angoli in basso Misure/raggio/finestra, padding Misure/spazio/8 su tutti i lati, gap Misure/spazio/8 tra barra laterale e area principale. E l'unico livello che da spazio tra il bordo della finestra e le aree.
- Niente barra degli strumenti.
- Barra laterale (184 di larghezza, fuori dal pannello) + Area principale > Pannello contenuto. Il frame "Margine riquadro" non c'e piu: lo spazio lo da il corpo.

## Barra laterale
- Padding 0 su tutti i lati (Misure/spazio/0), larghezza 184, gap Misure/spazio/4. Nessun bordo verso il contenuto (regola di tutte le barre laterali).
- 6 voci, componente "Voce della barra laterale" (Stato=Normale / Selezionata; sfondo della Selezionata Semantici/controllo/selezione-vetro, Chiaro #000000 8%, Scuro #FFFFFF 12%; Sopra: controllo/sopra-vetro, #000000 4% / #FFFFFF 6%), larghezza piena (184), altezza Misure/altezza/s, padding 8 / 0 (sfondo proprio: eccezione del punto 3), gap Misure/spazio/8, raggio Misure/raggio/controllo.
- In ordine: Aspetto (Selezionata, Simbolo/palette), Lingua e ora (Simbolo/globe), Tastiera e mouse (Simbolo/keyboard), App e sicurezza (Simbolo/shield), Rete (Simbolo/wifi), Informazioni (Simbolo/info). Nessuna intestazione di gruppo.
- Icone: componenti "Simbolo/..." Lucide 24 px, tratto 2, colore Colore icona/icona/colore; usate a Misure/icona/s (16). I sei Simbolo sono nuovi, in "Componenti base" > "Icone – Simbolo" (terza riga): palette 227:4106, globe 227:4117, keyboard 227:4124, shield 227:4143, wifi 227:4146, info 227:4155.

## Pannello contenuto
- Area principale: padding 0, il pannello la riempie (592 x 464).
- Pannello: Semantici/superficie/elevata, angoli Misure/raggio/pannello, padding Misure/spazio/16 su tutti i lati, gap Misure/spazio/16, ritaglio spento (non scorre).
- Riga "Tema": padding 0, etichetta a sinistra (testo/corpo-medio, Semantici/testo/primario "Tema"), a destra il componente "Gruppo" (Dimensione=M, Contenuto=Testo) con due elementi "Chiaro" (Selezionato=Sì) e "Scuro" (Selezionato=No); il terzo elemento del componente e nascosto.
- Riga "Sfondo della scrivania": padding 0, etichetta a sinistra; a destra, con gap Misure/spazio/16, anteprima 120 x 75 (rettangolo, raggio Misure/raggio/controllo, colori FISSI, non variabili del tema: lo sfondo della scrivania è lo stesso nei due temi, quindi l'anteprima è identica in chiaro e in scuro. Imita lo sfondo vero della sandbox: fondo #3060B6 e quattro luci radiali che sfumano a trasparente: viola #5A3CC0 95% in alto a sinistra (centro 27%/30%), azzurro #3A80C8 90% a destra (80%/60%), lilla #8C64B8 90% in basso al centro (48%/88%), blu notte #1E2868 85% nell'angolo in alto a sinistra) e "Pulsante" Tipo=Secondario, Dimensione=M, Stato=Normale, testo "Cambia", senza icona.
- Testo IBM Plex Sans, stili testo/corpo-medio (14/20) per le etichette, testo/corpo (14/20) nelle voci.

## Somme della spaziatura (punto 5)
- Barra laterale, prima voce: x = 8 (corpo) + 0 (barra) + 8 (voce, sfondo proprio) = 16 fino al testo; lo sfondo della voce sta a 8 dal bordo. y = 8 (corpo) + 0 + 0 = 8 fino allo sfondo della voce. Lo sfondo della prima voce e il pannello cominciano alla stessa altezza (8 sotto il titolo).
- Bordo del pannello -> primo contenuto: 16 in x e in y (il controllo "Gruppo" della riga Tema parte a 16; l'etichetta "Tema" e centrata nella riga alta 40, quindi la sua scatola sta a 26).
- Pannello, prima riga: x = 8 (corpo) + 0 (area) + 16 (pannello) = 24; y = 8 + 0 + 16 = 24 (righe, etichette e controlli hanno padding 0; "Gruppo" e "Pulsante" hanno sfondo proprio).
- Misure: barra 184, gap 8, area e pannello 592 x 464. Sfondo della voce selezionata a 8 dal bordo sinistro e a 8 dal pannello; pannello a 8 dal bordo destro e inferiore.

## Indirizzi
- Schermata 228:52, finestra 228:53, titolo 228:54, corpo 228:67, barra laterale 228:68, area 228:132, pannello 228:134, riga Tema 228:135, scelta tema 228:137, riga Sfondo 228:147, anteprima 228:150, pulsante Cambia 228:151.
