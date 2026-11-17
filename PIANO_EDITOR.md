# Piano: l'editor di testo (ripreso il 17/11/2026: primo passo fatto)

Messo da parte dalla persona per chiudere prima vim, poi ripreso. Quello che è segnato come
"deciso" lo è; il resto si decide quando si arriva al passo.

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

## Fatto: il passo 1 (17/11/2026)
Il componente **area di testo** di UIKit (`uikit/area_testo.h`) e l'app **Blocco note**
(`applicazioni/BloccoNote/`): una finestra con l'area di testo, documento nuovo e vuoto. Niente file
(Nuovo, Apri e Salva sono nella barra come nel disegno ma non fanno ancora niente), niente annulla e
ripeti, niente copia e incolla: sono i passi dopo. Il disegno è in `disegni/BloccoNote/`; le foto della
sandbox in `disegni/BloccoNote/codice/`; la prova è `test/prove/bloccotesto.py`.

**Deciso dalla persona per questo passo:**
- testo a **larghezza fissa disegnato a griglia come il Terminale** (una lettera, una casella; il testo
  è UTF-8: una lettera è un carattere Unicode, non un byte);
- il testo lo tiene il componente in un **buffer con il buco** (gap buffer);
- **a capo automatico a carattere** (deciso dopo il primo giro): la riga si riempie fino all'ultima
  colonna; se l'a capo cade in mezzo a una parola l'ultima casella mostra il segno `_` (colore
  `testo/secondario`), solo disegnato, che non sta nel testo e non si seleziona; se cade su uno spazio
  nessun segno;
- su/giù sui bordi come VS Code (sulla prima riga su va all'inizio, sull'ultima giù alla fine);
- la barra di scorrimento compare anche quando il testo scorre perché il cursore si muove da tastiera;
- l'icona dell'app è quella del disegno Figma (cornice "Blocco note" della pagina Scrivania);
- **Tab = un carattere `'\t'`, mostrato largo fino alla colonna multipla di 4**; con più righe
  selezionate Tab aggiunge un `'\t'` a ognuna; **Maiusc+Tab toglie il rientro** di ogni riga selezionata
  (o della riga del cursore): un `'\t'`, o fino a 4 spazi. Come Visual Studio Code;
- tastiera: frecce (colonna voluta), Home/Fine, PagSu/PagGiù, Ctrl+frecce (parola per parola),
  Ctrl+Home/Fine, Maiusc per selezionare, Ctrl+A; mouse: clic, trascinamento, doppio clic sulla parola;
  scorrimento con la rotella e seguendo il cursore.

## I pezzi, in ordine (1, 2 e 3 sono indipendenti)
1. **Tastiera**: il servizio eventi ripete tutti i tasti, compresi quelli fisici (oggi frecce,
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
- Cosa fare dei passi 1-3 e 5 che restano: annulla e ripeti; appunti (copia e incolla); Nuovo, Apri e
  Salva veri (il picker c'è già); il punto "•" nel titolo per le modifiche non salvate (proposta nel
  disegno, da confermare).

## Come lo fanno gli altri (riferimento)
- Mac: NSTextView su TextKit (testo, impaginazione, forma dell'area), sotto Core Text.
- Windows: EDIT (vecchio Blocco note), RichEdit (WordPad), oggi TextBox con DirectWrite.
- Linux (GTK): GtkTextView, testo in un albero, Pango e HarfBuzz per impaginare.
