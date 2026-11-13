/* UIKit, i componenti: gli oggetti (le icone di file e cartelle, nei colori del tema), quale
 * oggetto per un file o una cartella, il testo tagliato con "..." e il campo del nome nuovo
 * (lo stato Rinomina). Le dichiarazioni sono in oggetti.h; il campo del nome nuovo, che
 * usano la tabella e la griglia, in interno/nucleo.h. */
#include "uikit/uikit.h"
#include "uikit/interno/nucleo.h"

/* I dati, per tipo: [tipo][0 grande, 1 piccola]. I file con l'etichetta, nella misura
 * grande, sono tutti lo stesso foglio; la scritta e' in oggetto_etichetta. I numeri sono
 * quelli dei materiali (immagine_del_sistema, materiali.h). */
static const unsigned char oggetti_dati[OGGETTI][2] = {
    [OGGETTO_FILE]         = { MATERIALE_oggetto_file_grande,         MATERIALE_oggetto_file_piccola },
    [OGGETTO_CARTELLA]     = { MATERIALE_oggetto_cartella_grande,     MATERIALE_oggetto_cartella_piccola },
    [OGGETTO_TXT]          = { MATERIALE_oggetto_etichetta_grande,    MATERIALE_oggetto_txt_piccola },
    [OGGETTO_PDF]          = { MATERIALE_oggetto_etichetta_grande,    MATERIALE_oggetto_pdf_piccola },
    [OGGETTO_PNG]          = { MATERIALE_oggetto_etichetta_grande,    MATERIALE_oggetto_png_piccola },
    [OGGETTO_ZIP]          = { MATERIALE_oggetto_etichetta_grande,    MATERIALE_oggetto_zip_piccola },
    [OGGETTO_BIN]          = { MATERIALE_oggetto_etichetta_grande,    MATERIALE_oggetto_bin_piccola },
    [OGGETTO_HOME]         = { MATERIALE_oggetto_home_grande,         MATERIALE_oggetto_home_piccola },
    [OGGETTO_SCRIVANIA]    = { MATERIALE_oggetto_scrivania_grande,    MATERIALE_oggetto_scrivania_piccola },
    [OGGETTO_DOCUMENTI]    = { MATERIALE_oggetto_documenti_grande,    MATERIALE_oggetto_documenti_piccola },
    [OGGETTO_IMMAGINI]     = { MATERIALE_oggetto_immagini_grande,     MATERIALE_oggetto_immagini_piccola },
    [OGGETTO_DOWNLOAD]     = { MATERIALE_oggetto_download_grande,     MATERIALE_oggetto_download_piccola },
    [OGGETTO_APPLICAZIONI] = { MATERIALE_oggetto_applicazioni_grande, MATERIALE_oggetto_applicazioni_piccola },
};

/* La scritta del foglio grande, o 0. */
static const char *oggetto_etichetta(int quale) {
    switch (quale) {
    case OGGETTO_TXT: return "TXT";
    case OGGETTO_PDF: return "PDF";
    case OGGETTO_PNG: return "PNG";
    case OGGETTO_ZIP: return "ZIP";
    case OGGETTO_BIN: return "BIN";
    default:          return 0;
    }
}

int oggetto_e_cartella(int quale) {
    return quale == OGGETTO_CARTELLA || (quale >= OGGETTO_HOME && quale <= OGGETTO_APPLICAZIONI);
}

/* Mescola i tre colori (opachi) del tema con i pesi del pixel: premoltiplicato, come le immagini. */
static unsigned int oggetto_mescola(unsigned int pesi, unsigned int forte, unsigned int pieno,
                                    unsigned int tratto) {
    unsigned int a = pesi >> 24, r = pesi >> 16 & 0xFF, g = pesi >> 8 & 0xFF, b = pesi & 0xFF;
    unsigned int out = a << 24;
    for (int s = 0; s <= 16; s += 8) {
        unsigned int v = (r * (forte >> s & 0xFF) + g * (pieno >> s & 0xFF) + b * (tratto >> s & 0xFF) + 127) / 255;
        out |= (v > a ? a : v) << s;
    }
    return out;
}

void oggetto(struct pennello *p, int quale, int misura, int x, int y) {
    if (quale < 0 || quale >= OGGETTI) quale = OGGETTO_FILE;
    const struct immagine *dati = immagine_del_sistema(oggetti_dati[quale][misura == OGGETTO_PICCOLO]);
    static unsigned int pixel[OGGETTO_LATO_GRANDE * OGGETTO_LATO_GRANDE];
    int n = dati->l * dati->a;
    if (n > OGGETTO_LATO_GRANDE * OGGETTO_LATO_GRANDE) return;   /* un file sbagliato, troppo grande */
    unsigned int forte = OGGETTO_RIEMPIMENTO_FORTE, pieno = OGGETTO_RIEMPIMENTO, tratto = OGGETTO_TRATTO;
    for (int i = 0; i < n; i++) pixel[i] = oggetto_mescola(dati->pixel[i], forte, pieno, tratto);
    struct immagine colorata = { dati->l, dati->a, pixel };
    disegna_immagine(p, &colorata, x, y);
    const char *scritta = misura == OGGETTO_GRANDE ? oggetto_etichetta(quale) : 0;
    if (scritta) {
        pennello_colore(p, OGGETTO_TRATTO);
        testo_centrato(p, FONT_ETICHETTA, x + (dati->l - larghezza_testo(FONT_ETICHETTA, scritta)) / 2,
                       y + OGGETTO_ETICHETTA_Y, INTERLINEA_ETICHETTA, scritta);
    }
}

void elemento_disegna_oggetto(struct pennello *p, int x, int y, int alto, int quale,
                              const char *nome, int stato) {
    elemento_fondo(p, x, y, alto, stato);
    oggetto(p, quale, OGGETTO_GRANDE, x + (ELEMENTO_LARGHEZZA - OGGETTO_LATO_GRANDE) / 2,
            y + ELEMENTO_MARGINE + (ELEMENTO_ICONA - OGGETTO_LATO_GRANDE) / 2);
    elemento_nome(p, x, y, nome);
}

static char oggetto_minuscola(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 'a' - 'A') : c; }

static int oggetto_uguali(const char *a, const char *b) {
    while (*a && oggetto_minuscola(*a) == oggetto_minuscola(*b)) { a++; b++; }
    return oggetto_minuscola(*a) == oggetto_minuscola(*b);
}

int oggetto_di_file(const char *nome) {
    const char *punto = 0;
    for (const char *c = nome; *c; c++) if (*c == '.') punto = c;
    if (!punto || punto == nome) return OGGETTO_FILE;
    static const struct { const char *estensione; int oggetto; } note[] = {
        { "txt", OGGETTO_TXT }, { "pdf", OGGETTO_PDF }, { "png", OGGETTO_PNG },
        { "zip", OGGETTO_ZIP }, { "bin", OGGETTO_BIN },
    };
    for (unsigned int i = 0; i < sizeof note / sizeof note[0]; i++)
        if (oggetto_uguali(punto + 1, note[i].estensione)) return note[i].oggetto;
    return OGGETTO_FILE;
}

int oggetto_di_cartella(const char *percorso) {
    static const struct { const char *percorso; int oggetto; } ruoli[] = {
        { OGGETTO_CASA,                OGGETTO_HOME },
        { OGGETTO_CASA "/Scrivania",   OGGETTO_SCRIVANIA },
        { OGGETTO_CASA "/Documenti",   OGGETTO_DOCUMENTI },
        { OGGETTO_CASA "/Immagini",    OGGETTO_IMMAGINI },
        { OGGETTO_CASA "/Download",    OGGETTO_DOWNLOAD },
        { "/system/apps",              OGGETTO_APPLICAZIONI },
        { "/data/apps",                OGGETTO_APPLICAZIONI },
    };
    for (unsigned int i = 0; i < sizeof ruoli / sizeof ruoli[0]; i++) {
        const char *a = percorso, *b = ruoli[i].percorso;
        while (*a && *a == *b) { a++; b++; }
        if (!*b && (!*a || (*a == '/' && !a[1]))) return ruoli[i].oggetto;
    }
    return OGGETTO_CARTELLA;
}

void campo_rinomina_disegna(struct pennello *p, int x, int y, int l, int a, int raggio,
                            const char *testo_nome, int centrato) {
    const struct font *f = FONT_CORPO;
    struct tondo t = tondo(x, y, l, a, raggio);
    ombre_strati_fuori(p, &t, OMBRA_TAB(focus), OMBRA_N(focus));
    pennello_colore(p, SUPERFICIE_INCASSATA);
    riempi_forma(p, &t);
    ombre_strati_dentro(p, &t, OMBRA_TAB(focus), OMBRA_N(focus));
    pennello_colore(p, BORDO_FOCUS);
    contorna_forma(p, &t, BORDO_FOCUS_SPESSORE);
    int dentro = a < ALTEZZA_S ? SPAZIO_4 : RINOMINA_DENTRO;
    int largo = larghezza_testo(f, testo_nome) + CAMPO_CURSORE_SPAZIO + RINOMINA_CURSORE;
    int spazio = l - 2 * dentro;
    int tx = centrato && largo < spazio ? x + (l - largo) / 2 : x + dentro;
    if (largo > spazio) tx = x + l - dentro - largo;
    pennello_salva(p);
    pennello_ritaglia(p, x + 2, y + 1, l - 4, a - 2);
    pennello_colore(p, TESTO_PRIMARIO);
    int fine = testo_centrato(p, f, tx, y, a, testo_nome);
    riempi_rettangolo(p, fine + CAMPO_CURSORE_SPAZIO, y + (a - f->altezza) / 2, RINOMINA_CURSORE, f->altezza);
    pennello_ripristina(p);
}

void testo_tagliato(struct pennello *p, const struct font *f, int x, int y, int a, int largo,
                    const char *s, int destra) {
    char riga[1][MAX_RIGA_NOME];
    const char *t = s;
    if (larghezza_testo(f, s) > largo) {
        testo_a_capo(f, s, largo, riga, 1);
        t = riga[0];
    }
    testo_centrato(p, f, destra ? x + largo - larghezza_testo(f, t) : x, y, a, t);
}
