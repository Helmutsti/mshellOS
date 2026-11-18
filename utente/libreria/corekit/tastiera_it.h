/* CoreKit: LA TASTIERA ITALIANA, dal tasto premuto (codice di Linux, KEY_...) al carattere.
 *
 * Un solo codice per due che traducono i tasti (decisione della persona, 18/11/2026: la tabella
 * nostra, non libxkbcommon):
 *   - il servizio eventi (utente/programmi/eventi.c), che legge la tastiera vera;
 *   - GKit come client Wayland, che riceve dal compositore solo il codice del tasto e i
 *     modificatori (wl_keyboard.key e .modifiers) e il carattere se lo fa da se'.
 * La tabella e' quella di sempre (Maiusc, AltGr, Bloc Maiusc, Bloc Num e il tastierino); il
 * lavoro e' in interno/tastiera_it.c.
 *
 * Uso: una struct tastiera_it per chi traduce, tastiera_it_inizia una volta, poi un
 * tastiera_it_tasto per ogni tasto, nell'ordine in cui arrivano. */
#ifndef COREKIT_TASTIERA_IT_H
#define COREKIT_TASTIERA_IT_H

#define TASTIERA_IT_TASTIERINO 84   /* KEY_KPDOT + 1 */

struct tastiera_it {
    int maiusc;             /* Maiusc tenuto giu' (uno dei due) */
    int altgr;              /* AltGr (KEY_RIGHTALT) tenuto giu' */
    int bloc_maiusc;        /* Bloc Maiusc acceso */
    int bloc_num;           /* Bloc Num acceso (parte acceso, come su Windows e Mac) */
    /* della libreria: cosa ha dato ogni tasto del tastierino quando e' stato premuto, perche'
     * ripetizione e rilascio facciano lo stesso anche se nel frattempo Bloc Num e' cambiato */
    unsigned short tastierino[TASTIERA_IT_TASTIERINO];
    unsigned int maiusc_giu;   /* (della libreria) quali dei due Maiusc sono giu' */
};

/* Tutto spento tranne Bloc Num. */
void tastiera_it_inizia(struct tastiera_it *t);

/* Un tasto: 'codice' di Linux, 'valore' 1 premuto, 2 ripetuto, 0 lasciato. Aggiorna lo stato
 * (Maiusc, AltGr; i blocchi cambiano quando si premono) e dice cosa ne viene:
 *   *fisico    il tasto "fisico" da dare a chi vuole i tasti (frecce, Canc, F1-F12, Esc...):
 *              il codice stesso, oppure, per il tastierino con Bloc Num spento, il tasto di
 *              movimento (KEY_KP8 -> KEY_UP); 0 se non ne viene niente (il 5 con Bloc Num spento);
 *   *carattere il numero Unicode da scrivere (solo alla pressione e alla ripetizione), 0 se il
 *              tasto non scrive (modificatori, frecce, rilasci...).
 * Esattamente quello che faceva eventi.c fino al 18/11/2026. */
void tastiera_it_tasto(struct tastiera_it *t, unsigned int codice, int valore, unsigned int *fisico,
                       unsigned int *carattere);

/* Allinea lo stato a quello che dice un altro (per GKit: wl_keyboard.modifiers, all'arrivo del
 * fuoco e dopo ogni tasto che li cambia), in bit GREZZO_... (corekit/chiamate.h): vale per
 * Maiusc, AltGr, Bloc Maiusc e Bloc Num. */
void tastiera_it_modificatori(struct tastiera_it *t, unsigned int grezzo);

#endif
