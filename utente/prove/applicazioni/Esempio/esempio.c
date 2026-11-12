/* esempio: la prima applicazione con una finestra vera.
 *
 * Disegna da sola TUTTA la sua finestra, con gli strumenti di disegno di GKit, nella tela
 * che le da' la finestra di GKit: la barra con il titolo (blu quando ha il
 * fuoco, grigia quando no) e il contenuto. Non usa UIKit, quindi si fa tutto da
 * sola: anche accorgersi che si e' premuto sulla barra e chiedere al compositore di
 * trascinarla.
 * Un clic nel contenuto mette un quadretto dove si e' cliccato; un tasto cambia il
 * colore di sfondo.
 *
 * OGNI VOLTA SI RIDISEGNA TUTTO. Le tele sono due e si alternano: quella che si riceve
 * contiene il disegno di DUE volte fa, non dell'ultima. Per questo l'applicazione tiene
 * il suo stato (i quadretti, il colore, il fuoco) e ridisegna tutto da capo. */
/* Solo GKit (la finestra e gli strumenti di disegno), piu' il font del titolo letto con GKit
 * dalle risorse del sistema (/system/resources/fonts): niente stile, temi, materiali o
 * componenti di UIKit. */
#include "gkit/gkit.h"
#include "gkit/gkit_disegno.h"

#define LARGHEZZA 320
#define ALTEZZA   220
#define BARRA     30      /* altezza della barra */
#define LATO      10
#define MAX_QUADRETTI 64
#define TITOLO    "esempio"

static const unsigned int sfondi[] = { 0xF4E8C8, 0xC8E8F4, 0xD8F4C8, 0xF4C8D8 };
static int sfondo, fuoco;
static int qx[MAX_QUADRETTI], qy[MAX_QUADRETTI], quadretti;
static struct font font_titolo;   /* se il file manca: il font di riserva di GKit */

/* Il suo aspetto lo decide lei, e non e' quello del sistema: carta opaca, barra
 * colorata, titolo a sinistra. (Gli angoli arrotondati come quelli del sistema, perche'
 * l'ombra intorno la disegna il compositore con quella forma.) */
static void disegna(unsigned int *pixel) {
    struct tela t = { pixel, LARGHEZZA, ALTEZZA };
    struct pennello p;
    pennello_prepara(&p, &t);
    pulisci(&p);

    struct tondo tutta = tondo(0, 0, LARGHEZZA, ALTEZZA, 12);
    struct tondo barra = { 0, 0, LARGHEZZA, BARRA, { 12, 12, 0, 0 } };
    pennello_colore(&p, OPACO(sfondi[sfondo]));
    riempi_forma(&p, &tutta);
    pennello_colore(&p, OPACO(fuoco ? 0x3A6EA5 : 0x707880));
    riempi_forma(&p, &barra);
    pennello_colore(&p, OPACO(0xFFFFFF));
    testo(&p, &font_titolo, 14, (BARRA - font_titolo.altezza) / 2, TITOLO);

    /* il contenuto: ritagliato, cosi' i quadretti vicino al bordo non lo coprono */
    pennello_ritaglia(&p, 0, BARRA, LARGHEZZA, ALTEZZA - BARRA);
    pennello_colore(&p, OPACO(0x803030));
    for (int q = 0; q < quadretti; q++) riempi_tondo(&p, qx[q], qy[q], LATO, LATO, 3);
}

static int ridisegna(struct finestra *f) {
    unsigned int *tela = finestra_tela(f);
    if (!tela) return 0;
    disegna(tela);
    return finestra_mostra(f) >= 0;
}

int main(void) {
    char percorso[PERCORSO_RISORSA_MAX];
    percorso_risorsa(percorso, sizeof percorso, "fonts/inter-semibold-13.font");
    font_da_file(percorso, &font_titolo);

    struct finestra f;
    if (finestra_apri(&f, LARGHEZZA, ALTEZZA, TITOLO) < 0) {
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
