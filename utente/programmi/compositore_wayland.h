/* Il server Wayland del compositore: l'interfaccia tra compositore.c e compositore_wayland.c.
 *
 * Wayland e' il protocollo con cui un programma grafico dice al compositore "ecco la mia
 * finestra": un foglio di memoria condivisa (wl_shm) piu' messaggi su un socket. Il server
 * sta DENTRO il compositore (come in ChromeOS/Exo), non e' un programma a parte: la libreria
 * (libwayland-server, strumenti/compilazione/wayland.sh) legge il socket e chiama le funzioni
 * di compositore_wayland.c, che a sua volta usa il ponte (ponte_*, in compositore.c) per far
 * diventare una finestra Wayland una finestra del compositore come le altre.
 *
 * PASSO 1 della decisione "il compositore passa a Wayland" (SISTEMA.md): Wayland accanto al
 * protocollo di oggi. Ci sono wl_compositor (superfici, commit, frame), wl_shm (ARGB8888 e
 * XRGB8888) e xdg_wm_base (finestre con titolo, configure, chiusura). PASSO 2: wl_seat, con il
 * puntatore (wl_pointer) e la tastiera (wl_keyboard, con la keymap italiana).
 *
 * Questo file non include header di Wayland: compositore.c resta com'e'. */
#ifndef COMPOSITORE_WAYLAND_H
#define COMPOSITORE_WAYLAND_H

#include "gkit/gkit.h"   /* struct notizia */

/* (I posti delle finestre sono quelli di 'finestre' in compositore.c, senza un massimo: il
 * numero del posto e' anche l'indice nella tabella di qui, che cresce.) */

/* ---- quello che Wayland offre al compositore (compositore_wayland.c) ---- */

/* Apre il socket e crea i globali. Il descrittore da guardare nel ciclo, o -1 se non si e'
 * acceso (il compositore va avanti senza Wayland). Il socket e' /run/mshell/wayland-prova: solo
 * per la prova del passo 1, non lo trovano le app dal launcher (SISTEMA.md, "Wayland"). */
int wayland_inizia(void);

/* La connessione Wayland di un'app, dal launcher (PORTA_WAYLAND, protocollo.h): il capo 'd'
 * diventa un client del server, di quel 'programma' (/system/apps/<id>/code): le sue finestre
 * sono di quell'app. 0, o -1 (allora 'd' resta di chi chiama, che lo chiude). */
int wayland_cliente(int d, const char *programma);

/* Il descrittore e' pronto: legge i messaggi dei programmi e li serve. */
void wayland_servi(void);

/* Una notizia per la finestra del posto 'i' (le stesse che avvisa manda alle nostre finestre):
 * FUOCO (la cornice cambia, il programma e' "attivo" e riceve enter/leave della tastiera),
 * PUNTATORE (x, y sono della finestra, cornice compresa: il programma le vede sul contenuto),
 * USCITO e ROTELLA (wl_pointer). TASTO e TASTO_FISICO non servono: ci sono i tasti grezzi. */
void wayland_notizia(int i, const struct notizia *n);

/* Un tasto grezzo (EVENTO_TASTO_GREZZO, corekit/chiamate.h): 'codice' di Linux, 'valore' 1
 * premuto, 0 lasciato, 2 ripetuto (si scarta: ripete il programma), 'mod' i GREZZO_... DOPO il tasto.
 * Va al programma della finestra con il fuoco (wl_keyboard.key e modifiers). */
void wayland_tasto(unsigned int codice, int valore, unsigned int mod);

/* La X della cornice: il programma riceve "chiuditi" (xdg_toplevel.close) e decide lui. */
void wayland_chiedi_chiusura(int i);

/* IL RITMO DEI FOTOGRAMMI. Un programma che anima chiede "dimmi quando disegnare il prossimo"
 * (frame callback) e disegna quando riceve "fatto". Il compositore da' il "fatto" a un tic alla
 * volta (il ciclo di compositore.c): 'ms' e' il tempo del tic.
 * wayland_vuole_fotogramma: c'e' almeno un programma che aspetta, con la finestra visibile (solo
 * allora il ciclo accende l'orologio: se nessuno aspetta, nessun tic).
 * wayland_fotogramma: il "fatto" a quei programmi (le finestre ridotte aspettano) e si
 * spedisce quello che e' in coda. */
int wayland_vuole_fotogramma(void);
void wayland_fotogramma(unsigned int ms);

/* ---- quello che il compositore offre a Wayland (il ponte, in compositore.c) ---- */

/* Un posto per la finestra di un programma Wayland; 'cliente' e' il socket di quel programma.
 * Di chi e': 'programma' se la connessione l'ha data il launcher (wayland_cliente), se no (0)
 * lo dice il kernel dal socket. Il posto, o -1 se la memoria e' finita. */
int ponte_apri(int cliente, const char *programma);

/* L'immagine della finestra del posto 'i' e' pronta o e' cambiata: l x a pixel premoltiplicati
 * (0xAARRGGBB), cornice compresa. La prima volta la finestra compare. 0, o -1 se la memoria e'
 * finita: il compositore ha tolto la finestra (il posto non e' piu' di Wayland: va chiesta la
 * chiusura al programma). */
int ponte_mostra(int i, unsigned int *pixel, int l, int a, const char *titolo);

/* Wayland non ha piu' la finestra del posto 'i': sparisce. */
void ponte_togli(int i);

/* La finestra si vede (non e' ridotta)? */
int ponte_visibile(int i);

/* La misura dello schermo. */
void ponte_schermo(int *larghezza, int *altezza);

#endif
