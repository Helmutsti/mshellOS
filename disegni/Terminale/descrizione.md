# Terminale: il disegno

Fonte: Figma "mshell – interfaccia" (X9g7EOJgKBlE4QjmjrTLFt), pagina "App – Terminale" (217:2).
Finestra Chiaro 218:6, Scuro 218:209; dettagli 222:64 / 222:138; icona 224:106.

- `figma/`: foto del disegno, fatte una volta. Si rifanno solo se il disegno cambia.
- `codice/`: foto del terminale vero (sandbox). Si aggiornano a ogni cambiamento grafico.

## Finestra (760 x 589)
- Titolo finestra (libreria), Attiva, testo "Terminale – super amministratore", comandi riduci e chiudi.
- Niente barra degli strumenti, niente linee o bordi colorati, nessuna linea tra titolo e corpo (regola di tutte le finestre: sotto il titolo non si vede mai una linea).
- Corpo: colore della barra del titolo (superficie/vetro), angoli in basso raggio/finestra.
- Pannello contenuto (libreria): superficie/elevata, raggio/pannello, margine spazio/8 su tutti i lati, padding 0.
- Testo: padding 16 su tutti i lati (somma dal bordo del pannello: x 16, y 16; dal bordo della finestra: 24 e 24).
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
