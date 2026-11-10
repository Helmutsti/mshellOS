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
 *     e' bloccata: il compositore oscura la finestra e ci mette sopra un pulsante di
 *     chiusura d'emergenza. Come GNOME e il Mac.
 *
 * Una finestra per collegamento: per due finestre, due collegamenti.
 * Ogni messaggio comincia con il suo tipo. */
#ifndef PROTOCOLLO_H
#define PROTOCOLLO_H

#define NOME_COMPOSITORE "compositore"   /* il nome su cui ascolta */
#define MAX_TITOLO 32

/* I pixel nelle immagini: 4 byte ciascuno, 0x00RRGGBB, riga dopo riga, senza spazi
 * tra una riga e l'altra. L'immagine k sta nel foglio a k * larghezza * altezza * 4. */

/* --- dall'applicazione al compositore --- */

#define VOGLIO_FINESTRA 1   /* il primo messaggio, con il foglio allegato */
#define MOSTRA          2   /* "l'immagine 'immagine' e' pronta: mostrala" */
#define TRASCINAMI      3   /* "hanno premuto sulla mia barra: spostami tu". Vale solo
                             * mentre il pulsante premuto dentro la finestra e' ancora
                             * giu'; se e' gia' stato lasciato, il compositore la ignora */
#define PONG            4   /* "ci sono": la risposta al PING 'numero' */

struct voglio_finestra {
    unsigned int tipo;
    unsigned int larghezza, altezza;   /* tutta la finestra, barra compresa */
    char titolo[MAX_TITOLO];
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

struct notizia {
    unsigned int tipo;
    unsigned int immagine;   /* RILASCIATA */
    int x, y;                /* PUNTATORE */
    unsigned int pulsanti;   /* PUNTATORE */
    unsigned int tasto;      /* TASTO */
    unsigned int fuoco;      /* FUOCO */
    unsigned int numero;     /* PING */
};

#endif
