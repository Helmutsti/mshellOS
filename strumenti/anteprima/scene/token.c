/* I token del nuovo design (utente/libreria/tema.h e stile.h) nei due temi, uno accanto
 * all'altro: per OGNI colore semantico un campione col suo nome. Serve a vedere a occhio che
 * le tabelle dei temi (uikit/interno/tema.c) somigliano al disegno. Sotto i campioni, le
 * ombre (stile.h: le tabelle di struct strato_ombra, in uikit/interno/stile.c), una per
 * livello di profondita'.
 *
 * Il tema di partenza non conta: la scena imposta tema_corrente da sola, una volta per
 * pannello, e disegna con le MACRO (SUPERFICIE_SCENA, TESTO_PRIMARIO...), cosi' prova anche
 * quelle. Uso: anteprima.sh token */
#include "uikit/uikit.h"

/* nome e colore (letto dal tema in uso) di ogni token: stesso ordine del JSON */
#define TOKEN(n) { #n, n }
struct campione { const char *nome; unsigned int colore; };
#define TUTTI(X) \
    X(SUPERFICIE_SCENA) X(SUPERFICIE_ELEVATA) X(SUPERFICIE_VETRO) X(SUPERFICIE_INCASSATA) \
    X(CONTROLLO_SFONDO_NUOVO) X(CONTROLLO_SFONDO_SOPRA) X(CONTROLLO_SFONDO_PREMUTO) \
    X(CONTROLLO_SFONDO_DISABILITATO) X(AZIONE_SFONDO) X(AZIONE_SFONDO_SOPRA) \
    X(AZIONE_SFONDO_PREMUTO) X(AZIONE_TESTO) X(TESTO_PRIMARIO) X(TESTO_SECONDARIO) \
    X(TESTO_DISABILITATO) X(TESTO_INVERSO) X(BORDO_CONTROLLO) X(BORDO_SEPARATORE) X(BORDO_FOCUS) \
    X(CURSORE_RIEMPIMENTO) X(CURSORE_CONTORNO) X(OGGETTO_RIEMPIMENTO) X(OGGETTO_RIEMPIMENTO_FORTE) \
    X(OGGETTO_TRATTO) X(SCORRIMENTO_CURSORE) X(SCORRIMENTO_CURSORE_SOPRA) \
    X(SCORRIMENTO_CURSORE_TRASCINATO_NUOVO) X(SUGGERIMENTO_SFONDO) X(SUGGERIMENTO_TESTO) \
    X(SUGGERIMENTO_TESTO_SECONDARIO)

#define COL_L   300      /* una cella: campione + nome */
#define RIGA_H   28
#define PAN_M    16
#define PAN_L   (PAN_M * 2 + COL_L * 2)

static void ombre(struct pennello *p, int x0, int y0, const struct tema *t);

static void pannello(struct pennello *p, int x0, int y0, const struct tema *t, const char *titolo, int altezza) {
    tema_corrente = t;
    pennello_colore(p, SUPERFICIE_SCENA);
    riempi_rettangolo(p, x0, y0, PAN_L, altezza);
    pennello_colore(p, TESTO_PRIMARIO);
    testo(p, FONT_TITOLO_NUOVO, x0 + PAN_M, y0 + 12, titolo);

#define VOCE(N) { #N, N },
    struct campione v[] = { TUTTI(VOCE) };
#undef VOCE
    int n = (int)(sizeof v / sizeof v[0]), meta = (n + 1) / 2;
    for (int i = 0; i < n; i++) {
        int x = x0 + PAN_M + (i / meta) * COL_L, y = y0 + 48 + (i % meta) * RIGA_H;
        /* sotto il campione una scacchiera nera/bianca: si vede se e' trasparente */
        pennello_colore(p, OPACO(0xFFFFFF)); riempi_rettangolo(p, x, y, 48, 10);
        pennello_colore(p, OPACO(0x000000)); riempi_rettangolo(p, x, y + 10, 48, 10);
        pennello_colore(p, v[i].colore);     riempi_tondo(p, x, y, 48, 20, RAGGIO_CASELLA);
        pennello_colore(p, BORDO_CONTROLLO); contorna_tondo(p, x, y, 48, 20, RAGGIO_CASELLA, 1);
        pennello_colore(p, TESTO_PRIMARIO);
        testo(p, FONT_PICCOLO_NUOVO, x + 56, y + 3, v[i].nome);
    }
    ombre(p, x0, y0, t);
}

/* le ombre: un rettangolo arrotondato per livello, nell'ordine di gkit_disegno.h (esterne,
 * fondo, interne). Il fondo e' SUPERFICIE_ELEVATA tranne per l'incassato (SUPERFICIE_INCASSATA). */
static void ombre(struct pennello *p, int x0, int y0, const struct tema *t) {
    int scuro = t == &tema_scuro;
#define LIVELLO(N) ombre_##N.strati[scuro], ombre_##N.quanti[scuro]
    struct { const char *nome; const struct strato_ombra *s; int n; int r; } v[] = {
        { "incassato",       LIVELLO(incassato),       RAGGIO_CAMPO_M },
        { "sollevato",       LIVELLO(sollevato),       RAGGIO_CAMPO_M },
        { "sollevato_pieno", LIVELLO(sollevato_pieno), RAGGIO_CAMPO_M },
        { "sovrapposto",     LIVELLO(sovrapposto),     RAGGIO_PANNELLO },
        { "barra",           LIVELLO(barra),           RAGGIO_PANNELLO },
        { "finestra",        LIVELLO(finestra),        RAGGIO_PANNELLO },
        { "focus",           LIVELLO(focus),           RAGGIO_CAMPO_M },
        { "puntatore",       LIVELLO(puntatore),       RAGGIO_CAMPO_M },
    };
#undef LIVELLO
    for (int i = 0; i < 8; i++) {
        int x = x0 + PAN_M + 24 + (i % 4) * 140, y = y0 + 48 + 15 * RIGA_H + 24 + (i / 4) * 120;
        struct tondo r = tondo(x, y, 96, 56, v[i].r);
        ombre_strati_fuori(p, &r, v[i].s, v[i].n);
        pennello_colore(p, i == 0 ? SUPERFICIE_INCASSATA : SUPERFICIE_ELEVATA);
        riempi_forma(p, &r);
        ombre_strati_dentro(p, &r, v[i].s, v[i].n);
        pennello_colore(p, TESTO_SECONDARIO);
        testo(p, FONT_PICCOLO_NUOVO, x, y + 56 + 22, v[i].nome);
    }
}

static void disegna_token(struct widget *w, struct pennello *p) {
    const struct tema *prima = tema_corrente;
    pannello(p, w->x, w->y, &tema_chiaro, "Chiaro", w->a);
    pannello(p, w->x + PAN_L + 16, w->y, &tema_scuro, "Scuro", w->a);
    tema_corrente = prima;
}
static void misura_token(struct widget *w, int *l, int *a) {
    (void)w; *l = PAN_L * 2 + 16; *a = 48 + 15 * RIGA_H + 24 + 2 * 120 + 12;
}
static const struct tipo_widget tipo_token = { .misura = misura_token, .disegna = disegna_token };

static struct finestra_tk finestra;
static struct widget lavagna = { .tipo = &tipo_token };

int main(void) {
    finestra_tk_prepara(&finestra, "Token", &lavagna);
    return uikit_esegui(&finestra);
}
