/* LE INFORMAZIONI, dal file Figma ("Finestra mini - Informazioni", nella sezione "Finestra" di
 * "Componenti complessi"): le informazioni su una cosa (un file), in una MINI FINESTRA a parte,
 * larga INFORMAZIONI_LARGHEZZA (320): la barra del titolo mini (28, il titolo "etichetta" e la
 * sola X), sotto il contenuto con SPAZIO_16 di padding (quello della mini finestra) e le righe a
 * SPAZIO_8 l'una dall'altra. Ogni riga e' alta INTERLINEA_PICCOLO (16): a sinistra il nome in
 * "piccolo" TESTO_SECONDARIO, a destra il valore in "piccolo" TESTO_PRIMARIO (tagliato con
 * "..." se non ci sta).
 *
 *     static struct finestra_tk mini;
 *     static struct informazioni_tk info;
 *     informazioni_prepara(&mini, &info);
 *     informazioni_riga(&info, "Dimensione", "412 KB"); ...
 *     informazioni_mostra(&finestra, &mini, &info, "Informazioni - foto.png");
 *
 * Si chiude con la sua X o con Esc. */
#ifndef INFORMAZIONI_H
#define INFORMAZIONI_H

#define INFORMAZIONI_LARGHEZZA 320
#define INFORMAZIONI_TRA       SPAZIO_8
#define MAX_RIGHE_INFORMAZIONI 8

struct informazioni_tk {
    struct widget base;
    char etichette[MAX_RIGHE_INFORMAZIONI][MAX_TESTO_WIDGET];
    char valori[MAX_RIGHE_INFORMAZIONI][MAX_CELLA];
    int quante;
};

static inline void informazioni_misura(struct widget *w, int *l, int *a) {
    struct informazioni_tk *in = (struct informazioni_tk *)w;
    *l = INFORMAZIONI_LARGHEZZA - 2 * SPAZIO_16;
    *a = in->quante ? in->quante * INTERLINEA_PICCOLO + (in->quante - 1) * INFORMAZIONI_TRA : 0;
}

static inline void informazioni_disegna(struct widget *w, struct pennello *p) {
    struct informazioni_tk *in = (struct informazioni_tk *)w;
    for (int i = 0; i < in->quante; i++) {
        int y = w->y + i * (INTERLINEA_PICCOLO + INFORMAZIONI_TRA);
        int el = larghezza_testo(FONT_PICCOLO_NUOVO, in->etichette[i]);
        pennello_colore(p, TESTO_SECONDARIO);
        testo_centrato(p, FONT_PICCOLO_NUOVO, w->x, y, INTERLINEA_PICCOLO, in->etichette[i]);
        pennello_colore(p, TESTO_PRIMARIO);
        int resta = w->l - el - SPAZIO_16;
        testo_tagliato(p, FONT_PICCOLO_NUOVO, w->x + w->l - resta, y, INTERLINEA_PICCOLO, resta, in->valori[i], 1);
    }
}

static const struct tipo_widget tipo_informazioni = {
    informazioni_misura, 0, informazioni_disegna, 0, 0, 0, 0, 0, 0
};

/* Esc chiude la mini finestra. */
static inline int informazioni_tasto(struct finestra_tk *f, unsigned int codice, unsigned int modificatori,
                                     void *dati) {
    (void)modificatori; (void)dati;
    if (codice != TASTO_ESC) return 0;
    finestra_tk_chiudi(f);
    return 1;
}

/* Prepara la mini finestra 'mini' con dentro le informazioni 'in' (vuote). */
static inline void informazioni_prepara(struct finestra_tk *mini, struct informazioni_tk *in) {
    *in = (struct informazioni_tk){ .base = { .tipo = &tipo_informazioni, .largo = 1 } };
    finestra_tk_prepara(mini, "Informazioni", &in->base);
    finestra_tk_mini(mini);
    finestra_tk_ascolta_tasti(mini, informazioni_tasto, 0);
}

static inline void informazioni_svuota(struct informazioni_tk *in) { in->quante = 0; widget_cambiato(&in->base); }

static inline void informazioni_riga(struct informazioni_tk *in, const char *etichetta, const char *valore) {
    if (in->quante >= MAX_RIGHE_INFORMAZIONI) return;
    copia_testo(in->etichette[in->quante], etichetta, MAX_TESTO_WIDGET);
    copia_testo(in->valori[in->quante], valore, MAX_CELLA);
    in->quante++;
    widget_cambiato(&in->base);
}

/* Apre (o riapre, con le righe nuove) la mini finestra, come l'altra finestra di 'principale'.
 * Il titolo, se e' piu' lungo di quanto ci sta, finisce con "...". */
static inline int informazioni_mostra(struct finestra_tk *principale, struct finestra_tk *mini,
                                      struct informazioni_tk *in, const char *titolo) {
    (void)in;
    copia_testo(mini->titolo, titolo, MAX_TITOLO);
    int n = 0;
    while (titolo[n]) n++;
    if (n > MAX_TITOLO - 1) {
        mini->titolo[MAX_TITOLO - 4] = mini->titolo[MAX_TITOLO - 3] = mini->titolo[MAX_TITOLO - 2] = '.';
        mini->titolo[MAX_TITOLO - 1] = 0;
    }
    return finestra_tk_apri_altra(principale, mini);
}

#endif
