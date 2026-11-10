/* esempio: la prima applicazione con una finestra vera.
 *
 * Disegna da sola TUTTA la sua finestra, pixel per pixel, nella tela che le da' la
 * libreria delle finestre: il bordo, la barra del titolo (blu quando ha il fuoco, grigia
 * quando no) e il contenuto. Non usa un toolkit, quindi si fa tutto da sola: anche
 * accorgersi che si e' premuto sulla barra e chiedere al compositore di trascinarla.
 * Un clic nel contenuto mette un quadretto dove si e' cliccato; un tasto cambia il
 * colore di sfondo.
 *
 * OGNI VOLTA SI RIDISEGNA TUTTO. Le tele sono due e si alternano: quella che si riceve
 * contiene il disegno di DUE volte fa, non dell'ultima. Per questo l'applicazione tiene
 * il suo stato (i quadretti, il colore, il fuoco) e ridisegna tutto da capo. */
#include "finestra.h"

#define LARGHEZZA 322
#define ALTEZZA   223
#define BARRA     23      /* altezza della barra, bordo di sopra compreso */
#define LATO      10
#define MAX_QUADRETTI 64

static const unsigned int sfondi[] = { 0xF4E8C8, 0xC8E8F4, 0xD8F4C8, 0xF4C8D8 };
static int sfondo, fuoco;
static int qx[MAX_QUADRETTI], qy[MAX_QUADRETTI], quadretti;

static void rettangolo(unsigned int *tela, int x, int y, int l, int a, unsigned int colore) {
    for (int r = y; r < y + a; r++)
        for (int k = x; k < x + l; k++)
            if (k >= 0 && k < LARGHEZZA && r >= 0 && r < ALTEZZA) tela[r * LARGHEZZA + k] = colore;
}

static void disegna(unsigned int *tela) {
    rettangolo(tela, 0, 0, LARGHEZZA, ALTEZZA, 0x202020);                          /* il bordo */
    rettangolo(tela, 1, 1, LARGHEZZA - 2, BARRA - 1, fuoco ? 0x3A6EA5 : 0x707880); /* la barra */
    rettangolo(tela, 1, BARRA, LARGHEZZA - 2, ALTEZZA - BARRA - 1, sfondi[sfondo]);
    for (int q = 0; q < quadretti; q++)
        rettangolo(tela, qx[q], qy[q], LATO, LATO, 0x803030);
}

static int ridisegna(struct finestra *f) {
    unsigned int *tela = finestra_tela(f);
    if (!tela) return 0;
    disegna(tela);
    return finestra_mostra(f) >= 0;
}

int main(void) {
    struct finestra f;
    if (finestra_apri(&f, LARGHEZZA, ALTEZZA, "esempio") < 0) {
        scrivi_testo("esempio: non riesco ad aprire la finestra\n");
        return 0;
    }
    if (!ridisegna(&f)) return 0;

    unsigned int pulsanti_prima = 0;
    struct notizia n;
    while (finestra_notizia(&f, &n)) {
        int cambiato = 0;
        if (n.tipo == PUNTATORE) {
            int clic = (n.pulsanti & PULSANTE_SINISTRO) && !(pulsanti_prima & PULSANTE_SINISTRO);
            pulsanti_prima = n.pulsanti;
            if (clic && n.y < BARRA) {
                finestra_trascinami(&f);   /* sulla barra: ci sposta il compositore */
            } else if (clic && quadretti < MAX_QUADRETTI) {
                qx[quadretti] = n.x - LATO / 2;
                qy[quadretti] = n.y - LATO / 2;
                quadretti++;
                cambiato = 1;
            }
        } else if (n.tipo == USCITO) {
            pulsanti_prima = 0;   /* i pulsanti si sapranno di nuovo quando rientra */
        } else if (n.tipo == TASTO) {
            sfondo = (sfondo + 1) % (int)(sizeof sfondi / sizeof sfondi[0]);
            cambiato = 1;
        } else if (n.tipo == FUOCO) {
            fuoco = n.fuoco;
            cambiato = 1;
        }
        if (cambiato && !ridisegna(&f)) return 0;
    }
    return 0;   /* il compositore ha chiuso */
}
