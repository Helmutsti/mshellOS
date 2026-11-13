/* UIKit, i componenti piccoli: le informazioni su una cosa (un file), righe "nome  valore" in
 * una mini finestra. Qui il widget delle righe e la mini finestra preparata; aprirla
 * (informazioni_mostra) e' in informazioni_mostra.c, perche' parla con il ciclo degli eventi.
 * Le dichiarazioni e il disegno sono in informazioni.h. */
#include "uikit/uikit.h"

static void informazioni_misura(struct widget *w, int *l, int *a) {
    struct informazioni_tk *in = (struct informazioni_tk *)w;
    *l = INFORMAZIONI_LARGHEZZA - 2 * SPAZIO_16;
    *a = in->quante ? in->quante * INTERLINEA_PICCOLO + (in->quante - 1) * INFORMAZIONI_TRA : 0;
}

static void informazioni_disegna(struct widget *w, struct pennello *p) {
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
static int informazioni_tasto(struct finestra_tk *f, unsigned int codice, unsigned int modificatori,
                              void *dati) {
    (void)modificatori; (void)dati;
    if (codice != TASTO_ESC) return 0;
    finestra_tk_chiudi(f);
    return 1;
}

void informazioni_prepara(struct finestra_tk *mini, struct informazioni_tk *in) {
    *in = (struct informazioni_tk){ .base = { .tipo = &tipo_informazioni, .largo = 1 } };
    finestra_tk_prepara(mini, "Informazioni", &in->base);
    finestra_tk_mini(mini);
    finestra_tk_ascolta_tasti(mini, informazioni_tasto, 0);
}

void informazioni_svuota(struct informazioni_tk *in) { in->quante = 0; widget_cambiato(&in->base); }

void informazioni_riga(struct informazioni_tk *in, const char *etichetta, const char *valore) {
    if (in->quante >= MAX_RIGHE_INFORMAZIONI) return;
    copia_testo(in->etichette[in->quante], etichetta, MAX_TESTO_WIDGET);
    copia_testo(in->valori[in->quante], valore, MAX_CELLA);
    in->quante++;
    widget_cambiato(&in->base);
}
