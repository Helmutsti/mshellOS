/* Il protocollo delle finestre: cosa si dicono un'applicazione e il compositore.
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
 *     client"). Se usa un toolkit, sara' lui a disegnarle con lo stile del sistema;
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
 *     PANNELLO   sopra tutte le finestre (l'isola)
 *     finestre   le applicazioni, nel mezzo: si spostano e si portano davanti
 *     SFONDO     sotto tutto
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
 * Una finestra per collegamento: per due finestre, due collegamenti.
 * Ogni messaggio comincia con il suo tipo. */
#ifndef PROTOCOLLO_H
#define PROTOCOLLO_H

#include "chiamate.h"   /* MAX_PERCORSO */

#define NOME_COMPOSITORE "compositore"   /* il nome su cui ascolta */
#define MAX_TITOLO 32

/* I pixel nelle immagini: 4 byte ciascuno, 0xAARRGGBB PREMOLTIPLICATI (AA e' l'opacita',
 * e i colori sono gia' moltiplicati per lei: vedi disegno.h), come ARGB8888 di Wayland.
 * Riga dopo riga, senza spazi tra una riga e l'altra. Il compositore mescola ogni pixel
 * con quello che c'e' sotto: una finestra puo' essere di vetro, e i suoi angoli
 * arrotondati sono trasparenti. L'immagine k sta nel foglio a k * larghezza * altezza * 4. */

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
#define PORTA_DAVANTI   7   /* (solo un PANNELLO) la finestra 'numero' torna visibile,
                             * davanti a tutte, con il fuoco */

#define RUOLO_FINESTRA  0   /* una finestra normale */
#define RUOLO_SFONDO    1   /* sotto tutto (solo il sistema) */
#define RUOLO_PANNELLO  2   /* sopra tutte le finestre (solo il sistema) */
#define RAGGIO_PREDEFINITO 12

struct voglio_finestra {
    unsigned int tipo;
    unsigned int larghezza, altezza;   /* tutta la finestra, barra compresa */
    char titolo[MAX_TITOLO];
    unsigned int ruolo;                /* RUOLO_... */
    int x, y;                          /* dove sta: solo per i ruoli speciali */
    unsigned int raggio;               /* gli angoli arrotondati: li usa il compositore per
                                        * l'ombra e il vetro smerigliato sotto */
};

struct chiedi_schermo {
    unsigned int tipo;
};

struct riducimi {
    unsigned int tipo;
};

struct porta_davanti {
    unsigned int tipo;
    unsigned int numero;
};

struct mostra {
    unsigned int tipo;
    unsigned int immagine;             /* 0 o 1 */
};

struct trascinami {
    unsigned int tipo;
};

struct pong {
    unsigned int tipo;
    unsigned int numero;
};

/* --- dal compositore all'applicazione --- */

#define RILASCIATA 1   /* l'immagine 'immagine' e' tornata tua: ci puoi ridisegnare */
#define PUNTATORE  2   /* il puntatore e' in x, y (dentro la finestra), con 'pulsanti'
                        * premuti (PULSANTE_... di chiamate.h) */
#define USCITO     3   /* il puntatore e' uscito dalla finestra */
#define TASTO      4   /* e' stato premuto il tasto 'tasto' (code page 437) */
#define FUOCO      5   /* 'fuoco' = 1: i tasti ora vengono a te; 0: non piu' */
#define PING       6   /* "ci sei?": rispondi PONG con lo stesso 'numero' */
#define FINESTRA_APERTA 7   /* (solo a un PANNELLO) e' comparsa la finestra 'numero', del
                             * programma 'testo' (il suo percorso sul disco) */
#define FINESTRA_CHIUSA 8   /* (solo a un PANNELLO) la finestra 'numero' non c'e' piu' */
#define SCHERMO    9   /* la risposta a CHIEDI_SCHERMO: lo schermo e' x per y pixel */
#define FINESTRA_ATTIVA 10  /* (solo a un PANNELLO) ora ha il fuoco la finestra 'numero'
                             * (0: nessuna) */

struct notizia {
    unsigned int tipo;
    unsigned int immagine;   /* RILASCIATA */
    int x, y;                /* PUNTATORE */
    unsigned int pulsanti;   /* PUNTATORE */
    unsigned int tasto;      /* TASTO */
    unsigned int fuoco;      /* FUOCO */
    unsigned int numero;     /* PING, FINESTRA_APERTA, FINESTRA_CHIUSA */
    char testo[MAX_PERCORSO];   /* FINESTRA_APERTA */
};

#endif
