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
 * IL "RIDUCI". Un'applicazione chiede RIDUCIMI (il pulsante nella sua barra): il
 * compositore nasconde la finestra, che non si vede e non sente il puntatore finche'
 * la scrivania non la fa tornare con PORTA_DAVANTI.
 *
 * IL TEMA (Chiaro o Scuro) DI TUTTO IL SISTEMA. Chi lo cambia (Impostazioni) lo scrive nelle
 * preferenze e chiede CAMBIA_TEMA al compositore; il compositore controlla che possa (il sistema,
 * uid 0, o un'app del sistema con il permesso "settings.system" nel manifesto: lo chiede al
 * kernel quale programma c'e' all'altro capo, come il launcher per "apps.manage"), rilegge lui
 * stesso il tema (le ombre intorno alle finestre sono sue) e manda TEMA_CAMBIATO a tutte le
 * finestre e a tutti i pannelli aperti. Chi lo riceve rilegge il tema dalle preferenze e si
 * ridisegna: le app di UIKit lo fanno da sole, la scrivania ricarica anche le icone delle app.
 * Da chi non puo', CAMBIA_TEMA si ignora.
 *
 * Una finestra per collegamento: per due finestre, due collegamenti.
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
#define CAMBIA_TEMA     9   /* "il tema nelle preferenze e' cambiato: dillo a tutti" (vedi
                             * IL TEMA, sopra). Solo il sistema o un'app con il permesso
                             * "settings.system"; dagli altri si ignora */

#define MOSTRA_PUNTATORE 10 /* "il puntatore, mentre sta sulla mia finestra, deve avere la forma
                             * 'forma'" (FORMA_..., gkit.h). Il compositore la tiene finche' non
                             * ne arriva un'altra: l'attesa la mostra anche quando un'app e'
                             * bloccata. Una forma che non c'e': la finestra si scollega */

/* I ruoli speciali (RUOLO_FINESTRA, la finestra normale, e' in gkit.h) */
#define RUOLO_SFONDO    1   /* sotto tutto (solo il sistema) */
#define RUOLO_PANNELLO  2   /* sopra tutte le finestre (solo il sistema) */
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
};

struct chiedi_schermo {
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

struct cambia_tema {
    unsigned int tipo;
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
