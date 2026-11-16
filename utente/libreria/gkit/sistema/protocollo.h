/* GKit, DEL SISTEMA: il protocollo delle finestre, cioe' i messaggi che si scambiano
 * un'applicazione e il compositore, e le superfici speciali della scrivania (gkit.h spiega i
 * livelli). Lo usano il compositore (l'altro capo), la scrivania (i suoi pannelli e lo
 * sfondo), init (il nome su cui il compositore ascolta) e il codice di GKit
 * (gkit/interno/finestra.c). Un'applicazione non lo include: per lei il protocollo e' la
 * finestra di gkit.h, che manda e riceve questi messaggi al suo posto. Puo' cambiare senza
 * avviso, insieme al compositore.
 *
 * E' il "Wayland ridotto all'essenziale" di mkernel. Le idee sono quelle di Wayland:
 *   - l'applicazione disegna DA SOLA la sua finestra, in un foglio di memoria condivisa
 *     che passa al compositore; il compositore non disegna niente dentro le finestre;
 *   - il foglio contiene DUE immagini: mentre il compositore mostra una, l'applicazione
 *     disegna nell'altra. Con MOSTRA la passa al compositore, che quando non gli serve
 *     piu' quella di prima la restituisce con RILASCIATA. Cosi' non si vede mai una
 *     finestra disegnata a meta';
 *   - le coordinate del puntatore sono INTERNE alla finestra: l'applicazione non sa
 *     dov'e' sullo schermo e non vede le altre finestre;
 *   - la finestra e' TUTTA dell'applicazione, barra del titolo e bordo compresi: e'
 *     lei a decidere il suo aspetto (come su Mac e in GNOME, le "decorazioni lato
 *     client"). Se usa UIKit, sara' lui a disegnarle con lo stile del sistema;
 *   - a spostarla pero' e' il compositore, perche' l'applicazione non sa dov'e' sullo
 *     schermo: quando si preme sulla sua barra, lei gli dice TRASCINAMI e lui la sposta
 *     seguendo il mouse finche' il pulsante resta premuto;
 *   - un messaggio sbagliato, e il compositore la scollega;
 *   - ogni tanto il compositore chiede "ci sei?" (PING) e l'applicazione risponde
 *     "ci sono" (PONG). Lo fa la libreria delle finestre, da sola, ogni volta che
 *     l'applicazione aspetta le sue notizie. Se la risposta non arriva, l'applicazione
 *     e' bloccata: il compositore ne ridisegna la barra in rosso, con il pulsante di
 *     chiusura d'emergenza. Come GNOME e il Mac.
 *
 * I RUOLI SPECIALI (solo per i programmi del sistema, uid 0: il compositore lo chiede al
 * kernel con chi). Oltre alle finestre normali ci sono superfici che stanno in uno strato
 * fisso, e che sceglie la scrivania (come layer-shell di Wayland):
 *
 *     PANNELLO   sopra tutte le finestre (la userbar della scrivania, il vassoio delle app)
 *     finestre   le applicazioni, nel mezzo: si spostano e si portano davanti
 *     SFONDO     sotto tutto
 *
 * Un pannello e' di due tipi, che il compositore distingue solo per l'ombra intorno:
 * PANNELLO_BARRA (una barra che sta sempre li', come la userbar) e PANNELLO_SOPRA (si apre
 * sopra il resto e poi si richiude, come un menu o il vassoio delle app).
 *
 * Una superficie speciale sta dove dice lei (x, y), non si trascina e non prende il fuoco
 * della tastiera. A un PANNELLO il compositore racconta anche quali finestre ci sono:
 * FINESTRA_APERTA e FINESTRA_CHIUSA, con il programma a cui appartengono (come
 * foreign-toplevel di Wayland). Cosi' la scrivania sa quali applicazioni sono aperte.
 * Le racconta anche quale finestra ha il fuoco (FINESTRA_ATTIVA), e un pannello puo'
 * chiedere di riportarne una davanti (PORTA_DAVANTI), anche se era stata ridotta.
 *
 * LA FINESTRA DI SCELTA (RUOLO_SCELTA, "Apri/Salva"). Il servizio picker mostra la scelta di un
 * file SOPRA l'app che l'ha chiesta, in sovraimpressione: nel messaggio dice il programma
 * dell'app ('sopra'). Il compositore accetta questo ruolo solo dal collegamento per nome il
 * cui programma (lo dice il kernel) e' /system/services/picker/picker; da chiunque altro scollega.
 *   - sta centrata sopra la finestra dell'app 'sopra' piu' in primo piano (o al centro dello
 *     schermo se l'app non ne ha), in uno strato proprio: sopra tutte le finestre normali (e'
 *     sempre davanti, anche se ci si clicca) e sotto i pannelli della scrivania. Non e'
 *     annunciata alla scrivania (non e' un'app in piu') e prende il fuoco quando compare;
 *   - finche' e' aperta, ogni finestra dell'app 'sopra' e' velata (il velo lo disegna il
 *     compositore, con SUPERFICIE_VELO di UIKit, sulla forma della finestra) e non riceve ne'
 *     puntatore, ne' rotella, ne' tasti: un clic su una finestra velata porta davanti la
 *     scelta e le da' il fuoco;
 *   - quando si scollega, il velo sparisce e il fuoco torna alla finestra dell'app.
 *
 * IL "RIDUCI". Un'applicazione chiede RIDUCIMI (il pulsante nella sua barra): il
 * compositore nasconde la finestra, che non si vede e non sente il puntatore finche'
 * la scrivania non la fa tornare con PORTA_DAVANTI.
 *
 * IL TEMA (Chiaro o Scuro) DI TUTTO IL SISTEMA non passa di qui: lo tiene catalog
 * (corekit/catalog.h), che lo cambia per chi ha il permesso "settings.system" e avvisa chi osserva.
 * Il compositore osserva per le sue ombre; le app di UIKit e la scrivania osservano da sole e si
 * ridisegnano. (Fino al 16 novembre 2026 passava dal compositore, con CAMBIA_TEMA e TEMA_CAMBIATO:
 * i numeri 9 e 13 restano liberi.)
 *
 * Una finestra per collegamento: per due finestre, due collegamenti.
 *
 * LA PORTA DELL'APP (APPLICAZIONI.md, "Come funzionano le chiavi"). Un'app non cerca il
 * compositore per nome: quando la fa partire, il launcher crea una coppia di socket, ne da' un
 * capo all'app (lo trova al descrittore D_PORTA_APP) e manda l'altro al compositore con
 * PORTA_APP, dicendo di quale programma e'. Per ogni finestra l'app crea una coppia sua e ne
 * manda un capo sulla porta (NUOVO_COLLEGAMENTO): quel collegamento e' dell'app della porta,
 * lo dice il launcher, non il kernel e non l'app. Da li' si parla come sopra, senza il launcher.
 * I programmi del sistema (la scrivania) e chi non ha la porta si collegano ancora per nome, e
 * per loro chi sono lo dice il kernel.
 * Ogni messaggio comincia con il suo tipo.
 *
 * I pixel nelle immagini: 4 byte ciascuno, 0xAARRGGBB PREMOLTIPLICATI (AA e' l'opacita',
 * e i colori sono gia' moltiplicati per lei: vedi gkit_disegno.h), come ARGB8888 di Wayland.
 * Riga dopo riga, senza spazi tra una riga e l'altra. Il compositore mescola ogni pixel
 * con quello che c'e' sotto: una finestra puo' essere di vetro, e i suoi angoli
 * arrotondati sono trasparenti. L'immagine k sta nel foglio a k * larghezza * altezza * 4. */
#ifndef GKIT_SISTEMA_PROTOCOLLO_H
#define GKIT_SISTEMA_PROTOCOLLO_H

#include "gkit/gkit.h"

#define NOME_COMPOSITORE "compositore"   /* il nome su cui ascolta */

/* --- dall'applicazione al compositore --- */

#define VOGLIO_FINESTRA 1   /* il primo messaggio, con il foglio allegato */
#define MOSTRA          2   /* "l'immagine 'immagine' e' pronta: mostrala" */
#define TRASCINAMI      3   /* "hanno premuto sulla mia barra: spostami tu". Vale solo
                             * mentre il pulsante premuto dentro la finestra e' ancora
                             * giu'; se e' gia' stato lasciato, il compositore la ignora */
#define PONG            4   /* "ci sono": la risposta al PING 'numero' */
#define CHIEDI_SCHERMO  5   /* "quanto e' grande lo schermo?": risponde SCHERMO. Si puo'
                             * chiedere anche prima di VOGLIO_FINESTRA */
#define RIDUCIMI        6   /* "nascondimi": hanno premuto il mio pulsante riduci */
#define VOGLIO_TASTI_FISICI 8  /* "mandami anche TASTO_FISICO": si chiede una volta (facoltativo,
                             * UIKit non lo fa; serve ai giochi) */
#define PORTA_DAVANTI   7   /* (solo un PANNELLO) la finestra 'numero' torna visibile,
                             * davanti a tutte, con il fuoco */

#define MOSTRA_PUNTATORE 10 /* "il puntatore, mentre sta sulla mia finestra, deve avere la forma
                             * 'forma'" (FORMA_..., gkit.h). Il compositore la tiene finche' non
                             * ne arriva un'altra: l'attesa la mostra anche quando un'app e'
                             * bloccata. Una forma che non c'e': la finestra si scollega */

#define PORTA_APP       11  /* (solo il sistema: il launcher) "ecco la porta di un'app che sto
                             * facendo partire": allegato il capo del compositore, nel messaggio
                             * il programma dell'app. Su un collegamento fatto apposta, che il
                             * compositore chiude subito dopo */
#define NUOVO_COLLEGAMENTO 12 /* (sulla porta di un'app) "ecco il collegamento per una mia
                             * finestra": allegato un capo di una coppia fatta dall'app */

#define D_PORTA_APP     3   /* dove un'app fatta partire dal launcher trova la sua porta: dopo
                             * 0, 1 e 2, che restano l'ingresso, l'uscita e gli errori di ogni
                             * programma (il launcher li collega a /dev/null) */

/* I ruoli speciali (RUOLO_FINESTRA, la finestra normale, e' in gkit.h) */
#define RUOLO_SFONDO    1   /* sotto tutto (solo il sistema) */
#define RUOLO_PANNELLO  2   /* sopra tutte le finestre (solo il sistema) */
#define RUOLO_SCELTA    3   /* una finestra di scelta del sistema sopra le finestre dell'app
                             * 'sopra' (solo il servizio picker: /system/services/picker/picker) */
#define PANNELLO_BARRA  0   /* (solo un PANNELLO) una barra, sempre li' */
#define PANNELLO_SOPRA  1   /* (solo un PANNELLO) si apre sopra il resto, come un menu */

struct voglio_finestra {
    unsigned int tipo;
    unsigned int larghezza, altezza;   /* tutta la finestra, barra compresa */
    char titolo[MAX_TITOLO];
    unsigned int ruolo;                /* RUOLO_... */
    int x, y;                          /* dove sta: solo per i ruoli speciali */
    unsigned int raggio;               /* gli angoli arrotondati: li usa il compositore per
                                        * l'ombra e il vetro smerigliato sotto */
    unsigned int pannello;             /* PANNELLO_...: solo per un PANNELLO */
    char sopra[MAX_PERCORSO_PROGRAMMA];   /* solo per RUOLO_SCELTA: il programma dell'app che ha
                                        * chiesto la scelta (/system/apps/<id>/code) */
};

struct chiedi_schermo {
    unsigned int tipo;
};

struct porta_app {
    unsigned int tipo;
    char programma[MAX_PERCORSO_PROGRAMMA];   /* /system/apps/<id>/code: di chi e' la porta */
};

struct nuovo_collegamento {
    unsigned int tipo;
};

struct voglio_tasti_fisici {
    unsigned int tipo;
};

struct riducimi {
    unsigned int tipo;
};

struct porta_davanti {
    unsigned int tipo;
    unsigned int numero;
};

struct mostra_puntatore {
    unsigned int tipo;
    unsigned int forma;                /* FORMA_... (gkit.h) */
};


/* Con MOSTRA l'applicazione dice anche DOVE l'immagine nuova e' diversa da quella di prima:
 * le zone cambiate (vedi gkit.h, "le zone cambiate"). */
struct mostra {
    unsigned int tipo;
    unsigned int immagine;             /* 0 o 1 */
    unsigned int quante;               /* le zone cambiate; 0: tutta la finestra */
    struct zona_finestra zone[MAX_ZONE_MOSTRA];
};

struct trascinami {
    unsigned int tipo;
};

struct pong {
    unsigned int tipo;
    unsigned int numero;
};

/* --- dal compositore all'applicazione: le notizie che l'applicazione non vede ---
 * Arrivano in una struct notizia (gkit.h), come le altre: quelle per tutte le finestre
 * (PUNTATORE, TASTO...) sono in gkit.h; queste due le sbriga la libreria da sola, le altre
 * arrivano solo ai pannelli della scrivania. */

#define RILASCIATA 1   /* l'immagine 'immagine' e' tornata tua: ci puoi ridisegnare */
#define PING       6   /* "ci sei?": rispondi PONG con lo stesso 'numero' */
#define FINESTRA_APERTA 7   /* (solo a un PANNELLO) e' comparsa la finestra 'numero', del
                             * programma 'testo' (il suo percorso sul disco) */
#define FINESTRA_CHIUSA 8   /* (solo a un PANNELLO) la finestra 'numero' non c'e' piu' */
#define SCHERMO    9   /* la risposta a CHIEDI_SCHERMO: lo schermo e' x per y pixel */
#define FINESTRA_ATTIVA 10  /* (solo a un PANNELLO) ora ha il fuoco la finestra 'numero'
                             * (0: nessuna) */

/* --- le superfici speciali --- */

/* Apre un PANNELLO del tipo 'pannello' (PANNELLO_BARRA o PANNELLO_SOPRA), largo 'larghezza'
 * e alto 'altezza', con l'angolo in (x, y) sullo schermo e gli angoli arrotondati di
 * 'raggio'. Come finestra_apri_come (gkit.h) con RUOLO_PANNELLO, che invece apre sempre una
 * barra. 0 se fatto, -1 se il compositore non c'e' o il foglio non si crea. Se chi lo chiede
 * non e' il sistema, il compositore chiude il collegamento. */
int finestra_apri_pannello(struct finestra *f, int larghezza, int altezza, const char *titolo,
                           unsigned int pannello, int x, int y, unsigned int raggio);

#endif
