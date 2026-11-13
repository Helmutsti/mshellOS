/* UIKit: i materiali del sistema, caricati dai file la prima volta che servono (vedi
 * materiali.h). I file li legge GKit (gkit/interno/materiali_file.c); qui c'e' solo quale file e'
 * di quale materiale, e il ricordo di quelli gia' caricati. */
#include "materiali.h"

#define MATERIALE_FILE(nome, file) file,
static const char *const file_font[FONT_DEL_SISTEMA] = { MATERIALI_FONT(MATERIALE_FILE) };
static const char *const file_immagine[IMMAGINI_DEL_SISTEMA] = { MATERIALI_IMMAGINI(MATERIALE_FILE) };
static const char *const file_icona[ICONE_DEL_SISTEMA] = { MATERIALI_ICONE(MATERIALE_FILE) };
#undef MATERIALE_FILE

static struct font font[FONT_DEL_SISTEMA];
static struct immagine immagine[IMMAGINI_DEL_SISTEMA];
static struct icona icone[ICONE_DEL_SISTEMA][ICONA_MISURE];
static signed char icone_quante[ICONE_DEL_SISTEMA];   /* 0 = non ancora caricata */
static unsigned char font_pronto[FONT_DEL_SISTEMA], immagine_pronta[IMMAGINI_DEL_SISTEMA];

/* (un numero fuori elenco non succede: i numeri vengono dai nomi di materiali_elenco.h;
 * per sicurezza diventa il primo) */
static int nell_elenco(int quale, int quanti) { return quale >= 0 && quale < quanti ? quale : 0; }

const struct font *font_del_sistema(int quale) {
    quale = nell_elenco(quale, FONT_DEL_SISTEMA);
    if (!font_pronto[quale]) {
        char percorso[PERCORSO_RISORSA_MAX];
        percorso_risorsa(percorso, sizeof percorso, file_font[quale]);
        font_da_file(percorso, &font[quale]);
        font_pronto[quale] = 1;
    }
    return &font[quale];
}

const struct immagine *immagine_del_sistema(int quale) {
    quale = nell_elenco(quale, IMMAGINI_DEL_SISTEMA);
    if (!immagine_pronta[quale]) {
        char percorso[PERCORSO_RISORSA_MAX];
        percorso_risorsa(percorso, sizeof percorso, file_immagine[quale]);
        immagine_da_file(percorso, &immagine[quale]);
        immagine_pronta[quale] = 1;
    }
    return &immagine[quale];
}

const struct icona *icona_del_sistema(int quale, int *quante) {
    quale = nell_elenco(quale, ICONE_DEL_SISTEMA);
    if (!icone_quante[quale]) {
        char percorso[PERCORSO_RISORSA_MAX];
        percorso_risorsa(percorso, sizeof percorso, file_icona[quale]);
        int n = icona_da_file(percorso, icone[quale], ICONA_MISURE);
        icone_quante[quale] = (signed char)(n > 0 ? n : 1);   /* se manca: una misura vuota */
    }
    *quante = icone_quante[quale];
    return icone[quale];
}
