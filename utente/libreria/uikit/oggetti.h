/* GLI OGGETTI: le icone di file e cartelle del disegno nuovo (il componente "Oggetto" del file
 * Figma, pagina "Componenti base"). Due misure ottiche: GRANDE 48 px (tratto 2, per i tile
 * della griglia) e PICCOLA 24 px (tratto 1,5, per le righe delle tabelle e gli elenchi).
 *
 * Il tipo e' il piu' specifico che c'e' (la regola "la variante piu' specifica"):
 *   - una cartella con un ruolo ha il suo simbolo: la casa (Home), la Scrivania, i Documenti,
 *     le Immagini, i Download, le applicazioni; le altre sono la cartella semplice;
 *   - un file con un'estensione che il disegno conosce (TXT, PDF, PNG, ZIP, BIN) ha la sua
 *     variante: nella misura grande e' il foglio con la scritta, nella piccola un simbolo;
 *     gli altri sono il file semplice.
 *
 * I COLORI vengono dal tema (oggetto/riempimento-forte, oggetto/riempimento, oggetto/tratto):
 * i dati (materiali_dati.h, da strumenti/grafica/genera.py) dicono per ogni pixel quanto c'e'
 * di ognuno dei tre, e li si mescola qui con i colori del tema in uso. Livello piatto: niente
 * ombre. Lo stato (sopra, scelto) non e' dell'oggetto ma di chi lo contiene (il tile, la riga).
 *
 * C'e' anche il CAMPO DEL NOME NUOVO (lo stato "Rinomina" dell'icona, che il disegno elenca tra
 * quelli da aggiungere): un campo di testo incassato con il fuoco, come campo_disegna, che
 * prende il posto del nome nel tile o nella riga. */
#ifndef OGGETTI_H
#define OGGETTI_H

enum {
    OGGETTO_FILE, OGGETTO_CARTELLA,
    OGGETTO_TXT, OGGETTO_PDF, OGGETTO_PNG, OGGETTO_ZIP, OGGETTO_BIN,
    OGGETTO_HOME, OGGETTO_SCRIVANIA, OGGETTO_DOCUMENTI, OGGETTO_IMMAGINI, OGGETTO_DOWNLOAD,
    OGGETTO_APPLICAZIONI,
    OGGETTI                            /* quanti sono */
};
enum { OGGETTO_GRANDE, OGGETTO_PICCOLO };

#define OGGETTO_LATO_GRANDE  ICONA_OGGETTO            /* 48 */
#define OGGETTO_LATO_PICCOLO ICONA_OGGETTO_PICCOLA    /* 24 */
#define OGGETTO_ETICHETTA_Y  23    /* la scritta (TXT...) sul foglio grande: la cima, dal disegno */

/* I dati, per tipo: [tipo][0 grande, 1 piccola]. I file con l'etichetta, nella misura
 * grande, sono tutti lo stesso foglio; la scritta e' in oggetto_etichetta. */
static const struct immagine *const oggetti_dati[OGGETTI][2] = {
    [OGGETTO_FILE]         = { &oggetto_file_grande,         &oggetto_file_piccola },
    [OGGETTO_CARTELLA]     = { &oggetto_cartella_grande,     &oggetto_cartella_piccola },
    [OGGETTO_TXT]          = { &oggetto_etichetta_grande,    &oggetto_txt_piccola },
    [OGGETTO_PDF]          = { &oggetto_etichetta_grande,    &oggetto_pdf_piccola },
    [OGGETTO_PNG]          = { &oggetto_etichetta_grande,    &oggetto_png_piccola },
    [OGGETTO_ZIP]          = { &oggetto_etichetta_grande,    &oggetto_zip_piccola },
    [OGGETTO_BIN]          = { &oggetto_etichetta_grande,    &oggetto_bin_piccola },
    [OGGETTO_HOME]         = { &oggetto_home_grande,         &oggetto_home_piccola },
    [OGGETTO_SCRIVANIA]    = { &oggetto_scrivania_grande,    &oggetto_scrivania_piccola },
    [OGGETTO_DOCUMENTI]    = { &oggetto_documenti_grande,    &oggetto_documenti_piccola },
    [OGGETTO_IMMAGINI]     = { &oggetto_immagini_grande,     &oggetto_immagini_piccola },
    [OGGETTO_DOWNLOAD]     = { &oggetto_download_grande,     &oggetto_download_piccola },
    [OGGETTO_APPLICAZIONI] = { &oggetto_applicazioni_grande, &oggetto_applicazioni_piccola },
};

/* La scritta del foglio grande, o 0. */
static inline const char *oggetto_etichetta(int quale) {
    switch (quale) {
    case OGGETTO_TXT: return "TXT";
    case OGGETTO_PDF: return "PDF";
    case OGGETTO_PNG: return "PNG";
    case OGGETTO_ZIP: return "ZIP";
    case OGGETTO_BIN: return "BIN";
    default:          return 0;
    }
}

static inline int oggetto_e_cartella(int quale) {
    return quale == OGGETTO_CARTELLA || (quale >= OGGETTO_HOME && quale <= OGGETTO_APPLICAZIONI);
}

/* Mescola i tre colori (opachi) del tema con i pesi del pixel: premoltiplicato, come le immagini. */
static inline unsigned int oggetto_mescola(unsigned int pesi, unsigned int forte, unsigned int pieno,
                                           unsigned int tratto) {
    unsigned int a = pesi >> 24, r = pesi >> 16 & 0xFF, g = pesi >> 8 & 0xFF, b = pesi & 0xFF;
    unsigned int out = a << 24;
    for (int s = 0; s <= 16; s += 8) {
        unsigned int v = (r * (forte >> s & 0xFF) + g * (pieno >> s & 0xFF) + b * (tratto >> s & 0xFF) + 127) / 255;
        out |= (v > a ? a : v) << s;
    }
    return out;
}

/* Disegna l'oggetto 'quale' (OGGETTO_...) nella misura OGGETTO_GRANDE o OGGETTO_PICCOLO, con
 * l'angolo in (x, y), nei colori del tema. */
static inline void oggetto(struct pennello *p, int quale, int misura, int x, int y) {
    if (quale < 0 || quale >= OGGETTI) quale = OGGETTO_FILE;
    const struct immagine *dati = oggetti_dati[quale][misura == OGGETTO_PICCOLO];
    static unsigned int pixel[OGGETTO_LATO_GRANDE * OGGETTO_LATO_GRANDE];
    int n = dati->l * dati->a;
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

/* --- quale oggetto: dal nome di un file, dal percorso di una cartella --- */

static inline char oggetto_minuscola(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 'a' - 'A') : c; }

static inline int oggetto_uguali(const char *a, const char *b) {
    while (*a && oggetto_minuscola(*a) == oggetto_minuscola(*b)) { a++; b++; }
    return oggetto_minuscola(*a) == oggetto_minuscola(*b);
}

/* Un file, dal nome: la variante della sua estensione, se il disegno ce l'ha. */
static inline int oggetto_di_file(const char *nome) {
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

/* La casa della persona, come la vedono le app (vedi STATO.md, "Le cartelle nel sistema"). */
#define OGGETTO_CASA "/home"

/* Una cartella, dal suo percorso intero: le cartelle con un ruolo (la casa e le sue
 * Scrivania, Documenti, Immagini, Download; le cartelle delle applicazioni) hanno il loro
 * simbolo, le altre sono la cartella semplice. */
static inline int oggetto_di_cartella(const char *percorso) {
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

/* --- il campo del nome nuovo (lo stato Rinomina) --- */

#define RINOMINA_DENTRO   SPAZIO_8     /* nel campo, ai lati del testo (nel tile: SPAZIO_4) */
#define RINOMINA_CURSORE  2            /* la larghezza del cursore */

/* Il campo largo 'l' e alto 'a', raggio 'raggio': incassato con il fuoco come campo_disegna
 * (ombre "focus", SUPERFICIE_INCASSATA, bordo BORDO_FOCUS), con dentro il testo e il cursore in
 * fondo. Il testo centrato se 'centrato'; se e' piu' lungo del campo si vede la fine. */
static inline void campo_rinomina_disegna(struct pennello *p, int x, int y, int l, int a, int raggio,
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

/* Un testo su una riga, tagliato con "..." se e' piu' largo di 'largo' (il font non ha "…"),
 * centrato in verticale nel rettangolo alto 'a'. Allineato a destra (finisce in x + largo) se
 * 'destra'. Lo usano le righe e le voci che hanno "text-ellipsis" nel disegno. */
static inline void testo_tagliato(struct pennello *p, const struct font *f, int x, int y, int a, int largo,
                                  const char *s, int destra) {
    char riga[1][MAX_RIGA_NOME];
    const char *t = s;
    if (larghezza_testo(f, s) > largo) {
        testo_a_capo(f, s, largo, riga, 1);
        t = riga[0];
    }
    testo_centrato(p, f, destra ? x + largo - larghezza_testo(f, t) : x, y, a, t);
}

#endif
