/* I TASTI FISICI e le SCORCIATOIE da tastiera (Invio, F2, Ctrl+D, Canc, Esc...).
 *
 * I caratteri (le lettere, Invio, "cancella") arrivano come sempre al widget che ha il fuoco
 * (il campo 'tasto' del suo tipo). Le scorciatoie invece non sono caratteri: Ctrl, i tasti
 * funzione, Canc ed Esc il servizio eventi li manda solo come TASTO FISICO (il codice Linux
 * KEY_..., premuto o lasciato: vedi gkit.h, TASTO_FISICO). UIKit li chiede al compositore per
 * ogni sua finestra, tiene il conto dei modificatori (Ctrl, Maiuscole, Alt) e, a ogni tasto
 * premuto, chiama gli ASCOLTATORI della finestra, dall'ultimo aggiunto al primo, finche' uno
 * non risponde 1 ("l'ho usato io"). Cosi' un menu aperto (che si aggiunge quando si apre)
 * sente Esc prima dell'applicazione.
 *
 *     static int scorciatoie(struct finestra_tk *f, unsigned int codice, unsigned int modificatori,
 *                            void *dati) {
 *         if (codice == TASTO_F2 && !modificatori) { rinomina(); return 1; }
 *         return 0;
 *     }
 *     ...
 *     finestra_tk_ascolta_tasti(&finestra, scorciatoie, 0);
 *
 * Mentre si tiene premuto Ctrl (o Alt a sinistra) i caratteri NON arrivano al widget con il
 * fuoco: Ctrl+D non scrive una "d" nel campo di ricerca. Le funzioni sono in uikit.h (hanno
 * bisogno della finestra); qui ci sono i codici e i tipi. */
#ifndef TASTI_H
#define TASTI_H

/* I codici dei tasti (quelli di Linux, KEY_... in linux/input-event-codes.h) che servono alle
 * scorciatoie. Le lettere seguono la tastiera (QWERTY: la posizione, non il carattere). */
#define TASTO_ESC          1
#define TASTO_BACKSPACE   14
#define TASTO_TAB         15
#define TASTO_INVIO       28
#define TASTO_CTRL        29
#define TASTO_MAIUSC      42
#define TASTO_MAIUSC_DX   54
#define TASTO_ALT         56
#define TASTO_F1          59
#define TASTO_F2          60
#define TASTO_CTRL_DX     97
#define TASTO_ALTGR      100
#define TASTO_SU         103
#define TASTO_SINISTRA   105
#define TASTO_DESTRA     106
#define TASTO_GIU        108
#define TASTO_CANC       111
#define TASTO_A           30
#define TASTO_C           46
#define TASTO_D           32
#define TASTO_F           33
#define TASTO_I           23
#define TASTO_N           49
#define TASTO_V           47
#define TASTO_X           45

/* I modificatori premuti insieme al tasto (si sommano). */
#define MOD_CTRL    1u
#define MOD_MAIUSC  2u
#define MOD_ALT     4u

struct finestra_tk;

/* Un ascoltatore: 1 se ha usato il tasto (gli altri non lo sentono), 0 se no. */
typedef int (*ascolta_tasti_tk)(struct finestra_tk *f, unsigned int codice, unsigned int modificatori,
                                void *dati);

#define MAX_ASCOLTATORI 4
struct ascoltatore_tk {
    ascolta_tasti_tk funzione;     /* 0: posto libero */
    void *dati;
};

#endif
