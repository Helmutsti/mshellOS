# Piano: l'editor di testo (messo da parte il 17/11/2026)

Messo da parte dalla persona per chiudere prima vim. Niente qui è ancora deciso o iniziato,
tranne quello che è segnato come "deciso".

## Lo scopo
Un'app Blocco note grafica (come il Blocco note di Windows o TextEdit del Mac), fatta con un
componente "area di testo" della libreria dell'interfaccia (UIKit), riusabile anche da altre app.
Vim nel Terminale resta l'editor a riga di comando: le due cose convivono.

## Deciso dalla persona
- L'area di testo ha la **tabulazione** (Tab).
- **Tastiera completa (100%)**: frecce, Home/Fine, PagSu/PagGiù, Ins, Canc, tastierino numerico
  con Bloc Num, con Ctrl e Maiusc (parola per parola, inizio/fine testo, selezione), tutti con la
  ripetizione tenendo premuto; anche F1–F12 (scorciatoie delle app) e i tasti multimediali
  (ricevuti dal sistema, valgono in ogni app).

## I pezzi, in ordine (1, 2 e 3 sono indipendenti)
1. **Tastiera** (FATTO il 17/11/2026: ripetizione, tastierino con Bloc Num e spia, tasti multimediali fermati dal compositore e scritti nel registro; l'idea di partenza era): il servizio eventi ripete tutti i tasti, compresi quelli fisici (oggi frecce,
   Home, F1–F12... non si ripetono); tastierino numerico; tasti multimediali al sistema (un
   servizio del suono oggi non c'è). Serve anche al Terminale (vim).
2. **Lettere (UTF-8)**: oggi i font del sistema hanno i 256 caratteri cp437. Un editor legge e
   scrive file UTF-8: font rigenerati con più caratteri (europei, simboli, frecce) e testo del
   sistema in UTF-8. Va deciso insieme alla codifica del Terminale.
3. **Appunti** (copia e incolla): oggi non esistono. Un posto del sistema dove un'app mette il
   testo copiato e un'altra lo prende (su Mac e Windows li tiene il sistema delle finestre: per
   noi probabilmente il compositore).
4. **Area di testo** (componente UIKit): prima il disegno in Figma (cursore, selezione,
   scorrimento, rientri), poi il codice: testo con il "buco" dove si scrive (gap buffer),
   andare a capo con le larghezze del nostro font, Tab, tasti di movimento e selezione, annulla e
   ripeti, copia e incolla, scorrimento (c'è già).
5. **App Blocco note**: disegno in Figma, poi il codice, con l'area di testo e la finestra
   Apri/Salva (il picker, già fatto).

## Da decidere quando si riprende
- Larghezza della tabulazione (4 o 8) e se Maiusc+Tab toglie il rientro alle righe selezionate.
- Da dove partire: tastiera e UTF-8, il disegno in Figma, o i tre pezzi di base in parallelo.

## Come lo fanno gli altri (riferimento)
- Mac: NSTextView su TextKit (testo, impaginazione, forma dell'area), sotto Core Text.
- Windows: EDIT (vecchio Blocco note), RichEdit (WordPad), oggi TextBox con DirectWrite.
- Linux (GTK): GtkTextView, testo in un albero, Pango e HarfBuzz per impaginare.
