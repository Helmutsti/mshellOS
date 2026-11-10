/* La libreria delle finestre: il protocollo (protocollo.h) visto dall'applicazione.
 * E' il primo strato della grafica, come libwayland su Linux: apre la finestra, da' una
 * tela libera su cui disegnare, la fa mostrare, consegna gli eventi. COME disegnare
 * (rettangoli, testo...) non e' affar suo.
 *
 *     struct finestra f;
 *     finestra_apri(&f, 300, 200, "Ciao");
 *     unsigned int *tela = finestra_tela(&f);   ... si disegna: tela[y * 300 + x] = 0xRRGGBB
 *     finestra_mostra(&f);
 *     struct notizia n;
 *     while (finestra_notizia(&f, &n)) { ... }
 *
 * LE DUE IMMAGINI. finestra_tela da' sempre un'immagine che il compositore non sta
 * usando: se sono tutte e due sue, aspetta che ne restituisca una. Le notizie arrivate
 * nel frattempo (un clic, un tasto) si mettono da parte, e finestra_notizia le
 * consegnera' dopo, in ordine. */
#ifndef FINESTRA_H
#define FINESTRA_H

#include "libreria.h"
#include "protocollo.h"

#define DA_PARTE 16

struct finestra {
    int collegamento, foglio;
    int larghezza, altezza;
    unsigned int *immagini[2];
    int libera[2];             /* 1 se l'immagine e' nostra, 0 se e' del compositore */
    int in_uso;                /* l'immagine data dall'ultima finestra_tela */
    struct notizia messe_da_parte[DA_PARTE];
    int quante;
};

/* Si collega al compositore; se non e' ancora pronto riprova per qualche secondo. */
static inline int collega_compositore(void) {
    int d = -1;
    for (int prove = 0; prove < 100 && d < 0; prove++) {
        d = collega(NOME_COMPOSITORE);
        if (d < 0) dormi(50);
    }
    return d;
}

/* Quanto e' grande lo schermo: lo si chiede al compositore. 0, o -1. */
static inline int finestra_schermo(int *larghezza, int *altezza) {
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

/* Come finestra_apri, ma con un ruolo speciale (RUOLO_SFONDO, RUOLO_PANNELLO: solo per i
 * programmi del sistema), il posto sullo schermo e il raggio degli angoli. */
static inline int finestra_apri_come(struct finestra *f, int larghezza, int altezza,
                                     const char *titolo, unsigned int ruolo, int x, int y,
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

    struct voglio_finestra m = { .tipo = VOGLIO_FINESTRA, .larghezza = larghezza, .altezza = altezza,
                                 .ruolo = ruolo, .x = x, .y = y, .raggio = raggio };
    for (int i = 0; i < MAX_TITOLO - 1 && titolo[i]; i++) m.titolo[i] = titolo[i];
    return manda_foglio(f->collegamento, &m, sizeof m, f->foglio) < 0 ? -1 : 0;
}

/* Si collega al compositore (se non e' ancora pronto riprova per qualche secondo), crea
 * il foglio con le due immagini e chiede una finestra normale. 0 se fatto, -1 se no. */
static inline int finestra_apri(struct finestra *f, int larghezza, int altezza, const char *titolo) {
    return finestra_apri_come(f, larghezza, altezza, titolo, RUOLO_FINESTRA, 0, 0, RAGGIO_PREDEFINITO);
}

/* Riceve un messaggio dal compositore. Le immagini restituite si segnano qui, e al
 * "ci sei?" si risponde qui: l'applicazione non lo vede nemmeno. Finche' lei torna ad
 * aspettare le sue notizie, la risposta parte; se resta bloccata altrove, no, ed e'
 * proprio cosi' che il compositore se ne accorge.
 * 1 se e' arrivato qualcosa, 0 se il compositore ha chiuso. */
static inline int finestra_ricevi(struct finestra *f, struct notizia *n) {
    if (ricevi(f->collegamento, n, sizeof *n) != sizeof *n) return 0;
    if (n->tipo == RILASCIATA && n->immagine < 2) f->libera[n->immagine] = 1;
    if (n->tipo == PING) {
        struct pong p = { .tipo = PONG, .numero = n->numero };
        if (manda(f->collegamento, &p, sizeof p) < 0) return 0;
    }
    return 1;
}

/* I messaggi che la libreria sbriga da sola, senza passarli all'applicazione. */
static inline int finestra_di_servizio(const struct notizia *n) {
    return n->tipo == RILASCIATA || n->tipo == PING;
}

/* Un'immagine libera, su cui disegnare. 0 se il compositore ha chiuso. */
static inline unsigned int *finestra_tela(struct finestra *f) {
    while (!f->libera[0] && !f->libera[1]) {
        struct notizia n;
        if (!finestra_ricevi(f, &n)) return 0;
        if (!finestra_di_servizio(&n) && f->quante < DA_PARTE) f->messe_da_parte[f->quante++] = n;
    }
    f->in_uso = f->libera[0] ? 0 : 1;
    return f->immagini[f->in_uso];
}

/* L'immagine data dall'ultima finestra_tela e' pronta: il compositore la mostra. */
static inline int finestra_mostra(struct finestra *f) {
    struct mostra m = { .tipo = MOSTRA, .immagine = f->in_uso };
    f->libera[f->in_uso] = 0;
    return manda(f->collegamento, &m, sizeof m);
}

/* Da chiamare quando si preme sulla propria barra del titolo: il compositore sposta la
 * finestra seguendo il mouse, finche' il pulsante resta premuto. */
static inline int finestra_trascinami(struct finestra *f) {
    struct trascinami m = { .tipo = TRASCINAMI };
    return manda(f->collegamento, &m, sizeof m);
}

/* La prossima notizia (un clic, un tasto...); dorme se non ce n'e'. 0 se il
 * compositore ha chiuso. */
static inline int finestra_notizia(struct finestra *f, struct notizia *n) {
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

#endif
