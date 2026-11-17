/* CoreKit, pubblico: picker, il servizio che fa scegliere alla persona un file da aprire o dove
 * salvare (SISTEMA.md, sezione 1; APPLICAZIONI.md, sezione 9, "Aprire un file scelto dalla
 * persona"). Un'app senza il permesso sui file chiede "fammi scegliere un file": il servizio
 * mostra la finestra "Apri" (o "Salva") SOPRA l'app, con il velo sulle sue finestre; la persona
 * sceglie; l'app riceve SOLO quel file, gia' aperto. Non serve nessun permesso: e' la base di
 * ogni app.
 *
 * Il servizio gira come la persona (uid 1000) e apre i file al suo posto. La finestra parte
 * dalla Home (/data/users/<uid>/home) e non ne esce mai: niente "..", niente collegamenti che
 * portano fuori (il kernel lo garantisce: openat2 con RESOLVE_BENEATH, vedi utente/programmi/picker.c).
 * Ascolta sul nome "picker", che gli presta init.
 *
 * I messaggi seguono lo STANDARD di SISTEMA.md (sezione 3; messaggi.h). Qui il CONTRATTO; le
 * funzioni in fondo lo nascondono, e stanno in interno/picker.c.
 *
 * LE RICHIESTE (il valore di request):
 *   open    nessun dato. Si mostra "Apri". Risposta ok con name (il nome del file scelto, senza
 *           cartelle) e il file APERTO IN LETTURA allegato.
 *   save    name = il nome proposto (puo' essere vuoto). Si mostra "Salva", con il campo Nome.
 *           Risposta ok con name (quello scelto) e il file APERTO IN SCRITTURA allegato, creato
 *           o svuotato nella cartella scelta.
 *   Se la persona preme Annulla o chiude la finestra: result=error con reason=cancelled (e niente
 *   allegato). Se il file scelto non si apre: result=error con reason=open. Un nome proposto
 *   sbagliato (con "/", "." o "..", troppo lungo, o con caratteri di controllo) o una richiesta
 *   sconosciuta: invalid. Nessuno e' mai denied: non c'e' permesso.
 *   Una richiesta alla volta: mentre una finestra e' aperta, le altre aspettano il loro turno.
 *
 * CHI CHIEDE decide SOPRA CHE APP va la finestra: per un'app e' il suo programma (lo dice il
 * launcher dalla porta); per un programma del sistema che chiede per nome, il suo (lo dice il
 * kernel). */
#ifndef PICKER_H
#define PICKER_H

#include "corekit.h"

#define NOME_PICKER "picker"

/* LA PORTA (messaggi.h, "le porte delle app"): un'app fatta partire dal launcher trova la sua
 * porta verso picker a questo descrittore, ed e' una porta di BASE (tutte le app, senza permessi).
 * Le funzioni qui sotto la usano da sole (se non c'e', si collegano per nome). */
#define D_PORTA_PICKER 7

/* le richieste (il valore di request) */
#define PICKER_RICHIESTA_APRI   "open"
#define PICKER_RICHIESTA_SALVA  "save"

/* le chiavi */
#define PICKER_CHIAVE_NOME      "name"
#define PICKER_CHIAVE_MOTIVO    "reason"      /* con result=error */
#define PICKER_MOTIVO_ANNULLATO "cancelled"
#define PICKER_MOTIVO_APERTURA  "open"        /* il file scelto non si apre */

/* Gli esiti delle funzioni, oltre al descrittore del file (>= 0). */
#define PICKER_NON_RISPONDE (-1)   /* il servizio picker non risponde, o ha chiuso */
#define PICKER_ANNULLATO    (-2)   /* la persona ha premuto Annulla, o ha chiuso la finestra */
#define PICKER_NON_VALIDO   (-3)   /* il nome proposto non va (o la richiesta e' sbagliata) */
#define PICKER_ERRORE       (-4)   /* il file scelto non si e' potuto aprire */

/* Il tempo che la funzione aspetta la risposta: la persona ci mette quanto vuole, quindi NON c'e'
 * il limite di 5 secondi delle altre richieste (TEMPO_CHIEDI). 10 minuti: poi PICKER_NON_RISPONDE. */
#define PICKER_ATTESA_MS (10 * 60 * 1000)

/* Fa scegliere un file da aprire. Dorme finche' la persona sceglie. Il descrittore del file,
 * aperto in lettura (da chiudere con chiudi), e in 'nome' il suo nome (al massimo 'massimo' - 1
 * byte: se non ci sta, PICKER_ERRORE); oppure uno degli esiti qui sopra. */
int picker_apri(char *nome, int massimo);

/* Fa scegliere dove salvare un file, proponendo il nome 'proposto' (o 0: nessuno). Dorme finche'
 * la persona sceglie. Il descrittore del file, aperto in scrittura (da chiudere con chiudi), e
 * in 'nome' il nome scelto; oppure uno degli esiti qui sopra. */
int picker_salva(const char *proposto, char *nome, int massimo);

#endif
