/* LA USERBAR E IL VASSOIO DELLE APP, dal file Figma ("Userbar", "Vassoio app", "Voce del
 * vassoio"). Qui c'e' solo il DISEGNO e il "su quale voce e' il puntatore": dove stanno sullo
 * schermo, quali app ci sono e cosa fa un clic lo decide il programma della scrivania.
 *
 * LA USERBAR: un'isola di vetro sul bordo SINISTRO dello schermo, a USERBAR_DAL_BORDO (8) dal
 * bordo, centrata in altezza. In colonna: il pulsante App (Primario S, tondo, l'icona della
 * griglia) e sotto le app FISSATE alla barra (Secondario S, tondi, la loro icona), senza
 * nessun contenitore. SPAZIO_4 di margine intorno, SPAZIO_8 tra un pulsante e l'altro; raggio
 * a pillola, fondo SUPERFICIE_VETRO, ombra "barra". Per ora solo le icone delle app: niente
 * ora, stato o notifiche.
 *
 * IL VASSOIO DELLE APP: si apre a DESTRA del pulsante App, a SPAZIO_4 dalla barra, centrato in
 * altezza sul pulsante App (i menu si aprono sempre centrati sull'icona da cui partono). Un
 * pannello (SUPERFICIE_ELEVATA, raggio RAGGIO_PANNELLO, ombra "sovrapposto", SPAZIO_16 di
 * margine) con le app in una griglia di VASSOIO_COLONNE colonne, SPAZIO_8 tra le voci. Ogni
 * voce: un pulsante Secondario L tondo con l'icona e sotto il nome (FONT_PICCOLO_NUOVO).
 *
 * Gli STATI si passano come numeri: la voce sotto il puntatore e quella premuta (-1 nessuna).
 * Nella userbar la voce 0 e' il pulsante App, 1..n le app fissate. */
#ifndef USERBAR_H
#define USERBAR_H

#include "uikit.h"

#define USERBAR_DAL_BORDO   SPAZIO_8
#define USERBAR_MARGINE     SPAZIO_4
#define USERBAR_SPAZIO      SPAZIO_8
#define USERBAR_LARGHEZZA   (ALTEZZA_S + 2 * USERBAR_MARGINE)    /* 40 */

#define VASSOIO_COLONNE     3
#define VASSOIO_MARGINE     SPAZIO_16
#define VASSOIO_SPAZIO      SPAZIO_8
#define VASSOIO_DALLA_BARRA SPAZIO_4
#define VOCE_VASSOIO_LARGA  80
#define VOCE_VASSOIO_ALTA   (ALTEZZA_L + SPAZIO_4 + INTERLINEA_PICCOLO)   /* 48 + 4 + 16 = 68 */

/* --- la userbar --- */

/* Quanto e' alta con 'fissate' app sotto il pulsante App. */
static inline int userbar_altezza(int fissate) {
    int voci = 1 + fissate;
    return 2 * USERBAR_MARGINE + voci * ALTEZZA_S + (voci - 1) * USERBAR_SPAZIO;
}

/* Dove sta, su uno schermo alto 'schermo_a': x e y dell'angolo in alto a sinistra. */
static inline void userbar_posto(int schermo_a, int fissate, int *x, int *y) {
    *x = USERBAR_DAL_BORDO;
    *y = (schermo_a - userbar_altezza(fissate)) / 2;
}

/* Il centro in altezza del pulsante App, rispetto all'alto della userbar (per il vassoio). */
static inline int userbar_centro_app(void) { return USERBAR_MARGINE + ALTEZZA_S / 2; }

/* Un pulsante tondo di UIKit disegnato fuori da una finestra: stesso aspetto, stessi stati. */
static inline void userbar_tondo(struct pennello *p, int x, int y, int tipo, int misura, int icona_,
                                 int sopra, int premuto) {
    struct pulsante b;
    pulsante_prepara(&b, "", icona_, 0, 0);
    b.colore = tipo;
    b.dimensione = misura;
    int lato = misure_pulsante[misura].altezza;
    b.base.x = x; b.base.y = y; b.base.l = lato; b.base.a = lato;
    b.base.sopra = sopra || premuto;
    b.premuto = b.dentro = premuto;
    pulsante_disegna(&b.base, p);
}

/* Disegna la userbar con l'angolo in alto a sinistra in (x, y). 'icone' sono le icone delle
 * app fissate (ICONA_...), 'sopra' e 'premuta' le voci (0 App, 1..n le app; -1 nessuna). */
static inline void userbar_disegna(struct pennello *p, int x, int y, const int *icone, int fissate,
                                   int sopra, int premuta) {
    struct tondo t = tondo(x, y, USERBAR_LARGHEZZA, userbar_altezza(fissate), USERBAR_LARGHEZZA / 2);
    ombre_strati_fuori(p, &t, OMBRA_TAB(barra), OMBRA_N(barra));
    pennello_colore(p, SUPERFICIE_VETRO);
    riempi_forma(p, &t);
    ombre_strati_dentro(p, &t, OMBRA_TAB(barra), OMBRA_N(barra));
    int vx = x + USERBAR_MARGINE, vy = y + USERBAR_MARGINE;
    for (int i = 0; i <= fissate; i++, vy += ALTEZZA_S + USERBAR_SPAZIO)
        userbar_tondo(p, vx, vy, i ? SECONDARIO : PRIMARIO, MISURA_S,
                      i ? icone[i - 1] : ICONA_LAYOUT_GRID, sopra == i, premuta == i);
}

/* La voce della userbar nel punto (px, py) (coordinate come quelle di userbar_disegna): 0 il
 * pulsante App, 1..n le app fissate, -1 nessuna. */
static inline int userbar_voce_sotto(int x, int y, int fissate, int px, int py) {
    int vx = x + USERBAR_MARGINE;
    if (px < vx || px >= vx + ALTEZZA_S) return -1;
    for (int i = 0, vy = y + USERBAR_MARGINE; i <= fissate; i++, vy += ALTEZZA_S + USERBAR_SPAZIO)
        if (py >= vy && py < vy + ALTEZZA_S) return i;
    return -1;
}

/* --- il vassoio delle app --- */

struct voce_vassoio {
    int icona;              /* ICONA_... */
    const char *nome;
};

static inline void vassoio_misura(int quante, int *l, int *a) {
    int colonne = quante < VASSOIO_COLONNE ? (quante ? quante : 1) : VASSOIO_COLONNE;
    int righe = (quante + VASSOIO_COLONNE - 1) / VASSOIO_COLONNE;
    if (righe < 1) righe = 1;
    *l = 2 * VASSOIO_MARGINE + colonne * VOCE_VASSOIO_LARGA + (colonne - 1) * VASSOIO_SPAZIO;
    *a = 2 * VASSOIO_MARGINE + righe * VOCE_VASSOIO_ALTA + (righe - 1) * VASSOIO_SPAZIO;
}

/* Dove si apre, accanto a una userbar che sta in (bx, by): a destra, centrato in altezza sul
 * pulsante App. */
static inline void vassoio_posto(int bx, int by, int quante, int *x, int *y) {
    int l, a;
    vassoio_misura(quante, &l, &a);
    *x = bx + USERBAR_LARGHEZZA + VASSOIO_DALLA_BARRA;
    *y = by + userbar_centro_app() - a / 2;
}

/* L'angolo della voce 'i', dentro un vassoio in (x, y). */
static inline void vassoio_voce_posto(int x, int y, int i, int *vx, int *vy) {
    *vx = x + VASSOIO_MARGINE + (i % VASSOIO_COLONNE) * (VOCE_VASSOIO_LARGA + VASSOIO_SPAZIO);
    *vy = y + VASSOIO_MARGINE + (i / VASSOIO_COLONNE) * (VOCE_VASSOIO_ALTA + VASSOIO_SPAZIO);
}

static inline void vassoio_disegna(struct pennello *p, int x, int y, const struct voce_vassoio *voci,
                                   int quante, int sopra, int premuta) {
    int l, a;
    vassoio_misura(quante, &l, &a);
    struct tondo t = tondo(x, y, l, a, RAGGIO_PANNELLO);
    ombre_strati_fuori(p, &t, OMBRA_TAB(sovrapposto), OMBRA_N(sovrapposto));
    pennello_colore(p, SUPERFICIE_ELEVATA);
    riempi_forma(p, &t);
    ombre_strati_dentro(p, &t, OMBRA_TAB(sovrapposto), OMBRA_N(sovrapposto));
    for (int i = 0; i < quante; i++) {
        int vx, vy;
        vassoio_voce_posto(x, y, i, &vx, &vy);
        userbar_tondo(p, vx + (VOCE_VASSOIO_LARGA - ALTEZZA_L) / 2, vy, SECONDARIO, MISURA_L,
                      voci[i].icona, sopra == i, premuta == i);
        pennello_colore(p, TESTO_PRIMARIO);
        int nl = larghezza_testo(FONT_PICCOLO_NUOVO, voci[i].nome);
        testo_centrato(p, FONT_PICCOLO_NUOVO, vx + (VOCE_VASSOIO_LARGA - nl) / 2,
                       vy + ALTEZZA_L + SPAZIO_4, INTERLINEA_PICCOLO, voci[i].nome);
    }
}

/* La voce del vassoio nel punto (px, py), o -1. Conta tutta la voce (pulsante e nome). */
static inline int vassoio_voce_sotto(int x, int y, int quante, int px, int py) {
    for (int i = 0; i < quante; i++) {
        int vx, vy;
        vassoio_voce_posto(x, y, i, &vx, &vy);
        if (px >= vx && px < vx + VOCE_VASSOIO_LARGA && py >= vy && py < vy + VOCE_VASSOIO_ALTA) return i;
    }
    return -1;
}

#endif
