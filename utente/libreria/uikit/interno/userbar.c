/* UIKit, del sistema: il disegno della userbar e del vassoio delle app, per la scrivania. Le
 * dichiarazioni sono in sistema/userbar.h. */
#include "uikit/uikit.h"
#include "uikit/interno/nucleo.h"
#include "uikit/sistema/userbar.h"

int userbar_altezza(int fissate) {
    int voci = 1 + fissate;
    return 2 * USERBAR_MARGINE + voci * ALTEZZA_S + (voci - 1) * USERBAR_SPAZIO;
}

void userbar_posto(int schermo_a, int fissate, int *x, int *y) {
    *x = USERBAR_DAL_BORDO;
    *y = (schermo_a - userbar_altezza(fissate)) / 2;
}

/* Il centro in altezza del pulsante App, rispetto all'alto della userbar (per il vassoio). */
static int userbar_centro_app(void) { return USERBAR_MARGINE + ALTEZZA_S / 2; }

/* Un pulsante tondo di UIKit disegnato fuori da una finestra: stesso aspetto, stessi stati.
 * Con 'immagine' l'icona e' quella: tonda al posto del pulsante (USERBAR_ICONE_TONDE 1) o al
 * centro del pulsante (0); se no il simbolo 'icona_' nel pulsante. */
static void userbar_tondo(struct pennello *p, int x, int y, int tipo, int misura, int icona_,
                          const struct immagine *immagine, int sopra, int premuto) {
    int lato = misure_pulsante[misura].altezza;
#if USERBAR_ICONE_TONDE
    if (immagine) {
        disegna_immagine_tonda(p, immagine, x, y, lato);
        if (sopra || premuto) {
            int d = immagine->l < immagine->a ? immagine->l : immagine->a;
            pennello_colore(p, premuto ? USERBAR_VELO_PREMUTA : USERBAR_VELO_SOPRA);
            riempi_tondo(p, x + (lato - d) / 2, y + (lato - d) / 2, d, d, d / 2);
        }
        return;
    }
#endif
    struct pulsante b;
    pulsante_prepara(&b, "", immagine ? NESSUNA_ICONA : icona_, 0, 0);
    b.colore = tipo;
    b.dimensione = misura;
    b.base.x = x; b.base.y = y; b.base.l = lato; b.base.a = lato;
    b.base.sopra = sopra || premuto;
    b.premuto = b.dentro = premuto;
    pulsante_disegna(&b.base, p);
    if (immagine) disegna_immagine(p, immagine, x + (lato - immagine->l) / 2, y + (lato - immagine->a) / 2);
}

void userbar_ombra(struct pennello *p, int x, int y, int fissate) {
    struct tondo t = tondo(x, y, USERBAR_LARGHEZZA, userbar_altezza(fissate), USERBAR_RAGGIO);
    ombre_strati_fuori(p, &t, OMBRA_TAB(barra), OMBRA_N(barra));
}

void userbar_disegna(struct pennello *p, int x, int y, const struct voce_app *app,
                     const int *stati, int fissate, int sopra, int premuta) {
    struct tondo t = tondo(x, y, USERBAR_LARGHEZZA, userbar_altezza(fissate), USERBAR_RAGGIO);
    pennello_colore(p, SUPERFICIE_VETRO);
    riempi_forma(p, &t);
    ombre_strati_dentro(p, &t, OMBRA_TAB(barra), OMBRA_N(barra));
    int vx = x + USERBAR_MARGINE, vy = y + USERBAR_MARGINE;
    for (int i = 0; i <= fissate; i++, vy += ALTEZZA_S + USERBAR_SPAZIO) {
        userbar_tondo(p, vx, vy, i ? SECONDARIO : PRIMARIO, MISURA_S,
                      i ? app[i - 1].icona : ICONA_LAYOUT_GRID, i ? app[i - 1].immagine : 0,
                      sopra == i, premuta == i);
        int stato = i && stati ? stati[i - 1] : APP_CHIUSA;
        if (stato == APP_CHIUSA) continue;
        int alto = stato == APP_ATTIVA ? USERBAR_SEGNO_ATTIVA : USERBAR_SEGNO_APERTA;
        pennello_colore(p, TESTO_PRIMARIO);
        riempi_tondo(p, x + (USERBAR_MARGINE - USERBAR_SEGNO_LARGO) / 2, vy + (ALTEZZA_S - alto) / 2,
                     USERBAR_SEGNO_LARGO, alto, USERBAR_SEGNO_LARGO / 2);
    }
}

int userbar_voce_sotto(int x, int y, int fissate, int px, int py) {
    int vx = x + USERBAR_MARGINE;
    if (px < vx || px >= vx + ALTEZZA_S) return -1;
    for (int i = 0, vy = y + USERBAR_MARGINE; i <= fissate; i++, vy += ALTEZZA_S + USERBAR_SPAZIO)
        if (py >= vy && py < vy + ALTEZZA_S) return i;
    return -1;
}

void vassoio_misura(int quante, int *l, int *a) {
    int colonne = quante < VASSOIO_COLONNE ? (quante ? quante : 1) : VASSOIO_COLONNE;
    int righe = (quante + VASSOIO_COLONNE - 1) / VASSOIO_COLONNE;
    if (righe < 1) righe = 1;
    *l = 2 * VASSOIO_MARGINE + colonne * VOCE_VASSOIO_LARGA + (colonne - 1) * VASSOIO_SPAZIO;
    *a = 2 * VASSOIO_MARGINE + righe * VOCE_VASSOIO_ALTA + (righe - 1) * VASSOIO_SPAZIO;
}

void vassoio_posto(int bx, int by, int quante, int *x, int *y) {
    int l, a;
    vassoio_misura(quante, &l, &a);
    *x = bx + USERBAR_LARGHEZZA + VASSOIO_DALLA_BARRA;
    *y = by + userbar_centro_app() - a / 2;
}

/* L'angolo della voce 'i', dentro un vassoio in (x, y). */
static void vassoio_voce_posto(int x, int y, int i, int *vx, int *vy) {
    *vx = x + VASSOIO_MARGINE + (i % VASSOIO_COLONNE) * (VOCE_VASSOIO_LARGA + VASSOIO_SPAZIO);
    *vy = y + VASSOIO_MARGINE + (i / VASSOIO_COLONNE) * (VOCE_VASSOIO_ALTA + VASSOIO_SPAZIO);
}

void vassoio_ombra(struct pennello *p, int x, int y, int quante) {
    int l, a;
    vassoio_misura(quante, &l, &a);
    struct tondo t = tondo(x, y, l, a, VASSOIO_RAGGIO);
    ombre_strati_fuori(p, &t, OMBRA_TAB(sovrapposto), OMBRA_N(sovrapposto));
}

void vassoio_disegna(struct pennello *p, int x, int y, const struct voce_app *voci,
                     int quante, int sopra, int premuta) {
    int l, a;
    vassoio_misura(quante, &l, &a);
    struct tondo t = tondo(x, y, l, a, VASSOIO_RAGGIO);
    pennello_colore(p, SUPERFICIE_ELEVATA);
    riempi_forma(p, &t);
    ombre_strati_dentro(p, &t, OMBRA_TAB(sovrapposto), OMBRA_N(sovrapposto));
    for (int i = 0; i < quante; i++) {
        int vx, vy;
        vassoio_voce_posto(x, y, i, &vx, &vy);
        userbar_tondo(p, vx + (VOCE_VASSOIO_LARGA - ALTEZZA_L) / 2, vy, SECONDARIO, MISURA_L,
                      voci[i].icona, voci[i].immagine, sopra == i, premuta == i);
        pennello_colore(p, TESTO_PRIMARIO);
        int nl = larghezza_testo(FONT_PICCOLO_NUOVO, voci[i].nome);
        testo_centrato(p, FONT_PICCOLO_NUOVO, vx + (VOCE_VASSOIO_LARGA - nl) / 2,
                       vy + ALTEZZA_L + SPAZIO_4, INTERLINEA_PICCOLO, voci[i].nome);
    }
}

int vassoio_voce_sotto(int x, int y, int quante, int px, int py) {
    for (int i = 0; i < quante; i++) {
        int vx, vy;
        vassoio_voce_posto(x, y, i, &vx, &vy);
        if (px >= vx && px < vx + VOCE_VASSOIO_LARGA && py >= vy && py < vy + VOCE_VASSOIO_ALTA) return i;
    }
    return -1;
}
