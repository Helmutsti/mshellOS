# Terminale: il disegno

Fonte: Figma "mshell – interfaccia" (X9g7EOJgKBlE4QjmjrTLFt), pagina "App – Terminale" (217:2).
Finestra Chiaro 218:6, Scuro 218:209; dettagli 222:64 / 222:138; icona 224:106.

- `figma/finestra-chiaro.png` (218:5) e `figma/finestra-scuro.png` (218:208): foto ritagliate sulla finestra (760 x 588). Si rifanno solo se il disegno cambia. `figma/icona.png`: l'icona.
- `codice/`: foto del terminale vero (sandbox). Si aggiornano a ogni cambiamento grafico.

## Finestra (760 x 588)
- Titolo finestra (libreria), Attiva, testo "Terminale – super amministratore", comandi riduci e chiudi.
- Niente barra degli strumenti, niente linee o bordi colorati, nessuna linea tra titolo e corpo (regola di tutte le finestre: sotto il titolo non si vede mai una linea).
- Corpo: colore della barra del titolo (superficie/vetro), angoli in basso raggio/finestra.
- Corpo "Contenuto": padding spazio/8 su tutti i lati (il frame "Margine riquadro" non c'e piu).
- Pannello contenuto (libreria): superficie/elevata, raggio/pannello, padding spazio/16 su tutti i lati, ritaglio acceso.
- Frame "Terminale – testo": padding 0 (lo spazio lo da solo il pannello). Bordo del pannello -> primo testo: 16 in x e in y. Somma dal bordo della finestra al primo testo: x = 8 + 16 = 24, y = 8 + 16 = 24.
- Barra di scorrimento della libreria a destra nel pannello (visibile solo se c'è da scorrere).
- Il terminale segue il tema: chiaro nel Chiaro, scuro nello Scuro.

## Testo
- IBM Plex Mono 13/20; grassetto dove indicato.
- Prima riga: "Attenzione:" giallo grassetto + "questa shell ha i permessi di super amministratore." giallo.
- Seconda riga grigia: "Scrivi help per l'elenco dei comandi."
- Prompt `root@mshell <cartella> # `: root rosso grassetto, @mshell grigio, cartella blu grassetto, # rosso.
- `ls`: nomi in colonne, su piu righe se non stanno in una (come ls; sulla seriale uno per riga), cartelle blu grassetto, programmi verdi grassetto, collegamenti menta, file colore del testo.
- `ls -l`: permessi grigi, proprietario e gruppo col nome ("root" per l uid 0, per gli altri il numero) arancio, misura viola, nome colorato come in `ls`; collegamento "nome -> destinazione" con la freccia grigia. Niente data.
- Errori rossi, per esempio "cd: /sistema: cartella inesistente".
- `help`: comando verde grassetto, descrizione grigia, colonna allineata.
- Cursore a blocco. Selezione: colore informazione/tenue.
- Colori dai Semantici: sistema/<colore>/testo-pastello; seguono Chiaro/Scuro.

## Icona
Piastrella arrotondata grigio scuro (Chiaro #1c1c1e, Scuro #37373b) con riflesso in alto e glifo Lucide "terminal" bianco. Misure 96 / 32 / 18.
