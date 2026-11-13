# Attività: il disegno

Fonte: Figma "mshell – interfaccia" (X9g7EOJgKBlE4QjmjrTLFt), pagina "App – Attività" (81:735).
Schermate (cornice con padding spazio/32): Processi 133:921, Processi riga selezionata 133:1090, Processi avviso di riavvio 133:1257 (con velo 138:908 e avviso 133:1424), Processi servizi di sistema visibili 264:4034, Memoria 133:1539, Disco 133:1609. Icona dell'app: sezione 166:477.

- `figma/`: foto ritagliate sulla finestra, in chiaro e scuro (`-chiaro.png`, `-scuro.png`): `processi` (133:921), `processi-riga-selezionata` (133:1090), `processi-avviso` (133:1257), `processi-servizi-visibili` (264:4034), `memoria` (133:1539), `disco` (133:1609). Scuro = Semantici sul modo "Scuro" sulla cornice, poi tolto: nessuna cornice ha un modo esplicito (sono in Chiaro).

## Struttura della finestra (768 di larghezza; l'altezza nel disegno segue il contenuto: Processi 438 con 6 righe, 614 con 10 righe; nell'app la finestra ha la sua altezza e l'elenco delle righe scorre)
- Titolo finestra (libreria), linea sotto il titolo nascosta.
- Corpo "Contenuto" (verticale): riempimento superficie/vetro, angoli in basso raggio/finestra, padding spazio/8, gap spazio/8.
  - Barra degli strumenti: padding 0, nessun riempimento, gap spazio/8, elementi centrati in verticale. Schede (Dimensione=S, riempie lo spazio libero) e, con una riga selezionata, "Azioni" (Riavvia, Termina processo). Nient'altro.
  - Pannello contenuto: superficie/elevata, raggio/pannello.
    - Processi: padding spazio/8; Tabella processi padding 0 (intestazione 32, righe 40, celle con padding orizzontale 8: le righe hanno sfondo proprio); Piede alto 32, orizzontale, padding 0,8,0,8 (spazio/8, come le righe: il pannello ha tolto 8 per le righe, punto 3), gap spazio/16, centrato in verticale:
      - a sinistra il conteggio ("6 processi · 18,0 MB in uso": conta solo le righe mostrate), riempie lo spazio;
      - a destra "Servizi di sistema": orizzontale, padding 0, gap spazio/8, centrato. Etichetta "Mostra i servizi di sistema" (testo/corpo, Semantici testo/primario) + Interruttore (Componenti base 94:1068): Acceso=No, Stato=Normale di base; Acceso=Sì nella schermata 264:4034. L'interruttore finisce a 16 dal bordo destro del pannello, come il testo dell'ultima colonna delle righe.
    - Colonne (righe e intestazione, gap 24 tra le colonne): PID 48 (titolo "PID" e numeri fino a 5 cifre: "99999" misura 42), Processo riempie (236 con la finestra a 768), Utente 120, Stato 120, Memoria 100 (a destra).
    - Intestazione: "Intestazione di colonna" (105:403); nessuna colonna mostra la freccia di ordinamento (PID = Ordinamento Nessuno), perché l'ordine è quello dell'albero.

## Albero dei processi (colonna "Processo")
- Righe: "Riga di tabella" (Componenti complessi 105:446), varianti Contenuto=Processo, Stato=Normale / Selezionata / Sopra, Livello=0/1/2/3. Livello=0: Normale 132:836, Sopra 132:859, Selezionata 132:882. Livello 1, 2, 3: Normale 268:1538, 268:1553, 268:1568; Sopra 268:1583, 268:1598, 268:1613; Selezionata 268:1628, 268:1643, 268:1658. Le varianti Contenuto=File hanno solo Livello=0.
- Proprietà testo della riga: PID (prima "Numero"), Processo, Utente, Stato del processo, Memoria.
- Cella "Processo" della riga: orizzontale, padding 0, gap 0, centrata. Contiene:
  - "Rientro" (solo Livello 1-3): frame vuoto, senza riempimento, larghezza fissa Livello x 24 (24 = spazio/24, legata alla variabile; 48 e 72 numeri fissi), alto 16;
  - "Nome": orizzontale, padding 0, gap spazio/8, riempie, centrato. Contiene "Freccia" (spazio fisso 16 x 16, senza riempimento) e il nome (riempie).
  - Dentro "Freccia" c'è l'icona 16 x 16: Simbolo/right (92:784, chevron-right, ramo chiuso) o Simbolo/giu (105:79, chevron-down, ramo aperto); colore icona/colore (collezione Colore icona, modo Primario, come la freccia di ordinamento).
  - Proprietà nuove della riga: "Freccia" (sì/no, di base no: mostra l'icona) e "Simbolo freccia" (scambio: right o giu). Chi non ha figli ha "Freccia" spenta: l'icona sparisce ma lo spazio di 16 resta, così i nomi restano allineati.
- Rientro: proprietà di variante "Livello" (0, 1, 2, 3) della riga, rientro = Livello x 24 (spazio/24 = freccia 16 + gap 8): lo spazio della freccia del figlio comincia esattamente sotto il nome del padre. Nessun padding nelle istanze: la cella "Processo" ha padding 0 in tutte le righe.
- Ordine: albero; i figli dello stesso padre in ordine di numero.
- Esempio (servizi visibili, 264:4034): 1 avvio (aperto) > 2 eventi, 3 compositore, 4 launcher (aperto) > 9 attività, 12 files, 15 terminale (aperto) > 16 shell; poi 5 scrivania, 6 shell (figli di 1). Piede "10 processi · 38,3 MB in uso".

## Interruttore "Mostra i servizi di sistema"
- Spento (di base): si nascondono avvio (1), eventi (2), compositore (3), launcher (4). Chi ha il padre nascosto sale al primo livello: 5 scrivania, 6 shell, 9 attività, 12 files, 15 terminale (aperto) > 16 shell. Piede "6 processi · 18,0 MB in uso".
- Acceso: albero completo dal processo 1.
- Schermate: spento in 133:921, 133:1090 (riga selezionata: files, 12), 133:1257 (avviso "Riavviare «files»?"); acceso in 264:4034.
    - Memoria e Disco: padding spazio/16; Riepilogo, barra di utilizzo e legenda con padding 0.
- Il vecchio frame "Contenuto" (a tutta larghezza, superficie/elevata) e ora il "Pannello contenuto" incassato.

## Somme della spaziatura (punto 5)
- Bordo del pannello -> primo testo: Processi x = 8 (pannello) + 8 (riga/intestazione) = 16, y = 8 + 8 = 16; Memoria e Disco x = 16, y = 16. Misurato sulle foto: 16 in x, primo pixel delle lettere a 19-20 in y (scatola del testo a 16).
- Dal bordo della finestra:
- Processi: x = 8 (corpo) + 8 (pannello) + 8 (cella) = 24; y = 8 + 8 + 8 (intestazione 32, testo 16) = 24.
- Memoria e Disco: x = 8 + 16 = 24; y = 8 + 16 = 24.
- Barra degli strumenti: a 8 dal bordo sinistro e dal titolo.
- Piede: x = 8 (corpo) + 8 (pannello) + 8 (piede) = 24 a sinistra e a destra; dal bordo del pannello 16 su entrambi i lati.
- Misurato sulle foto Processi (tutte e quattro, chiaro e scuro): pannello a 8 dai bordi sinistro, destro e in basso della finestra; bordo del pannello -> primo testo ("PID") x 16 (primo pixel della lettera a 17), y 16 (scatola del testo; primo pixel a 20); piede: conteggio a 16 dal bordo sinistro del pannello, interruttore a 16 dal bordo destro (17 sul bordo sfumato dell'interruttore spento in scuro; nell'avviso il velo lo copre); sfondo della riga selezionata -> pannello 8.

## Voce della barra laterale (componente 133:1721, tutte le app)
- Stato=Selezionata (133:1716): sfondo Semantici/controllo/selezione-vetro (Chiaro #000000 8%, Scuro #FFFFFF 12%).
- Stato=Sopra (133:1711): sfondo Semantici/controllo/sopra-vetro (Chiaro #000000 4%, Scuro #FFFFFF 6%).
- Padding orizzontale 8, verticale 0 (sfondo proprio, punto 3).
