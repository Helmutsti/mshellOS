/* Quello che UIKit da' al compositore (vedi sistema/uikit_compositore.h): la barra del titolo
 * congelata, l'ombra e il vetro delle finestre, con lo stile del sistema (stile.h), cosi' sono
 * uguali a quelli delle applicazioni. */
#include "gkit/gkit.h"   /* MAX_TITOLO */
#include "uikit/stile.h"
#include "uikit/uikit.h"   /* testo_centrato, larghezza_testo_stile */
#include "uikit/sistema/uikit_compositore.h"

/* Le misure della barra, come la testata della finestra di UIKit (finestra_tk, uikit.h): alta
 * TITOLO_ALTEZZA (40), o ALTEZZA_BARRA_MINI (28) nella mini finestra; i comandi tondi da
 * ALTEZZA_S (32), o ALTEZZA_XS (24), a destra, a TITOLO_MARGINE dal bordo e l'uno
 * dall'altro, centrati in altezza. La mini finestra ha solo chiudi. */
static int congelata_alta(int mini) { return mini ? ALTEZZA_BARRA_MINI : TITOLO_ALTEZZA; }
static int congelata_lato(int mini) { return mini ? ALTEZZA_XS : ALTEZZA_S; }
/* dove sta un comando (riduci, chiudi), dentro la finestra larga l */
static int congelata_comando_x(int l, int mini, int chiudi) {
    int lato = congelata_lato(mini);
    return l - TITOLO_MARGINE - lato - (chiudi ? 0 : lato + TITOLO_MARGINE);
}
static int congelata_comando_y(int mini) { return (congelata_alta(mini) - congelata_lato(mini)) / 2; }

int barra_congelata_altezza(int mini) { return congelata_alta(mini); }

int barra_congelata_sul_chiudi(int larghezza, int mini, int x, int y) {
    int cx = congelata_comando_x(larghezza, mini, 1), cy = congelata_comando_y(mini);
    int lato = congelata_lato(mini);
    return x >= cx && x < cx + lato && y >= cy && y < cy + lato;
}

/* La barra di una finestra che non risponde, al posto di quella dell'applicazione: il vetro
 * della finestra (SUPERFICIE_VETRO) con sopra un velo rosso, l'avviso e il titolo al centro, e
 * i comandi tondi come quelli di UIKit, col "chiudi" in rosso. Con i soli angoli in alto
 * arrotondati: il divisore e tutto il resto sono ancora dell'applicazione (il compositore
 * disegna la sua immagine da barra_congelata_altezza in giu').
 *
 * IL FONDO SCURO. Prima qui c'era VETRO_FONDO sotto il vetro, per far leggere il testo
 * bianco sopra uno sfondo chiaro. Il nuovo vetro ha il suo testo (scuro sul vetro chiaro,
 * chiaro sul vetro scuro): il fondo serve solo a contraddire il tema, e non c'e' piu'. */
void barra_congelata_disegna(struct pennello *p, int x, int y, int l, int a, const char *titolo,
                             int mini) {
    int alta = congelata_alta(mini), lato = congelata_lato(mini);
    int segno = mini ? ICONA_XS : ICONA_S;
    const struct font *font = mini ? FONT_ETICHETTA : FONT_TITOLO_FINESTRA;
    struct tondo barra = { x, y, l, alta, { RAGGIO_FINESTRA_NUOVO, RAGGIO_FINESTRA_NUOVO, 0, 0 } };
    pennello_colore(p, SUPERFICIE_VETRO);
    riempi_forma(p, &barra);
    pennello_colore(p, STATO_ERRORE_VELO);
    riempi_forma(p, &barra);

    char avviso[MAX_TITOLO + 16];
    int n = 0;
    for (const char *c = titolo; *c; c++) avviso[n++] = *c;
    for (const char *c = " non risponde"; *c; c++) avviso[n++] = *c;
    avviso[n] = 0;
    int largo = segno + GAP_S + larghezza_testo(font, avviso);
    int ax = x + (l - largo) / 2;
    pennello_salva(p);
    pennello_ritaglia(p, x, y, l, alta);
    pennello_colore(p, TESTO_PRIMARIO);
    icona(p, ICONA_TRIANGLE_ALERT, segno, ax, y + (alta - segno) / 2);
    testo(p, font, ax + segno + GAP_S, y + (alta - font->altezza) / 2, avviso);
    pennello_ripristina(p);

    for (int chiudi = mini ? 1 : 0; chiudi < 2; chiudi++) {
        int cx = x + congelata_comando_x(l, mini, chiudi), cy = y + congelata_comando_y(mini);
        pennello_colore(p, CONTROLLO_SFONDO_NUOVO);
        riempi_cerchio(p, cx + lato / 2, cy + lato / 2, lato / 2);
        pennello_colore(p, chiudi ? STATO_ERRORE : COLORE_ICONA(COLORE_ICONA_PRIMARIO));
        icona(p, chiudi ? ICONA_X : ICONA_MINUS, segno, cx + (lato - segno) / 2, cy + (lato - segno) / 2);
    }
    struct tondo tutta = tondo(x, y, l, a, RAGGIO_FINESTRA_NUOVO);
    pennello_colore(p, STATO_ERRORE);
    contorna_forma(p, &tutta, 256);
}

/* --- LA CORNICE di una finestra che non e' di UIKit (Wayland) ---
 * Una finestra Wayland disegna solo il suo contenuto: la cornice (il vetro, la testata con il
 * titolo, i comandi riduci e chiudi, il filo di luce interno) la mette il compositore, con
 * lo stesso disegno della finestra di UIKit (finestra_tk.c: finestra_tk_contenuto_disegna e
 * finestra_tk_titolo_disegna). La testata e' quella della finestra normale (40 di altezza,
 * comandi da 32); la finestra di UIKit disegna il fondo, il titolo e i comandi; qui, uguale. */
int cornice_testata_altezza(void) { return congelata_alta(0); }
int cornice_raggio(void) { return RAGGIO_FINESTRA_NUOVO; }

int cornice_comando(int l, int x, int y) {
    if (x < 0 || x >= l || y < 0 || y >= congelata_alta(0)) return CORNICE_NIENTE;
    if (barra_congelata_sul_chiudi(l, 0, x, y)) return CORNICE_CHIUDI;
    int cx = congelata_comando_x(l, 0, 0), cy = congelata_comando_y(0), lato = congelata_lato(0);
    if (x >= cx && x < cx + lato && y >= cy && y < cy + lato) return CORNICE_RIDUCI;
    return CORNICE_TESTATA;
}

void cornice_disegna(struct pennello *p, int l, int a, const char *titolo, int attiva) {
    int alta = congelata_alta(0), lato = congelata_lato(0);
    pulisci(p);
    struct tondo contenuto = { 0, alta, l, a - alta, { 0, 0, RAGGIO_FINESTRA_NUOVO, RAGGIO_FINESTRA_NUOVO } };
    pennello_colore(p, SUPERFICIE_VETRO);
    riempi_forma(p, &contenuto);
    struct tondo testata = { 0, 0, l, alta, { RAGGIO_FINESTRA_NUOVO, RAGGIO_FINESTRA_NUOVO, 0, 0 } };
    riempi_forma(p, &testata);

    /* il titolo al centro, tra lo spazio vuoto a sinistra (largo quanto i comandi) e i comandi */
    const struct font *font = FONT_TITOLO_FINESTRA;
    pennello_salva(p);
    int margine = TITOLO_COMANDI_LARGHEZZA + SPAZIO_8;
    pennello_ritaglia(p, margine, 0, l - 2 * margine, alta);
    pennello_colore(p, attiva ? TESTO_PRIMARIO : TESTO_SECONDARIO);
    testo_centrato(p, font, (l - larghezza_testo_stile(font, titolo)) / 2, 0, alta, titolo);
    pennello_ripristina(p);

    /* i comandi, senza fondo a riposo (il puntatore su di loro non si segue: nessun seat) */
    pennello_salva(p);
    if (!attiva) pennello_opacita(p, OPACITA_COMANDI_INATTIVI);
    pennello_colore(p, COLORE_ICONA(COLORE_ICONA_PRIMARIO));
    for (int chiudi = 0; chiudi < 2; chiudi++) {
        int cx = congelata_comando_x(l, 0, chiudi), cy = congelata_comando_y(0);
        icona(p, chiudi ? ICONA_X : ICONA_MINUS, ICONA_S, cx + (lato - ICONA_S) / 2, cy + (lato - ICONA_S) / 2);
    }
    pennello_ripristina(p);

    struct tondo tutta = tondo(0, 0, l, a, RAGGIO_FINESTRA_NUOVO);
    ombre_strati_dentro(p, &tutta, OMBRA_TAB(finestra), OMBRA_N(finestra));
}

void velo_finestra_disegna(struct pennello *p, int x, int y, int l, int a, int raggio) {
    struct tondo velo = tondo(x, y, l, a, raggio);
    pennello_colore(p, SUPERFICIE_VELO);
    riempi_forma(p, &velo);
}

const struct strato_ombra *ombra_finestra(int *quanti) {
    *quanti = OMBRA_N(finestra);
    return OMBRA_TAB(finestra);
}

const struct strato_ombra *ombra_pannello(int sopra, int *quanti) {
    if (sopra) {
        *quanti = OMBRA_N(sovrapposto);
        return OMBRA_TAB(sovrapposto);
    }
    *quanti = OMBRA_N(barra);
    return OMBRA_TAB(barra);
}

int sfocatura_finestra(void) { return OMBRA_FINESTRA_SFOCATURA_SFONDO; }

void compositore_scegli_tema(const struct catalog_valori *v) { tema_da_catalog(v); }
