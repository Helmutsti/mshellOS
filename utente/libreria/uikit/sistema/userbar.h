/* UIKit, DEL SISTEMA (vedi uikit.h): la userbar e il vassoio delle app li disegna solo la
 * scrivania. Un'app non include questo file: puo' cambiare senza avviso.
 *
 * LA USERBAR E IL VASSOIO DELLE APP, dal file Figma ("Userbar", "Vassoio app", "Voce del
 * vassoio", pagina "Scrivania"). Qui c'e' solo il DISEGNO e il "su quale voce e' il
 * puntatore": dove stanno sullo schermo, quali app ci sono e cosa fa un clic lo decide il
 * programma della scrivania.
 *
 * LA USERBAR: una pillola di vetro sul bordo SINISTRO dello schermo, a USERBAR_DAL_BORDO (8) dal
 * bordo, centrata in altezza. In colonna: il pulsante App (Primario S, tondo, l'icona della
 * griglia) e sotto le app FISSATE alla barra (Secondario S, tondi, la loro icona), senza
 * nessun contenitore. SPAZIO_4 di margine intorno, SPAZIO_8 tra un pulsante e l'altro; raggio
 * a pillola, fondo SUPERFICIE_VETRO, ombra "barra". Per ora solo le icone delle app: niente
 * ora, stato o notifiche.
 *
 * IL SEGNO DELLE APP APERTE (il disegno non lo mostra; scelta nostra, con i token): una
 * lineetta verticale TESTO_PRIMARIO larga 2 nel margine a sinistra del pulsante, come il
 * puntino del Dock del Mac quando sta a sinistra. Alta 4 (un puntino) se l'app e' aperta,
 * USERBAR_SEGNO_ATTIVA (14) se e' quella in primo piano.
 *
 * IL VASSOIO DELLE APP: si apre a DESTRA del pulsante App, a SPAZIO_4 dalla barra, centrato in
 * altezza sul pulsante App (i menu si aprono sempre centrati sull'icona da cui partono). Un
 * pannello (SUPERFICIE_ELEVATA, raggio RAGGIO_PANNELLO, ombra "sovrapposto", SPAZIO_16 di
 * margine) con le app in una griglia di VASSOIO_COLONNE colonne, SPAZIO_8 tra le voci. Ogni
 * voce: un pulsante Secondario L tondo con l'icona e sotto il nome (FONT_PICCOLO_NUOVO).
 *
 * LE ICONE DELLE APP: l'icona vera dell'app (una PNG del suo pacchetto, gia' caricata da chi
 * disegna: 'immagine'). Se l'app non ne ha una, il simbolo 'icona' (ICONA_..., del sistema),
 * nel pulsante tondo. Due modi, con USERBAR_ICONE_TONDE qui sotto:
 *   1 (scelto per ora): l'icona e' TONDA, ritagliata a cerchio, e riempie il posto del
 *     pulsante: niente cerchio dietro. Sopra il puntatore e premuta si scurisce con un velo
 *     tondo trasparente. Serve la PNG di 32 pixel nella
 *     userbar e quella di 48 nel vassoio (le icone delle app le hanno: se manca si usa la
 *     piu' vicina, al suo posto e centrata, senza ingrandirla).
 *   0 (il vecchio): l'icona piccola (16 nella userbar, 20 nel vassoio) dentro il pulsante tondo.
 *
 * LE OMBRE ESTERNE. Sulla scrivania la userbar e il vassoio sono pannelli del compositore
 * (gkit/sistema/protocollo.h), che disegna lui le ombre fuori dalla forma, come per le finestre: userbar_disegna
 * e vassoio_disegna disegnano solo la forma e quello che c'e' dentro (con le ombre interne).
 * Chi li disegna su una tela sua (l'anteprima) chiama prima userbar_ombra e vassoio_ombra.
 *
 * Gli STATI si passano come numeri: la voce sotto il puntatore e quella premuta (-1 nessuna).
 * Nella userbar la voce 0 e' il pulsante App, 1..n le app fissate.
 *
 * Il codice e' in interno/userbar.c. */
#ifndef USERBAR_H
#define USERBAR_H

#include "uikit/uikit.h"

/* UN SOLO INTERRUTTORE tra le due maniere (vedi in cima): 1 icone tonde, 0 icona piccola nel
 * cerchio. Lo legge anche la scrivania per scegliere la misura della PNG da caricare. */
#ifndef USERBAR_ICONE_TONDE
#define USERBAR_ICONE_TONDE 1
#endif
#if USERBAR_ICONE_TONDE
#define USERBAR_PNG_BARRA   ALTEZZA_S
#define USERBAR_PNG_VASSOIO ALTEZZA_L
#else
#define USERBAR_PNG_BARRA   ICONA_S
#define USERBAR_PNG_VASSOIO ICONA_L
#endif

/* Il velo sull'icona tonda: nero trasparente (alfa 0x29 = 16%, 0x4D = 30%) */
#define USERBAR_VELO_SOPRA  0x29000000u
#define USERBAR_VELO_PREMUTA 0x4D000000u

#define USERBAR_DAL_BORDO   SPAZIO_8
#define USERBAR_MARGINE     SPAZIO_4
#define USERBAR_SPAZIO      SPAZIO_8
#define USERBAR_LARGHEZZA   (ALTEZZA_S + 2 * USERBAR_MARGINE)    /* 40 */
#define USERBAR_RAGGIO      (USERBAR_LARGHEZZA / 2)              /* a pillola */
#define USERBAR_SEGNO_LARGO 2
#define USERBAR_SEGNO_APERTA 4
#define USERBAR_SEGNO_ATTIVA 14

#define VASSOIO_COLONNE     3
#define VASSOIO_MARGINE     SPAZIO_16
#define VASSOIO_SPAZIO      SPAZIO_8
#define VASSOIO_DALLA_BARRA SPAZIO_4
#define VASSOIO_RAGGIO      RAGGIO_PANNELLO
#define VOCE_VASSOIO_LARGA  80
#define VOCE_VASSOIO_ALTA   (ALTEZZA_L + SPAZIO_4 + INTERLINEA_PICCOLO)   /* 48 + 4 + 16 = 68 */

/* Un'app nella userbar o nel vassoio. */
struct voce_app {
    int icona;                          /* ICONA_...: se non c'e' l'immagine */
    const struct immagine *immagine;    /* l'icona vera, o 0 */
    const char *nome;                   /* nel vassoio, sotto il pulsante */
};

/* Lo stato di un'app fissata, per il segno a sinistra */
#define APP_CHIUSA  0
#define APP_APERTA  1
#define APP_ATTIVA  2   /* aperta e in primo piano */

/* --- la userbar --- */

/* Quanto e' alta con 'fissate' app sotto il pulsante App. */
int userbar_altezza(int fissate);

/* Dove sta, su uno schermo alto 'schermo_a': x e y dell'angolo in alto a sinistra. */
void userbar_posto(int schermo_a, int fissate, int *x, int *y);

/* Le ombre esterne della userbar (solo fuori da un pannello del compositore, vedi in cima). */
void userbar_ombra(struct pennello *p, int x, int y, int fissate);

/* Disegna la userbar con l'angolo in alto a sinistra in (x, y). 'app' sono le app fissate,
 * 'stati' il loro APP_... (0 se non serve il segno), 'sopra' e 'premuta' le voci (0 App,
 * 1..n le app; -1 nessuna). */
void userbar_disegna(struct pennello *p, int x, int y, const struct voce_app *app,
                     const int *stati, int fissate, int sopra, int premuta);

/* La voce della userbar nel punto (px, py) (coordinate come quelle di userbar_disegna): 0 il
 * pulsante App, 1..n le app fissate, -1 nessuna. */
int userbar_voce_sotto(int x, int y, int fissate, int px, int py);

/* --- il vassoio delle app --- */

/* Quanto e' largo (l) e alto (a) il vassoio con 'quante' app: VASSOIO_COLONNE per riga,
 * almeno una riga e una colonna anche se e' vuoto. */
void vassoio_misura(int quante, int *l, int *a);

/* Dove si apre, accanto a una userbar che sta in (bx, by): a destra, centrato in altezza sul
 * pulsante App. */
void vassoio_posto(int bx, int by, int quante, int *x, int *y);

/* Le ombre esterne del vassoio (solo fuori da un pannello del compositore, vedi in cima). */
void vassoio_ombra(struct pennello *p, int x, int y, int quante);

/* Disegna il vassoio con l'angolo in alto a sinistra in (x, y): le 'quante' app in 'voci',
 * 'sopra' e 'premuta' le voci (-1 nessuna). */
void vassoio_disegna(struct pennello *p, int x, int y, const struct voce_app *voci,
                     int quante, int sopra, int premuta);

/* La voce del vassoio nel punto (px, py), o -1. Conta tutta la voce (pulsante e nome). */
int vassoio_voce_sotto(int x, int y, int quante, int px, int py);

#endif
