/* IL MENU, dal file Figma (pagina "Componenti complessi", sezione "Menu"): il menu del tasto
 * destro e i menu delle applicazioni. Livello SOVRAPPOSTO: fondo SUPERFICIE_ELEVATA, raggio
 * RAGGIO_PANNELLO, ombra "sovrapposto". E' fatto SOLO di tre pezzi, ognuno un componente del
 * disegno:
 *
 *   - la VOCE DI MENU, S (32) o M (40), rientrata di SPAZIO_8 dai lati del menu, con SPAZIO_8 di
 *     padding dentro: [simbolo 18] [testo "corpo"] ... [scorciatoia "piccolo" secondaria | freccia
 *     del sottomenu 16 | spunta 16]. Piatta; sotto il puntatore il fondo CONTROLLO_SFONDO (raggio
 *     RAGGIO_CONTROLLO_NUOVO); Disabilitata: testo e scorciatoia TESTO_DISABILITATO, non reagisce.
 *     Se una voce di un gruppo ha il simbolo ce l'hanno tutte (lo decide chi scrive le voci);
 *   - il SEPARATORE: una linea BORDO_SEPARATORE da bordo a bordo, SPAZIO_4 sopra e sotto (9);
 *   - l'INTESTAZIONE: un titolo di gruppo, "etichetta" in TESTO_SECONDARIO, padding 8 sopra, 4
 *     sotto, 8 ai lati (alta 28). Non si clicca. La usa anche la barra laterale.
 *
 * LO SPAZIO, come vuole la regola della spaziatura: dal bordo del menu al primo testo ci sono 16
 * pixel, in x (8 di rientro della voce + 8 dentro) e in y (il padding del menu + lo spazio
 * sopra il testo nella voce: 10 + 6 con le voci S, 6 + 10 con le M; con un'intestazione in cima,
 * 8 + 8). Il padding verticale del menu si calcola cosi' (MENU_PADDING_Y), quello di sotto e'
 * lo stesso.
 *
 * IL SOTTOMENU: una voce con 'sottomenu' ha la freccia a destra; sotto il puntatore (o al clic)
 * si apre accanto, a destra (a sinistra se a destra non ci sta), con la prima voce alla stessa
 * altezza della voce che lo apre.
 *
 * COME SI USA. Il menu e' un widget che si aggiunge per ultimo alla finestra (sta sopra a
 * tutto); nascosto finche' non lo si apre. Aperto, copre tutta la finestra: un clic fuori lo
 * chiude (e il clic finisce li', come sul Mac). Esc lo chiude. Scegliere una voce lo chiude e
 * chiama 'al_scelta' con la sua azione. Se una voce si puo' scegliere, e se ha la spunta, lo
 * chiede a 'attiva' e 'spuntata' (0: sempre attiva, mai spuntata).
 *
 *     static const struct voce_menu_tk voci[] = {
 *         { APRI, "Apri", NESSUNA_ICONA, "Invio" },
 *         { MENU_VOCE_SEPARATORE },
 *         { ELIMINA, "Elimina", NESSUNA_ICONA, "Canc" },
 *     };
 *     menu_prepara(&menu, MISURA_S, 220, attiva, 0, scelta, 0);
 *     menu_aggiungi(&finestra, &menu);
 *     ...
 *     menu_apri(&menu, voci, 3, x, y);              (x, y: l'angolo, nella finestra) */
#ifndef MENU_H
#define MENU_H

#define MENU_VOCE_SEPARATORE    (-1)
#define MENU_VOCE_INTESTAZIONE  (-2)

#define MENU_LARGHEZZA_NORMALE     240           /* quella del componente; Files usa 220, come nel suo disegno */
#define MENU_RIENTRO       SPAZIO_8      /* tra il bordo del menu e la voce */
#define VOCE_MENU_DENTRO   SPAZIO_8      /* dentro la voce, ai lati */
#define VOCE_MENU_GAP      GAP_M         /* tra simbolo, testo e cosa a destra */
#define VOCE_MENU_SIMBOLO  ICONA_M       /* 18 */
#define VOCE_MENU_DESTRA   ICONA_S       /* 16: la freccia del sottomenu, la spunta */
#define MENU_PRIMO_TESTO   SPAZIO_16     /* dal bordo del menu al primo testo, in x e in y */
#define SEPARATORE_MENU_ALTO (SPAZIO_4 + 1 + SPAZIO_4)
#define INTESTAZIONE_MENU_SOPRA SPAZIO_8
#define INTESTAZIONE_MENU_SOTTO SPAZIO_4
#define INTESTAZIONE_MENU_ALTA (INTESTAZIONE_MENU_SOPRA + INTERLINEA_ETICHETTA + INTESTAZIONE_MENU_SOTTO)   /* 28 */
#define INTESTAZIONE_MENU_DENTRO SPAZIO_8
#define SOTTOMENU_STACCO   SPAZIO_4      /* tra il menu e il sottomenu (il disegno non lo dice) */
#define MENU_BORDO         SPAZIO_4      /* il menu non va piu' vicino di cosi' al bordo della finestra */

/* Una voce: un'azione (un numero dell'applicazione, >= 0), MENU_VOCE_SEPARATORE o
 * MENU_VOCE_INTESTAZIONE (il testo e' il titolo). */
struct voce_menu_tk {
    int azione;
    const char *testo;
    int icona;                             /* ICONA_..., o NESSUNA_ICONA (0 vale l'icona 0!) */
    const char *scorciatoia;               /* "Ctrl+D", o 0 */
    const struct voce_menu_tk *sottomenu;  /* le voci del sottomenu, o 0 */
    int quante_sotto;
};

/* L'intestazione di un gruppo (in un menu o nella barra laterale): il testo a
 * INTESTAZIONE_MENU_DENTRO dal bordo sinistro 'x', con la cima della riga in y. */
static inline void intestazione_menu_disegna(struct pennello *p, int x, int y, const char *titolo) {
    pennello_colore(p, TESTO_SECONDARIO);
    testo_centrato(p, FONT_ETICHETTA, x + INTESTAZIONE_MENU_DENTRO, y + INTESTAZIONE_MENU_SOPRA,
                   INTERLINEA_ETICHETTA, titolo);
}

struct menu_tk;
typedef void (*al_scelta_menu)(struct menu_tk *m, int azione, void *dati);
typedef int (*domanda_menu)(int azione, void *dati);

struct pannello_menu_tk {
    const struct voce_menu_tk *voci;
    int quante;
    int x, y;                              /* nella finestra */
};

struct menu_tk {
    struct widget base;                    /* aperto, copre tutta la finestra */
    int misura;                            /* MISURA_S (voci da 32) o MISURA_M (40) */
    int larghezza;
    struct pannello_menu_tk pannelli[2];   /* il menu e il sottomenu */
    int quanti;                            /* 0 chiuso, 1, 2 con il sottomenu */
    int sopra;                             /* la voce sotto il puntatore: pannello * 100 + voce, o -1 */
    int premuta;                           /* quella su cui si e' premuto, o -1 */
    int aperta_da;                         /* la voce del menu che ha aperto il sottomenu, o -1 */
    domanda_menu attiva, spuntata;
    al_scelta_menu al_scelta;
    void *dati;
};

static inline int menu_voce_alta(const struct menu_tk *m) { return m->misura == MISURA_M ? ALTEZZA_M : ALTEZZA_S; }

/* Il padding sopra (dipende da cosa c'e' in cima) e sotto il menu. */
static inline int menu_padding_sotto(const struct menu_tk *m) {
    return MENU_PRIMO_TESTO - (menu_voce_alta(m) - INTERLINEA_CORPO) / 2;
}
static inline int menu_padding_sopra(const struct menu_tk *m, const struct pannello_menu_tk *pn) {
    return pn->quante && pn->voci[0].azione == MENU_VOCE_INTESTAZIONE
               ? MENU_PRIMO_TESTO - INTESTAZIONE_MENU_SOPRA : menu_padding_sotto(m);
}

static inline int menu_alta_voce(const struct menu_tk *m, const struct voce_menu_tk *v) {
    return v->azione == MENU_VOCE_SEPARATORE ? SEPARATORE_MENU_ALTO
         : v->azione == MENU_VOCE_INTESTAZIONE ? INTESTAZIONE_MENU_ALTA : menu_voce_alta(m);
}

static inline int menu_altezza(const struct menu_tk *m, const struct pannello_menu_tk *pn) {
    int a = menu_padding_sopra(m, pn) + menu_padding_sotto(m);
    for (int i = 0; i < pn->quante; i++) a += menu_alta_voce(m, &pn->voci[i]);
    return a;
}

/* Dove comincia la voce i del pannello (y nella finestra). */
static inline int menu_voce_y(const struct menu_tk *m, const struct pannello_menu_tk *pn, int i) {
    int y = pn->y + menu_padding_sopra(m, pn);
    for (int k = 0; k < i; k++) y += menu_alta_voce(m, &pn->voci[k]);
    return y;
}

static inline int menu_voce_attiva(const struct menu_tk *m, int azione) {
    return !m->attiva || m->attiva(azione, m->dati);
}

/* LA VOCE DI MENU: nel rettangolo (x, y, l, a) che ha gia' il rientro. 'sopra': sotto il
 * puntatore (o con il suo sottomenu aperto). */
static inline void voce_menu_disegna(struct pennello *p, const struct voce_menu_tk *v, int x, int y, int l,
                                     int a, int attiva, int sopra, int spuntata) {
    if (sopra && attiva) {
        pennello_colore(p, CONTROLLO_SFONDO_NUOVO);
        riempi_tondo(p, x, y, l, a, RAGGIO_CONTROLLO_NUOVO);
    }
    int tx = x + VOCE_MENU_DENTRO, destra = x + l - VOCE_MENU_DENTRO;
    /* le icone nel colore d'icona Primario (Attenuato se la voce e' spenta), come i ghost */
    pennello_colore(p, COLORE_ICONA(attiva ? COLORE_ICONA_PRIMARIO : COLORE_ICONA_ATTENUATO));
    if (v->icona != NESSUNA_ICONA) {
        icona(p, v->icona, VOCE_MENU_SIMBOLO, tx, y + (a - VOCE_MENU_SIMBOLO) / 2);
        tx += VOCE_MENU_SIMBOLO + VOCE_MENU_GAP;
    }
    int fine_testo = destra;                   /* il testo si taglia prima della cosa a destra */
    if (v->sottomenu || spuntata) {
        icona(p, v->sottomenu ? ICONA_CHEVRON_RIGHT : ICONA_CHECK, VOCE_MENU_DESTRA,
              destra - VOCE_MENU_DESTRA, y + (a - VOCE_MENU_DESTRA) / 2);
        fine_testo -= VOCE_MENU_DESTRA + VOCE_MENU_GAP;
    } else if (v->scorciatoia) {
        int sl = larghezza_testo(FONT_PICCOLO_NUOVO, v->scorciatoia);
        pennello_colore(p, attiva ? TESTO_SECONDARIO : TESTO_DISABILITATO);
        testo_centrato(p, FONT_PICCOLO_NUOVO, destra - sl, y, a, v->scorciatoia);
        fine_testo -= sl + VOCE_MENU_GAP;
    }
    pennello_colore(p, attiva ? TESTO_PRIMARIO : TESTO_DISABILITATO);
    testo_tagliato(p, FONT_CORPO, tx, y, a, fine_testo - tx, v->testo, 0);
}

static inline void menu_pannello_disegna(struct menu_tk *m, struct pennello *p, int numero) {
    const struct pannello_menu_tk *pn = &m->pannelli[numero];
    struct tondo t = tondo(pn->x, pn->y, m->larghezza, menu_altezza(m, pn), RAGGIO_PANNELLO);
    ombre_strati_fuori(p, &t, OMBRA_TAB(sovrapposto), OMBRA_N(sovrapposto));
    pennello_colore(p, SUPERFICIE_ELEVATA);
    riempi_forma(p, &t);
    ombre_strati_dentro(p, &t, OMBRA_TAB(sovrapposto), OMBRA_N(sovrapposto));
    int x = pn->x + MENU_RIENTRO, l = m->larghezza - 2 * MENU_RIENTRO;
    for (int i = 0; i < pn->quante; i++) {
        const struct voce_menu_tk *v = &pn->voci[i];
        int y = menu_voce_y(m, pn, i), a = menu_alta_voce(m, v);
        if (v->azione == MENU_VOCE_SEPARATORE) {
            pennello_colore(p, BORDO_SEPARATORE);
            riempi_rettangolo(p, pn->x, y + SPAZIO_4, m->larghezza, 1);
        } else if (v->azione == MENU_VOCE_INTESTAZIONE) {
            intestazione_menu_disegna(p, x, y, v->testo);
        } else {
            int sopra = m->sopra == numero * 100 + i || (numero == 0 && m->quanti > 1 && m->aperta_da == i);
            voce_menu_disegna(p, v, x, y, l, a, menu_voce_attiva(m, v->azione), sopra,
                              m->spuntata && m->spuntata(v->azione, m->dati));
        }
    }
}

static inline void menu_disegna(struct widget *w, struct pennello *p) {
    struct menu_tk *m = (struct menu_tk *)w;
    for (int i = 0; i < m->quanti; i++) menu_pannello_disegna(m, p, i);
}

/* La voce nel punto (x, y): pannello * 100 + voce (non i separatori e le intestazioni), -1 se
 * il punto e' fuori dai pannelli, -2 se e' dentro ma non su una voce. */
static inline int menu_voce_in(const struct menu_tk *m, int x, int y) {
    for (int n = m->quanti - 1; n >= 0; n--) {
        const struct pannello_menu_tk *pn = &m->pannelli[n];
        if (x < pn->x || x >= pn->x + m->larghezza || y < pn->y || y >= pn->y + menu_altezza(m, pn)) continue;
        if (x < pn->x + MENU_RIENTRO || x >= pn->x + m->larghezza - MENU_RIENTRO) return -2;
        for (int i = 0; i < pn->quante; i++) {
            int vy = menu_voce_y(m, pn, i), a = menu_alta_voce(m, &pn->voci[i]);
            if (y >= vy && y < vy + a) return pn->voci[i].azione < 0 ? -2 : n * 100 + i;
        }
        return -2;
    }
    return -1;
}

static inline const struct voce_menu_tk *menu_voce_di(const struct menu_tk *m, int id) {
    return id < 0 ? 0 : &m->pannelli[id / 100].voci[id % 100];
}

/* COSA RIDISEGNARE. Il menu copre tutta la finestra (per sentire il clic fuori), ma si vede
 * solo nei suoi pannelli: si ridisegnano solo loro (con l'ombra, che sborda) o solo le voci
 * che si accendono e si spengono. */
static inline void menu_segna_pannello(struct menu_tk *m, const struct pannello_menu_tk *pn) {
    int s = ombra_sporgenza(OMBRA_TAB(sovrapposto), OMBRA_N(sovrapposto));
    widget_cambiato_zona(&m->base, pn->x - s, pn->y - s, m->larghezza + 2 * s, menu_altezza(m, pn) + 2 * s);
}

static inline void menu_segna_voce(struct menu_tk *m, int id) {
    if (id < 0 || id / 100 >= m->quanti) return;
    const struct pannello_menu_tk *pn = &m->pannelli[id / 100];
    int i = id % 100;
    if (i >= pn->quante) return;
    widget_cambiato_zona(&m->base, pn->x, menu_voce_y(m, pn, i), m->larghezza, menu_alta_voce(m, &pn->voci[i]));
}

/* Tutti i pannelli aperti. */
static inline void menu_segna(struct menu_tk *m) {
    for (int n = 0; n < m->quanti; n++) menu_segna_pannello(m, &m->pannelli[n]);
}

/* Il sottomenu della voce i del menu: accanto, a destra (a sinistra se non ci sta). */
static inline void menu_apri_sottomenu(struct menu_tk *m, int i) {
    const struct pannello_menu_tk *pn = &m->pannelli[0];
    const struct voce_menu_tk *v = &pn->voci[i];
    struct pannello_menu_tk *sotto = &m->pannelli[1];
    *sotto = (struct pannello_menu_tk){ v->sottomenu, v->quante_sotto, pn->x + m->larghezza + SOTTOMENU_STACCO, 0 };
    sotto->y = menu_voce_y(m, pn, i) - menu_padding_sopra(m, sotto);
    if (sotto->x + m->larghezza > m->base.l - MENU_BORDO) sotto->x = pn->x - m->larghezza - SOTTOMENU_STACCO;
    if (sotto->x < MENU_BORDO) sotto->x = MENU_BORDO;
    int a = menu_altezza(m, sotto);
    if (sotto->y + a > m->base.a - MENU_BORDO) sotto->y = m->base.a - MENU_BORDO - a;
    m->quanti = 2;
    m->aperta_da = i;
}

static inline void menu_chiudi(struct menu_tk *m);

/* Premuto fuori dai pannelli: il menu si chiude. Lasciato sulla stessa voce su cui si e'
 * premuto: si fa quello che dice (una voce con il sottomenu lo apre). */
static inline void menu_puntatore(struct widget *w, int x, int y, unsigned int pulsanti) {
    struct menu_tk *m = (struct menu_tk *)w;
    int id = menu_voce_in(m, x, y);
    if (pulsanti & (PULSANTE_SINISTRO | PULSANTE_DESTRO)) {
        if (m->premuta == -1) m->premuta = id;
        if (id == -1) menu_chiudi(m);
        return;
    }
    int premuta = m->premuta;
    m->premuta = -1;
    if (id < 0 || id != premuta) return;
    const struct voce_menu_tk *v = menu_voce_di(m, id);
    if (!menu_voce_attiva(m, v->azione)) return;
    if (v->sottomenu) {
        menu_segna(m);
        menu_apri_sottomenu(m, id % 100);
        menu_segna(m);
        return;
    }
    int azione = v->azione;
    menu_chiudi(m);
    if (m->al_scelta) m->al_scelta(m, azione, m->dati);
}

/* Il puntatore sopra le voci: si accendono; sopra una voce con il sottomenu lo si apre, sopra
 * un'altra voce del menu lo si chiude. */
static inline void menu_movimento(struct widget *w, int x, int y) {
    struct menu_tk *m = (struct menu_tk *)w;
    int id = menu_voce_in(m, x, y);
    if (id < -1) id = -1;
    if (id == m->sopra) return;
    /* prima: la voce accesa e, se c'e', il sottomenu con la voce che lo apre */
    int prima_quanti = m->quanti, prima_da = m->aperta_da;
    struct pannello_menu_tk prima_sotto = m->pannelli[1];
    menu_segna_voce(m, m->sopra);
    m->sopra = id;
    const struct voce_menu_tk *v = menu_voce_di(m, id);
    if (v && id / 100 == 0) {
        if (v->sottomenu && menu_voce_attiva(m, v->azione)) menu_apri_sottomenu(m, id % 100);
        else m->quanti = 1;
    }
    menu_segna_voce(m, id);
    int cambiato = prima_quanti != m->quanti ||
                   (m->quanti > 1 && (prima_da != m->aperta_da || prima_sotto.voci != m->pannelli[1].voci));
    if (cambiato && prima_quanti > 1) {
        menu_segna_pannello(m, &prima_sotto);
        menu_segna_voce(m, prima_da);
    }
    if (cambiato && m->quanti > 1) {
        menu_segna_pannello(m, &m->pannelli[1]);
        menu_segna_voce(m, m->aperta_da);
    }
}

/* Il puntatore e' uscito dal menu (dalla finestra, o la finestra ha perso il fuoco): la voce
 * accesa si spegne. */
static inline void menu_sopra_cambiato(struct widget *w) {
    struct menu_tk *m = (struct menu_tk *)w;
    if (w->sopra || m->sopra < 0) return;
    menu_segna_voce(m, m->sopra);
    m->sopra = -1;
}

static const struct tipo_widget tipo_menu_tk = {
    0, 0, menu_disegna, menu_puntatore, 0, 0, menu_sopra_cambiato, menu_movimento, 0
};

static inline void menu_prepara(struct menu_tk *m, int misura, int larghezza, domanda_menu attiva,
                                domanda_menu spuntata, al_scelta_menu al_scelta, void *dati) {
    *m = (struct menu_tk){ .base = { .tipo = &tipo_menu_tk, .nascosto = 1 }, .misura = misura,
                           .larghezza = larghezza ? larghezza : MENU_LARGHEZZA_NORMALE, .sopra = -1, .premuta = -1,
                           .aperta_da = -1, .attiva = attiva, .spuntata = spuntata, .al_scelta = al_scelta,
                           .dati = dati };
}

/* Il menu sta sopra a tutto: lo si aggiunge alla finestra dopo tutto il resto. */
static inline void menu_aggiungi(struct finestra_tk *f, struct menu_tk *m) {
    widget_aggiungi(&f->base, &m->base);
}

static inline int menu_aperto(const struct menu_tk *m) { return m->quanti > 0; }

/* Esc chiude il menu (lo ascolta solo mentre e' aperto). */
static inline int menu_tasto(struct finestra_tk *f, unsigned int codice, unsigned int modificatori, void *dati) {
    (void)f; (void)modificatori;
    struct menu_tk *m = dati;
    if (!menu_aperto(m)) return 0;
    if (codice == TASTO_ESC) { menu_chiudi(m); return 1; }
    return 1;                                  /* con il menu aperto, le scorciatoie non arrivano sotto */
}

/* Apre il menu con l'angolo nel punto (x, y) della finestra (dove si e' cliccato), spostandolo
 * quanto basta perche' stia tutto nella finestra (sotto la testata). */
static inline void menu_apri(struct menu_tk *m, const struct voce_menu_tk *voci, int quante, int x, int y) {
    struct finestra_tk *f = widget_finestra(&m->base);
    if (!m->base.nascosto) menu_segna(m);    /* era gia' aperto altrove */
    m->quanti = 1;
    m->sopra = m->premuta = m->aperta_da = -1;
    m->base.x = m->base.y = 0;
    m->base.l = f->base.l;
    m->base.a = f->base.a;
    struct pannello_menu_tk *pn = &m->pannelli[0];
    *pn = (struct pannello_menu_tk){ voci, quante, x, y };
    int a = menu_altezza(m, pn);
    if (pn->x + m->larghezza > m->base.l - MENU_BORDO) pn->x = m->base.l - MENU_BORDO - m->larghezza;
    if (pn->y + a > m->base.a - MENU_BORDO) pn->y = m->base.a - MENU_BORDO - a;
    if (pn->x < MENU_BORDO) pn->x = MENU_BORDO;
    if (pn->y < testata_altezza(f)) pn->y = testata_altezza(f);
    m->base.nascosto = 0;
    menu_segna(m);
    if (f->base.tipo == &tipo_finestra_tk) {
        uikit_sotto(f, 0);
        finestra_tk_ascolta_tasti(f, menu_tasto, m);
    }
}

static inline void menu_chiudi(struct menu_tk *m) {
    if (!m->quanti) return;
    menu_segna(m);
    m->quanti = 0;
    m->base.nascosto = 1;
    struct finestra_tk *f = widget_finestra(&m->base);
    if (f->base.tipo == &tipo_finestra_tk) {
        finestra_tk_smetti_tasti(f, menu_tasto, m);
        uikit_sotto(f, 0);                     /* il puntatore e' ancora li': si ritrova il widget sotto */
    }
}

#endif
