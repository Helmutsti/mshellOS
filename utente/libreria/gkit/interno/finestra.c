/* GKit: la finestra, il lato dell'applicazione (le dichiarazioni, con cosa fa ogni funzione,
 * sono in gkit.h; i messaggi che si scambiano con il compositore in sistema/protocollo.h). */
#include "corekit/corekit.h"
#include "gkit/gkit.h"
#include "gkit/sistema/protocollo.h"

/* Si collega al compositore; se non e' ancora pronto riprova per qualche secondo. */
static int collega_compositore(void) {
    int d = -1;
    for (int prove = 0; prove < 100 && d < 0; prove++) {
        d = collega(NOME_COMPOSITORE);
        if (d < 0) dormi(50);
    }
    return d;
}

int finestra_schermo(int *larghezza, int *altezza) {
    int d = collega_compositore();
    if (d < 0) return -1;
    struct chiedi_schermo m = { .tipo = CHIEDI_SCHERMO };
    struct notizia n;
    int fatto = manda(d, &m, sizeof m) >= 0 && ricevi(d, &n, sizeof n) == sizeof n && n.tipo == SCHERMO;
    chiudi(d);
    if (!fatto) return -1;
    *larghezza = n.x;
    *altezza = n.y;
    return 0;
}

/* Si collega al compositore e chiede una superficie: il lavoro di finestra_apri_come e di
 * finestra_apri_pannello. */
static int finestra_apri_superficie(struct finestra *f, int larghezza, int altezza, const char *titolo,
                                    unsigned int ruolo, unsigned int pannello, int x, int y,
                                    unsigned int raggio) {
    f->collegamento = collega_compositore();
    if (f->collegamento < 0) return -1;

    unsigned int pixel = larghezza * altezza;
    f->foglio = foglio(2 * pixel * 4);
    unsigned int *memoria = f->foglio < 0 ? 0 : mappa(f->foglio, 0);
    if (!memoria) return -1;
    f->larghezza = larghezza;
    f->altezza = altezza;
    f->immagini[0] = memoria;
    f->immagini[1] = memoria + pixel;
    f->libera[0] = f->libera[1] = 1;
    f->in_uso = 0;
    f->quante = 0;
    f->quante_arretrate[0] = f->quante_arretrate[1] = -1;   /* tutte e due da fare da capo */

    struct voglio_finestra m = { .tipo = VOGLIO_FINESTRA, .larghezza = larghezza, .altezza = altezza,
                                 .ruolo = ruolo, .x = x, .y = y, .raggio = raggio,
                                 .pannello = pannello };
    for (int i = 0; i < MAX_TITOLO - 1 && titolo[i]; i++) m.titolo[i] = titolo[i];
    return manda_foglio(f->collegamento, &m, sizeof m, f->foglio) < 0 ? -1 : 0;
}

int finestra_apri_come(struct finestra *f, int larghezza, int altezza, const char *titolo,
                       unsigned int ruolo, int x, int y, unsigned int raggio) {
    return finestra_apri_superficie(f, larghezza, altezza, titolo, ruolo, PANNELLO_BARRA, x, y, raggio);
}

int finestra_apri_pannello(struct finestra *f, int larghezza, int altezza, const char *titolo,
                           unsigned int pannello, int x, int y, unsigned int raggio) {
    return finestra_apri_superficie(f, larghezza, altezza, titolo, RUOLO_PANNELLO, pannello, x, y,
                                    raggio);
}

void finestra_chiudi(struct finestra *f) {
    if (f->collegamento >= 0) chiudi(f->collegamento);
    if (f->immagini[0]) togli(f->immagini[0]);
    if (f->foglio >= 0) chiudi(f->foglio);
    f->collegamento = f->foglio = -1;
    f->immagini[0] = f->immagini[1] = 0;
}

int finestra_apri(struct finestra *f, int larghezza, int altezza, const char *titolo) {
    return finestra_apri_come(f, larghezza, altezza, titolo, RUOLO_FINESTRA, 0, 0, RAGGIO_PREDEFINITO);
}

int finestra_ricevi(struct finestra *f, struct notizia *n) {
    if (ricevi(f->collegamento, n, sizeof *n) != sizeof *n) return 0;
    if (n->tipo == RILASCIATA && n->immagine < 2) f->libera[n->immagine] = 1;
    if (n->tipo == PING) {
        struct pong p = { .tipo = PONG, .numero = n->numero };
        if (manda(f->collegamento, &p, sizeof p) < 0) return 0;
    }
    return 1;
}

int finestra_di_servizio(const struct notizia *n) {
    return n->tipo == RILASCIATA || n->tipo == PING;
}

unsigned int *finestra_tela(struct finestra *f) {
    while (!f->libera[0] && !f->libera[1]) {
        struct notizia n;
        if (!finestra_ricevi(f, &n)) return 0;
        if (!finestra_di_servizio(&n) && f->quante < DA_PARTE) f->messe_da_parte[f->quante++] = n;
    }
    f->in_uso = f->libera[0] ? 0 : 1;
    return f->immagini[f->in_uso];
}

int finestra_libera(const struct finestra *f) { return f->libera[0] || f->libera[1]; }

int finestra_arretrate(const struct finestra *f, struct zona_finestra *zone) {
    int n = f->quante_arretrate[f->in_uso];
    for (int k = 0; k < n; k++) zone[k] = f->arretrate[f->in_uso][k];
    return n;
}

int finestra_mostra_zone(struct finestra *f, const struct zona_finestra *zone, int quante) {
    struct mostra m = { .tipo = MOSTRA, .immagine = f->in_uso };
    struct zona_finestra tutta = { 0, 0, f->larghezza, f->altezza };
    int n = 0;
    for (int k = 0; k < quante; k++) zone_aggiungi(m.zone, &n, MAX_ZONE_MOSTRA, zona_dentro(zone[k], tutta));
    if (quante > 0 && !n) {            /* niente di cambiato dentro la finestra: una zona vuota */
        m.zone[0] = (struct zona_finestra){ 0, 0, 0, 0 };
        n = 1;
    }
    m.quante = (unsigned int)n;
    int altra = 1 - f->in_uso;
    if (quante <= 0) f->quante_arretrate[altra] = -1;
    else if (f->quante_arretrate[altra] >= 0)
        for (int k = 0; k < n; k++)
            zone_aggiungi(f->arretrate[altra], &f->quante_arretrate[altra], MAX_ZONE_MOSTRA, m.zone[k]);
    f->quante_arretrate[f->in_uso] = 0;
    f->libera[f->in_uso] = 0;
    return manda(f->collegamento, &m, sizeof m);
}

int finestra_mostra(struct finestra *f) { return finestra_mostra_zone(f, 0, 0); }

int finestra_trascinami(struct finestra *f) {
    struct trascinami m = { .tipo = TRASCINAMI };
    return manda(f->collegamento, &m, sizeof m);
}

int finestra_tasti_fisici(struct finestra *f) {
    struct voglio_tasti_fisici m = { .tipo = VOGLIO_TASTI_FISICI };
    return manda(f->collegamento, &m, sizeof m);
}

int finestra_riducimi(struct finestra *f) {
    struct riducimi m = { .tipo = RIDUCIMI };
    return manda(f->collegamento, &m, sizeof m);
}

int finestra_forma_puntatore(struct finestra *f, unsigned int forma) {
    struct mostra_puntatore m = { .tipo = MOSTRA_PUNTATORE, .forma = forma };
    return manda(f->collegamento, &m, sizeof m);
}

int finestra_cambia_tema(struct finestra *f) {
    struct cambia_tema m = { .tipo = CAMBIA_TEMA };
    return manda(f->collegamento, &m, sizeof m);
}

int finestra_notizia(struct finestra *f, struct notizia *n) {
    if (f->quante) {
        *n = f->messe_da_parte[0];
        f->quante--;
        for (int i = 0; i < f->quante; i++) f->messe_da_parte[i] = f->messe_da_parte[i + 1];
        return 1;
    }
    do {
        if (!finestra_ricevi(f, n)) return 0;
    } while (finestra_di_servizio(n));
    return 1;
}
